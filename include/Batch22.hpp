#pragma once

/**
 * 第 22 批：角度归一化到 `(-PI, PI]`。
 *
 * 共享常量（`.rdata` 实测）：
 *
 * ```
 * 0x4BC81C =  3.1415927410125732f   (+PI)
 * 0x4BC850 =  6.2831854820251465f   (2PI)
 * 0x4BCA34 = -3.1415927410125732f   (-PI)
 * ```
 *
 * ## ★★ 三条关键
 *
 * ### 1. `__vectorcall`
 *
 * `float` 参数在 `xmm0`，返回也在 `xmm0`（不是 x87 `st(0)`）。
 *
 * ### 2. `int old = n` 必须在自增**之前**保存
 *
 * ```cpp
 * while (a > kPi) { int old = n; a -= k2Pi; ++n; if (old > 0x20) break; }
 * ```
 *
 * 目标用 `mov eax,ecx` 保存旧值、`cmp eax,0x20` 判断旧值。
 *
 * 如果写成 `while (a > kPi && n <= 0x20)`，MSVC 会在循环**入口**判断，
 * 生成 67 字节（不是 71）。
 *
 * ### 3. 两个 while 共享同一个 `n`（不重置）
 *
 * ## 两个实例
 *
 * | 地址 | 长度 | 输入 | 输出 |
 * |---|---:|---|---|
 * | `0x004029B0` | 71 | `xmm0` | `xmm0` |
 * | `0x00404B10` | 77 | `xmm1` | `[ecx]` + 返回 `ecx`（指针）|
 */


struct AngleBox {
    float value;      ///< +0x00
    char  unknown_4[0x18];   ///< +0x04..+0x1c
    float field_1c;   ///< +0x1c

    float  __vectorcall normalize(float a);
    float* __vectorcall normalize_store(float unused, float in);
    void   __vectorcall normalize_to_field(float unused, float in);
    float* __vectorcall normalize_add_store(float unused, float in);
};