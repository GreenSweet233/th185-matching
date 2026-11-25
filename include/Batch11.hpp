#pragma once

/**
 * 第 11 批：12 字节以内的聚合拷贝。
 *
 * ★ 本批最重要的一条：**聚合类型必须整体赋值**
 *
 * ```cpp
 * f = *source;              // 8b4508 ... 8b10 ... 8b4004   (eax 作基址)
 * x = s[0]; y = s[1];       // 8b5508 ... 8b02 ... 8b4204   (edx 作基址)  ✗
 * ```
 *
 * 语义相同，但 MSVC 选了不同寄存器作基址，字节全错。
 */

struct Pair2 {
    int a;
    int b;
};

struct ObjPair {
    char p[8];
    Pair2 f;

    void copy_from(const Pair2* source);
};