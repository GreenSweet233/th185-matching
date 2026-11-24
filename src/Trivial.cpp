#include "Trivial.hpp"

// 用编译器内建函数，避免 <cmath> 依赖（P1 只编译单函数，不带头文件）
#include <cmath>

namespace th185 {

// --- 形状 A：返回 this（`mov eax, ecx; ret`）---
//
// 2 个二进制函数共享此形状：0x00404AA0（6 个调用方）、0x00417AF0（15 个调用方）
//@witness func=0x00404AA0+0x00417AF0 kind=thiscall
void* __fastcall return_this(void* self, void* unused_edx) {
    (void)unused_edx;
    return self;
}

// --- 形状 B：空函数（`ret 0`）---
//
// 13 个二进制函数共享此形状。0x00404C20 有 11 个调用方。
// 这是「虚函数的空实现」最常见的形态。
//@witness func=0x00404C20+0x00441640+0x00448100+0x0045CA20+0x004717D0+0x00473800+0x00478740+0x0047A210+0x0048AFA0+0x0048C910+0x0048C920+0x00490592+0x00490595 kind=thiscall
void do_nothing() {
}

// --- 形状 C：读首字段（`mov eax, [ecx]; ret`）---
//
// 2 个：0x0041FA20（7 个调用方）、0x00433B40（4 个调用方）
//@witness func=0x0041FA20+0x00433B40 kind=thiscall
unsigned __fastcall read_first_field(void* self, void* unused_edx) {
    (void)unused_edx;
    return *reinterpret_cast<unsigned*>(self);
}

// --- 形状 D：读首字段为 float（`movss xmm0, [ecx]; ret`）---
//
// 2 个：0x00404B60、0x00441610
//@witness func=0x00404B60+0x00441610 kind=thiscall
float __fastcall read_first_float(void* self, void* unused_edx) {
    (void)unused_edx;
    return *reinterpret_cast<float*>(self);
}

// --- 形状 E：极坐标 → Vec2 ---
//
// 20 个函数共享 `fsincos` 形状。语义见 Trivial.hpp 的注释。
// x = sin(angle)*length，y = cos(angle)*length（注意顺序）。
//@witness func=0x00404B70+0x00405A70+0x00406290+0x00407E10+0x00417B00+0x004299D0+0x004305B0+0x00430720+0x00441670+0x0044CDA0+0x004587B0+0x00463DF0+0x00467150+0x0047FFE0+0x00485B20+0x00486080+0x0048F370 kind=stdcall
void __fastcall polar_to_vec2(void* self, void* unused_edx, float angle, float length) {
    (void)unused_edx;
    float* out = reinterpret_cast<float*>(self);
    out[0] = std::cos(angle) * length;
    out[1] = std::sin(angle) * length;
}

// --- 形状 F：返回 0 ---
//
// `xor eax, eax; ret`。MSVC 生成 `return 0;` 的标准形式。
//@witness func=0x004903B9+0x00490553+0x0049055C+0x0049055F kind=cdecl
int return_zero() {
    return 0;
}

// --- 形状 G：Vec3 拷贝 ---
//
// 反汇编（三个地址逐字节相同）：
//
//   mov  eax, [ebp+8]        ; eax = src
//   movq xmm0, qword [eax]   ; 前 8 字节
//   movq [ecx], xmm0         ; 写入 this 前 8 字节
//   mov  eax, [eax+8]        ; 后 4 字节
//   mov  [ecx+8], eax        ; 写入 this 后 4 字节
//   ret 4
//
// ★ 这是**线性**代码，没有循环。
//
// 我第一版写成 `for (i=0; i<12; ++i) dst[i]=src[i];` ——
// 结果 G3 报 `loop_count_mismatch`：源码 1 个循环 vs 二进制 0 条回边。
// 编译器即使展开循环，源码结构仍与二进制不对齐。
//
// 改成三个 float 的直接赋值，结构上就是一串直线代码 + 1 个 ret。
// 注意顺序：前 8 字节作为一个 movq 一起动，符合「先 2 个 float 再 1 个」的字段序。
// Phase A 写法要简洁：中间变量会让 token/指令比超上限（实测 4.67 > 4.00）。
// 原版就是三次读取 + 三次写入，没有暂存。
//@witness func=0x00405830+0x00417730+0x00441650 kind=thiscall
namespace {
// 12 字节的向量结构。
//
// ★ 必须用 struct 整体赋值，不能逐字段赋。
//   实测：3 次 u32 赋值生成 3 条 mov（18 字节），
//   struct 赋值才生成原版那样的 movq xmm0(8 字节) + mov(4 字节)。
//   这是 MSVC 对 12 字节聚合类型的特殊代码生成路径。
struct Vec3Raw {
    int x;
    int y;
    int z;
};
}  // namespace

void __fastcall copy_vec3(void* self, void* unused_edx, const void* source) {
    (void)unused_edx;
    *static_cast<Vec3Raw*>(self) = *static_cast<const Vec3Raw*>(source);
}

// --- 形状 H：写首字段 ---
//
// `this[0..3] = value`。
//@witness func=0x00406720+0x00433BC0+0x00433C80 kind=thiscall
void __fastcall set_first_field(void* self, void* unused_edx, unsigned value) {
    (void)unused_edx;
    *reinterpret_cast<unsigned*>(self) = value;
}

// --- 形状 E2：极坐标 → Vec2，x/y 用不同 length ---
//
// 与形状 E 只差一点：E 两个分量乘同一个 length，这里各乘一个。
// `ret 0xC` = 3 个参数，确认是 2 个 float + this。
//@witness func=0x00417750+0x00441580 kind=stdcall
void __fastcall polar_to_vec2_scaled(void* self, void* unused_edx,
                                     float angle, float length_x, float length_y) {
    (void)unused_edx;
    float* out = reinterpret_cast<float*>(self);
    // ★ 实测修正：x 用 cos*length_x，y 用 sin*length_y
    out[0] = std::cos(angle) * length_x;
    out[1] = std::sin(angle) * length_y;
}

// --- 形状 I：指针编码 ---
//
// 原版的循环移位量是 `32 - (cookie & 31)`，但这个值可能等于 32。
// x86 的 `ror eax, cl` 对 32 位操作数会把 CL **按 5 位掩码**，
// 所以 cl=32 时实际旋转 0 次 —— 等价于不旋转。
//
// ★ 写成 `(value >> shift) | (value << (32 - shift))` 在 shift==32 时是
//   **未定义行为**（C++ 规定移位量 >= 类型宽度是 UB）。
//   G5 本可以抓到这个问题（UBSan 会报 shift-out-of-bounds），
//   但 G5 因编译器缺口无法运行 —— 所以这里靠人工推导修正。
//   正确写法是先掩码到 0..31，与 x86 硬件行为一致。
//@witness func=0x0049DC88+0x0049E757+0x0049E776+0x0049EB13+0x0049EBF3 kind=cdecl
unsigned encode_value(unsigned value) {
    const unsigned cookie = *reinterpret_cast<const unsigned*>(0x004C8A4Cu);
    // 掩码到 5 位：与 x86 对 `ror` 计数器的处理一致
    const unsigned shift = (32u - (cookie & 0x1Fu)) & 0x1Fu;
    if (shift == 0u) {
        return value ^ cookie;
    }
    const unsigned rotated = (value >> shift) | (value << (32u - shift));
    return rotated ^ cookie;
}

}  // namespace th185