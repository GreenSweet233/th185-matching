#pragma once

#include <xmmintrin.h>

/**
 * 第 21 批：二维距离平方。
 *
 * ## ★★ 两个关键
 *
 * ### 1. 必须 `__vectorcall`
 *
 * | 调用约定 | 返回 | 长度 |
 * |---|---|---:|
 * | `__fastcall` | `fld`（x87）| 31（末字节不同）|
 * | **`__vectorcall`** | **`xmm0`** | **31 ★** |
 *
 * ### 2. `dy` 必须先算
 *
 * ```cpp
 * float dy = y - o[1];   // ★ 先
 * float dx = x - o[0];
 * return dx*dx + dy*dy;
 * ```
 *
 * 顺序反了会变成 `xmm0` / `xmm1` 互换 —— **长度都对，只有字节不同**。
 *
 * ### 与 batch20 的关系
 *
 * 两者都靠 `__vectorcall` 解决：
 *
 * | 批次 | 症状 | 根因 |
 * |---|---|---|
 * | batch20 | 参数在 `xmm1` | 前面有一个未用的向量参数 |
 * | batch21 | 返回值在 `xmm0` | `__vectorcall` 的返回约定 |
 */

struct Vec2Box {
    float x;   ///< +0x00
    float y;   ///< +0x04

    float __vectorcall dist2(const float* o);
};