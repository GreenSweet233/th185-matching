#pragma once

/**
 * 第 15 批：关卡初始化（一串全局赋值）。
 *
 * ★ 这串赋值的顺序必须和目标一致：
 *
 * ```
 * 0x4D7C58 / 0x4D101C / 0x4D1020 / 0x4D1004 / (add eax) / 0x4D1008 / 0x4D7C54
 * ```
 *
 * ★★ 关键陷阱：目标是 `mov dword [0x4D7C58], 0x4CD778`（直接存常量地址）。
 *    所以源码头文件里 `0x4CD778` 处的对象必须声明成**数组**（数组名退化为地址），
 *    不能声明成 `extern void* p;`（那样 `a = p` 会读 `[p]` 的内容，不是地址 p）。
 */

// ★ extern 行不能带行尾注释（会被 check_bytes.py 的提取条件漏掉）
extern void* g_4D7C58;
extern int g_4D101C;
extern int g_4D1020;
extern int g_4D1004;
extern int g_4D1008;
extern void* g_4D7C54;
// 两个符号写成数组 —— 数组名退化为地址常量，才能生成 mov [mem], imm32
extern char g_bank_4CD778[];
extern char g_table_4CBA90[];

struct StageInit {
    void init(int idx);
};