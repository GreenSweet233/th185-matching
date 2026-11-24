#!/usr/bin/env python3
r"""Bootstrap 完成度检查器 —— 防止 V1 违规复发（见 /ledger/violations.md）。

## 为什么需要这个脚本

AGENTS.md §0.1 规定：

> 按 §4 实现顺序：G1→G2→G4→G3→G5→G6。在六个脚本全部跑通之前，
> **任何「match:」提交都视为违反 §12 反作弊条款**。

我违反过一次（V1，commit b12930a）：当时只有 G1/G2/G4 三个脚本，
就提交了 `match: 0x00402A50`。

**根因是「数一遍」这种人工检查不可靠。** 所以改成脚本强制：
提交前跑这个脚本，它会检查六个门槛脚本是否都存在、是否都能运行。

## 检查什么

1. 六个脚本文件都存在
2. 六个脚本都能被 Python 解析（ast.parse）
3. 每个脚本至少跑过一次（dashboard 里有对应的 _gN_last_run 记录）

注意：**不要求六门都 PASS**（G2/G5 因客观缺口无法 PASS），
只要求「六个脚本都实现且都跑过」。这与 §0.1 的措辞一致。

## 退出码
  0 = Bootstrap 完成，允许提交 match:
  1 = Bootstrap 未完成，禁止提交 match:
"""

import ast
import json
import os
import sys

GATES = [
    ("G1", "check_types.py"),
    ("G2", "check_witness.py"),
    ("G3", "check_structure.py"),
    ("G4", "check_style.py"),
    ("G5", "check_stability.py"),
    ("G6", "check_evidence.py"),
]


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    tools = os.path.join(root, "tools")
    dash_path = os.path.join(root, "dashboard", "metrics.json")

    try:
        with open(dash_path, encoding="utf-8") as fh:
            dash = json.load(fh)
    except (OSError, json.JSONDecodeError) as exc:
        print("[FAIL] cannot read dashboard/metrics.json: %s" % exc)
        return 1

    problems = []
    rows = []

    for gate, fname in GATES:
        path = os.path.join(tools, fname)
        exists = os.path.isfile(path)
        parses = False
        if exists:
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    ast.parse(fh.read())
                parses = True
            except (OSError, SyntaxError):
                parses = False
        ran = ("_%s_last_run" % gate.lower()) in dash

        if not exists:
            problems.append("%s: tools/%s 不存在" % (gate, fname))
        elif not parses:
            problems.append("%s: tools/%s 无法解析（语法错误）" % (gate, fname))
        elif not ran:
            problems.append("%s: 脚本存在但从未成功写过 dashboard（未跑过或崩溃）"
                            % gate)

        rows.append((gate, fname, exists, parses, ran))

    print("=== Bootstrap completeness (AGENTS.md §0.1) ===")
    print("  %-4s %-22s %-8s %-8s %-8s" % ("gate", "script", "exists", "parses", "ran"))
    for gate, fname, exists, parses, ran in rows:
        print("  %-4s %-22s %-8s %-8s %-8s"
              % (gate, fname,
                 "yes" if exists else "NO",
                 "yes" if parses else "NO",
                 "yes" if ran else "NO"))
    print()

    if problems:
        print("[FAIL] Bootstrap 未完成，禁止提交 match: 提交")
        for p in problems:
            print("       - %s" % p)
        print()
        print("       依据 AGENTS.md §0.1；违规先例见 /ledger/violations.md 的 V1。")
        return 1

    print("[PASS] Bootstrap 完成：六个脚本都已实现且都跑过。")
    print("       注意：这只表示「可以提交 match」，不表示六门都 PASS。")
    print("       当前非 PASS 的门：%s"
          % (", ".join(g for g in ("G1","G2","G3","G4","G5","G6")
                        if not dash.get("gates_implemented", {}).get(g, False)) or "无"))
    return 0


if __name__ == "__main__":
    sys.exit(main())