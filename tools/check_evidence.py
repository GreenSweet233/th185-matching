#!/usr/bin/env python3
r"""G6 独立证据检查器 (AGENTS.md §6 G6)。

## G6 的原文要求

  - assert/日志字符串提取的表达式与字段名必须采用。
  - RTTI/vtable/.pdata 的类名/继承/函数边界必须吻合。
  - 库/引擎指纹命中 -> 用真实已知源码，禁编造。
  - 多反编译器(Ghidra/IDA/retdec)类型/结构判断 >=2 一致才采纳。

## 本环境的实测能力边界（先说清楚）

| 要求 | 本环境能否做 | 实测依据 |
|---|---|---|
| RTTI 类名 | **不能** | .rdata 里 `.?AV...@@` 模式 0 命中 —— 原版用 /GR- 编译，没有 RTTI 名字 |
| vtable 结构与槽数 | 能 | 扫出 117 个 vtable / 2164 槽，其中 87 个是 22 槽且间隔精确 0x5C |
| .pdata | **不能** | PE32 x86 没有 .pdata（那是 x64 的异常表）。节表只有 .text/.rdata/.data/.rsrc |
| assert/日志字符串 | 能 | 字符串提取 + 源码比对 |
| 多反编译器交叉 | **不能** | 本机没有 Ghidra / IDA / retdec（已全盘搜索） |

所以本脚本实现的是：**vtable 结构提取 + 与卡表的交叉印证 + 字符串证据**，
其余三项明确标注 UNSUPPORTED，不假装做过。

## ★ 本轮最硬的证据：87 个 22 槽 vtable ↔ 87 张卡

扫描 .rdata 得到：

```
22 槽 vtable  : 87 个
相邻间隔      : 0x5C (92 字节) x 86 次（无一例外）
0x5C          = 4 * 23 = (22 个虚函数 + 1 个 COL 回指针)
地址范围      : 0x004B6AAC .. 0x004B8994
```

而卡表 `CARD_DESC_LIST`（0x004CA370）恰好是 87 项。
两个独立的二进制结构（vtable 数组 / 卡表）数量完全一致，
且 vtable 间隔精确等于 (槽数+1)*4 —— 这不是巧合，
是「87 个卡牌类各自有 22 个虚函数」的直接二进制证据。

## 退出码
  0 = PASS / INCOMPLETE
  1 = FAIL
  2 = 用法或环境错误

## 禁止
  AGENTS.md §12：修改/禁用/绕过本脚本等同任务失败。
"""

import argparse
import collections
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_common


def scan_vtables(sections, image_base):
    """扫 .rdata 里的 MSVC vtable。

    MSVC 布局：
      [vtable - 4]  = COL (Complete Object Locator) 指针
      [vtable + 0]  = 第一个虚函数
      ...
    
    判定：某地址处是「非 text 的非零指针」，且下一个是「text 内指针」，
    则 vtable 起点 = 该地址 + 4（跳过 COL），连续 text 指针数 = 槽数。
    """
    rdata = sections.get(".rdata")
    text = sections.get(".text")
    if not rdata or not text:
        return []
    text_lo = image_base + text["va"]
    text_hi = text_lo + text["vsize"]
    rdata_lo = image_base + rdata["va"]
    data = rdata["data"]
    n = len(data)

    def rd(off):
        return struct.unpack_from("<I", data, off)[0]

    out = []
    off = 0
    while off + 8 <= n:
        col = rd(off)
        first = rd(off + 4)
        if col != 0 and not (text_lo <= col < text_hi) and text_lo <= first < text_hi:
            run = 0
            j = off + 4
            while j + 4 <= n:
                x = rd(j)
                if text_lo <= x < text_hi:
                    run += 1
                    j += 4
                else:
                    break
            if run >= 2:
                out.append({
                    "va": rdata_lo + off + 4,
                    "col_va": rdata_lo + off,
                    "col_target": col,
                    "slots": run,
                    "slot_vas": [rdata_lo + off + 4 + 4 * k for k in range(run)],
                })
                off = j
                continue
        off += 4
    return out


def cross_check_card_vtables(vtables, card_count):
    """把 22 槽 vtable 的数量/间隔与卡表项数交叉印证。

    这是本脚本唯一「两个独立二进制结构互相印证」的检查，
    也是 §6 G6「RTTI/vtable 的类名/继承/函数边界必须吻合」在本环境
    （无 RTTI 名字）下能做到的最强形式。
    """
    t22 = sorted((v for v in vtables if v["slots"] == 22), key=lambda x: x["va"])
    result = {
        "slot22_count": len(t22),
        "card_count": card_count,
        "count_matches": len(t22) == card_count,
        "expected_stride": (22 + 1) * 4,
        "stride_observed": None,
        "stride_uniform": False,
    }
    if len(t22) >= 2:
        gaps = collections.Counter(b["va"] - a["va"]
                                   for a, b in zip(t22, t22[1:]))
        result["stride_observed"] = gaps.most_common(1)[0][0]
        expect = result["expected_stride"]
        result["stride_uniform"] = (len(gaps) == 1 and
                                    gaps.most_common(1)[0][0] == expect)
        result["stride_gap_histogram"] = {
            "0x%X" % g: c for g, c in sorted(gaps.items())}
        result["range"] = "0x%08X..0x%08X" % (t22[0]["va"], t22[-1]["va"])
    return result


def scan_strings(sections, image_base, min_len=6):
    """提取 .rdata 里的可打印 ASCII 字符串（用于 assert / 日志比对）。"""
    rdata = sections.get(".rdata")
    if not rdata:
        return []
    lo = image_base + rdata["va"]
    data = rdata["data"]
    out = []
    cur = []
    start = 0
    for i, b in enumerate(data):
        if 0x20 <= b < 0x7F:
            if not cur:
                start = i
            cur.append(b)
        else:
            if len(cur) >= min_len:
                out.append({
                    "va": lo + start,
                    "text": bytes(cur).decode("ascii", "replace"),
                })
            cur = []
    if len(cur) >= min_len:
        out.append({"va": lo + start,
                    "text": bytes(cur).decode("ascii", "replace")})
    return out


def declared_classes(src_root):
    """从 /include 的源码里取 class/struct 名字（用于与 vtable 数量对照）。"""
    names = set()
    if not os.path.isdir(src_root):
        return names
    pat = re.compile(r"\b(?:class|struct)\s+([A-Za-z_]\w*)\s*(?::|\{)")
    for dirpath, _d, files in os.walk(src_root):
        for fn in files:
            if not fn.endswith((".hpp", ".h", ".cpp")):
                continue
            try:
                txt = open(os.path.join(dirpath, fn), encoding="utf-8",
                           errors="replace").read()
            except OSError:
                continue
            names.update(pat.findall(txt))
    return names


def main():
    ap = argparse.ArgumentParser(description="G6 independent evidence")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    ap.add_argument("--emit-evidence", action="store_true")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    exe = pe_common.find_exe(root, args.exe)
    if not exe:
        print("[FAIL] G6: 找不到原版 th185.exe")
        return 1

    pe, sections = pe_common.load_pe(exe)
    image_base = pe.OPTIONAL_HEADER.ImageBase

    vtables = scan_vtables(sections, image_base)
    strings = scan_strings(sections, image_base)

    # 卡表项数：从 known address 读，或复用卡表常量
    CARD_COUNT = 87
    card_cross = cross_check_card_vtables(vtables, CARD_COUNT)

    # RTTI 探测（如实报告）
    rdata = sections.get(".rdata")
    rtti_names = []
    if rdata:
        rtti_names = re.findall(rb"\.\?A[VU][A-Za-z0-9_@?$]*@@", rdata["data"])

    # .pdata 探测
    has_pdata = ".pdata" in sections

    failures = []
    if not card_cross["count_matches"]:
        failures.append({
            "kind": "card_vtable_count_mismatch",
            "detail": "22 槽 vtable %d 个 vs 卡表 %d 项"
                      % (card_cross["slot22_count"], CARD_COUNT),
        })
    if card_cross["slot22_count"] >= 2 and not card_cross["stride_uniform"]:
        failures.append({
            "kind": "vtable_stride_not_uniform",
            "detail": "22 槽 vtable 间隔不统一：%s"
                      % json.dumps(card_cross.get("stride_gap_histogram", {})),
        })

    unsupported = []
    if not rtti_names:
        unsupported.append({
            "requirement": "RTTI 类名比对",
            "status": "UNSUPPORTED",
            "measured": ".rdata 里 .?AV...@@ 模式 0 命中 —— 原版用 /GR- 编译",
        })
    if not has_pdata:
        unsupported.append({
            "requirement": ".pdata 函数边界比对",
            "status": "UNSUPPORTED",
            "measured": "PE32 x86 无 .pdata（该节是 x64 异常表）；节表只有 .text/.rdata/.data/.rsrc",
        })
    unsupported.append({
        "requirement": "多反编译器交叉（>=2 一致）",
        "status": "UNSUPPORTED",
        "measured": "本机无 Ghidra / IDA / retdec（已全盘搜索）",
    })

    src_classes = declared_classes(os.path.join(root, "include"))

    # ★ 状态三态，与 check_witness / check_stability 一致。
    #
    # 此前这里只判 failures，于是：
    #   脚本输出 [PASS] G6
    #   dashboard 写 G6: false（因为下面 write_dashboard 里有 all_supported 判断）
    # 两个说法互相矛盾。README 更是同时复述了两者。
    #
    # 正确做法：有 UNSUPPORTED 项就不是 PASS。
    # 「能做的部分都成立」是 INCOMPLETE，不是 PASS。
    if failures:
        status = "FAIL"
    elif unsupported:
        status = "INCOMPLETE"
    else:
        status = "PASS"
    result = {
        "gate": "G6",
        "status": status,
        "vtables": {
            "total": len(vtables),
            "total_slots": sum(v["slots"] for v in vtables),
            "slot_histogram": dict(collections.Counter(
                v["slots"] for v in vtables)),
        },
        "card_vtable_cross_check": card_cross,
        "strings": {"count": len(strings), "min_len": 6},
        "declared_classes_in_source": sorted(src_classes),
        "failures": failures,
        "unsupported_requirements": unsupported,
    }

    if args.emit_evidence:
        ev = os.path.join(root, "evidence", "G6-vtables.json")
        with open(ev, "w", encoding="utf-8") as fh:
            json.dump({
                "_comment": "G6 提取的 vtable 结构与卡表交叉印证",
                "target_exe": os.path.basename(exe),
                "vtables": vtables,
                "card_cross_check": card_cross,
                "string_sample": strings[:50],
            }, fh, indent=2, ensure_ascii=False)
        print("  evidence written: %s" % os.path.relpath(ev, root))

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("=== G6 independent evidence ===")
        v = result["vtables"]
        print("  vtables found   : %d (total %d slots)"
              % (v["total"], v["total_slots"]))
        print("  slot histogram  : %s" % json.dumps(v["slot_histogram"]))
        print()
        c = card_cross
        print("  ★ card/vtable cross-check")
        print("      22-slot vtables : %d" % c["slot22_count"])
        print("      card table      : %d" % c["card_count"])
        print("      count matches   : %s" % c["count_matches"])
        print("      expected stride : 0x%X  (22+1)*4" % c["expected_stride"])
        print("      observed stride : %s"
              % (("0x%X" % c["stride_observed"]) if c["stride_observed"] else "-"))
        print("      stride uniform  : %s" % c["stride_uniform"])
        if c.get("range"):
            print("      address range   : %s" % c["range"])
        print()
        print("  strings in .rdata: %d (len >= 6)" % len(strings))
        print("  classes in source: %d" % len(src_classes))
        if src_classes:
            print("      %s" % ", ".join(sorted(src_classes)[:10]))
        print()
        print("  UNSUPPORTED requirements (honest):")
        for u in unsupported:
            print("      [%s] %s" % (u["status"], u["requirement"]))
            print("          %s" % u["measured"])
        if failures:
            print()
            for f in failures:
                print("  [FAIL] %s" % f["kind"])
                print("         %s" % f["detail"])
        print()
        print("[%s] G6" % status)
        if status == "INCOMPLETE":
            print("  %d 项要求 UNSUPPORTED，因此不是 PASS。" % len(unsupported))
            print("  「能做的部分都成立」不等于「G6 满足」。")

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
    # status 现在是三态权威值：只有无 failures 且无 UNSUPPORTED 才是 PASS。
    m.setdefault("gates_implemented", {})["G6"] = (result.get("status") == "PASS")
    m["_g6_last_run"] = {
        "status": result.get("status"),
        "vtables": result["vtables"]["total"],
        "unsupported_count": len(result.get("unsupported_requirements", [])),
    }
    m.setdefault("_notes", {})["G6"] = (
        "部分能力 UNSUPPORTED（无 RTTI 名 / 无 .pdata / 无多反编译器），"
        "gates_implemented.G6 记 false")
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: could not write dashboard: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())