#!/usr/bin/env python3
r"""G2+ 行为等价验证器 —— 用 unicorn 执行原版函数，与源码语义对比。

## 为什么需要这个（它比静态门槛强在哪）

G1/G2/G3/G4/G6 都是**静态**检查：它们看反汇编、看 AST、看结构统计。
静态检查能发现「形式不对」，但发现不了「语义算错了」。

举例：`0x00402A50` 的语义是「递减 [ecx+0x8C]；归负时清零三个字段」。
如果我把 `>=` 写成 `>`，或者把清零条件写成先减后判 vs 先判后减，
**静态门槛全都不会报**，但行为完全不同。

这个脚本直接**执行原版机器码**，用一组输入跑出真实结果，
再与 Python 描述的「我认为的语义」对比。这是唯一能在没有编译器时
证明语义正确的办法。

## G5 的关系

G5 要求「-O0/-O2/-Os 各编一份跑同组随机输入，行为一致」。
本脚本做的是它的**一半**：验证「源码语义 == 原版行为」。
缺的那一半（不同优化级之间一致）仍需编译器。

## 实现要点

  1. 映射镜像（0x400000 起）、栈、堆到互不重叠的地址
  2. 在函数入口前设好 ecx = this，栈上放参数
  3. 用返回哨兵地址检测 ret（原版函数内部没有 call，纯叶子）
  4. 跑完后读回被改的内存，与预期对比

## 退出码
  0 = 全部一致
  1 = 有差异
  2 = 环境错误

## 禁止
  AGENTS.md §12：修改/禁用/绕过本脚本等同任务失败。
  **尤其是「把预期值改成实际输出的值」** —— 那等于用测试结果定义正确性。
"""

import argparse
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_common
import check_witness as cw
try:
    import g2plus_cases
except ImportError:
    g2plus_cases = None

try:
    from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32,
                         UC_HOOK_CODE)
    from unicorn.x86_const import (UC_X86_REG_ECX, UC_X86_REG_EAX,
                                  UC_X86_REG_ESP, UC_X86_REG_EBP,
                                  UC_X86_REG_EIP)
except ImportError:
    print("[FAIL] G2+ 需要 unicorn（未安装）")
    sys.exit(2)


# 地址布局：必须互不重叠
IMAGE_BASE = 0x00400000
IMAGE_SIZE = 0x00200000      # 2 MB，覆盖 0x400000..0x600000
STACK_BASE = 0x00100000
STACK_SIZE = 0x00100000      # 1 MB 栈，远离镜像
HEAP_BASE  = 0x00600000      # 镜像之后
HEAP_SIZE  = 0x00100000
SENTINEL   = 0x003FFFF0      # ret 哨兵，在 image 之前


class Runner:
    """执行原版单个叶子函数。

    ## 关于退出机制

    最早用 `emu_start(func_va, until=SENTINEL)` —— 让 unicorn 跑到 until 地址就停。
    实测**全部 UC_ERR_FETCH_UNMAPPED**：until 只是「跑到该地址时停止」，
    但它仍会先尝试取指，取不到就报错，而不是干净地停。

    正确做法：用 UC_HOOK_CODE 在哨兵地址主动 emu_stop()。
    """

    def __init__(self, sections, image_base):
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.image_base = image_base
        self.stopped_at_sentinel = False

        # 映射镜像
        self.uc.mem_map(IMAGE_BASE, IMAGE_SIZE)
        for name, s in sections.items():
            if not s["data"]:
                continue
            va = image_base + s["va"]
            self.uc.mem_write(va, s["data"])

        self.uc.mem_map(STACK_BASE, STACK_SIZE)
        self.uc.mem_map(HEAP_BASE, HEAP_SIZE)
        self.uc.mem_map(SENTINEL & 0xFFFFF000, 0x1000)

        # 哨兵 hook：跑到就停
        def _on_code(uc, addr, size, user):
            if addr == SENTINEL:
                self.stopped_at_sentinel = True
                uc.emu_stop()

        self.uc.hook_add(UC_HOOK_CODE, _on_code)

    def run_leaf(self, func_va, this_va, this_bytes, args=(),
                 timeout=10000):
        """执行一个叶子函数，返回 (eax, 对象字节, 错误)。"""
        if this_bytes:
            self.uc.mem_write(this_va, bytes(this_bytes))

        # 栈布局（x86 cdecl 语义）：
        #
        #   低地址  [esp+0]  = 返回地址（哨兵）
        #           [esp+4]  = 第 1 个参数
        #           [esp+8]  = 第 2 个参数
        #   高地址
        #
        # ★ 踩过的坑：我最初把哨兵写在 esp 处，然后把 esp += 4 再压参数，
        #   最后 reg_write(ESP, esp) 指向了**参数**而不是哨兵。
        #   于是 `ret` 从 [esp] 弹出的是参数值（0），跳到了地址 0，
        #   表现为 EIP=0x00000000 + UC_ERR_FETCH_UNMAPPED。
        #
        # 正确做法：先算好总大小，一次性写好，再设 ESP 指向哨兵。
        n_args = len(args)
        total = 4 + 4 * n_args
        base_esp = STACK_BASE + STACK_SIZE - 0x100 - total

        self.uc.mem_write(base_esp, struct.pack("<I", SENTINEL))
        for k, a in enumerate(args):
            self.uc.mem_write(base_esp + 4 + 4 * k,
                              struct.pack("<I", a & 0xFFFFFFFF))

        self.uc.reg_write(UC_X86_REG_ESP, base_esp)
        self.uc.reg_write(UC_X86_REG_ECX, this_va)

        # until 用哨兵（它已映射，取指不会失败），hook 保证停下来
        try:
            # until 传 0 = 不指定结束地址，靠 hook 在哨兵处 emu_stop()
            self.uc.emu_start(func_va, 0, timeout=timeout)
        except UcError as exc:
            return None, None, str(exc)

        eip = self.uc.reg_read(UC_X86_REG_EIP)
        if not self.stopped_at_sentinel and eip != SENTINEL:
            return None, None, "did not reach sentinel (EIP=0x%08X)" % eip

        eax = self.uc.reg_read(UC_X86_REG_EAX)
        out = self.uc.mem_read(this_va, len(this_bytes)) if this_bytes else b""
        return eax, bytes(out), None

def read_file_offset(sections, image_base, va, size):
    for name, s in sections.items():
        lo = image_base + s["va"]
        if lo <= va < lo + len(s["data"]):
            off = va - lo
            return s["data"][off:off + size]
    return None


def main():
    ap = argparse.ArgumentParser(description="G2+ behavioral equivalence")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    exe = pe_common.find_exe(root, args.exe)
    if not exe:
        print("[FAIL] 找不到原版 th185.exe")
        return 1

    if g2plus_cases is None:
        print("[FAIL] 无法导入 g2plus_cases（用例集缺失）")
        return 2

    pe, sections = pe_common.load_pe(exe)
    image_base = pe.OPTIONAL_HEADER.ImageBase

    print("=== G2+ behavioral equivalence ===")
    print("  target: %s" % os.path.basename(exe))
    print("  method: unicorn 执行原版机器码，与 Python 独立重算的语义对比")
    print()

    all_results = []
    total_pass = 0
    total_fail = 0

    for label, func_va, fn_name in g2plus_cases.CASES:
        fn = getattr(g2plus_cases, fn_name, None)
        if fn is None:
            print("  %-32s SKIP（无验证器）" % label)
            continue
        print("  %s" % label)
        try:
            rs = fn(Runner, HEAP_BASE, sections, image_base)
        except Exception as exc:
            print("    %-50s EXCEPTION: %s" % ("(verifier crashed)", exc))
            total_fail += 1
            continue
        for r in rs:
            all_results.append(r)
            if r["status"] == "PASS":
                total_pass += 1
                print("    %-50s PASS" % r["case"])
            else:
                total_fail += 1
                print("    %-50s %s" % (r["case"], r["status"]))
                if r.get("binary"):
                    print("        binary   : %s" % json.dumps(r["binary"]))
                if r.get("expected"):
                    print("        expected : %s" % json.dumps(r["expected"]))
                if r.get("error"):
                    print("        error    : %s" % r["error"])
        print()

    total = total_pass + total_fail
    print("  合计: %d PASS / %d FAIL / %d total" % (total_pass, total_fail, total))
    print()
    status = "PASS" if total_fail == 0 and total > 0 else "FAIL"
    print("[%s] G2+" % status)
    if status == "PASS":
        print("  这证明 Phase A 的语义描述与原版机器码行为一致。")
        print("  注意：**不等于字节相等** —— 那只在 P1 才能验证。")

    if args.write_dashboard:
        write_dashboard(root, all_results, status)

    return 0 if status == "PASS" else 1

def write_dashboard(root, results, status):
    path = os.path.join(root, "dashboard", "metrics.json")
    try:
        with open(path, encoding="utf-8") as fh:
            m = json.load(fh)
    except (OSError, json.JSONDecodeError):
        m = {}
    m["behavioral_equivalence"] = {
        "status": status,
        "cases": len(results),
        "passed": sum(1 for r in results if r["status"] == "PASS"),
        "method": "unicorn 执行原版机器码，与 Python 独立重算的语义对比",
        "_not_p1": "行为等价 ≠ 字节相等；后者仍需编译器",
    }
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())