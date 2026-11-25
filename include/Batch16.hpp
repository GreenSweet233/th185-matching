#pragma once

/**
 * 第 16 批：链表遍历查找。
 *
 * struct 定义和辅助函数都在 cpp 的匿名 namespace 里（原因见 cpp 顶部注释）。
 *
 * ## ★★ 关键：必须是自由函数 + 全局指针
 *
 * | 写法 | 生成 |
 * |---|---|
 * | 成员函数 | `8b411c`（直接 `[ecx+0x1c]`）|
 * | **自由函数 + 全局** | **`a1 <addr> / 8b401c`**（先取全局再取字段）|
 *
 * 目标用的是后者。
 */

// ★ extern 行不能带行尾注释（会被 check_bytes.py 的提取条件漏掉）
extern char* g_p_4D7AB8;
extern char* g_p_4D7AF4;

int __fastcall list_a_contains(void* self, void* unused_edx, int key);
int __fastcall list_b_contains(void* self, void* unused_edx, int key);