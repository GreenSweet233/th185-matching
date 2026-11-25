#pragma once

/**
 * 第 25 批：位域开关 + 条件置无效。
 *
 * ## ★★ 关键：用掩码翻转惯用法 `x ^ ((v ^ x) & mask)`
 *
 * | 写法 | 长度 |
 * |---|---:|
 * | `state ^= (bit << 4) & 0x30;` | 39 ✗ |
 * | **`state = d ^ (((bit << 4) ^ d) & 0x30);`** | **41 ★** |
 *
 * 目标生成「先 `xor` 再 `and` 再 `xor`」三连：
 *
 * ```asm
 * 33c2        xor  eax, edx        ; (bit<<4) ^ d
 * 83e030      and  eax, 0x30       ; & mask
 * 33c2        xor  eax, edx        ; ^ d      <- 把 d 的 mask 位换成 bit 的值
 * ```
 *
 * 而 `^=` 写法只生成「and 后 xor」两条。
 *
 * **`x ^ ((v ^ x) & mask)` 是「把 x 的 mask 位设为 v」的标准惯用法。**
 * 看到目标有三连 `xor/and/xor`，就应该写成这个形式。
 */

struct StateBox {
    char unknown_0[8];   ///< +0x00..+0x08
    int  state;          ///< +0x08
    char unknown_c[0x58]; ///< +0x0c..+0x64
    int  invalid_flag;   ///< +0x64

    void toggle(int bit);
};