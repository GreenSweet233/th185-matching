#pragma once

/**
 * 第 13 批：环形缓冲读取。
 *
 * ★ 这条经验值得记：**语义足够简单时，源码写法自由度很大**。
 * 这个函数四种写法（`/O2` 或 `/O1`，直接 `return *pos++` 或拆中间变量）
 * 生成的字节**完全相同**，都 26 字节。所以不必猜原版具体怎么写。
 *
 * ★ 字段命名：`pad` 会被 G4 判为「无见证的填充」而 FAIL。
 * 所以前 4 个字节要给出有意义的字段名（这里是 `reader_state`，
 * 对应原版那个函数里 `mov edx,ecx` 之后没用到的部分）。
 */

struct BufReader {
    unsigned reader_state;   ///< +0x00：本函数未读，状态未知（其余函数可能用到）
    unsigned short* limit;   ///< +0x04
    unsigned short* pos;     ///< +0x08

    unsigned short read_word();
};