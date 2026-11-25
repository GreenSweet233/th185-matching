#pragma once

/**
 * 第 10 批：全局标志比较 + 双写。
 *
 * ★ 本批最重要的教训：**`int` 返回 vs `bool` 返回**
 *
 * ```
 * int  f() { return a == b; }   ->  xor eax,eax / cmp / sete al / ret
 * bool f() { return a == b; }   ->  cmp / sete al / ret        （少 xor）
 * ```
 *
 * 判据：`sete al` 前面有 `33c0`（xor eax,eax）就是 `int`。
 *
 * ★ 本批踩的两个坑：
 *
 * 1. **extern 行不能带行尾注释**
 *    `check_bytes.py` 的提取条件是 `s.startswith("extern ") and s.endswith(";")`，
 *    写成 `extern int g;  // 注释` 会因为末尾不是 `;` 而被跳过，
 *    导致函数体里 C2065「未声明的标识符」。
 *    注释要单独放一行。
 *
 * 2. **函数体不能放在 namespace 里**
 *    extern 声明会被提到全局作用域，namespace 内的函数引用不到（C2065）。
 */

// 0x4C8A4C：__security_cookie
extern int g_security_cookie;
// 0x4D7520：被比较的全局标志
extern int g_flag_4D7520;
// 0x4D7B0C：全局对象指针
extern void* g_ptr_4D7B0C;
// 0x4D100C：另一个全局
extern int g_last_4D100C;

int __fastcall is_global_cookie_flag(void* self, void* unused_edx);
void __fastcall set_param_to_two_places(void* self, void* unused_edx, int v);