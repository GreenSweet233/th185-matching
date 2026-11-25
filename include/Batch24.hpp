#pragma once

/**
 * 第 24 批：初始化循环 + 索引写入。
 *
 * ## 1. `0x00460800` —— do-while 递减计数器
 *
 * `sub ecx,1 / jne` 在循环**末尾**，说明是 do-while：
 *
 * ```cpp
 * int c = 12;
 * do { *p = 1; p += 0x100/sizeof(int); --c; } while (c);
 * ```
 *

 */

struct SlotBlock {
    char unknown_0[0xb54];           ///< +0x00..+0xb54
    int  slots[12][0x40];            ///< +0xb54 起，每个槽 0x100 字节
    char unknown_tail[0x100];        ///< 末尾预留（reset 会写到第 12 个槽之外）

    void reset(int unused);
};
