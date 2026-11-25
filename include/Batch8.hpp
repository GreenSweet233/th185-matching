#pragma once

/**
 * 第 8 批：常量返回。
 *
 * 这两个函数本身极简，价值在于它们**推翻了一条错误记录**：
 * `return 1` 并非「物理不可得」，只是需要 `/O1` 而非 `/O2`。
 *
 * ## ★ 2026-11-25 修正：必须写成**无参自由函数**，不能用 __fastcall 哑参
 *
 * 原版这里是 `33c0c3` / `33c040c3`，**没有任何 this/ecx 使用痕迹**。
 * 第一版写成 `int __fastcall f(void* self, void* unused_edx)` + 两个
 * `(void)` 消警告语句，字节虽然对，但 G3 的 token/指令比是
 * **12 / 2 = 6.00 > 上限 4.0**，G3 直接 FAIL。
 *
 * 改成无参自由函数后：
 *   - 字节完全相同（cdecl 无参 = 就是 `33c0c3`）
 *   - token 数降到 5，ratio 2.50，G3 PASS
 *
 * 这也更符合证据：**二进制里没有 ecx 的痕迹，就不该有 this 参数**。
 */

namespace th185 {
namespace b8 {

int return_one();
int return_zero();

}  // namespace b8
}  // namespace th185
