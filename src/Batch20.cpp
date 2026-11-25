#include "Batch20.hpp"
#include <xmmintrin.h>

// ===========================================================================
// 1. 从 *p 减去一个 __m128 的低位
// ===========================================================================
//
// 原版字节（16 字节）：
//   8b410c              mov   eax, [ecx+0xc]        ; p
//   f30f1000            movss xmm0, [eax]          ; *p
//   f30f5cc1            subss xmm0, xmm1           ; ★ 减 xmm1
//   f30f1100            movss [eax], xmm0
//   c3                  ret
//
// ★★ 关键：**`xmm1` 说明是 `__vectorcall`，且第 1 个向量参数未用**
//
//   `__vectorcall` 的参数顺序：
//     ecx/edx = 前两个整数参数  →  ecx = this
//     xmm0..xmm3 = 前四个向量参数
//
//   目标用 `xmm1` 而不是 `xmm0`，说明：
//   - 第 1 个向量参数放在 `xmm0`，但本函数**没用它**
//   - 第 2 个向量参数在 `xmm1`，才是真正要减的值
//
//   所以源码签名是 `void sub(__m128 unused, __m128 v)`。
//
//   试过的失败写法：
//
//   | 写法 | 生成 | 差异 |
//   |---|---|---|
//   | `void sub(__m128 v)`（1 个向量参数）| `subss xmm0, ???` | 用 xmm0，不是 xmm1 |
//   | `__fastcall(self, d)` 用 float | `fld`/`fsub` | x87 |
//
//@witness func=0x00433970 kind=thiscall
void SubBox::sub(__m128 unused, __m128 v) {
    (void)unused;
    *p = *p - _mm_cvtss_f32(v);
}