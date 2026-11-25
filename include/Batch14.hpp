#pragma once

/**
 * 第 14 批：整除判断。
 *
 * ★ 本批最重要的经验：**让多个 `return 0` 共用尾部出口**
 *
 * ```cpp
 * int f(int d) {
 *     if (b == a) return 0;
 *     int m = b % d;        // ★ 存进局部变量再判断
 *     if (m != 0) return 0;
 *     return 1;
 * }
 * ```
 *
 * 这样编译器会把两个 `return 0` 合并到函数末尾（共用 `xor eax,eax; pop ebp; ret`）。
 * 直接写 `return (b % d) == 0;` 会让编译器选择「就近展开」，字节就不同了。
 */

struct DivCheck {
    int a;   ///< +0x00
    int b;   ///< +0x04

    int divisible(int d);
};