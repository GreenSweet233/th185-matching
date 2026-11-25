#include "Batch15.hpp"

// ===========================================================================
// 1. 关卡初始化：一串全局赋值
// ===========================================================================
//
// 原版字节（68 字节）：
//   558bec           push ebp / mov ebp,esp
//   8b4d08           mov  ecx, [ebp+8]        ; idx
//   69c1e4020000     imul eax, ecx, 0x2e4     ; off = idx * 0x2e4
//   c705 587c4d00 78d74c00   mov [0x4D7C58], 0x4CD778
//   c705 1c104d00 ffffffff   mov [0x4D101C], -1
//   c705 20104d00 00000000   mov [0x4D1020], 0
//   890d 04104d00            mov [0x4D1004], ecx
//   0590ba4c00               add eax, 0x4CBA90
//   890d 08104d00            mov [0x4D1008], ecx
//   a3547c4d00               mov [0x4D7C54], eax
//   5dc20400                 pop ebp / ret 4
//
// ★ 关键：**这串全局赋值的顺序不能改**。目标顺序是：
//   0x4D7C58 / 0x4D101C / 0x4D1020 / 0x4D1004 / (add eax) / 0x4D1008 / 0x4D7C54
//
//   注意 `add eax,0x4CBA90` 夹在 0x4D1004 和 0x4D1008 之间 ——
//   这是编译器把 `g_4D7C54 = base + off` 的加法提前调度到那里，源码照写即可。
//
//@witness func=0x004315A0 kind=thiscall
void StageInit::init(int idx) {
    const int off = idx * 0x2e4;
    g_4D7C58 = g_bank_4CD778;
    g_4D101C = -1;
    g_4D1020 = 0;
    g_4D1004 = idx;
    g_4D7C54 = g_table_4CBA90 + off;
    g_4D1008 = idx;
}