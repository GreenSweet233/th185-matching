#include "Misc.hpp"

#include <xmmintrin.h>

namespace th185 {
namespace misc {

// 全局变量定义。地址由二进制见证给出，实际布局由链接器决定。
unsigned g_at_4D74F8 = 0;
unsigned g_at_4D72F4 = 0;

// 全局对象指针（0x004D7AE0 / 0x004D7C14 处的东西）。
// 地址由二进制见证给出，实际布局由链接器决定。
void* g_p_kitchen = nullptr;

namespace {
// 取 +offset 处的 float 地址。★ 必须是宏（inline/template 会生成 add 而非 disp8）。
#define FLOAT_AT(self, offset) \
    reinterpret_cast<float*>(static_cast<char*>(self) + (offset))

// 取 +offset 处的 32 位字段。
// ★ 必须是宏 —— inline 函数/template 会生成 add 而非 disp8。
#define INT_AT(self, offset) \
    reinterpret_cast<int*>(static_cast<char*>(self) + (offset))
}  // namespace

// ===========================================================================
// 返回常量
// ===========================================================================

//@witness func=0x0045C9E0 kind=thiscall
void* __fastcall return_579C70(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return reinterpret_cast<void*>(0x00579C70u);
}

//@witness func=0x00490556 kind=thiscall
unsigned __fastcall return_4000(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return 0x4000u;
}

//@witness func=0x004905C1 kind=thiscall
void* __fastcall return_57A3A4(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return reinterpret_cast<void*>(0x0057A3A4u);
}

//@witness func=0x004905C7 kind=thiscall
void* __fastcall return_57A3A0(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return reinterpret_cast<void*>(0x0057A3A0u);
}

//@witness func=0x0049056E kind=thiscall
bool __fastcall return_true(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return true;
}

// ===========================================================================
// 清字段
// ===========================================================================

//@witness func=0x004407F0 kind=thiscall
void __fastcall clear_3C(void* self, void* unused_edx) {
    (void)unused_edx;
    *INT_AT(self, 0x3C) = 0;
}

//@witness func=0x00440B40 kind=thiscall
void __fastcall clear_50(void* self, void* unused_edx) {
    (void)unused_edx;
    *INT_AT(self, 0x50) = 0;
}

// ===========================================================================
// 置位
// ===========================================================================

//@witness func=0x00433DE0 kind=thiscall
void __fastcall set_bit_6368(void* self, void* unused_edx) {
    (void)unused_edx;
    *reinterpret_cast<unsigned*>(INT_AT(self, 0x6368)) |= 0x2000000u;
}

// ===========================================================================
// 全局读写（需要 relocation 屏蔽：未链接时地址是 0）
// ===========================================================================

//@witness func=0x0049EBDE kind=thiscall
unsigned __fastcall read_global_4D74F8(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return g_at_4D74F8;
}

//@witness func=0x00490463 kind=thiscall
void __fastcall zero_global_4D72F4(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    g_at_4D72F4 = 0;
}

// ===========================================================================
// 第 9 批：位运算组合 / 多字段写 / 全局指针链
// ===========================================================================

// --- 0x00429A10 / 0x00441260：先 and 掩码再 or 置位 ---
// 字节: 8b4140 83e0f2 83c802 894140 c3   (and 0xFFFFFFF2 | or 2)
//@witness func=0x00429A10 kind=thiscall
void __fastcall set_flags_40_bits1(void* self, void* unused_edx) {
    (void)unused_edx;
    unsigned* p = reinterpret_cast<unsigned*>(INT_AT(self, 0x40));
    *p = (*p & 0xFFFFFFF2u) | 2u;
}

//@witness func=0x00441260 kind=thiscall
void __fastcall set_flags_40_bits01(void* self, void* unused_edx) {
    (void)unused_edx;
    unsigned* p = reinterpret_cast<unsigned*>(INT_AT(self, 0x40));
    *p = (*p & 0xFFFFFFF3u) | 3u;
}

// --- 0x00429B90：一次写两个相邻 float（xmm1, xmm2）---
//
// ★ 第 1 个 __m128 参数占 xmm0（丢弃），第 2/3 个分别落 xmm1/xmm2。
// 字节: f30f114948 f30f11514c c3
//@witness func=0x00429B90 kind=thiscall
void __fastcall store_48_4C(void* self, __m128 ignored, __m128 v1, __m128 v2) {
    (void)ignored;
    _mm_store_ss(FLOAT_AT(self, 0x48), v1);
    _mm_store_ss(FLOAT_AT(self, 0x4C), v2);
}

// --- 0x00441620：读两个 float 后相乘 ---
// 字节: f30f104178 f30f594158 c3
//@witness func=0x00441620 kind=thiscall
__m128 __fastcall mul_78_x_58(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_mul_ss(_mm_load_ss(FLOAT_AT(self, 0x78)),
                      *reinterpret_cast<__m128*>(FLOAT_AT(self, 0x58)));
}

// --- 0x0048AC40：置位后写 float ---
// 字节: 83893405000004 f30f114944 c3
//@witness func=0x0048AC40 kind=thiscall
void __fastcall set_534_and_store_44(void* self, __m128 ignored, __m128 v) {
    (void)ignored;
    *reinterpret_cast<unsigned*>(INT_AT(self, 0x534)) |= 4u;
    _mm_store_ss(FLOAT_AT(self, 0x44), v);
}

// --- 0x00433820：通过全局指针写 float ---
// 字节: a1 e07a4d00 f30f114844 c3
// ★ 全局地址在未链接的 .obj 里是 0，靠 relocation 屏蔽比对。
//@witness func=0x00433820 kind=thiscall
void __fastcall store_44_via_global(void* self, __m128 ignored, __m128 v) {
    (void)self;
    (void)ignored;
    _mm_store_ss(reinterpret_cast<float*>(static_cast<char*>(g_p_kitchen) + 0x44), v);
}

// --- 0x00433850：全局指针 -> 再取字段 ---
// 字节: a1 147c4d00 8b80 2cbbe600 c3
//@witness func=0x00433850 kind=thiscall
void* __fastcall load_via_global(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return *reinterpret_cast<void**>(
        static_cast<char*>(g_p_kitchen) + 0xE6BB2C);
}

}  // namespace misc
}  // namespace th185