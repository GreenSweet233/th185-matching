#pragma once

#include <xmmintrin.h>

/**
 * 第 20 批：`__vectorcall` 与向量参数位置。
 *
 * ## ★★ 关键：`xmm1` 说明是 `__vectorcall` + 第 1 个向量参数未用
 *
 * `__vectorcall` 的参数顺序：
 *
 * ```
 * ecx / edx   = 前两个整数参数（ecx = this）
 * xmm0..xmm3  = 前四个向量参数
 * ```
 *
 * 目标用 `subss xmm0, xmm1` —— **`xmm1` 而不是 `xmm0`**，说明：
 *
 * - 第 1 个向量参数在 `xmm0`，但本函数没用它
 * - 第 2 个向量参数在 `xmm1`，才是要减的值
 *
 * 所以签名是 `void sub(__m128 unused, __m128 v)`。
 *
 * ## 与 U12 的关系
 *
 * 两者都指向 `__vectorcall`：
 *
 * | 现象 | 原因 |
 * |---|---|
 * | `float` 返回值在 `xmm0` 而非 `st(0)` | `__vectorcall` 的返回约定 |
 * | 向量参数在 `xmm1` 而非 `xmm0` | 前面有一个未用的向量参数 |
 */

struct SubBox {
    char unknown[0xc];   ///< +0x00..+0xc
    float* p;            ///< +0x0c

    void sub(__m128 unused, __m128 v);
};