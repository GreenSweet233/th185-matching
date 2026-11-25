#include "Batch25.hpp"

// ===========================================================================
// 1. 位域开关 + 条件置无效
// ===========================================================================
//
// 原版字节（41 字节）：
//   558bec               push ebp / mov ebp,esp
//   8b5108               mov  edx, [ecx+8]      ; d = state
//   8bc2                 mov  eax, edx
//   2430                 and  al, 0x30
//   3c20                 cmp  al, 0x20
//   7407                 je   skip
//   c74164ffffffff       mov  dword [ecx+0x64], -1
// skip:
//   8b4508               mov  eax, [ebp+8]      ; bit
//   83e003               and  eax, 3
//   c1e004               shl  eax, 4
//   33c2                 xor  eax, edx         ; ★ (bit<<4) ^ d
//   894108               mov  [ecx+8], eax      ; ★ 但目标这里没有 and！
//   5dc20400             pop ebp / ret 4
//
// 等等 —— 重新核对目标：
//   `8b4508 83e003 c1e004 33c2 83e030 33c2 894108`
//   即 `((bit & 3) << 4) ^ d` 再 `& 0x30` 再 `^ d`。
//
// ★★ 关键：**不能用 `state ^= (bit<<4) & 0x30`**
//
//   | 写法 | 长度 |
//   |---|---:|
//   | `state ^= (bit << 4) & 0x30;` | 39 ✗ |
//   | **`state = d ^ (((bit << 4) ^ d) & 0x30);`** | **41 ★** |
//
//   后者让编译器生成「先 xor 再 and 再 xor」三连（掩码翻转模式），
//   前者只生成「and 后 xor」两条。
//
//   这是标准位操作惯用法：`x ^ ((v ^ x) & mask)` 等价于「把 x 的 mask 位设为 v」。
//
//@witness func=0x00468A20 kind=thiscall
void StateBox::toggle(int bit) {
    const int d = state;
    if ((d & 0x30) != 0x20) {
        invalid_flag = -1;
    }
    state = d ^ (((bit << 4) ^ d) & 0x30);
}