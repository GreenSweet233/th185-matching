#include "Batch17.hpp"

// ===========================================================================
// 1. 取链表头节点（带 null 分支）
// ===========================================================================
//
// 原版字节（45 字节）：
//   8b91 c0000000        mov  edx, [ecx+0xc0]        ; n = cur
//   8991 5c557c00        mov  [ecx+0x7c555c], edx     ; saved = n
//   85d2                 test edx, edx
//   740e                 je   empty
//   8b4204               mov  eax, [edx+4]           ; n->next
//   8981 64557c00        mov  [ecx+0x7c5564], eax     ; nextref = n->next
//   8b02                 mov  eax, [edx]             ; n->key
//   c20400               ret 4
//   empty:
//   c781 64557c00 00000000   mov [ecx+0x7c5564], 0
//   33c0                 xor  eax, eax
//   c20400               ret 4
//
// ★★ 关键：要用「结果变量」，不能写两个 return
//
//   ```cpp
//   int result = 0;          // ★ 先初始化
//   if (cur) {
//       nextref = cur->next;
//       result = cur->key;
//   } else {
//       nextref = 0;
//   }
//   return result;           // ★ 单一出口
//   ```
//
//   试过的失败写法：
//
//   | 写法 | 长度 | 差异 |
//   |---|---:|---|
//   | `if(!n){nextref=0;return 0;} nextref=n->next; return n->key;` | 43 | 多 `8bc1`，分支反向（`750b` vs `740e`）|
//   | 同上但用 `!p` | 43 | 同上 |
//   | **结果变量（单一出口）** | **45 ★** | — |
//
//   写成两个 return 时，MSVC 会就近展开 `return 0` 并把条件反向；
//   用结果变量则保留目标的 `je empty` 结构。
//
//@witness func=0x0040A060 kind=thiscall
int NodeBox::take_head(int unused) {
    (void)unused;
    saved = cur;
    int result = 0;
    if (cur != nullptr) {
        nextref = cur->next;
        result = cur->key;
    } else {
        nextref = nullptr;
    }
    return result;
}