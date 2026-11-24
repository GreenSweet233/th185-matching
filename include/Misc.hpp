#pragma once

#include <xmmintrin.h>

/**
 * 杂项：返回常量、清字段、全局读写、位操作。
 *
 * 这些函数的共同点是**极短**（3-11 字节），语义完全由机器码可见。
 */

namespace th185 {

// 全局变量（地址来自二进制见证）。定义在 Misc.cpp，extern 保证产生重定位。
extern unsigned g_at_4D74F8;
extern unsigned g_at_4D72F4;
extern void* g_p_kitchen;
namespace misc {

// ---- 返回常量 / 常量地址 ----
void* __fastcall return_579C70(void* self, void* unused_edx);
unsigned __fastcall return_4000(void* self, void* unused_edx);
void* __fastcall return_57A3A4(void* self, void* unused_edx);
void* __fastcall return_57A3A0(void* self, void* unused_edx);
bool __fastcall return_true(void* self, void* unused_edx);

// ---- 清字段 ----
void __fastcall clear_3C(void* self, void* unused_edx);
void __fastcall clear_50(void* self, void* unused_edx);

// ---- 置位 ----
void __fastcall set_bit_6368(void* self, void* unused_edx);

// ---- 全局读写 ----
unsigned __fastcall read_global_4D74F8(void* self, void* unused_edx);
void __fastcall zero_global_4D72F4(void* self, void* unused_edx);

// ---- 第 9 批 ----
void __fastcall set_flags_40_bits1(void* self, void* unused_edx);
void __fastcall set_flags_40_bits01(void* self, void* unused_edx);
void __fastcall store_48_4C(void* self, __m128 ignored, __m128 v1, __m128 v2);
__m128 __fastcall mul_78_x_58(void* self, void* unused_edx);
void __fastcall set_534_and_store_44(void* self, __m128 ignored, __m128 v);
void __fastcall store_44_via_global(void* self, __m128 ignored, __m128 v);
void* __fastcall load_via_global(void* self, void* unused_edx);

}  // namespace misc
}  // namespace th185