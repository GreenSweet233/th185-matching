#pragma once

/**
 * SSE 标量字段访问器（`movss` 读写）。
 *
 * ## 为什么返回/接受 __m128 而不是 float
 *
 * MSVC 在 x86 上：
 *   float 返回值  -> fld  dword [ecx]    （x87 栈，ST0）
 *   __m128 返回值 -> movss xmm0,[ecx]    （XMM 寄存器）
 *
 * 原版用后者，所以返回类型必须是 __m128。
 *
 * 参数同理：float 参数走栈，__m128 参数占 XMM 寄存器。
 * 写入器 `movss [ecx+0x24], xmm1` 里的 xmm1 说明它是**第 2 个 __m128 参数**
 * （第 1 个占 xmm0，被丢弃）。
 */

#include <xmmintrin.h>

namespace th185 {
namespace sse {

// ---- 读取器：__m128 f(void* self) ----
__m128 __fastcall load_00(void* self, void* unused_edx);
__m128 __fastcall load_04(void* self, void* unused_edx);
__m128 __fastcall load_18(void* self, void* unused_edx);
__m128 __fastcall load_20(void* self, void* unused_edx);
__m128 __fastcall load_24(void* self, void* unused_edx);
__m128 __fastcall load_28(void* self, void* unused_edx);
__m128 __fastcall load_2C(void* self, void* unused_edx);

// ---- 写入器：void f(void* self, __m128 ignored, __m128 value) ----
void __fastcall store_00(void* self, __m128 ignored, __m128 value);
void __fastcall store_04(void* self, __m128 ignored, __m128 value);
void __fastcall store_18(void* self, __m128 ignored, __m128 value);
void __fastcall store_20(void* self, __m128 ignored, __m128 value);
void __fastcall store_24(void* self, __m128 ignored, __m128 value);
void __fastcall store_2C(void* self, __m128 ignored, __m128 value);
void __fastcall store_34(void* self, __m128 ignored, __m128 value);
void __fastcall store_38(void* self, __m128 ignored, __m128 value);
void __fastcall store_6C(void* self, __m128 ignored, __m128 value);
void __fastcall store_74(void* self, __m128 ignored, __m128 value);
void __fastcall store_78(void* self, __m128 ignored, __m128 value);

}  // namespace sse
}  // namespace th185