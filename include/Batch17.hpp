#pragma once

/**
 * 第 17 批：取链表头节点。
 *
 * ## ★★ 关键：用「结果变量 + 单一出口」
 *
 * ```cpp
 * int result = 0;
 * if (cur) { nextref = cur->next; result = cur->key; }
 * else     { nextref = 0; }
 * return result;
 * ```
 *
 * 写成两个 `return` 会让 MSVC 就近展开并把条件反向（`750b` 而非 `740e`），
 * 长度从 45 掉到 43。
 *
 * 这与 batch14（整除判断）互补：
 *
 * | 场景 | 写法 |
 * |---|---|
 * | batch14 | 两个 `return 0` **共用尾部出口**（先存局部变量）|
 * | batch17 | **单一出口 + 结果变量**（不要两个 return）|
 *
 * 判据：看目标末尾有几处 `ret`。
 * - 两处 `ret 4`（各自 `pop ebp`）→ 独立出口，可以直接两个 return
 * - 一处 `ret 4` + 中间 `je` 跳到 `xor eax,eax` → 需要合并/单一出口
 */

struct ListNode {
    int key;
    ListNode* next;
};

struct NodeBox {
    char unknown_0[0xc0];       ///< +0x00..+0xc0
    ListNode* cur;              ///< +0xc0
    char unknown_c4[0x7c5498];  ///< +0xc4..+0x7c555c
    ListNode* saved;            ///< +0x7c555c
    ListNode* extra;            ///< +0x7c5560
    ListNode* nextref;          ///< +0x7c5564

    int take_head(int unused);
};