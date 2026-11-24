#include "Misc.hpp"

#include <xmmintrin.h>

namespace th185 {
namespace misc {

// 全局变量定义。地址由二进制见证给出，实际布局由链接器决定。
unsigned g_at_4D74F8 = 0;
unsigned g_at_4D72F4 = 0;
void* g_p_kitchen = nullptr;
unsigned g_at_4D7820 = 0;
unsigned g_at_4D7824 = 0;

// ===========================================================================
// 形态 1：不碰 ecx —— 普通函数
// ===========================================================================

//@witness func=0x0045C9E0 kind=cdecl
void* return_579C70() { return reinterpret_cast<void*>(0x00579C70u); }

//@witness func=0x00490556 kind=cdecl
unsigned return_4000() { return 0x4000u; }

//@witness func=0x004905C1 kind=cdecl
void* return_57A3A4() { return reinterpret_cast<void*>(0x0057A3A4u); }

//@witness func=0x004905C7 kind=cdecl
void* return_57A3A0() { return reinterpret_cast<void*>(0x0057A3A0u); }

//@witness func=0x0049056E kind=cdecl
bool return_true() { return true; }

//@witness func=0x0049EBDE kind=cdecl
unsigned read_global_4D74F8() { return g_at_4D74F8; }

//@witness func=0x004A1738 kind=cdecl
unsigned read_global_4D7820() { return g_at_4D7820; }

//@witness func=0x004A173E kind=cdecl
unsigned read_global_4D7824() { return g_at_4D7824; }

// --- 0x00490463：`and dword [0x4D72F4], 0` ---
//
// ★ 这是一个**已知不可匹配**项（ledger/unresolved.md）。
//   现代 MSVC 对 `g = 0` 一律生成 `mov dword [addr],0`（c705 + imm32），
//   不会生成 `and [mem],0`（8325 + disp32 + imm8）。
//   原版用 `and` 是当年更省字节的写法（8 字节 vs 10 字节），
//   但 MSVC 前端在有 `and` 优化机会时选择了 `mov`。
//   实测 45 种写法/选项组合均为 `mov`。
//   保留源码形态（语义正确），但不声称字节匹配。
//@witness func=0x00490463 kind=cdecl
void zero_global_4D72F4() { g_at_4D72F4 = 0; }

//@witness func=0x0049EEBB kind=cdecl
void* return_addr_4D750C() { return reinterpret_cast<void*>(0x004D750Cu); }

//@witness func=0x004A1726 kind=cdecl
void* return_addr_4D7818() { return reinterpret_cast<void*>(0x004D7818u); }

//@witness func=0x004A172C kind=cdecl
void* return_addr_4D781C() { return reinterpret_cast<void*>(0x004D781Cu); }

//@witness func=0x004A1732 kind=cdecl
void* return_addr_4D7814() { return reinterpret_cast<void*>(0x004D7814u); }

//@witness func=0x00433850 kind=cdecl
void* load_via_global() {
    return *reinterpret_cast<void**>(static_cast<char*>(g_p_kitchen) + 0xE6BB2C);
}

// ===========================================================================
// 形态 3：只碰全局指针 + xmm 参数
// ===========================================================================

//@witness func=0x00433820 kind=thiscall
void __fastcall store_44_via_global(__m128, __m128 value) {
    _mm_store_ss(reinterpret_cast<float*>(static_cast<char*>(g_p_kitchen) + 0x44),
                 value);
}

// ===========================================================================
// 形态 2：碰 ecx —— 成员函数
// ===========================================================================

//@witness func=0x004407F0 kind=thiscall
void ObjClear3C::clear() { f_3C = 0; }

//@witness func=0x00440B40 kind=thiscall
void ObjClear50::clear() { f_50 = 0; }

//@witness func=0x00433DE0 kind=thiscall
void ObjBit6368::set_bit() { f_6368 |= 0x2000000u; }

// --- 0x00429A10：`and 0xFFFFFFF2 | or 2` ---
//@witness func=0x00429A10 kind=thiscall
void ObjFlags40::set_mask_f2_or_2() { f_40 = (f_40 & 0xFFFFFFF2u) | 2u; }

//@witness func=0x00441260 kind=thiscall
void ObjFlags40::set_mask_f3_or_3() { f_40 = (f_40 & 0xFFFFFFF3u) | 3u; }

// --- 0x0048AC40：置位 +0x534 后写 float +0x44 ---
// 字节: 83893405000004 f30f114944 c3
//@witness func=0x0048AC40 kind=thiscall
void ObjSse44::set_534_and_store_44(__m128, __m128 value) {
    f_534 |= 4u;
    _mm_store_ss(&f_44, value);
}

// --- 0x00429B90：一次写两个相邻 float（xmm1, xmm2）---
// 字节: f30f114948 f30f11514c c3
//@witness func=0x00429B90 kind=thiscall
void ObjSse484C::store_both(__m128, __m128 v1, __m128 v2) {
    _mm_store_ss(&f_48, v1);
    _mm_store_ss(&f_4C, v2);
}

// --- 0x00441620：读两个 float 后相乘 ---
// 字节: f30f104178 f30f594158 c3
// ★ 第二个操作数必须保持**内存形式**：
//   _mm_mul_ss(a, _mm_load_ss(&b)) -> 先 load 到寄存器再 mul（字节不符）
//   _mm_mul_ss(a, *(__m128*)&b)    -> mulss 直接读内存（MATCH）
//@witness func=0x00441620 kind=thiscall
__m128 __fastcall ObjMul7858::mul(ObjMul7858* self) {
    return _mm_mul_ss(_mm_load_ss(&self->f_78),
                      *reinterpret_cast<__m128*>(&self->f_58));
}

}  // namespace misc
}  // namespace th185
