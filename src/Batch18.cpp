#include "Batch18.hpp"

// ===========================================================================
// 1. 多条件门禁检查
// ===========================================================================
//
// 原版字节（69 字节，叶子函数 —— 注意**没有 push ebp**）：
//   833d cc104d00 00   cmp  dword ptr [0x4D10CC], 0
//   7e39                jle  ret0
//   a1 dc7a4d00         mov  eax, [0x4D7ADC]
//   85c0                test eax, eax
//   7430                je   ret0
//   837830 01           cmp  dword ptr [eax+0x30], 1
//   742a                je   ret0
//   a1 087b4d00         mov  eax, [0x4D7B08]
//   85c0                test eax, eax
//   7421                je   ret0
//   83b8 a0020000 00    cmp  dword ptr [eax+0x2a0], 0
//   7518                jne  ret0
//   a1 f47a4d00         mov  eax, [0x4D7AF4]
//   85c0                test eax, eax
//   7411                je   ret0
//   83b8 444f0000 00    cmp  dword ptr [eax+0x4f44], 0
//   7409                je   ret0
//   b801000000          mov  eax, 1
//   c3                  ret
//   ret0:
//   33c0                xor  eax, eax
//   c3                  ret
//
// ★ 关键：**所有失败分支都跳到同一个 ret0**（`mov eax,1 / ret` 之后）。
//   所以源码要写成「连续 if 提前返回 0，最后返回 1」，
//   MSVC 自然会把它们合并到最后那个 `xor eax,eax; ret`。
//
//   注意顺序不能改 —— 目标第 1 个判断是 `g_4D10CC <= 0`（`jle`，有符号）。
//
//@witness func=0x00428CF0 kind=thiscall
int __fastcall gate_check(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    if (g_4D10CC <= 0) {
        return 0;
    }
    if (g_p_4D7ADC == nullptr) {
        return 0;
    }
    if (*(int*)(g_p_4D7ADC + 0x30) == 1) {
        return 0;
    }
    if (g_p_4D7B08 == nullptr) {
        return 0;
    }
    if (*(int*)(g_p_4D7B08 + 0x2a0) != 0) {
        return 0;
    }
    if (g_p_4D7AF4 == nullptr) {
        return 0;
    }
    if (*(int*)(g_p_4D7AF4 + 0x4f44) == 0) {
        return 0;
    }
    return 1;
}