#pragma once

/**
 * 带栈帧的字段写入器（`push ebp / mov eax,[ebp+8] / mov [ecx+N],eax / ret 4`）。
 *
 * ## 关键规则
 *
 * ★ **必须用 `/Oy-`** —— `/O2` 默认省略帧指针，但原版这些函数保留栈帧。
 * ★ `ret 4` 说明是 `thiscall`（MSVC 自动清理 1 个栈参数）。
 *
 * 源码形式：`void __fastcall f(void* self, void* unused_edx, int v)`
 * 其中 `unused_edx` 占 edx（模拟 thiscall 的 ecx 已有 self），`v` 走栈。
 */

namespace th185 {
namespace fw {

void __fastcall set_34(void* self, void* unused_edx, int v);
void __fastcall set_28(void* self, void* unused_edx, int v);
void __fastcall set_2C(void* self, void* unused_edx, int v);
void __fastcall set_04(void* self, void* unused_edx, int v);
void __fastcall set_08(void* self, void* unused_edx, int v);
void __fastcall set_278(void* self, void* unused_edx, int v);
void __fastcall set_3C(void* self, void* unused_edx, int v);

void __fastcall set_1030_byte(void* self, void* unused_edx, char v);

}  // namespace fw
}  // namespace th185