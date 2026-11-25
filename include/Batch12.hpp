#pragma once

/**
 * 第 12 批：全局表线性搜索。
 *
 * ## 为什么是 static 成员函数
 *
 * `check_bytes.py` 的 TU 拼装规则：
 *
 * | 来源 | 带什么 |
 * |---|---|
 * | hpp | `constexpr` / `static const` 行、`extern ` 开头且 `;` 结尾的行 |
 * | cpp | 匿名 namespace 块 |
 * | cpp | `extern "C"` 单行 |
 * | **hpp 整体** | **仅当函数签名是 `Cls::name(` 时** |
 *
 * 所以 struct 定义要进 TU，函数就**必须**写成成员函数（static 也行）。
 *
 * ## ★ 但 constexpr 不能放这里
 *
 * 它会被 prefix **单独提取一次**，同时成员函数路径又把整个 hpp 拼接一次
 * -> C2374 重定义。所以数组长度直接写字面量 `87`。
 *
 * ## 为什么表是 extern 声明
 *
 * 若在本 TU 内定义 `Entry g_table[87];`，MSVC 认为它无外部引用，
 * 把函数当死代码删掉 -> `.obj` 里没有 `.text` -> COFF_ERROR。
 */

/// 表项：key 在 +4，不在 +0。
struct Entry {
    int x;
    int key;
    char rest[0x3c];
};

// ★ extern 行不能带行尾注释（会被 check_bytes.py 的提取条件漏掉）
extern Entry g_table[87];
extern Entry g_sentinel;

struct Finder {
    static Entry* __fastcall find_by_key(unsigned key);
};