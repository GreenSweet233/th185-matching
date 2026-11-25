#pragma once

/**
 * 第 18 批：多条件门禁检查。
 *
 * ## ★ 所有失败分支都跳到同一个 `ret0`
 *
 * 目标把 7 个失败出口全部合并到最后那个 `xor eax,eax; ret`。
 * 源码自然写法（连续 if 提前返回 0，最后返回 1）就能触发这个合并。
 *
 * ## 注意
 *
 * - 函数是**叶子函数**（没有 `push ebp`），参数全在寄存器里 → `__fastcall`
 * - 第一个判断是 `g_4D10CC <= 0`（`jle`，有符号）
 * - 判断顺序不能改
 */

// ★ extern 行不能带行尾注释（会被 check_bytes.py 的提取条件漏掉）
extern int g_4D10CC;
extern char* g_p_4D7ADC;
extern char* g_p_4D7B08;
extern char* g_p_4D7AF4;

int __fastcall gate_check(void* self, void* unused_edx);