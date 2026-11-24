#!/usr/bin/env python3
r"""G2 二进制见证检查器 (AGENTS.md §6 G2)。

## G2 的原文要求

  - 每个变量/常量/字段/call 必须映射到二进制 artifact
    (栈槽/立即数/偏移/调用目标/reloc)。
  - 优化后消失且无见证(dummy/魔法pad) -> FAIL。
  - 反向覆盖：二进制每个栈槽/立即数/call 都被源码解释，覆盖率=100%。
  - 产出 evidence/{func}.witness.json。

## 本脚本实际能做到什么（诚实边界）

G2 的完整形态需要「编译自己的 .obj -> 与 /orig 的 .text 同名函数逐条比对」。
但本机工具链现状（已实测，见 toolchain.lock）：

  原版：MSVC 三档 build (27412 / 31328 / 31332) 即 VS2019 16.0 / 16.8 / 16.9
  本机：只有 VS2022 Community 的 MSVC 14.44.35207
        C:\Program Files (x86)\Microsoft Visual Studio\2019 目录存在但是空的

按 AGENTS.md §2「禁止为凑匹配改编译器」，本脚本**不去用 VS2022 冒充 VS2019**。
它做的是 G2 中不依赖编译器的那一半：

  1. 建立「目标二进制事实库」：反汇编 .text，索引所有
     - 直接 call 目标
     - 立即数常量
     - 栈帧偏移
  2. 对每条源码声明（/src 里带 //@witness 注释的条目）检查它引用的
     artifact 是否真实存在于目标二进制。
  3. 反向覆盖率：源码声明的 artifact 集合 / 二进制里被「需要解释」的集合。

## 源码如何声明见证

在 /src 的函数上方写：

    //@witness func=0x414F20 kind=this
    //@witness call 0x401000            # 本函数调用了 0x401000
    //@witness imm 0x64                 # 常量 0x64 有出处
    //@witness slot -0xC                # 栈槽 ebp-0xC
    void AddCard(...) {...}

## 退出码
  0 = PASS
  1 = FAIL (见证不成立 / 覆盖率不达标 / 找不到原版)
  2 = 用法或环境错误
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_common


# 匹配 //@witness <任意非空内容到行尾>
#
# ★ 踩过的坑：原正则写作
#     //\s*@witness\s+(\w+)\s+(.+)$
#   它要求 @witness 后先是一个 \w+ 词、再是空白。
#   但 `func=0x00402A50 kind=thiscall` 里 "func" 后面紧跟的是 "="，
#   不是空白，于是 \s+ 失败，整条匹配 None ——
#   所有 func 声明被静默忽略，witness declarations 恒为 0。
#   改为只捕获 @witness 之后到行尾的全部内容，再按 key=value 解析。
WITNESS_RE = re.compile(r"//\s*@witness\s+(.+)$")


def parse_witnesses(sources_root):
    """扫描 /src，收集 //@witness 声明。

    返回 {func_key: [ {kind, value, file, line, raw} ]}
    """
    decls = {}
    for dirpath, _dirnames, filenames in os.walk(sources_root):
        for fn in filenames:
            if not fn.endswith((".cpp", ".cc", ".cxx", ".hpp", ".h")):
                continue
            path = os.path.join(dirpath, fn)
            try:
                with open(path, encoding="utf-8", errors="replace") as fh:
                    lines = fh.readlines()
            except OSError:
                continue
            current = None
            for idx, raw in enumerate(lines, 1):
                m = WITNESS_RE.search(raw)
                if not m:
                    continue
                payload = m.group(1).strip()
                # 两种写法都支持：
                #   //@witness func=0x00402A50 kind=thiscall   （新，推荐）
                #   //@witness disp 0x8C                      （旧，位置参数）
                kv = dict(re.findall(r"(\w+)=([^\s]+)", payload))
                if kv:
                    kind = payload.split("=")[0].strip()
                else:
                    parts = payload.split(None, 1)
                    kind = parts[0] if parts else ""
                    rest = parts[1].strip() if len(parts) > 1 else ""
                if kind == "func":
                    # 声明写法：//@witness func=0x00402A50 kind=thiscall
                    raw_funcs = kv.get("func")
                    if not raw_funcs:
                        continue
                    # 支持「一个源码实现对应多个相同形状的二进制地址」：
                    #   //@witness func=0x00404C20+0x00441640 kind=thiscall
                    # 这是批量匹配的关键——同一段源码被多个类实例化时，
                    # 二进制里会有 N 个形状完全相同的函数。
                    for one in raw_funcs.split("+"):
                        one = one.strip()
                        if not one:
                            continue
                        current = one
                        decls.setdefault(current, []).append({
                            "kind": "func", "value": one,
                            "mode": kv.get("kind", ""),
                            "file": path, "line": idx, "raw": raw.strip(),
                        })
                else:
                    if not kv:
                        pass  # rest 已在上面的位置参数分支里赋值
                    key = current or ("<file:%s>" % os.path.basename(path))
                    decls.setdefault(key, []).append({
                        "kind": kind, "value": rest,
                        "file": path, "line": idx, "raw": raw.strip(),
                    })
    return decls


def norm_value(kind, value):
    """把声明值规范化成可比较的整数（能转就转，不能就原样返回）。

    ★ 这里必须覆盖「所有会在判定里做 int 比较的 kind」。
    我最初只写了 call / imm，后来新增 disp 却忘了加进来，
    结果是 disp 的见证值以字符串 "0x8C" 返回，
    `isinstance(val, int)` 为 False，所有 disp 见证一律 FAIL ——
    而且报错信息是「displacement not found」，看起来像是二进制里没有，
    实际是类型没转换。新增见证类型时务必同步改这里。
    """
    v = value.strip()
    if kind in ("call", "imm", "disp"):
        # ★ 裸十六进制（无 0x 前缀）必须按 16 进制解析。
        #
        # 见证声明写成 `//@witness disp 4D7AE0`（不带 0x，因为地址本来就是
        # 十六进制），旧代码走 `int(v, 0)` —— 对 "4D7AE0" 会抛 ValueError
        # （`0` 前缀只认 0o/0x/0b，裸串被当成十进制，"D" 不是数字），
        # 于是返回字符串 "4D7AE0"，`isinstance(val, int)` 为 False，
        # 该 disp 见证一律 FAIL，报错还写着「not inside this function」——
        # 看起来像二进制里没有，实际是解析失败。
        #
        # 判据：如果串里含 a-f 且不含 0x 前缀，它只能是十六进制。
        try:
            if v.lower().startswith("0x"):
                return int(v, 16)
            try:
                return int(v, 0)          # 十进制 / 0o / 0b
            except ValueError:
                return int(v, 16)         # 裸十六进制：4D7AE0
        except ValueError:
            return v
    if kind == "slot":
        try:
            return int(v, 0) if not v.lower().startswith("0x") else int(v, 16)
        except ValueError:
            return v
    return v


def build_target_facts(sections, image_base, md):
    """从目标二进制提取可被见证的 artifact 集合。"""
    insns = pe_common.scan_text(sections, image_base, md)
    calls = set()
    imms = set()
    disps = set()
    slots = set()
    for ins in insns:
        if ins.get("is_data"):
            continue
        if ins["mnemonic"] == "call" and ins["op_str"].startswith("0x"):
            try:
                calls.add(int(ins["op_str"], 16))
            except ValueError:
                pass
        # 立即数与内存位移必须分开。
        #
        # mov [ecx + 0x8C], edx   的 0x8C 是**位移**（方括号内）
        # mov eax, 0x8C           的 0x8C 是**立即数**（方括号外）
        #
        # 早先只用一个 imms 集合把两者混在一起，造成两类误判：
        #   1. 位移被当成「立即数存在」，假的 imm 见证也能过
        #   2. `imm 0x00`（0 只作为位移出现）被误判为不存在
        # 现分开收，各自独立判定。
        disp_parts = re.findall(r"\[[^\]]*\]", ins["op_str"])
        outside = ins["op_str"]
        for d in disp_parts:
            outside = outside.replace(d, "")
        imms.update(extract_numbers(outside))
        for d in disp_parts:
            disps.update(extract_numbers(d))
        # 栈槽：ebp - 0xNN / ebp + 0xNN
        for m in re.finditer(r"ebp ([+-]) 0x([0-9a-fA-F]+)", ins["op_str"]):
            sign = -1 if m.group(1) == "-" else 1
            slots.add(sign * int(m.group(2), 16))
    real_insns = [i for i in insns if not i.get("is_data")]
    return {
        "instruction_count": len(real_insns),
        "data_bytes": sum(i["size"] for i in insns if i.get("is_data")),
        "call_targets": calls,
        "immediates": imms,
        "displacements": disps,
        "stack_slots": slots,
    }


def extract_numbers(text):
    """从反汇编文本里提取所有数值，**同时支持十六进制与十进制**。

    ## 为什么必须两种都支持

    capstone 对位移/立即数的打印格式取决于大小：

        [eax + 0x8c]   -> 十六进制（带 0x）
        [eax + 8]      -> **十进制**（不带 0x）

    我第一版只写了 `0x([0-9a-fA-F]+)`，于是：
      - `[eax + 8]` 提取不到（没有 0x 前缀）-> disp 集合缺 8
      - 声明 `disp 0x8` 的见证被判 FAIL，报错「not inside this function」，
        但反汇编里它明明就在

    实测证据（0x00405830 的 9 条指令全部 disp=[]）：
        mov eax, dword ptr [eax + 8]   brackets=['[eax + 8]']   提取: []
        movq qword ptr [ecx], xmm0     brackets=['[ecx]']       提取: []

    这是**系统性漏检**：所有小于 0x0A 的位移都提取不到。
    """
    out = set()
    # 十六进制
    for m in re.finditer(r"0x([0-9a-fA-F]+)", text):
        out.add(int(m.group(1), 16))
    # 十进制（去掉 0x 形式的残留后，剩下的纯数字）
    stripped = re.sub(r"0x[0-9a-fA-F]+", " ", text)
    for m in re.finditer(r"\b([0-9]+)\b", stripped):
        out.add(int(m.group(1), 10))
    return out


def facts_for_range(sections, image_base, md, lo, hi):
    """提取 [lo, hi) 这个 VA 范围内的 artifact。

    ## 为什么必须按范围而不是全局

    第一版用全局集合判定见证，实测抓不到假见证：
    声明 disp 0x7F8 时全 PASS，因为 .text 里别处确实用了 0x7F8。
    验证过程：
      目标函数 0x00402A50..0x00402A7C 内只有 0x8C 和 0xD4 两个位移
      0x7F8 在目标函数外出现（别的函数用了）
      -> 全局存在 ≠ 这个函数里有

    见证的意义是「这个函数的这段码里出现了这个 artifact」，
    所以判定必须限定在函数的地址范围内。否则 G2 就是摆设。
    """
    insns = pe_common.scan_text(sections, image_base, md)
    calls, imms, disps, slots = set(), set(), set(), set()
    body = []
    for ins in insns:
        if not (lo <= ins["va"] < hi):
            continue
        if ins.get("is_data"):
            continue
        body.append(ins)
        if ins["mnemonic"] == "call" and ins["op_str"].startswith("0x"):
            try:
                calls.add(int(ins["op_str"], 16))
            except ValueError:
                pass
        disp_parts = re.findall(r"\[[^\]]*\]", ins["op_str"])
        outside = ins["op_str"]
        for d in disp_parts:
            outside = outside.replace(d, "")
        imms.update(extract_numbers(outside))
        for d in disp_parts:
            disps.update(extract_numbers(d))
        for m in re.finditer(r"ebp ([+-]) 0x([0-9a-fA-F]+)", ins["op_str"]):
            sign = -1 if m.group(1) == "-" else 1
            slots.add(sign * int(m.group(2), 16))
    return {
        "instruction_count": len(body),
        "call_targets": calls,
        "immediates": imms,
        "displacements": disps,
        "stack_slots": slots,
        "body": body,
    }


def find_function_start(insns, addr, by_va):
    """从 addr 往前找函数真正起点。

    ## 为什么需要这个

    我原先假设「call 的目标地址就是函数起点」。**这个假设是错的。**

    实测反例（0x0044DA15）：

        0x0044D9F0  push ebp            <- 函数真正起点
        0x0044D9F3  movss xmm0, [ebp+8]
        0x0044D9FD  mov ecx, [ebp+0xC]
        0x0044DA10  call 0x4587B0
        0x0044DA15  movss xmm0, [ecx]   <- 我的代码当成函数起点
        0x0044DA41  ret 8               <- 函数真正终点

    `0x0044DA15` 只是 call 之后的下一条指令。从它开始切，
    切到的是函数中部——于是「4 个函数共享一段代码」的假象出现了，
    实际是同一个函数的 4 个不同入口偏移。

    判定规则：往前找最近的「函数序言」特征——
      push ebp / mov ebp, esp        （标准帧）
      或前一条指令是 ret / int3      （前一函数结束）
    """
    if not insns:
        return addr
    # 建立有序地址表，用二分查找而不是每次线性扫（1678 个目标 × 每次都全扫是 O(n²)）
    ordered = sorted(i["va"] for i in insns if not i.get("is_data"))
    import bisect
    pos = bisect.bisect_left(ordered, addr)
    if pos >= len(ordered) or ordered[pos] != addr:
        # 该地址不是已知指令边界（可能是数据或 thunk 目标），不回溯
        return addr

    text_lo = ordered[0]
    for _ in range(64):          # 最多回溯 64 条指令
        if pos <= 0:
            return ordered[0]
        prev_va = ordered[pos - 1]
        prev = by_va[prev_va]
        # ★ 边界：回溯不得越过 .text 起点
        if prev_va < text_lo:
            return ordered[pos]
        cur = ordered[pos]
        # 前一条是 ret / int3 -> 当前位置就是函数起点
        if prev["mnemonic"].startswith("ret") or prev["mnemonic"] == "int3":
            return cur
        # ★★ 前一条是 jmp -> **不能**继续回溯，这就是本函数的起点。
        #
        # 实测反例（0x004903B9）：
        #
        #   0x004903B4  jmp 0x49040A   <- 尾调用桩，属于**另一个函数**
        #   0x004903B9  xor eax, eax   <- 我们要找的函数
        #   0x004903BB  ret
        #
        # 旧逻辑把 jmp 当普通指令继续往前，回溯到 0x004903B4，
        # 于是把一个只有 2 条指令的函数膨胀成 3 条，并吞掉了邻居的 jmp。
        # 后果：形状分组把 0x004903B9 和另外三个 2 条指令的 xor 函数拆开，
        # 我却在源码里把它们写进了同一个函数。
        if prev["mnemonic"] == "jmp":
            return cur
        # ★ 前一条是 mov edi, edi -> MSVC 的 hotpatch 占位符（2 字节 nop）
        #
        # 实测本二进制有 424 处 `mov edi,edi / push ebp` 作为函数开头，
        # 其中 401 个是精确的 call 目标。这是 MSVC /hotpatch 或增量链接的产物。
        #
        # 当前没有实际错位（call 目标落在序言内部的案例为 0），
        # 因为编译器不会把 call 目标放进 2 字节 nop 之后。
        # 但规则里显式保留这一条，避免依赖编译器的这种「善意」。
        if (prev["mnemonic"] == "mov"
                and prev["op_str"].replace(" ", "") == "edi,edi"):
            return cur
        # 前一条是 push ebp（函数序言）-> 从它起
        if prev["mnemonic"] == "push" and prev["op_str"].strip() == "ebp":
            return prev_va
        pos -= 1
    return ordered[pos]


def infer_signature(body, mode=""):
    """从函数体推导 (ret_bytes, param_count, convention)。

    ## 为什么需要

    形状分组如果只看指令序列，会把**签名不同**的函数归到一起。
    实测：`0x00417750` 是 `ret 0xC`（3 个参数），
    而 `0x00458 7B0` 是 `ret 8`（2 个参数）—— 指令序列几乎一样，
    因为都是 fsincos 极坐标，但多了一个 length 参数。

    ret 操作数是最可靠的签名证据：
      ret      -> cdecl / thiscall 无参数（调用方清栈）
      ret N    -> stdcall / thiscall 带栈清理，N 字节参数
    参数个数 = N / 4。

    ## 参数个数的盲点

    我第一版用 `int(retop, 16) // 4`，但 `ret 0xC` 的 op_str 是字符串 "0xc"，
    `int("0xc", 16)` = 12，12//4 = 3 —— 这条其实是对的。
    真正的坑是 `ret` 无操作数时 op_str 是空串，被当成 0 个参数，
    而它实际表示「调用方清栈」，参数个数无法从 ret 推出。
    """
    retop = None
    for ins in reversed(body):
        if ins["mnemonic"].startswith("ret"):
            retop = ins["op_str"].strip()
            break
    if retop is None:
        return {"ret_bytes": None, "params": None, "convention": "unknown"}
    if retop == "":
        # 无操作数：无法从 ret 推参数个数
        return {"ret_bytes": 0, "params": None,
                "convention": mode or "cdecl?"}
    try:
        n = int(retop, 16) if not retop.startswith("0x") else int(retop, 16)
    except ValueError:
        return {"ret_bytes": None, "params": None, "convention": "unknown"}
    return {"ret_bytes": n, "params": n // 4,
            "convention": mode or ("thiscall" if n % 4 == 0 else "?")}


def function_extent(label, insns):
    """给定 func 声明的标签，返回 (lo, hi)。

    标签形式：
      0x00402A50         -> 跟随控制流直到所有出口
      0x00402A50+0x53    -> 显式长度

    ## ★ 第 4 次修正：函数可以有多个 ret，不能遇到第一个就停

    实测反例（0x00402A50）：

        0x00402A50  mov edx, [ecx+0x8C]
        0x00402A56  sub edx, 1
        0x00402A59  mov [ecx+0x8C], edx
        0x00402A5F  jns 0x00402A7C        <- 非负分支跳到 L2
        0x00402A61  ...清零...
        0x00402A7B  ret                    <- ★ 旧逻辑在这里结束函数
        0x00402A7C  L2:                    <- 但这里还有一半函数体
        0x00402A7C  mov eax, [ecx+edx*4+0x0C]
        0x00402A80  mov [ecx], eax
        0x00402A82  mov eax, [ecx+edx*4+0x4C]
        0x00402A86  mov [ecx+8], eax
        0x00402A89  mov [ecx+0xD4], 0
        0x00402A93  ret

    旧逻辑返回 hi = 0x00402A7C，于是：
      - G2 判 `disp 0x0C` / `disp 0x4C` 不在函数内（它们确实在 0x00402A7C 之后）
      - G3 的循环/分支统计只算半个函数

    **证据**：call 0x00402A50 有 6 个调用方，call 0x00402A7C 有 0 个，
    指向 0x00402A7C 的跳转只有 0x00402A5F 的 jns。
    从不被 call、只被函数内跳转引用的地址是**内部基本块**。

    ## 新算法

    从 lo 开始沿指令顺序走，同时收集跳转目标（记在 worklist）；
    hi = 所有可达指令的最大结束地址。遇到 ret 不停，除非所有分支都已汇合。
    简化实现：走到「连续 int3 之后」或「下一个已知的 call 目标」为止。
    """
    if "+" in label:
        addr_s, len_s = label.split("+", 1)
        try:
            lo = int(addr_s, 16)
            hi = lo + int(len_s, 16)
            return lo, hi
        except ValueError:
            return None, None
    try:
        lo = int(label, 16)
    except ValueError:
        return None, None

    by_va = {i["va"]: i for i in insns if not i.get("is_data")}
    lo = find_function_start(insns, lo, by_va)

    ordered = sorted(by_va)
    pos_of = {va: k for k, va in enumerate(ordered)}
    if lo not in pos_of:
        return lo, lo + 0x400

    # 收集所有 call 目标，用于识别「下一个函数起点」
    call_targets = set()
    for ins in insns:
        if ins.get("is_data"):
            continue
        if ins["mnemonic"] == "call" and ins["op_str"].startswith("0x"):
            try:
                call_targets.add(int(ins["op_str"], 16))
            except ValueError:
                pass

    idx = pos_of[lo]
    hi = lo
    branch_targets = set()
    seen_ret = False
    guard = 0
    while idx < len(ordered) and guard < 4000:
        guard += 1
        va = ordered[idx]
        ins = by_va[va]
        # 遇到「别的函数的 call 目标」就停（但要允许本函数内部的跳转目标）
        if va != lo and va in call_targets and seen_ret and va not in branch_targets:
            break
        hi = va + ins["size"]
        mn = ins["mnemonic"]
        if mn == "jmp" or (mn.startswith("j") and mn != "jmp"):
            if ins["op_str"].startswith("0x"):
                try:
                    branch_targets.add(int(ins["op_str"], 16))
                except ValueError:
                    pass
            if mn == "jmp":
                # 无条件跳出 -> 后续可能是别的分支体，继续扫（保守）
                pass
        if mn.startswith("ret"):
            seen_ret = True
        # 以下三个停止条件只在「已经见过 ret」之后生效，
        # 避免把同函数内 ret 之后的分支体误判为下一个函数。
        if seen_ret:
            nxt = ordered[idx + 1] if idx + 1 < len(ordered) else None
            if nxt is not None:
                nins = by_va[nxt]
                # 条件 1：连续 int3 说明函数体结束
                if nins["mnemonic"] == "int3":
                    break
                # 条件 2：下一条是 push ebp —— 极强的函数序言信号
                #
                # 实测反例（0x004587B0）：
                #   0x004587B0 ... 0x004587CD  ret 8     <- 完整函数（30 字节）
                #   0x004587D0  push ebp                <- 下一个函数起点
                # 旧逻辑没有这个条件，把后面 800 多字节的下一个函数也吞了。
                if (nins["mnemonic"] == "push"
                        and nins["op_str"].strip() == "ebp"):
                    break
                # 条件 3：下一条是 mov edi,edi（hotpatch 序言）
                if (nins["mnemonic"] == "mov"
                        and nins["op_str"].replace(" ", "") == "edi,edi"):
                    break
                # 条件 4：下一条是别的函数的 call 目标，且不在本函数分支目标里
                if nxt in call_targets and nxt not in branch_targets:
                    break
        idx += 1
    return lo, hi

def main():
    ap = argparse.ArgumentParser(description="G2 binary witness check")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--exe", default=None, help="path to original th185.exe")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    ap.add_argument("--emit-evidence", action="store_true",
                    help="write evidence/{func}.witness.json for each declared func")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    exe = pe_common.find_exe(root, args.exe)
    if not exe:
        print("[FAIL] G2: 找不到原版 th185.exe。")
        print("       已尝试：--exe / TH185_EXE / /orig/th185.exe / local.target.json")
        print("       原版二进制不入库（旧规约第 1 条），请在本地提供。")
        return 1

    try:
        pe, sections = pe_common.load_pe(exe)
    except Exception as exc:  # noqa: BLE001
        print("[FAIL] G2: 无法解析 PE: %s" % exc)
        return 1
    try:
        md = pe_common.make_disassembler()
    except Exception as exc:  # noqa: BLE001
        print("[FAIL] G2: 无法初始化反汇编器: %s" % exc)
        return 1

    image_base = pe.OPTIONAL_HEADER.ImageBase
    facts = build_target_facts(sections, image_base, md)

    src = os.path.join(root, "src")
    decls = parse_witnesses(src) if os.path.isdir(src) else {}

    failures = []
    checked_items = 0
    missing_items = []

    all_insns = pe_common.scan_text(sections, image_base, md)

    for func_key, items in decls.items():
        if func_key.startswith("<file:"):
            continue
        # 先确定这个函数的地址范围，判定只在该范围内做。
        lo, hi = function_extent(func_key, all_insns)
        local = None
        if lo is not None:
            local = facts_for_range(sections, image_base, md, lo, hi)

        for it in items:
            if it["kind"] == "func":
                # func 声明本身也要验证：目标地址必须真的在 .text 里，
                # 且（若有后续 ret）范围可确定。
                checked_items += 1
                if lo is None or local is None or local["instruction_count"] == 0:
                    missing_items.append({
                        "func": func_key, "kind": "func", "value": it["value"],
                        "file": os.path.relpath(it["file"], root), "line": it["line"],
                        "reason": "function address not found in target .text",
                    })
                continue
            checked_items += 1
            val = norm_value(it["kind"], it["value"])
            ok = True
            detail = ""
            if local is None:
                ok = False
                detail = "cannot locate function extent; refusing to fall back to global scan"
            elif it["kind"] == "call":
                ok = isinstance(val, int) and val in local["call_targets"]
                detail = "call target not inside this function"
            elif it["kind"] == "imm":
                ok = isinstance(val, int) and val in local["immediates"]
                detail = "immediate not inside this function"
            elif it["kind"] == "slot":
                ok = isinstance(val, int) and val in local["stack_slots"]
                detail = "stack slot not inside this function"
            elif it["kind"] == "disp":
                ok = isinstance(val, int) and val in local["displacements"]
                detail = "memory displacement not inside this function"
            else:
                ok = False
                detail = "unknown witness kind %r" % it["kind"]
            if not ok:
                missing_items.append({
                    "func": func_key, "kind": it["kind"], "value": it["value"],
                    "file": os.path.relpath(it["file"], root), "line": it["line"],
                    "reason": detail,
                })

    if missing_items:
        failures.append({
            "kind": "witness_not_found",
            "count": len(missing_items),
            "sample": missing_items[:15],
        })

    # 反向覆盖：源码解释了二进制的多少。
    #
    # ★ 口径修正（重要）：
    #   旧版只统计 `call` 见证，得到 (已声明的 call 目标) / (全部 call 目标)。
    #   但叶子函数（如 `mov eax, ecx; ret`）**内部没有任何 call**，
    #   所以匹配了 3 个叶子函数后覆盖率仍是 0.0000% —— 指标看起来毫无进展，
    #   实际是口径选错了：它衡量的是「call 图的边被解释了多少」，
    #   而不是「函数被匹配了多少」。
    #
    #   改为同时报告两个口径：
    #     function_coverage = 已声明并验证的函数数 / 二进制可识别函数数
    #     call_coverage     = 已解释的 call 目标 / 全部 call 目标
    #   判定用 function_coverage（更贴合「匹配了多少函数」这个目标）。
    declared_calls = {norm_value(i["kind"], i["value"]) for items in decls.values()
                      for i in items if i["kind"] == "call"}
    declared_calls = {v for v in declared_calls if isinstance(v, int)}
    binary_calls = facts["call_targets"]
    call_coverage = (len(declared_calls & binary_calls) / len(binary_calls)) \
                    if binary_calls else 0.0

    # 已声明的函数（不含 <file:> 桶）
    declared_funcs = {k for k in decls if not k.startswith("<file:")}
    # 二进制里可识别的函数 = 所有直接 call 目标（这是可枚举的下界）
    binary_funcs = facts["call_targets"]
    matched_funcs = set()
    for label in declared_funcs:
        try:
            va = int(label.split("+")[0], 16)
        except ValueError:
            continue
        if va in binary_funcs:
            matched_funcs.add(va)
    function_coverage = (len(matched_funcs) / len(binary_funcs)) \
                    if binary_funcs else 0.0

    coverage = function_coverage

    # 判定口径（重要，避免「什么都没查却报 PASS」）：
    #   FAIL       = 声明了见证但目标二进制里找不到
    #   INCOMPLETE = 没有 FAIL，但反向覆盖率未达 100%（bootstrap 期正常）
    #   PASS       = 无 FAIL 且反向覆盖率 = 100%
    #
    # 之前只看 failures 就报 PASS，导致「1678 个 call 目标无一被解释」
    # 也被标成 PASS —— 那会让 §11 仪表盘骗人。
    if failures:
        status = "FAIL"
    elif binary_funcs and len(matched_funcs) < len(binary_funcs):
        status = "INCOMPLETE"
    elif not binary_calls:
        status = "INCOMPLETE"
    else:
        status = "PASS"

    result = {
        "gate": "G2",
        "status": status,
        "target_exe_sha_path": exe,
        "instruction_count": facts["instruction_count"],
        "binary_artifacts": {
            "call_targets": len(facts["call_targets"]),
            "immediates": len(facts["immediates"]),
            "displacements": len(facts["displacements"]),
            "stack_slots": len(facts["stack_slots"]),
        },
        "witness_declarations": {
            "functions": len([k for k in decls if not k.startswith("<file:")]),
            "items": checked_items,
        },
        "reverse_coverage": {
            "function_coverage": round(function_coverage, 6),
            "matched_functions": len(matched_funcs),
            "binary_functions": len(binary_funcs),
            "call_coverage": round(call_coverage, 6),
            "declared_call_targets": len(declared_calls),
            "binary_call_targets": len(binary_calls),
            "_denominator": "function_coverage = matched funcs / binary call targets（call 目标作为可枚举函数下界）",
        },
        "witnessless_elements": len(missing_items),
        "failures": failures,
    }

    if args.emit_evidence:
        ev_dir = os.path.join(root, "evidence")
        os.makedirs(ev_dir, exist_ok=True)
        for func_key, items in decls.items():
            if func_key.startswith("<file:"):
                continue
            payload = {
                "func": func_key,
                "declared": [
                    {"kind": i["kind"], "value": i["value"],
                     "file": os.path.relpath(i["file"], root), "line": i["line"]}
                    for i in items
                ],
                "target_exe": os.path.basename(exe),
                "_note": "G2 见证表；双向覆盖率见 dashboard",
            }
            safe = re.sub(r"[^A-Za-z0-9_.-]", "_", func_key)
            with open(os.path.join(ev_dir, "%s.witness.json" % safe), "w",
                      encoding="utf-8") as fh:
                json.dump(payload, fh, indent=2, ensure_ascii=False)

    if args.json:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print("=== G2 binary witness ===")
        print("  target exe          : %s" % exe)
        print("  .text instructions  : %d" % facts["instruction_count"])
        print("  binary artifacts    : calls=%d  imm=%d  disp=%d  slots=%d"
              % (len(facts["call_targets"]), len(facts["immediates"]),
                 len(facts["displacements"]), len(facts["stack_slots"])))
        print("  witness declarations: %d item(s) in %d function(s)"
              % (checked_items, result["witness_declarations"]["functions"]))
        print("  function coverage   : %.4f%% (%d / %d functions matched)"
              % (function_coverage * 100, len(matched_funcs), len(binary_funcs)))
        print("  call coverage       : %.4f%% (%d / %d call targets explained)"
              % (call_coverage * 100, len(declared_calls & binary_calls),
                 len(binary_calls)))
        print("  witnessless         : %d" % len(missing_items))
        if failures:
            print()
            for f in failures:
                print("  [FAIL] %s  (%d)" % (f["kind"], f["count"]))
                for s in f.get("sample", [])[:10]:
                    print("         %s::%s %s  (%s:%d)"
                          % (s["func"], s["kind"], s["value"], s["file"], s["line"]))
                    print("           -> %s" % s["reason"])
        print()
        if status == "FAIL":
            print("[FAIL] G2")
        elif status == "INCOMPLETE":
            print("[INCOMPLETE] G2")
            print("             %d / %d functions matched (%.4f%%)."
                  % (len(matched_funcs), len(binary_funcs),
                     function_coverage * 100))
            print("             这不是 PASS：源码尚未解释目标二进制。")
            print("             按 AGENTS.md §11，反向覆盖率 < 100% 不算完成。")
        else:
            print("[PASS] G2")

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
    m.setdefault("gates_implemented", {})["G2"] = (result["status"] == "PASS")
    m["witnessless_elements"] = result["witnessless_elements"]
    # ★ 顶层 coverage 必须与 _g2_last_run 一致。
    #
    # 曾经长期是 {"matched": 0, "total": null, "ratio": 0}，
    # 而 _g2_last_run.function_coverage 是 0.020858（35/1678）——
    # 两个字段互相矛盾，仪表盘读者会以为进度是 0。
    # 根因：coverage 是早期手写的初始值，后来加 _g2_last_run 时没同步更新它。
    rc = result["reverse_coverage"]
    m["coverage"] = {
        "matched": rc["matched_functions"],
        "total": rc["binary_functions"],
        "ratio": rc["function_coverage"],
        "_definition": "matched functions / binary call targets（call 目标作为可枚举函数下界）",
    }
    m.setdefault("_notes", {})["witnessless_elements"] = (
        "G2 实测值。0 表示当前源码的见证声明全部命中；"
        "不表示二进制已被完全解释（见 reverse_coverage）")
    m["_g2_last_run"] = {
        "status": result["status"],
        "instruction_count": result["instruction_count"],
        "binary_call_targets": result["binary_artifacts"]["call_targets"],
        "witness_items": result["witness_declarations"]["items"],
        "function_coverage": result["reverse_coverage"]["function_coverage"],
        "matched_functions": result["reverse_coverage"]["matched_functions"],
    }
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: could not write dashboard: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())
