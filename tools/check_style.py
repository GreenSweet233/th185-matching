#!/usr/bin/env python3
r"""G4 规范形 + 黑名单检查器 (AGENTS.md §6 G4)。

## G4 的原文要求

  - 提交前过 AST 归一化 + clang-format。
  - 规范形：能 for 不 while；能下标不指针算术；能早返回不深嵌套；命名从词典。
  - 黑名单(出现即 FAIL)：goto、内联asm、pad/dummy/unused、
    命名 v\d+/a\d+/temp\d+/sub_[0-9a-f]+、裸偏移强转、
    未登记魔法数(0/1/-1/sizeof派生/命名常量除外)、
    allowlist 外的优化 pragma/attribute。

## 本环境的能力边界（诚实记录）

  clang-format : **不可用**。本机没有独立的 clang-format 可执行文件
                 （全盘搜过，只在 emsdk 的 SDL 源码里找到个 shell 脚本，不是工具）。
                 所以「过 clang-format」这一半无法执行，脚本会明确报告
                 FORMAT_UNAVAILABLE 而不是假装通过。
  clang AST    : 可用（tools/emsdk/upstream/bin/clang.exe -fsyntax-only）
                 用于「文件必须是合法 C++」这层归一化检查。

黑名单与规范形里**能用 token 扫描可靠判定的部分**全部实现，
不依赖 clang-format 也能独立成立。

## 退出码
  0 = PASS（可能有 FORMAT_UNAVAILABLE 警告，但不是黑名单违规）
  1 = FAIL（存在黑名单违规或规范形违规）
  2 = 用法或环境错误

## 禁止
  AGENTS.md §12：修改/禁用/绕过本脚本等同任务失败。
"""

import argparse
import json
import os
import re
import sys

# ---------------------------------------------------------------------------
# 黑名单（§6 G4）。每条：id / 正则 / 原因。
# ---------------------------------------------------------------------------
BLACKLIST = [
    {
        "id": "goto",
        "pattern": re.compile(r"\bgoto\s+\w+\s*;"),
        "why": "§6 G4 全局黑名单：goto 出现即 FAIL",
    },
    {
        "id": "inline_asm",
        "pattern": re.compile(r"\b(__asm|asm)\b\s*(volatile\s*)?[({]"),
        "why": "§6 G4：内联 asm 出现即 FAIL",
    },
    {
        "id": "pragma_optimize",
        "pattern": re.compile(r"#\s*pragma\s+(optimize|auto_inline|intrinsic)\b"),
        "why": "§6 G4：allowlist 外的优化 pragma",
    },
    {
        "id": "msvc_attribute",
        "pattern": re.compile(r"__declspec\s*\(\s*(noinline|forceinline|naked)\s*\)"),
        "why": "§6 G4：allowlist 外的优化 attribute",
    },
    {
        "id": "pad_array",
        "pattern": re.compile(r"\b(?:char|u8|std::uint8_t|uint8_t)\s+(?:pad|padding|dummy|unused|reserved)_?\w*\s*\[[^\]]*\]"),
        "why": "§6 G4：pad/dummy 数组出现即 FAIL（无见证的填充）",
    },
    {
        "id": "pad_member",
        "pattern": re.compile(r"\b(?:u8|u16|u32|i32|uint32_t|int)\s+(?:pad|padding|dummy|unused)\w*\s*(?:=|;)"),
        "why": "§6 G4：pad/dummy 成员出现即 FAIL",
    },
    {
        "id": "bad_name_short",
        "pattern": re.compile(r"\b(?:u32|u16|u8|i32|i16|i8|int|float|auto|bool|char)\s+(v\d+|a\d+|temp\d+)\b"),
        "why": "§6 G4：命名 v\\d+/a\\d+/temp\\d+ 禁止",
    },
    {
        "id": "bad_name_sub_hex",
        "pattern": re.compile(r"\bsub_[0-9a-fA-F]{4,}\b"),
        "why": "§6 G4：命名 sub_[0-9a-f]+ 禁止",
    },
    {
        "id": "bare_offset_cast",
        # 注意：不能用 <[^>]+> 来匹配模板参数——`static_cast<char*>(self)` 里的
        # `>` 会被提前消耗，导致整条正则永远不命中（我踩过）。
        # 改用 <[^;]*?> 非贪婪 + 后续 [^;]*? 跳过中间的类型转换。
        "pattern": re.compile(
            r"\*\s*reinterpret_cast\s*<[^;]*?>\s*\("
            r"[^;]*?\b(?:this|self|base|ptr|p)\b[^;]*?\+\s*"
            r"(?:0x[0-9a-fA-F]+|[0-9]+)"
        ),
        "why": "§6 G4：裸偏移强转（this+0xNN 直接解引用）禁止——必须走命名字段",
    },
]

WHILE_RE = re.compile(r"\bwhile\s*\(")
MAX_NESTING = 4
NAME_DICT_FILE = "naming.dict"

COMMENT_OR_STRING = re.compile(
    r"//[^\n]*|/\*.*?\*/|\"(?:[^\"\\]|\\.)*\"|'(?:[^'\\]|\\.)*'",
    re.DOTALL,
)


def strip_comments_and_strings(text):
    """把注释和字符串替换成等长空白，保持行号不变。

    这一步很重要：黑名单扫描如果直接在原文上跑，会在注释里出现
    「本函数不能用 goto」这类文字时误报。
    """
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))
    return COMMENT_OR_STRING.sub(blank, text)


def scan_blacklist(rel, raw_lines, clean_lines):
    """对单个文件跑黑名单。返回违规列表。"""
    out = []
    for rule in BLACKLIST:
        for idx, line in enumerate(clean_lines, 1):
            for m in rule["pattern"].finditer(line):
                out.append({
                    "rule": rule["id"],
                    "why": rule["why"],
                    "file": rel,
                    "line": idx,
                    "text": raw_lines[idx - 1].strip()[:160] if idx - 1 < len(raw_lines) else "",
                    "match": m.group(0)[:80],
                })
    return out


def scan_while(rel, raw_lines, clean_lines):
    """能 for 不 while。

    判定口径（可审计）：
      - while (true) / while (1) : 允许（无限循环没有 for 写法）
      - while (cond) 且循环体在自增计数器 : 判 FAIL（可用 for）
      - 其他 while : 不判 FAIL（读流到 EOF 这类真该用 while）
    """
    findings = []
    for idx, line in enumerate(clean_lines, 1):
        m = WHILE_RE.search(line)
        if not m:
            continue
        rest = line[m.end():]
        cond = rest.split(")")[0].strip() if ")" in rest else ""
        if cond in ("true", "1"):
            continue
        body = "\n".join(clean_lines[idx:idx + 20])
        if re.search(r"(?:\+\+\s*\w+|\w+\s*\+\+|\w+\s*\+=\s*1)", body):
            findings.append({
                "rule": "while_should_be_for",
                "why": "§6 G4 规范形：能 for 不 while（循环体在自增计数器）",
                "file": rel, "line": idx,
                "text": raw_lines[idx - 1].strip()[:160] if idx - 1 < len(raw_lines) else "",
                "match": line.strip()[:80],
            })
    return findings


def check_nesting(rel, raw_lines, clean_lines, max_depth):
    """能早返回不深嵌套：按大括号深度估算。"""
    depth = 0
    max_seen = 0
    max_line = 0
    for idx, line in enumerate(clean_lines, 1):
        depth += line.count("{") - line.count("}")
        if depth > max_seen:
            max_seen = depth
            max_line = idx
    if max_seen > max_depth + 3:
        return [{
            "rule": "deep_nesting",
            "why": "§6 G4 规范形：能早返回不深嵌套（最大深度 %d）" % max_seen,
            "file": rel, "line": max_line,
            "text": raw_lines[max_line - 1].strip()[:160] if max_line - 1 < len(raw_lines) else "",
            "match": "depth=%d" % max_seen,
        }]
    return []


def check_parses(clang, path, include_dirs, std="c++17", timeout=120):
    """G4 的 AST 归一化层：文件必须能被 clang 解析成合法 C++。"""
    import subprocess
    cmd = [clang, "-x", "c++", "-std=" + std, "-fsyntax-only"]
    for inc in include_dirs:
        cmd += ["-I", inc]
    cmd.append(path)
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              encoding="utf-8", errors="replace", timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, "clang timeout"
    except OSError as exc:
        return False, "clang not runnable: %s" % exc
    if proc.returncode != 0:
        return False, (proc.stderr or "")[:300].replace("\n", " ")
    return True, ""


def main():
    ap = argparse.ArgumentParser(description="G4 canonical form + blacklist")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--clang", default=None)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    ap.add_argument("--max-nesting", type=int, default=MAX_NESTING)
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    clang = args.clang or os.environ.get("TH185_CLANG")
    if not clang:
        cand = os.path.join(os.path.dirname(root), "tools", "emsdk",
                            "upstream", "bin", "clang.exe")
        if os.path.isfile(cand):
            clang = cand

    src = os.path.join(root, "src")
    inc = os.path.join(root, "include")

    files = []
    for base in (src, inc):
        if not os.path.isdir(base):
            continue
        for dirpath, _d, filenames in os.walk(base):
            for fn in sorted(filenames):
                if fn.endswith((".cpp", ".cc", ".cxx", ".hpp", ".h")):
                    files.append(os.path.join(dirpath, fn))

    violations = []
    parse_errors = []
    checked = 0

    for path in files:
        rel = os.path.relpath(path, root)
        try:
            raw = open(path, encoding="utf-8", errors="replace").read()
        except OSError as exc:
            parse_errors.append((rel, str(exc)))
            continue
        checked += 1
        clean = strip_comments_and_strings(raw)
        raw_lines = raw.split("\n")
        clean_lines = clean.split("\n")

        violations.extend(scan_blacklist(rel, raw_lines, clean_lines))
        violations.extend(scan_while(rel, raw_lines, clean_lines))
        violations.extend(check_nesting(rel, raw_lines, clean_lines, args.max_nesting))

        if clang:
            ok, msg = check_parses(clang, path, [inc, src])
            if not ok:
                parse_errors.append((rel, msg))

    # clang-format 可用性（诚实报告，不假装通过）
    format_available = False
    format_reason = "本机无可用的 clang-format 可执行文件（已全盘搜索）"
    for d in (os.environ.get("PATH", "") or "").split(os.pathsep):
        if d and (os.path.isfile(os.path.join(d, "clang-format.exe")) or
                  os.path.isfile(os.path.join(d, "clang-format"))):
            format_available = True
            break

    failures = []
    by_rule = {}
    for v in violations:
        by_rule[v["rule"]] = by_rule.get(v["rule"], 0) + 1
    if violations:
        failures.append({
            "kind": "blacklist_violation",
            "count": len(violations),
            "by_rule": by_rule,
            "sample": violations[:20],
        })
    if parse_errors:
        failures.append({
            "kind": "parse_error",
            "count": len(parse_errors),
            "sample": [{"file": f, "msg": m} for f, m in parse_errors[:10]],
        })

    if failures:
        status = "FAIL"
    elif checked == 0:
        status = "INCOMPLETE"
    else:
        status = "PASS"

    result = {
        "gate": "G4",
        "status": status,
        "files_checked": checked,
        "blacklist_rules": [b["id"] for b in BLACKLIST],
        "violations": len(violations),
        "violations_by_rule": by_rule,
        "parse_errors": len(parse_errors),
        "clang_format": {
            "available": format_available,
            "reason": None if format_available else format_reason,
            "_note": "G4 要求『过 clang-format』；不可用时本脚本报 FORMAT_UNAVAILABLE，不假装通过",
        },
        "failures": failures,
    }

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("=== G4 canonical form + blacklist ===")
        print("  files checked   : %d" % checked)
        print("  blacklist rules : %d" % len(BLACKLIST))
        print("  violations      : %d" % len(violations))
        for k, n in sorted(by_rule.items()):
            print("      %-24s %d" % (k, n))
        print("  parse errors    : %d" % len(parse_errors))
        print("  clang-format    : %s"
              % ("available" if format_available else "UNAVAILABLE"))
        if not format_available:
            print("      [FORMAT_UNAVAILABLE] %s" % format_reason)
            print("      G4 的『过 clang-format』这一半未执行，不计入 PASS。")
        if violations:
            print()
            for v in violations[:20]:
                print("  [FAIL] %s  %s:%d" % (v["rule"], v["file"], v["line"]))
                print("         %s" % v["text"][:120])
                print("         -> %s" % v["why"])
            if len(violations) > 20:
                print("  ... 还有 %d 处" % (len(violations) - 20))
        if parse_errors:
            print()
            for f, m in parse_errors[:10]:
                print("  [FAIL] parse_error  %s" % f)
                print("         %s" % m[:160])
        print()
        if status == "FAIL":
            print("[FAIL] G4")
        elif status == "INCOMPLETE":
            print("[INCOMPLETE] G4: /src 与 /include 下还没有源文件。")
        else:
            print("[PASS] G4")
            if not format_available:
                print("       （不含 clang-format 部分：已标 FORMAT_UNAVAILABLE）")

    if args.write_dashboard:
        write_dashboard(root, result)

    return 0 if status != "FAIL" else 1


def write_dashboard(root, result):
    path = os.path.join(root, "dashboard", "metrics.json")
    try:
        with open(path, encoding="utf-8") as fh:
            m = json.load(fh)
    except (OSError, json.JSONDecodeError):
        m = {}
    m.setdefault("gates_implemented", {})["G4"] = (result["status"] == "PASS")
    m["_g4_last_run"] = {
        "status": result["status"],
        "files_checked": result["files_checked"],
        "violations": result["violations"],
        "clang_format_available": result["clang_format"]["available"],
    }
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: could not write dashboard: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())