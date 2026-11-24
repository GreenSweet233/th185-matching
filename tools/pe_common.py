#!/usr/bin/env python3
"""共享的 PE 读取与 x86 反汇编工具。

供 G2 (check_witness.py) / G3 (check_structure.py) / G6 (check_evidence.py) 复用。

## 原版二进制从哪里来

原版 th185.exe **不入库**（旧规约第 1 条铁律，版权 + 体积）。定位优先级
与 th185_web/scripts/verify-target.mjs 保持一致，单一事实源：

  1. --exe 命令行参数
  2. TH185_EXE 环境变量
  3. /orig/th185.exe（若使用者自己放进去）
  4. th185_web/local.target.json 的 gameDir + th185.exe

拿不到就返回 None，调用方必须把这种情况当作 FAIL（不能静默跳过）。

## 为什么不用 libclang / 其他重型依赖

本环境只有 capstone 5.0.7 + pefile 2024.8.26 + unicorn 2.1.4，没有 libclang。
反汇编走 capstone，PE 结构走 pefile，够 G2/G3 用。
"""

import json
import os

try:
    import pefile
except ImportError:  # pragma: no cover
    pefile = None

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_32
except ImportError:  # pragma: no cover
    Cs = None


IMAGE_BASE_EXPECTED = 0x00400000


def find_exe(root, cli_path=None):
    """按上面文档的优先级定位原版 exe。找不到返回 None（调用方判 FAIL）。"""
    if cli_path and os.path.isfile(cli_path):
        return os.path.abspath(cli_path)
    env = os.environ.get("TH185_EXE")
    if env and os.path.isfile(env):
        return os.path.abspath(env)

    orig = os.path.join(root, "orig", "th185.exe")
    if os.path.isfile(orig):
        return orig

    # 第 4 步：本仓库自己的 local.target.json（不入库）
    #
    # ★ 2026-11-24 拆分仓库时加入。原来的实现只找 `th185_web/local.target.json`，
    #   那是功能还原仓库的路径。字节匹配仓库（本仓库）独立后不该依赖它。
    for rel in ("local.target.json", os.path.join("orig", "local.target.json")):
        local = os.path.join(root, rel)
        if not os.path.isfile(local):
            continue
        try:
            with open(local, encoding="utf-8") as fh:
                game_dir = json.load(fh).get("gameDir")
            if game_dir:
                cand = os.path.join(game_dir, "th185.exe")
                if os.path.isfile(cand):
                    return os.path.abspath(cand)
        except (OSError, json.JSONDecodeError):
            pass

    # 第 5 步：兼容旧的 sibling 布局（万一两个仓库放在同一父目录）
    sibling = os.path.join(os.path.dirname(root), "th185", "th185_web", "local.target.json")
    if os.path.isfile(sibling):
        try:
            with open(sibling, encoding="utf-8") as fh:
                game_dir = json.load(fh).get("gameDir")
            if game_dir:
                cand = os.path.join(game_dir, "th185.exe")
                if os.path.isfile(cand):
                    return os.path.abspath(cand)
        except (OSError, json.JSONDecodeError):
            pass
    return None


def load_pe(exe_path):
    """返回 (pe, sections_by_name)。sections_by_name: name -> dict。"""
    if pefile is None:
        raise RuntimeError("pefile 未安装")
    pe = pefile.PE(exe_path, fast_load=True)
    sections = {}
    for s in pe.sections:
        name = s.Name.decode("ascii", "replace").rstrip("\0")
        sections[name] = {
            "name": name,
            "va": s.VirtualAddress,
            "vsize": s.Misc_VirtualSize,
            "raw_ptr": s.PointerToRawData,
            "raw_size": s.SizeOfRawData,
            "data": s.get_data(),
        }
    return pe, sections


def make_disassembler():
    """返回启用了 skipdata 的反汇编器。

    ## 为什么必须 skipdata

    capstone 默认遇到无法解码的字节就**停止**。实测 th185.exe 的 .text：
      默认行为       -> 只反汇编 37854 条指令，覆盖 124029 / 711114 字节 (17.4%)
                        在 0x0041F47D 停下，后面 587085 字节全被丢掉
                        直接 call 目标只有 194 个
      skipdata=True  -> 208003 条指令，覆盖 711114 / 711114 (100.0%)
                        直接 call 目标 1678 个
                        skipdata 伪指令仅 487 条 (0.2%)

    差 8.6 倍的 call 目标。用默认行为建「二进制事实库」等于只看前 17%，
    后面的见证全部会误判为「不存在」。这是 G2 能成立的前提。
    """
    if Cs is None:
        raise RuntimeError("capstone 未安装")
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    md.skipdata = True
    return md


def disasm_range(md, code_bytes, base_va, max_instructions=None):
    """反汇编一段代码。返回 [(va, size, mnemonic, op_str, bytes)]。"""
    out = []
    for ins in md.disasm(code_bytes, base_va):
        # skipdata 产出的 ".byte" 是「这里解码失败」的占位，不是指令。
        # 仍然记录它（反向覆盖要算字节），但标记出来，调用方可以过滤。
        if ins.mnemonic == ".byte":
            out.append({
                "va": ins.address, "size": ins.size,
                "mnemonic": ".byte", "op_str": ins.op_str,
                "bytes": ins.bytes.hex(), "is_data": True,
            })
            if max_instructions and len(out) >= max_instructions:
                break
            continue
        out.append({
            "va": ins.address,
            "size": ins.size,
            "mnemonic": ins.mnemonic,
            "op_str": ins.op_str,
            "bytes": ins.bytes.hex(),
            "is_data": False,
        })
        if max_instructions and len(out) >= max_instructions:
            break
    return out


def rva_to_va(pe, rva):
    return pe.OPTIONAL_HEADER.ImageBase + rva


def va_to_rva(pe, va):
    return va - pe.OPTIONAL_HEADER.ImageBase


def find_section_for_va(sections, va, image_base):
    rva = va - image_base
    for name, s in sections.items():
        if s["va"] <= rva < s["va"] + max(s["vsize"], s["raw_size"]):
            return name, rva - s["va"]
    return None, None




def scan_jump_tables(sections, image_base, min_entries=3):
    """扫 .text 段内部的内嵌跳转表。

    ## 为什么需要

    MSVC 会把 switch 的跳转表放在 .text 里（不是 .rdata）。
    线性扫描（尤其是 skipdata）会把表项当指令，然后**错位到后面的函数**。

    实测反例（0x00407080，一个有 80+ 调用方的热函数）：

        0x00407050  5B 8B E5 5D C3     pop ebx / mov esp,ebp / pop ebp / ret  <- 前一函数结束
        0x00407055  0F 1F 00           nop（对齐）
        0x00407058  2E 6F 40 00        0x00406F2E  <- 跳转表开始
        ...
        0x0040707C  D2 6F 40 00        0x00406FD2
        0x00407080  55 8B EC           push ebp / mov ebp, esp  <- 真函数序言

    线性扫描从 0x0040707D 开始解码：

        0x0040707D  6f                outsd
        0x0040707F  00558b            add byte ptr [ebp-0x75], dl
        0x00407082  ec                in al, dx          <- 错位产物

    `in al, dx` 是端口 I/O 指令，不可能出现在普通函数开头 —— 这是 detectable 的。

    ## 判据

    连续 >= min_entries 个 4 字节值都落在 .text 的地址范围内。
    实测本二进制有 85 个这样的表、共 1624 个表项。
    """
    text = sections.get(".text")
    if not text:
        return []
    lo = image_base + text["va"]
    hi = lo + text["vsize"]
    data = text["data"][:text["vsize"]]
    n = len(data)
    out = []
    i = 0
    while i + 4 <= n:
        v = int.from_bytes(data[i:i + 4], "little")
        if lo <= v < hi:
            run = 0
            j = i
            while j + 4 <= n:
                v2 = int.from_bytes(data[j:j + 4], "little")
                if lo <= v2 < hi:
                    run += 1
                    j += 4
                else:
                    break
            if run >= min_entries:
                out.append({
                    "va": image_base + text["va"] + i,
                    "end": image_base + text["va"] + j,
                    "entries": run,
                })
                i = j
                continue
        i += 4
    return out




def scan_data_blobs(sections, image_base, min_run=8):
    """扫 .text 里的非跳转表数据区（递增序列 / 高频重复字节）。

    ## 为什么需要（跳转表扫描不够）

    85 个 4 字节对齐的跳转表之外，还有两类数据嵌在 .text 里：

    ### 类型一：字节查表（非 4 字节对齐）

        0x00434660  02 07 07 00 07 07 00 07 07 07 07 07   <- 0x07 是填充/默认值
        ...
        0x00434690  55 8B EC 83 EC 08   <- 真函数序言紧跟其后

    线性扫描把 `00 55 8B EC` 读成 `add byte ptr [ebp-0x75], dl`，
    于是 0x00434692 变成 `in al, dx`（端口 I/O，不可能出现在普通代码里）。

    ### 类型二：递增字节序列

        0x0043D9A0  55 56 57 58 59 5A 5B 5C 5D 5E 5F 60 61 62 63 ...
        （这看着像 push 序列，实际是递增索引表）

    ## 判据

      - 连续 >= min_run 个字节严格递增 1（(b[i] == b[i-1]+1) & 0xFF）
      - 或连续 >= 16 个相同字节（填充）

    注意：`int3` 填充（0xCC）不在此列 —— 那是真实的对齐填充，
    反汇编成 int3 是正确的，不影响后续指令对齐。
    """
    text = sections.get(".text")
    if not text:
        return []
    base = image_base + text["va"]
    data = text["data"][:text["vsize"]]
    n = len(data)

    ranges = []

    # 递增序列
    run_start = 0
    run = 1
    for i in range(1, n + 1):
        if i < n and data[i] == (data[i - 1] + 1) & 0xFF:
            run += 1
        else:
            if run >= min_run:
                ranges.append((run_start, i))
            run_start = i
            run = 1

    # 高频重复字节（>= 16 连续相同，排除 0xCC = int3 填充）
    run_start = 0
    run = 1
    for i in range(1, n + 1):
        same = i < n and data[i] == data[i - 1]
        if same:
            run += 1
        else:
            # 阈值 8：实测 0x00434660 附近的查表填充分段出现，
            # 中间夹着 00/01/02/03 索引值，凑不出 16 字节连续相同。
            if run >= 8 and data[run_start] != 0xCC:
                ranges.append((run_start, i))
            run_start = i
            run = 1

    # 合并重叠
    ranges.sort()
    merged = []
    for a, b in ranges:
        if merged and a <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    return [{"start": base + a, "end": base + b, "bytes": b - a}
            for a, b in merged]

def refine_with_prologues(sections, image_base, ranges):
    """用函数序言反向修正数据区边界。

    ## 关键洞察

    `55 8B EC`（push ebp / mov ebp, esp）在 x86 里几乎只作为函数序言出现。
    所以任何 `55 8B EC` 的位置都必须是某条指令的起点。

    如果线性扫描把某个 55 8B EC 吃进了前一条指令，说明前面那段是数据。
    做法：找到所有 55 8B EC，凡是不在已知数据区内的，
    就把它之前的一小段标为数据，强制下轮扫描从序言处开始。

    ## 实测反例（0x00434690）

        0x00434660  02 07 07 00 07 07 00 07 ...   <- 字节查表
        0x00434690  55 8B EC 83 EC 08              <- 真函数序言

    线性扫描从表中间解，把 00 55 8B EC 读成 add [ebp-0x75], dl，
    于是 0x00434690 不在指令边界上。
    """
    text = sections.get(".text")
    if not text:
        return ranges
    data = text["data"][:text["vsize"]]

    needle = bytes([0x55, 0x8B, 0xEC])
    offsets = []
    pos = 0
    while True:
        i = data.find(needle, pos)
        if i < 0:
            break
        offsets.append(i)
        pos = i + 1

    if not offsets:
        return ranges

    def covered(off):
        for a, b in ranges:
            if a <= off < b:
                return True
        return False

    extra = []
    for off in offsets:
        if covered(off):
            continue
        start = off - 0x10
        if start < 0:
            start = 0
        while start % 4 != 0 and start < off:
            start += 1
        if start < off:
            extra.append((start, off))
    return ranges + extra


def table_byte_ranges(sections, image_base):

    """返回跳转表占据的 .text 内字节区间（相对 .text 起点）。

    供 scan_text 在反汇编时跳过这些区间，避免把数据当代码。
    """
    text = sections.get(".text")
    if not text:
        return []
    base = image_base + text["va"]
    out = [((t["va"] - base), (t["end"] - base))
           for t in scan_jump_tables(sections, image_base)]
    out += [(b["start"] - base, b["end"] - base)
            for b in scan_data_blobs(sections, image_base)]
    # 序言锚定已撤除：实测它修好 1 处（0x00434690）却引入 31 处新错位。
    # 启发式方法每修一处就破坏别处，净收益为负。见 evidence/scan-text-limits.json。
    out.sort()
    merged = []
    for a, b in out:
        if merged and a <= merged[-1][1]:
            merged[-1] = (merged[-1][0], max(merged[-1][1], b))
        else:
            merged.append((a, b))
    return merged

def scan_text(sections, image_base, md, max_instructions=None):
    """旧的线性扫描实现（保留作为对照 / 回退）。

    问题：即使跳过 85 个跳转表和 23 个数据区，仍有 2 处错位
    （0x00434692 的 in al,dx、0x0049CEA2 的 into）。
    原因是数据区检测靠启发式，永远有漏网的。
    """
    text = sections.get('.text')
    if not text:
        return []
    size = min(text['vsize'], len(text['data']))
    data = text['data'][:size]
    base_va = image_base + text['va']
    ranges = table_byte_ranges(sections, image_base)
    if not ranges:
        return disasm_range(md, data, base_va, max_instructions)
    out = []
    cursor = 0
    for tstart, tend in sorted(ranges):
        if tstart > cursor:
            out.extend(disasm_range(md, data[cursor:tstart],
                                    base_va + cursor, None))
        if tend > cursor:
            cursor = tend
        if max_instructions and len(out) >= max_instructions:
            break
    if cursor < size:
        out.extend(disasm_range(md, data[cursor:], base_va + cursor, None))
    if max_instructions:
        out = out[:max_instructions]
    return out


def scan_text_recursive(sections, image_base, md, max_instructions=None):
    """递归下降反汇编：只从确定的入口跟跳转，不猜。

    ## 为什么必须递归下降

    线性扫描（即使配 skipdata + 跳转表跳过 + 数据区跳过）仍会错位：

        实测残留：0x00434692 = in al,dx、0x0049CEA2 = into
        这两个指令都不该出现在普通函数里 —— 是数据被当代码的痕迹

    根本问题：**你怎么知道某段字节是代码还是数据？**
    启发式（递增序列、重复字节、跳转表模式）永远会漏。

    递归下降不问这个问题，它从**已知入口**出发：
      1. 种子 = 入口点 + 所有 call 目标 + 所有跳转目标 + 导出表
      2. 从每个种子沿控制流走，遇到 jmp/jcc 就记下目标继续跟
      3. 遇到 ret / 无条件跳出函数范围就停
      4. 走不到的字节就是数据（或死代码），不反汇编

    代价：死代码不会被发现。但对匹配反编译来说这是**正确的取舍** ——
    我们只关心活的函数，死代码本来就不该产生匹配证据。

    双遍策略：
      第一遍用线性扫描收集所有 call/jmp 目标当种子（种子可能含假阳性，
      但假种子顶多多反汇编几段数据，不会漏真函数）
      第二遍从种子递归下降
    """
    text = sections.get('.text')
    if not text:
        return []
    size = min(text['vsize'], len(text['data']))
    data = text['data'][:size]
    base_va = image_base + text['va']
    hi_va = base_va + size

    # --- 第一遍：线性扫描拿种子 ---
    seeds = set()
    for ins in scan_text_linear(sections, image_base, md):
        if ins.get('is_data'):
            continue
        if ins['mnemonic'] in ('call', 'jmp') and ins['op_str'].startswith('0x'):
            try:
                t = int(ins['op_str'], 16)
            except ValueError:
                continue
            if base_va <= t < hi_va:
                seeds.add(t)
    if not seeds:
        return []

    # --- 第二遍：递归下降 ---
    visited = {}          # va -> insn dict
    worklist = sorted(seeds)
    while worklist:
        va = worklist.pop()
        while base_va <= va < hi_va and va not in visited:
            off = va - base_va
            got = None
            for ins in md.disasm(data[off:off + 16], va):
                got = ins
                break
            if got is None or got.mnemonic == '.byte':
                break
            visited[va] = {
                'va': va,
                'size': got.size,
                'mnemonic': got.mnemonic,
                'op_str': got.op_str,
                'bytes': got.bytes.hex(),
                'is_data': False,
            }
            # 跟跳转
            if got.mnemonic == 'jmp' and got.op_str.startswith('0x'):
                try:
                    worklist.append(int(got.op_str, 16))
                except ValueError:
                    pass
                break
            if got.mnemonic.startswith('j') and got.op_str.startswith('0x'):
                try:
                    worklist.append(int(got.op_str, 16))
                except ValueError:
                    pass
            if got.mnemonic.startswith('ret'):
                break
            va += got.size

    out = [visited[k] for k in sorted(visited)]
    if max_instructions:
        out = out[:max_instructions]
    return out

def call_targets(instructions):
    """提取所有直接 call 的目标 VA。"""
    targets = set()
    for ins in instructions:
        if ins["mnemonic"] == "call" and ins["op_str"].startswith("0x"):
            try:
                targets.add(int(ins["op_str"], 16))
            except ValueError:
                pass
    return targets
