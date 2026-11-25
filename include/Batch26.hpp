#pragma once

/**
 * 第 26 批：返回 64 位上下界。
 *
 * ## ★★ 两个独立出口，不是 cmov
 *
 * 目标是两个 `ret`：
 *
 * ```asm
 * 80790400        cmp  byte [ecx+4], 0
 * 7409            je   zero
 * 83c8ff          or   eax, -1          ; 大值分支
 * baffff0f00      mov  edx, 0xFFFFF
 * c3              ret
 * zero:
 * b8ffff7f00      mov  eax, 0x7FFFFF    ; 小值分支
 * 33d2            xor  edx, edx
 * c3              ret
 * ```
 *
 * | 写法 | 生成 |
 * |---|---|
 * | `if (flag == 0) return 小; return 大;` | `cmov`（16 字节，单出口）|
 * | **`if (flag != 0) return 大; return 小;`** | **两出口（23 字节）★** |
 *
 * **分支顺序很关键**：先写 `!= 0` 返回大值，编译器才生成「跳转到小值块」的两出口形态。
 *
 * ## 返回类型
 *
 * `unsigned long long`（64 位）：`eax` + `edx`。
 * 大值的低 32 位是 `0xFFFFFFFF`，所以用 `or eax,-1`（等价 `mov eax,-1` 但更短）。
 */

struct QRangeA {
    char unknown_0[4];   ///< +0x00..+0x04
    char flag;           ///< +0x04

    unsigned long long bounds();
};

struct QRangeB {
    char unknown_0[4];   ///< +0x00..+0x04
    char flag;           ///< +0x04

    unsigned long long bounds();
};