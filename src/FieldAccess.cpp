#include "FieldAccess.hpp"

#include <xmmintrin.h>

// 被 0x0048F711 调用的 CPU 特性探测函数（含 cpuid，473 字节）。
// 这里只声明不实现 —— P1 比对时 rel32 是占位值，需要用 relocation 处理。
extern "C" void th185_CpuFeatureProbe(int features);

namespace th185 {

namespace {
// 取 +offset 处的 32 位有符号字段。
//
// ★ 用宏而非 inline 函数：实测只有文本替换能让偏移折叠进 disp8。
//   inline / __forceinline / template 都会生成 `add eax, imm` 而非 `[ecx+disp]`。
//   见 evidence/P3-macro-required-for-disp.json
#define INT_AT(self, offset) \
    reinterpret_cast<int*>(static_cast<char*>(self) + (offset))

// 取 +offset 处的 float 地址。
//
// ★★ 必须是宏，不能是 inline 函数或模板。实测：
//
//   inline float* float_at(void* s, unsigned o)  ->  558bec8b4508 0345 0c5dc3
//       （offset 走栈 [ebp+0xC]，生成 add eax,[ebp+0xC]）
//   __forceinline 同上
//   template<unsigned O>                         ->  558bec 8b4508 83c044 5dc3
//       （生成了 add eax,0x44 —— 仍多一条指令）
//   宏（文本替换）                               ->  f30f104144c3  MATCH
//
// 原因：MSVC 不会把函数参数/模板参数的常量折叠进内存操作数的位移字段，
// 只有预处理阶段的文本替换能让 `[ecx+0x44]` 成为真正的 disp8 编码。
#define FLOAT_AT(self, offset) \
    reinterpret_cast<float*>(static_cast<char*>(self) + (offset))

// 12 字节聚合类型。
//
// ★ 必须用 struct 整体赋值 —— 实测逐字段赋值生成 3 条普通 mov（18 字节），
//   而 struct 赋值生成 movq xmm0(8字节) + mov(4字节)，与原版一致。
struct Vec3Raw {
    int x;
    int y;
    int z;
};
}  // namespace

//@witness func=0x00404D10 kind=stdcall
int __stdcall first_of_three(int a, int b, int c) {
    (void)b;
    (void)c;
    return a;
}

//@witness func=0x00407E40 kind=thiscall
//@witness disp 0x2A0
int __fastcall is_field_2A0_set(void* self, void* unused_edx) {
    (void)unused_edx;
    return *INT_AT(self, kField2A0Offset) != 0;
}

//@witness func=0x0040A050 kind=thiscall
void* __fastcall clear_first_and_return_this(void* self, void* unused_edx) {
    (void)unused_edx;
    *reinterpret_cast<int*>(self) = 0;
    return self;
}

//@witness func=0x00409F60 kind=thiscall
//@witness disp 0x20
void __fastcall set_field_20(void* self, void* unused_edx, int value) {
    (void)unused_edx;
    *INT_AT(self, kField20Offset) = value;
}

//@witness func=0x00433BC0 kind=thiscall
void __fastcall set_first_field(void* self, void* unused_edx, int value) {
    (void)unused_edx;
    *reinterpret_cast<int*>(self) = value;
}

//@witness func=0x004063C0 kind=thiscall
//@witness disp 0x5F0
//@witness disp 0x5F8
void __fastcall copy_vec3_to_5F0(void* self, void* unused_edx, const void* src) {
    (void)unused_edx;
    Vec3Raw* dst = reinterpret_cast<Vec3Raw*>(INT_AT(self, kVec3At5F0Offset));
    *dst = *static_cast<const Vec3Raw*>(src);
}

// --- 0x00404B60 / 0x00405A60：SSE 标量读取 ---
//
// ★ 这两个函数返回 __m128 而不是 float。
//
//   返回 float       -> d9 01        fld dword [ecx]        （x87 栈）
//   返回 __m128+load -> f3 0f 10 01  movss xmm0,[ecx]       （SSE 寄存器）
//
// MSVC 在 x86 上用 x87 返回 float，只有返回 __m128 才走 XMM0。
// 字节: 0x00404B60 = f30f1001c3（5 字节）
//@witness func=0x00404B60 kind=thiscall
__m128 __fastcall read_first_as_m128(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(static_cast<float*>(self));
}

//@witness func=0x00405A60 kind=thiscall
//@witness disp 0x44
__m128 __fastcall read_44_as_m128(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAT_AT(self, kSseField44Offset));
}

// --- 0x0041C710：SSE 标量乘法 ---
//
// ★ 关键是第二个操作数保持**内存形式**：
//   _mm_mul_ss(a, _mm_load_ss(&b))  -> 48 字节（先 load 到寄存器再 mul）
//   _mm_mul_ss(a, *(__m128*)&b)     -> 43 字节 MATCH（mulss 直接读内存）
// 字节: f30f104174 f30f594154 c3
//@witness func=0x0041C710 kind=thiscall
//@witness disp 0x74
//@witness disp 0x54
__m128 __fastcall mul_74_x_54(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_mul_ss(_mm_load_ss(FLOAT_AT(self, kSseField74Offset)),
                      *reinterpret_cast<__m128*>(FLOAT_AT(self, kSseField54Offset)));
}

// ===========================================================================
// 第三批：极简访问器（2-3 条指令，全部无栈帧）
// ===========================================================================

// --- 0x00406730：读 +0x24 ---
// 字节: 8b4124c3 = mov eax,[ecx+0x24] ; ret
// --- 0x004176D0：读 +0x20 ---
// 字节: 8b4120c3
// --- 0x00429A50：取 +0x1c 的地址 ---
// 字节: 8d411cc3 = lea eax,[ecx+0x1c] ; ret
// --- 0x00407E30：取 +0x620 的地址 ---
// 字节: 8d8120060000c3
// --- 0x00424480：清 +0x28 ---
// 字节: c7412800000000c3 = mov dword [ecx+0x28],0 ; ret
// --- 0x00404BD0：返回常量地址 ---
// 字节: b8689c5700c3 = mov eax,0x579C68 ; ret
//@witness func=0x00404BD0 kind=cdecl
void* __fastcall return_const_addr(void* self, void* unused_edx) {
    (void)unused_edx;
    (void)self;
    return reinterpret_cast<void*>(0x00579C68u);
}

// --- 0x00429AB0：清低 4 位 ---
// 字节: 836140f0c3 = and dword [ecx+0x40],0xFFFFFFF0 ; ret
//@witness func=0x00429AB0 kind=thiscall
void __fastcall clear_low_nibble_at_40(void* self, void* unused_edx) {
    (void)unused_edx;
    *reinterpret_cast<unsigned*>(INT_AT(self, kField40Offset)) &= 0xFFFFFFF0u;
}

// ===========================================================================
// 第四批：极简访问器（大偏移量与前几个字节）
// ===========================================================================

// ===========================================================================
// 第五批：含 call 的包装器
// ===========================================================================

// --- 0x0048F711：转发给 CPU 特性探测函数（被 202 个函数调用）---
//
// 字节: 558bec ff7508 e863090000 59 5d c3
//   = push ebp / mov ebp,esp / push [ebp+8] / call 0x49007F / pop ecx / pop ebp / ret
//
// ★ 两个关键发现：
//   ① 必须是 /Os（优化体积）而非 /O2 —— 只有 /Os 生成 `pop ecx` 而非 `add esp,4`
//   ② 必须是 cdecl 且返回 void —— 末尾是 c3(ret) 而非 c20400(ret 4)
////@witness func=0x0048F711 kind=cdecl
void __cdecl forward_to_cpu_probe(int features) {
    th185_CpuFeatureProbe(features);
}

// ===========================================================================
// 第六批：最后几个 2-7 字节纯访问器
// ===========================================================================

}  // namespace th185