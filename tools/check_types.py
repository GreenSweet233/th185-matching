#!/usr/bin/env python3
"""G1 全局类型一致性检查器 (AGENTS.md §6 G1)。

## 这个脚本检查什么

G1 的原文要求：
  - 所有访问(基类型, 偏移, 宽度, 解释类型)进全局表，冲突即 FAIL。
  - 锁定字段/签名不可被新函数推翻；要改走 conflict proposal。
  - 签名与所有调用点一致。

实现思路：
  1. 用 clang 的 AST JSON 提取 /src 与 /include 里所有 struct/class/enum 的
     字段列表（名字 + 声明类型）。
  2. 按 MSVC x86 (32 位) ABI 计算每个字段的字节偏移与宽度。
  3. 与 /types/lock.json 的已锁定记录比对：
       - 锁定字段的偏移/宽度/类型变了解释 -> FAIL (type_conflict)
       - 新字段与锁定字段布局冲突 -> FAIL
  4. 同一 struct 名在多个 TU 里定义不一致 -> FAIL。

## 为什么自己算偏移而不是用 -fdump-record-layouts

本仓库能用的 clang（emsdk 的 clang 24。0。0git）**不支持**
-fdump-record-layouts。AST JSON 里有字段声明顺序与类型，MSVC x86 的
自然对齐规则是确定性的（4 字节基数，无 padding attribute 时按成员最大对齐），
自己算既可行又可审计。

## 退出码
  0 = PASS
  1 = FAIL (有 type_conflict / 布局冲突 / 跨 TU 不一致)
  2 = 用法或环境错误

## 禁止
  AGENTS.md §12：修改/禁用/绕过本脚本等同任务失败。
"""

import argparse
import json
import os
import re
import subprocess
import sys

# --- MSVC x86 ABI 尺寸表 -------------------------------------------------
# 只覆盖本项目实际会用到的类型。遇到未知类型 -> 记为 unknown，不猜。
TYPE_SIZES = {
    "char": 1, "signed char": 1, "unsigned char": 1, "bool": 1,
    "short": 2, "unsigned short": 2, "short int": 2, "unsigned short int": 2,
    "int": 4, "unsigned int": 4, "unsigned": 4,
    "long": 4, "unsigned long": 4,
    "long long": 8, "unsigned long long": 8, "__int64": 8,
    "float": 4, "double": 8,
    "std::uint8_t": 1, "std::int8_t": 1,
    "std::uint16_t": 2, "std::int16_t": 2,
    "std::uint32_t": 4, "std::int32_t": 4,
    "std::uint64_t": 8, "std::int64_t": 8,
    "u8": 1, "i8": 1, "u16": 2, "i16": 2, "u32": 4, "i32": 4,
    "u64": 8, "i64": 8,
    "size_t": 4, "std::size_t": 4, "ptrdiff_t": 4,
}

# 指针在 x86 上是 4 字节
POINTER_RE = re.compile(r"^(?:const\s+)?(.+?)\s*\*+(?:\s+const)?$")

# 函数指针：本项目用到了 (u32)->i32 这类，仍是 4 字节
FUNC_PTR_RE = re.compile(r"\(.*\)\s*\*$")


def type_size(qual_type, known_structs, ptr_size=4):
    """返回 (size, is_unknown)。未知类型返回 (0, True)，不猜。"""
    t = qual_type.strip()
    # 去掉 const/volatile
    t = re.sub(r"\b(const|volatile)\b", "", t).strip()
    t = re.sub(r"\s+", " ", t)

    if FUNC_PTR_RE.search(t) or POINTER_RE.match(t):
        return ptr_size, False
    # 数组 T[N]
    m = re.match(r"^(.+?)\s*\[\s*(\d+)\s*\]$", t)
    if m:
        inner, n = m.group(1), int(m.group(2))
        sz, unk = type_size(inner, known_structs, ptr_size)
        return sz * n, unk
    if t in TYPE_SIZES:
        return TYPE_SIZES[t], False
    if t in known_structs:
        return known_structs[t], False
    # 枚举按 int
    if t in ("enum " + t, ) or known_structs.get("__enum__" + t):
        return 4, False
    return 0, True


def align_up(value, alignment):
    if alignment <= 1:
        return value
    return (value + alignment - 1) // alignment * alignment


def compute_layout(fields, known_structs, ptr_size=4):
    """按 MSVC x86 规则算偏移。返回 (layout, unknowns)。

    MSVC x86 规则：每个成员按自身对齐（size 与 min(size, pack_default) 取小，
    pack_default 对 x86 是 8），struct 对齐 = 最大成员对齐。
    """
    layout = []
    unknowns = []
    offset = 0
    max_align = 1
    for f in fields:
        name = f.get("name") or "<anonymous>"
        qt = f.get("type") or ""
        size, unk = type_size(qt, known_structs, ptr_size)
        if unk:
            unknowns.append({"field": name, "type": qt})
            # 未知类型：不猜偏移，记 0 并标记，让上层判 FAIL
            layout.append({"name": name, "type": qt, "offset": None, "size": None})
            continue
        align = min(size, 8) if size <= 8 else 8
        if align < 1:
            align = 1
        offset = align_up(offset, align)
        layout.append({"name": name, "type": qt, "offset": offset, "size": size})
        offset += size
        max_align = max(max_align, align)
    total = align_up(offset, max_align)
    return layout, unknowns, total, max_align


def run_clang_ast(clang, cpp_path, include_dirs, std="c++17"):
    """跑 clang -ast-dump=json，返回解析后的 JSON 或 None。"""
    cmd = [clang, "-x", "c++", "-std=" + std, "-fsyntax-only",
           "-Xclang", "-ast-dump=json"]
    for inc in include_dirs:
        cmd += ["-I", inc]
    cmd.append(cpp_path)
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=120)
    except subprocess.TimeoutExpired:
        return None, "clang timeout"
    if not proc.stdout.strip():
        return None, "clang produced no AST: " + (proc.stderr or "")[:400]
    try:
        return json.loads(proc.stdout), None
    except json.JSONDecodeError as exc:
        return None, "AST is not valid JSON: %s" % exc


def collect_records(node, out, source_prefixes, state=None):
    """递归收集 CXXRecordDecl / EnumDecl。

    只收定义（completeDefinition 或 EnumDecl 有 enumerators），
    且只收来自我们源码的（loc.file 在 source_prefixes 下），
    跳过系统头文件里的。
    """
    if not isinstance(node, dict):
        return
    kind = node.get("kind")
    loc = node.get("loc") or {}

    # ---- BUG FIX（G1，2026-11-24）：恢复被 clang 省略的 file 字段 ----
    #
    # clang 的 `-ast-dump=json` **只在 file 变化时才输出 file 键**（省带宽）。
    # 于是「在 hpp 里定义、被 cpp include 进来」的 struct，其 loc 里没有
    # `file`，只有 `includedFrom.file`。旧代码只看 loc.file，判 in_scope=False，
    # 把这些 struct **全部静默丢弃**。
    #
    # 实测后果：include/*.hpp 里有 59 个 struct 定义，G1 只认出 2 个，
    # 却照样报 [PASS] —— 一个几乎没有被测对象的 PASS。
    #
    # 修法：按**文档顺序**遍历（clang 输出即文档顺序），维护一个"当前文件"
    # 状态；loc 没给 file 时沿用上一个。这与 clang 自身的省略语义一致。
    if state is not None:
        if loc.get("file"):
            state["file"] = loc["file"]
        elif loc.get("includedFrom", {}).get("file") and not state.get("file"):
            pass
        f = loc.get("file") or state.get("file") or ""
    else:
        f = loc.get("file") or ""

    # clang 给的 loc.file 可能是相对路径（"include\\Enemy.hpp"），也可能
    # 是绝对路径。用「规范化后是否落在 src/ 或 include/ 下」判定，
    # 不做前缀比较——前缀比较会因相对/绝对混用而永远不匹配，
    # 结果是 definitions_checked=0 却报 PASS（我曾踩过）。
    in_scope = False
    if f:
        # ---- BUG FIX (G1, 2026-11-24): path test must be a REAL containment test.
        #
        # The old code used a substring test
        #     ("/" + pref + "/") in f_norm
        # which also matches Windows SDK paths, because those look like
        #     c:/program files (x86)/windows kits/10/include/10.0.19041.0/ucrt/...
        #                                    ^^^^^^^ contains "/include/"
        # So structs from system headers (__mm_load_ss_struct etc.) were
        # treated as "our types".
        #
        # This stayed hidden only because loc.file was missing; once that was
        # fixed, this latent misclassification immediately pulled ~60 system
        # types into the statistics.
        #
        # Now uses os.path prefix semantics (normalized, case-insensitive).
        f_norm = os.path.normcase(os.path.normpath(os.path.abspath(f)))
        for pref in source_prefixes:
            p_norm = os.path.normcase(os.path.normpath(os.path.abspath(pref)))
            if f_norm == p_norm or f_norm.startswith(p_norm + os.sep):
                in_scope = True
                break

    if kind == "CXXRecordDecl" and node.get("completeDefinition"):
        if in_scope:
            name = node.get("name")
            if name:
                out.setdefault(name, []).append({
                    "kind": node.get("tagUsed") or "struct",
                    "file": f,
                    "fields": extract_fields(node, out),
                })
    elif kind == "EnumDecl":
        if in_scope:
            name = node.get("name")
            if name:
                out.setdefault("__enum__" + name, []).append({
                    "kind": "enum", "file": f, "fields": [],
                })
    for child in node.get("inner") or []:
        collect_records(child, out, source_prefixes, state)


def extract_fields(record_node, records_out):
    """取该 record 的直接 FieldDecl。"""
    fields = []
    for child in record_node.get("inner") or []:
        if child.get("kind") == "FieldDecl":
            t = child.get("type") or {}
            fields.append({
                "name": child.get("name"),
                "type": t.get("qualType") or "",
            })
        elif child.get("kind") == "CXXRecordDecl" and not child.get("isImplicit"):
            # 嵌套 record 单独处理，但它的字段也算进本 record 的布局
            nested = child.get("inner") or []
            seen_decl = [c for c in nested if c.get("kind") == "FieldDecl"]
            if seen_decl:
                # 匿名 struct/union 成员：展平
                for c in seen_decl:
                    t = c.get("type") or {}
                    fields.append({
                        "name": c.get("name"),
                        "type": t.get("qualType") or "",
                    })
    return fields


def main():
    ap = argparse.ArgumentParser(description="G1 global type consistency check")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))), help="repo root")
    ap.add_argument("--clang", default=None, help="path to clang.exe")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--write-dashboard", action="store_true",
                    help="write result into dashboard/metrics.json")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    clang = args.clang or os.environ.get("TH185_CLANG")
    if not clang:
        # 已知可用位置（全盘搜过，不是猜的）
        for cand in [
            os.path.join(os.path.dirname(root), "tools", "emsdk", "upstream", "bin", "clang.exe"),
        ]:
            if os.path.isfile(cand):
                clang = cand
                break
    if not clang or not os.path.isfile(clang):
        print("[FAIL] G1 cannot find clang.exe. Pass --clang or set TH185_CLANG.")
        print("       已知可用：%s" % os.path.join(
            os.path.dirname(root), "tools", "emsdk", "upstream", "bin", "clang.exe"))
        return 2

    src = os.path.join(root, "src")
    inc = os.path.join(root, "include")
    types_dir = os.path.join(root, "types")

    sources = []
    for base in (src, inc):
        if not os.path.isdir(base):
            continue
        for dirpath, _dirnames, filenames in os.walk(base):
            for fn in filenames:
                if fn.endswith((".cpp", ".cc", ".cxx")):
                    sources.append(os.path.join(dirpath, fn))

    if not sources:
        print("[PASS] G1: no sources under /src or /include yet (bootstrap).")
        print("       Nothing to check; coverage is 0 definitions.")
        if args.write_dashboard:
            write_dashboard(root, 0, 0, 0, 0)
        return 0

    failures = []
    all_records = {}
    parse_errors = []

    include_dirs = [inc, src]
    for path in sources:
        ast, err = run_clang_ast(clang, path, include_dirs)
        if err:
            parse_errors.append((path, err))
            continue
        found = {}
        # 同时接受绝对路径和相对路径两种 loc.file 形态。
        collect_records(ast, found, [src, inc, "src", "include"],
                        state={"file": path})
        for name, defs in found.items():
            all_records.setdefault(name, []).extend(defs)

    if parse_errors:
        print("[FAIL] G1: %d source file(s) failed to parse:" % len(parse_errors))
        for p, e in parse_errors[:10]:
            print("       %s: %s" % (os.path.relpath(p, root), e.replace("\n", " ")[:200]))
        return 1

    # 1。 同名 struct 在不同 TU 里定义必须一致
    for name, defs in all_records.items():
        if name.startswith("__enum__"):
            continue
        sigs = set()
        for d in defs:
            sig = tuple((f["name"], f["type"]) for f in d["fields"])
            sigs.add(sig)
        if len(sigs) > 1:
            failures.append({
                "kind": "cross_tu_mismatch",
                "type": name,
                "detail": "defined differently in %d TU(s)" %
                          len({d["file"] for d in defs}),
                "variants": len(sigs),
            })

    # 2。 未知类型：不猜，记 FAIL
    unknown_fields = []
    for name, defs in all_records.items():
        if name.startswith("__enum__"):
            continue
        known = {}
        for nm, ds in all_records.items():
            if nm.startswith("__enum__"):
                known[nm.replace("__enum__", "")] = 4
                continue
            d0 = ds[0]
            _layout, _unk, total, _al = compute_layout(d0["fields"], known)
            known[nm] = total
        d0 = defs[0]
        _layout, unk, _total, _al = compute_layout(d0["fields"], known)
        for u in unk:
            unknown_fields.append({"type": name, "field": u["field"],
                                   "declared": u["type"]})
    if unknown_fields:
        failures.append({
            "kind": "unknown_type",
            "count": len(unknown_fields),
            "detail": "cannot size these fields without guessing",
            "sample": unknown_fields[:10],
        })

    # 3。 已锁定类型的偏移/宽度/类型不可变
    lock_path = os.path.join(types_dir, "lock.json")
    locked = {}
    if os.path.isfile(lock_path):
        try:
            locked = json.load(open(lock_path, encoding="utf-8")).get("locked", {})
        except (OSError, json.JSONDecodeError) as exc:
            print("[FAIL] G1: types/lock.json unreadable: %s" % exc)
            return 1

    conflicts = 0
    for name, entry in locked.items():
        if name not in all_records:
            continue
        defs = all_records[name]
        known = {}
        for nm, ds in all_records.items():
            if nm.startswith("__enum__"):
                known[nm.replace("__enum__", "")] = 4
                continue
            _l, _u, total, _a = compute_layout(ds[0]["fields"], known)
            known[nm] = total
        layout, _unk, total, _al = compute_layout(defs[0]["fields"], known)
        got = {f["name"]: f for f in layout}
        for fname, want in (entry.get("fields") or {}).items():
            have = got.get(fname)
            if have is None:
                failures.append({"kind": "locked_field_missing",
                                 "type": name, "field": fname})
                conflicts += 1
                continue
            for key in ("offset", "size", "type"):
                if key in want and have.get(key) != want[key]:
                    failures.append({
                        "kind": "type_conflict", "type": name, "field": fname,
                        "attr": key, "locked": want[key], "found": have.get(key)})
                    conflicts += 1

    # 统计与输出
    total_defs = sum(len(v) for k, v in all_records.items() if not k.startswith("__enum__"))
    checked = len([k for k in all_records if not k.startswith("__enum__")])

    result = {
        "gate": "G1",
        "status": "FAIL" if failures else "PASS",
        "definitions_checked": checked,
        "definition_instances": total_defs,
        "locked_types": len(locked),
        "type_conflicts": conflicts,
        "failures": failures,
    }

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("=== G1 global type consistency ===")
        print("  definitions checked : %d (%d instances)" % (checked, total_defs))
        print("  locked types        : %d" % len(locked))
        print("  type_conflicts      : %d" % conflicts)
        if failures:
            print()
            for f in failures:
                print("  [FAIL] %s" % f["kind"])
                if f["kind"] == "cross_tu_mismatch":
                    print("         type=%s  %s  (%d variants)"
                          % (f["type"], f["detail"], f["variants"]))
                elif f["kind"] == "unknown_type":
                    print("         %d field(s) with unsizable types; first:"
                          % f["count"])
                    for s in f["sample"][:5]:
                        print("           %s::%s  %s"
                              % (s["type"], s["field"], s["declared"]))
                elif f["kind"] == "type_conflict":
                    print("         %s::%s  %s locked=%r found=%r"
                          % (f["type"], f["field"], f["attr"],
                             f["locked"], f["found"]))
                else:
                    print("         %s" % json.dumps(f, ensure_ascii=False))
            print()
            print("[FAIL] G1")
        else:
            print()
            print("[PASS] G1")

    if args.write_dashboard:
        write_dashboard(root, conflicts, len(failures), checked, total_defs)

    return 0 if not failures else 1


def write_dashboard(root, conflicts, failure_count, checked, total_defs):
    path = os.path.join(root, "dashboard", "metrics.json")
    try:
        with open(path, encoding="utf-8") as fh:
            m = json.load(fh)
    except (OSError, json.JSONDecodeError):
        m = {}
    m.setdefault("gates_implemented", {})["G1"] = True
    if checked > 0:
        m["type_conflicts"] = conflicts
    else:
        m["type_conflicts"] = None
        m.setdefault("_notes", {})["type_conflicts"] = \
            "null = 尚无定义被测得（bootstrap 期 /src 与 /include 为空）"
    m["_g1_last_run"] = {
        "failures": failure_count,
        "definitions_checked": checked,
    }
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: could not write dashboard: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())
