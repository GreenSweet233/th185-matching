#pragma once

/**
 * 第 23 批：角度归一化（两参数相加版）。
 *
 * ## ★★ 关键：自由函数 + `__vectorcall`，不是成员函数
 *
 * `__vectorcall` 的参数分配：
 *
 * ```
 * ecx / edx    = 前两个**整数**参数
 * xmm0..xmm3   = 前四个**向量/浮点**参数
 * ```
 *
 * 目标是 `addss xmm0, xmm1` —— 两个 float 参数分别落在 `xmm0` 和 `xmm1`。
 *
 * | 写法 | 长度 | 说明 |
 * |---|---:|---|
 * | 成员函数 `float f(float x, float y)` | 85 | 多一条 `movss xmm0,[mem]` |
 * | **自由函数 `float f(void* self, float x, float y)`** | **82 ★** | `self` 占 `ecx` 不用 |
 *
 * **判据：目标如果直接 `addss xmm0, xmm1` 开头，说明是自由函数。**
 * 成员函数会在参数传递上多一条搬运指令。
 *
 * ## 与 batch22 的关系
 *
 * batch22 的 4 个实例都是**成员函数**（`ecx` = this，被真正使用）。
 * 本批这个是**自由函数**（`ecx` = 未用的 self）。
 *
 * 区分方法：看函数开头有没有读 `[ecx+...]`。
 */

constexpr float kPi = 3.1415927410125732f;
constexpr float k2Pi = 6.2831854820251465f;
constexpr float kNegPi = -3.1415927410125732f;

float __vectorcall angle_add_normalize(void* self, float x, float y);