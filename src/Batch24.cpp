#include "Batch24.hpp"

// ===========================================================================
// 1. 循环初始化 12 个槽位（步长 0x100）
// ===========================================================================
//
// 原版字节（36 字节，叶子函数）：
//   8d81540b0000        lea  eax, [ecx+0xb54]
//   b90c000000          mov  ecx, 0xc
//   0f1f440000          nop  dword [eax+eax]      ; 循环对齐
// loop:
//   c70001000000        mov  dword [eax], 1
//   8d8000010000        lea  eax, [eax+0x100]
//   83e901              sub  ecx, 1
//   75ef                jne  loop
//   c20400              ret  4
//
// ★ 关键：**必须是 do-while（先执行后判断）**
//   `sub ecx,1 / jne` 在循环**末尾**，说明是 do-while 或逆序 for。
//   这是 `--c` 递减计数器的典型形态。
//
//@witness func=0x00460800 kind=thiscall
void SlotBlock::reset(int unused) {
    (void)unused;
    int* p = &slots[0][0];
    int c = 12;
    do {
        *p = 1;
        p = reinterpret_cast<int*>(reinterpret_cast<char*>(p) + 0x100);
        --c;
    } while (c != 0);
}
