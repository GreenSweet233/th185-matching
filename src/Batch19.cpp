#include "Batch19.hpp"
#include <emmintrin.h>

// ===========================================================================
// 1. 大结构体逐字段拷贝（含 8 字节整数）
// ===========================================================================
//
// 原版字节（106 字节）：
//   558bec               push ebp / mov ebp,esp
//   8b5508               mov  edx, [ebp+8]        ; src
//   f30f7e02             movq xmm0, [edx]         ; ★ 加载 8 字节
//   660fd601             movq [ecx], xmm0         ; ★ 存 8 字节
//   8b4208  894108       mov  eax,[edx+8] / mov [ecx+8],eax
//   f30f7e420c 660fd6410c   movq xmm0,[edx+0xc] / movq [ecx+0xc],xmm0  ; ★ 未对齐
//   8b4214 894114 ...    +0x14 .. +0x30 逐个 int
//   f30f7e4234 660fd64134   movq xmm0,[edx+0x34] / movq [ecx+0x34],xmm0
//   8b423c  89413c       +0x3c
//   8b4240  894140       +0x40
//   8bc1                 mov  eax, ecx
//   5dc20400             pop ebp / ret 4
//
// ★★ 四条关键，缺一不可：
//
//   1. **`movq xmm0,[mem]` 只能通过 intrinsic 生成**
//      | 写法 | 生成 | 长度 |
//      |---|---|---:|
//      | `long long` + 普通赋值 | `mov eax,[src] / mov edx,[src+4]` | 118 |
//      | `__m64` 字段 | MMX `0f6f`（mm 寄存器）| 100 |
//      | **`_mm_loadl_epi64` / `_mm_storel_epi64`** | **`f30f7e` / `660fd6`** | **106 ★** |
//
//   2. **8 字节块必须用 `char[8]` 声明**
//      目标里 `+0xc`（非 8 的倍数）也有 8 字节拷贝 —— 需要 1 字节对齐。
//      - `long long` 默认 8 字节对齐 -> 字段被推到 +0x10（字节不对）
//      - `#pragma pack(1)` 能对齐，但 **`#pragma` 行会被 check_bytes.py 过滤掉** -> C2143
//      - **`char[8]` 天然 1 字节对齐，且不需要 pragma** ✓
//
//   3. **必须逐字段写，不能 `*dst = *src`**
//      整体赋值被优化成 `rep movsd`（/O1，23 字节）或 `movups`（/O2，48 字节）。
//
//   4. **必须写成 static 成员函数**
//      因为 check_bytes.py 只在函数签名形如 `Cls::name(` 时才拼入 hpp 的 struct 定义；
//      自由函数的话 `CopyBlock` 类型未定义 -> C2143。
//
//@witness func=0x0040DCA0 kind=thiscall
CopyBlock* __fastcall Copier::copy_big(CopyBlock* dst, void* unused_edx, const CopyBlock* src) {
    (void)unused_edx;
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst->m0),
                     _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src->m0)));
    dst->i0 = src->i0;
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst->m1),
                     _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src->m1)));
    dst->i1 = src->i1;
    dst->i2 = src->i2;
    dst->i3 = src->i3;
    dst->i4 = src->i4;
    dst->i5 = src->i5;
    dst->i6 = src->i6;
    dst->i7 = src->i7;
    dst->i8 = src->i8;
    _mm_storel_epi64(reinterpret_cast<__m128i*>(dst->m2),
                     _mm_loadl_epi64(reinterpret_cast<const __m128i*>(src->m2)));
    dst->i9 = src->i9;
    dst->i10 = src->i10;
    return dst;
}