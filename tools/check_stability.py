#!/usr/bin/env python3
r"""G5 无 UB + 扰动稳定检查器 (AGENTS.md §6 G5)。

## G5 的原文要求

  - UBSan/ASan 开；-O0/-O2/-Os 各编一份跑同组随机输入，行为必须一致。
  - 安全扰动(改注释/格式/重命名局部/字面量换constexpr)后 .text 必须不变。

## 本环境的硬阻塞（先说清楚，不假装通过）

G5 的两个要求都依赖「能编译出与原版一致的机器码」：

| 要求 | 依赖 | 本环境状态 |
|---|---|---|
| -O0/-O2/-Os 行为一致 | 能编译的 C++ 工具链 | 有 clang（emsdk 自带），但不是原版工具链 |
| 扰动后 .text 不变 | 产出目标格式 .obj 并与原版比对 | 缺 VS2019（原版 build 27412/31328/31332） |

按 AGENTS.md §2「禁止为凑匹配改编译器」，本脚本不用 clang 冒充 MSVC。
clang 编出的 .text 与 MSVC 本来就不会逐字节相同，
拿它测「扰动后 .text 不变」，测的是 clang 的稳定性而不是目标的。

## 那本脚本做什么

1. 探测并如实报告阻塞（找到哪个编译器、版本、与原版差距）。
2. UB 静态探测：用 clang 的告警开关（-Wuninitialized / -Wnull-dereference /
   -Warray-bounds / -Wstrict-aliasing 等）检查源码本身是否健康。
   这不依赖匹配原版工具链，因为它看的是源码。
3. 多优化级告警一致性：-O0/-O2/-Os 各做一次 -fsyntax-only，
   比较告警集合。告警随优化级变化说明代码对优化级敏感（UB 征兆）。
   这是弱化代理，不是 G5 原文要求，输出里明确标注。

## 退出码
  0 = PASS / INCOMPLETE
  1 = FAIL
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

ORIGINAL_TOOLCHAIN = {
    "_source": "toolchain.lock 的 compiler.builds（从原版 Rich Header 解出）",
    "builds": ["27412", "31328", "31332"],
    "approx_vs": "VS2019 16.0.x / 16.8 / 16.9",
}


def find_clang(root):
    cand = os.path.join(os.path.dirname(root), "tools", "emsdk",
                        "upstream", "bin", "clang.exe")
    return cand if os.path.isfile(cand) else None


def clang_version(clang):
    try:
        p = subprocess.run([clang, "--version"], capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=60)
        return (p.stdout or "").splitlines()[0] if p.stdout else ""
    except (subprocess.TimeoutExpired, OSError):
        return ""


def find_msvc():
    """
    找本机可用的 cl.exe。

    搜索范围：
      C:/D: 的 Program Files (x86)/Microsoft Visual Studio/<year>/<Edition>/VC/Tools/MSVC/<ver>
      以及旧版 VC98/Bin/CL.EXE

    为什么用 glob 而不是 os.walk：
      实测 os.path.isdir(C:/Program Files/Microsoft Visual Studio) 为 True，
      但 os.walk 对该目录返回 0 个文件（Windows 上该子树有 reparse point /
      权限特殊性，walk 静默跳过）。glob 走底层 FindFirstFile，不受影响。

    glob 模式为什么写两套：
      VS2022 是 <root>/2022/Community/VC/...（多一层年份），
      VS2019 BuildTools 装在 D 盘时是 <root>/2019/BuildTools/VC/...（同样多一层）。
      但早期布局可能是 <root>/<Edition>/VC/...，两套都试。
    """
    import glob
    roots = [
        "C:\\Program Files\\Microsoft Visual Studio",
        "C:\\Program Files (x86)\\Microsoft Visual Studio",
        "D:\\Program Files\\Microsoft Visual Studio",
        "D:\\Program Files (x86)\\Microsoft Visual Studio",
    ]
    patterns = []
    for root in roots:
        for mid in (os.path.join("*", "*"), "*"):
            for host in ("Hostx64", "Hostx86"):
                patterns.append(os.path.join(
                    root, mid, "VC", "Tools", "MSVC", "*",
                    "bin", host, "x64", "cl.exe"))
        patterns.append(os.path.join(root, "*", "VC98", "Bin", "CL.EXE"))
    found = []
    for pat in patterns:
        try:
            found.extend(glob.glob(pat))
        except OSError:
            pass
    return sorted(set(found))

def probe(clang, path, include_dirs, extra_flags):
    cmd = [clang, "-x", "c++", "-std=c++17", "-fsyntax-only",
           "-Wall", "-Wextra"] + extra_flags
    for inc in include_dirs:
        cmd += ["-I", inc]
    cmd.append(path)
    try:
        p = subprocess.run(cmd, capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=120)
    except (subprocess.TimeoutExpired, OSError) as exc:
        return None, str(exc)
    lines = [ln for ln in (p.stderr or "").splitlines()
             if "warning:" in ln or "error:" in ln]
    return lines, ""


def main():
    ap = argparse.ArgumentParser(description="G5 no-UB + perturbation stability")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--clang", default=None)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    clang = args.clang or os.environ.get("TH185_CLANG") or find_clang(root)
    src = os.path.join(root, "src")
    inc = os.path.join(root, "include")

    files = []
    for base in (src, inc):
        if not os.path.isdir(base):
            continue
        for dirpath, _d, fns in os.walk(base):
            for fn in sorted(fns):
                if fn.endswith((".cpp", ".cc", ".cxx")):
                    files.append(os.path.join(dirpath, fn))

    msvc = find_msvc()
    msvc_versions = []
    for exe in msvc:
        m = re.search(r"MSVC\\[0-9.]+", exe)
        if m:
            msvc_versions.append(m.group(0).replace("MSVC\\", ""))
        else:
            mm = re.search(r"VC98", exe)
            msvc_versions.append("VC98" if mm else "unknown")
    msvc_versions = sorted(set(msvc_versions))

    blockers = []
    if not msvc:
        blockers.append({
            "kind": "no_msvc",
            "detail": "本机找不到任何 cl.exe，无法编译目标格式",
        })
    else:
        blockers.append({
            "kind": "toolchain_mismatch",
            "detail": "原版需要 MSVC build %s（%s）；本机找到 MSVC %s"
                      % ("/".join(ORIGINAL_TOOLCHAIN["builds"]),
                         ORIGINAL_TOOLCHAIN["approx_vs"],
                         ", ".join(msvc_versions) or "无"),
            "consequence": "G5 的两个要求都依赖产出与原版一致的机器码；当前无法执行",
        })

    failures = []
    ub_probe = {}
    warnings_by_level = {}

    if clang and files:
        for path in files:
            rel = os.path.relpath(path, root)
            ub_flags = ["-Wuninitialized", "-Wmaybe-uninitialized",
                        "-Wnull-dereference", "-Warray-bounds",
                        "-Wstrict-aliasing", "-Wformat=2"]
            ub_lines, err = probe(clang, path, [inc, src], ub_flags)
            if err:
                failures.append({"kind": "probe_error", "file": rel,
                                 "detail": err})
            else:
                # 先把「编译器不认识的告警开关」滤掉——那不是代码问题。
                # 实测：clang 不支持 -Wmaybe-uninitialized 时，
                # stderr 里会出现 "unknown warning option"，
                # 而它包含 "uninitialized" 字样，被旧逻辑误判为 ub_suspect。
                real = [ln for ln in ub_lines
                        if "unknown warning option" not in ln
                        and "unknown argument" not in ln]
                ub_probe[rel] = {"warnings": len(real), "raw": len(ub_lines)}
                serious = [ln for ln in real
                           if "uninitialized" in ln or "null" in ln
                           or "out of bounds" in ln or "array-bounds" in ln]
                if serious:
                    failures.append({
                        "kind": "ub_suspect", "file": rel,
                        "count": len(serious), "sample": serious[:5],
                    })

            per_level = {}
            for lvl in ("-O0", "-O2", "-Os"):
                lines, err = probe(clang, path, [inc, src], [lvl])
                if err:
                    per_level[lvl] = ["<error: %s>" % err]
                else:
                    per_level[lvl] = sorted(set(
                        re.sub(r":[0-9]+:[0-9]+:", ":", ln) for ln in lines))
            warnings_by_level[rel] = {lvl: len(v) for lvl, v in per_level.items()}
            sigs = {lvl: tuple(v) for lvl, v in per_level.items()}
            if len(set(sigs.values())) > 1:
                failures.append({
                    "kind": "opt_level_sensitive", "file": rel,
                    "detail": "告警集合随优化级变化，可能对优化敏感（UB 征兆）",
                    "counts": {lvl: len(v) for lvl, v in per_level.items()},
                })

    if not files:
        status = "INCOMPLETE"
    elif failures:
        status = "FAIL"
    else:
        status = "INCOMPLETE"

    result = {
        "gate": "G5",
        "status": status,
        "files_probed": len(files),
        "toolchain": {
            "clang": clang_version(clang) if clang else None,
            "msvc_found": msvc_versions,
            "original_required": ORIGINAL_TOOLCHAIN,
        },
        "blockers": blockers,
        "ub_probe": ub_probe,
        "warnings_by_opt_level": warnings_by_level,
        "failures": failures,
        "not_executed": [
            {
                "requirement": "-O0/-O2/-Os 各编一份跑同组随机输入，行为一致",
                "status": "NOT_EXECUTED",
                "reason": "需要能产出与原版一致的目标码；本机工具链不匹配（见 blockers）",
            },
            {
                "requirement": "安全扰动后 .text 不变",
                "status": "NOT_EXECUTED",
                "reason": "同上。且用 clang 测扰动稳定性测的是 clang 而非目标 MSVC",
            },
        ],
        "what_was_substituted": {
            "claim": "多优化级告警集合一致性（弱化代理）",
            "why_weaker": "检查的是源码对优化级的敏感性，不是运行时行为一致性",
            "honesty": "不能替代 G5 原文要求，只作为 UB 早期预警",
        },
    }

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("=== G5 no-UB + perturbation stability ===")
        print("  files probed    : %d" % len(files))
        print("  clang           : %s" % (clang_version(clang) if clang else "未找到"))
        print("  MSVC found      : %s" % (", ".join(msvc_versions) or "无"))
        print("  original needs  : MSVC build %s (%s)"
              % ("/".join(ORIGINAL_TOOLCHAIN["builds"]),
                 ORIGINAL_TOOLCHAIN["approx_vs"]))
        print()
        print("  ★ 阻塞（如实报告，不假装通过）:")
        for b in blockers:
            print("      [%s] %s" % (b["kind"], b["detail"]))
            if b.get("consequence"):
                print("         -> %s" % b["consequence"])
        print()
        print("  未执行的要求:")
        for n in result["not_executed"]:
            print("      [NOT_EXECUTED] %s" % n["requirement"])
            print("                     %s" % n["reason"])
        if ub_probe:
            print()
            print("  实际做了的（UB 静态探测）:")
            for f, v in ub_probe.items():
                print("      %-28s %d warning(s)" % (f, v["warnings"]))
        if warnings_by_level:
            print()
            print("  多优化级告警数（弱化代理，非 G5 原文要求）:")
            for f, v in warnings_by_level.items():
                print("      %-28s %s" % (f, json.dumps(v)))
        if failures:
            print()
            for f in failures:
                print("  [FAIL] %s  %s" % (f["kind"], f.get("file", "")))
                if f.get("detail"):
                    print("         %s" % f["detail"])
                for s in (f.get("sample") or [])[:3]:
                    print("         %s" % s[:140])
        print()
        print("[%s] G5" % status)
        if status == "INCOMPLETE":
            print("  G5 无法 PASS：编译器缺口未解，原文的两个要求都未执行。")

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
    m.setdefault("gates_implemented", {})["G5"] = (result.get("status") == "PASS")
    m["ub_reports"] = len([f for f in result.get("failures", [])
                           if f.get("kind") == "ub_suspect"])
    m["_g5_last_run"] = {
        "status": result.get("status"),
        "files_probed": result.get("files_probed", 0),
        "not_executed": len(result.get("not_executed", [])),
        "blockers": len(result.get("blockers", [])),
    }
    m.setdefault("_notes", {})["G5"] = (
        "INCOMPLETE：编译器缺口（缺 MSVC 27412/31328/31332）导致"
        "两个原文要求都无法执行")
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: could not write dashboard: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())