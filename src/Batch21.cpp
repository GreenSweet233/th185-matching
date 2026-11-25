#include "Batch21.hpp"
#include <xmmintrin.h>

// ===========================================================================
// 1. 二维距离平方（返回 float，走 xmm0）
// ===========================================================================
//
// 原版字节（31 字节，叶子函数）：
//   f30f104904   movss xmm1, [ecx+4]    ; y
//   f30f1001     movss xmm0, [ecx]      ; x
//   f30f5c02     subss xmm0, [edx]      ; dx = x - o[0]
//   f30f5c4a04   subss xmm1, [edx+4]    ; dy = y - o[1]
//   f30f59c0     mulss xmm0, xmm0
//   f30f59c9     mulss xmm1, xmm1
//   f30f58c1     addss xmm0, xmm1       ; 结果在 xmm0
//   c3           ret
//
// ★★ 两个关键：
//
//   1. **必须 `__vectorcall`**
//      - `__fastcall` -> 返回走 x87（`fld`），31 字节但最后一字节不同
//      - `__vectorcall` -> 返回走 xmm0 ✓
//
//   2. **`dy` 必须先算**
//      ```cpp
//      float dy = y - o[1];   // ★ 先
//      float dx = x - o[0];
//      return dx*dx + dy*dy;
//      ```
//      顺序反了会变成 `xmm0↔xmm1` 互换（31 字节但内容不同）。
//
//   也可以用 inline 表达式：
//      `return (x-o[0])*(x-o[0]) + (y-o[1])*(y-o[1]);`
//      同样匹配（编译器自然调度成 dy 先）。
//
//@witness func=0x004415C0 kind=thiscall
float __vectorcall Vec2Box::dist2(const float* o) {
    const float dy = y - o[1];
    const float dx = x - o[0];
    return dx * dx + dy * dy;
}