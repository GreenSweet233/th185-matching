#pragma once
// 极简叶子函数组 —— Phase A 语义重建。
//
// 这些函数的反汇编形状完全相同，是「同一段源码在多个类里实例化」的典型。
// 按 AGENTS.md §5「叶子/简单函数先行」的原则，它们是第一批匹配对象。
//
// ## 形状 A：`mov eax, ecx; ret`（3 字节）
//
// ```
// mov eax, ecx     ; eax = this
// ret
// ```
//
// thiscall 返回 this 指针。C++ 里对应返回 `*this` 或自身指针的访问器。
// 例：0x00417AF0（15 个调用方）、0x00404AA0（6 个调用方）
//
// ## 形状 B：`ret 0`（3 字节）
//
// ```
// ret 0            ; stdcall/thiscall 的零参数空函数返回
// ```
//
// 空函数体。13 个函数共享此形状（0x00404C20 有 11 个调用方）。
// 典型用途：虚函数的空实现（纯虚之外的默认空体）。
//
// ## 形状 C：`mov eax, [ecx]; ret`（3 字节）
//
// ```
// mov eax, [ecx]   ; 读首字段
// ret
// ```
//
// 返回 this 的第一个 32 位字段。例：0x0041FA20（7 个调用方）

namespace th185 {

// --- 形状 A：返回 this ---
//
// 用 void* 而不是具体类型：本函数不碰任何字段，
// 具体类型目前无证据（AGENTS §0 第 3 条），不编造。
void* __fastcall return_this(void* self, void* unused_edx);

// --- 形状 B：空函数 ---
//
// 无参数、无返回值、无副作用。
void do_nothing();

// --- 形状 C：读首字段 ---
//
// 返回 *reinterpret_cast<unsigned*>(self)。
unsigned __fastcall read_first_field(void* self, void* unused_edx);

// --- 形状 D：读首字段为 float ---
//
// `movss xmm0, [ecx]; ret` —— 与形状 C 只差数据宽度。
float __fastcall read_first_float(void* self, void* unused_edx);

// --- 形状 E：极坐标 → Vec2（`fsincos` 形状，20 个函数共享）---
//
// 反汇编（0x00404B70，20 个之一）：
//
//   mov eax, [ebp-4]        ; eax = this
//   fld  dword [ebp+8]      ; st0 = angle
//   fsincos                 ; st0 = sin(angle), st1 = cos(angle)
//   fmul dword [ebp+0xC]    ; * length
//   fstp dword [eax]        ; this->x = sin(angle) * length
//   fmul dword [ebp+0xC]    ; * length
//   fstp dword [eax+4]      ; this->y = cos(angle) * length
//   ret 8                   ; stdcall/thiscall，2 个 float 参数
//
// 语义：给定角度与长度，算出 (cos*len, sin*len) 写进 this 的前两个 float。
//
// ★★ 这里我错过一次，值得记录。
//
//   我原本从 fsincos 的 x87 栈序推断「st0=sin、st1=cos，第一次 fstp 弹出 sin
//   写到 [eax]，所以 x=sin」。听起来很合理，**但错了**。
//
//   用 unicorn 实际执行 0x00404B70 得到：
//     angle=0.0, length=2.0  ->  x=2.0, y=0.0
//   而 sin(0)=0、cos(0)=1，所以 x 必然是 cos*length。
//
//   实测结论：x = cos(angle)*length，y = sin(angle)*length。
//
//   **教训**：x87 栈序推断容易出错（fsincos 的 st0/st1 顺序、fstp 的弹出语义、
//   以及 fmul 作用在哪一层，任一处记错都会导致结论反了）。这类语义必须实测。
void __fastcall polar_to_vec2(void* self, void* unused_edx, float angle, float length);

// --- 形状 F：返回 0（`xor eax, eax; ret`）---
//
// `xor eax, eax` 是 MSVC 生成 `return 0;` 的标准方式（比 mov eax,0 短 2 字节）。
// 4 个函数：0x004903B9 / 0x00490553 / 0x0049055C / 0x0049055F
int return_zero();

// --- 形状 G：Vec3 拷贝（`movq` + 8 字节）---
//
// 反汇编（0x00405830）：
//
//   mov  eax, [ebp+8]     ; eax = src
//   movq xmm0, [eax]      ; xmm0 = src[0..7]  （前 8 字节，两个 float）
//   movq [ecx], xmm0      ; this[0..7] = xmm0
//   mov  eax, [eax+8]     ; eax = src[8..11] （第三个 float）
//   mov  [ecx+8], eax     ; this[8..11] = eax
//   ret  4
//
// 即 `*this = *src`，12 字节 = 3 个 float（Vec3）。
// MSVC 用 movq 拷前 8 字节 + mov 拷剩余 4 字节，是 12 字节结构体的标准拷贝序列。
// 3 个函数：0x00405830 / 0x00417730 / 0x00441650
void __fastcall copy_vec3(void* self, void* unused_edx, const void* source);

// --- 形状 H：写首字段为 u32（`mov [ecx], eax; ret 4`）---
//
// 反汇编（0x00406720）：
//
//   mov eax, [ebp+8]      ; eax = value
//   mov [ecx], eax        ; this[0..3] = value
//   ret 4
//
// 即 `this->first = value`。3 个函数：0x00406720 / 0x00433BC0 / 0x00433C80
void __fastcall set_first_field(void* self, void* unused_edx, unsigned value);

// --- 形状 E2：极坐标 → Vec2，x/y 用**不同的** length（椭圆）---
//
// 反汇编（0x00417750，`ret 0xC` = 3 个参数）：
//
//   fld  dword [ebp+8]      ; st0 = angle
//   fsincos                 ; st0 = sin, st1 = cos
//   fmul dword [ebp+0xC]    ; * length_x
//   fstp dword [eax]        ; x = sin(angle) * length_x
//   fmul dword [ebp+0x10]   ; * length_y   <- 不同的参数！
//   fstp dword [eax+4]      ; y = cos(angle) * length_y
//   ret 0xC
//
// 与形状 E 的区别：E 用同一个 length 乘两次（圆），
// 这里 x 和 y 各用一个（椭圆 / 各向异性缩放）。
//
// ★ 实测：x = cos(angle) * length_x，y = sin(angle) * length_y（同形状 E 的修正）。
//
// 2 个函数：0x00417750 / 0x00441580
void __fastcall polar_to_vec2_scaled(void* self, void* unused_edx,
                                     float angle, float length_x, float length_y);

// --- 形状 I：指针编码（MSVC /GS 的 __security_check_cookie 用法）---
//
// 反汇编（0x0049DC88）：
//
//   mov  eax, dword [0x4C8A4C]   ; __security_cookie
//   and  eax, 0x1F               ; cookie & 31
//   push 0x20
//   pop  ecx                     ; ecx = 32
//   sub  ecx, eax                ; ecx = 32 - (cookie & 31)  旋转量
//   mov  eax, dword [ebp+8]      ; value
//   ror  eax, cl                 ; 右旋
//   xor  eax, dword [0x4C8A4C]   ; 再与 cookie 异或
//   ret
//
// 这是**指针编码**（encode/decode），不是哈希：
// 同一个变换应用两次会还原（ror + xor 的组合）。
// MSVC 用它来混淆返回值里的指针。
//
// 5 个函数：0x0049DC88 / 0x0049E757 / 0x0049E776 / 0x0049EB13 / 0x0049EBF3
unsigned encode_value(unsigned value);

}  // namespace th185