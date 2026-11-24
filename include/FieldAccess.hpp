#pragma once

#include <xmmintrin.h>
// 字段访问器族 —— 第二批匹配（P1 已验证字节相等）。
//
// 这批函数全部来自对 .text 的重新扫描，特点是：
//   - 无 call、3-10 条指令
//   - 原版保留完整栈帧（对应编译选项 /Oy-）
//
// 调用约定统一用 __fastcall + 哑 edx 模拟 thiscall。

namespace th185 {

// 二进制见证的字段偏移（不是推测）。
constexpr unsigned kField2A0Offset = 0x2A0;
constexpr unsigned kField20Offset = 0x20;
constexpr unsigned kVec3At5F0Offset = 0x5F0;
constexpr unsigned kSseField44Offset = 0x44;
constexpr unsigned kSseField74Offset = 0x74;
constexpr unsigned kSseField54Offset = 0x54;
constexpr unsigned kField24Offset = 0x24;
constexpr unsigned kField1COffset = 0x1C;
constexpr unsigned kField28Offset = 0x28;
constexpr unsigned kField40Offset = 0x40;
constexpr unsigned kField620Offset = 0x620;
constexpr unsigned kField1274Offset = 0x1274;
constexpr unsigned kField34Offset = 0x34;
constexpr unsigned kField04Offset = 0x04;
constexpr unsigned kField50Offset = 0x50;
constexpr unsigned kField19260Offset = 0x19260;
constexpr unsigned kField47FACOffset = 0x47FAC;
constexpr unsigned kField00Offset = 0x00;
constexpr unsigned kField3COffset = 0x3C;
constexpr unsigned kField4FF4Offset = 0x4FF4;
constexpr unsigned kField1230Offset = 0x1230;

// --- 0x00404D10：stdcall，3 参数，只返回第一个 ---
// 字节: 55 8bec 8b4508 5d c20c00
int __stdcall first_of_three(int a, int b, int c);

// --- 0x00407E40：thiscall，返回 int（不是 bool） ---
// 字节: 33c0 3981 a0020000 0f95c0 c3
//
// ★ 返回类型必须是 int 而非 bool：
//   int  -> xor eax,eax (33c0) + cmp [ecx+0x2A0],eax (3981)
//   bool -> cmp [ecx+0x2A0],0 (83b9)
//   两者都 4 字节差异，但指令不同。
int __fastcall is_field_2A0_set(void* self, void* unused_edx);

// --- 0x0040A050：thiscall，清零首字段并返回 this ---
// 字节: c70100000000 8bc1 c3
void* __fastcall clear_first_and_return_this(void* self, void* unused_edx);

// --- 0x00409F60：thiscall，写 +0x20 ---
// 字节: 55 8bec 8b4508 894120 5d c20400
void __fastcall set_field_20(void* self, void* unused_edx, int value);

// --- 0x00433BC0 / 0x00406720：thiscall，写 +0x00 ---
// 字节: 55 8bec 8b4508 8901 5d c20400
void __fastcall set_first_field(void* self, void* unused_edx, int value);

// --- 0x004063C0：thiscall，12 字节 struct 拷贝到 +0x5F0 ---
// 字节: 55 8bec 8b4508 f30f7e00 660fd681f0050000 8b4008 8981f8050000 5d c20400
void __fastcall copy_vec3_to_5F0(void* self, void* unused_edx, const void* src);

// --- SSE 标量访问（返回 __m128 而非 float）---
__m128 __fastcall read_first_as_m128(void* self, void* unused_edx);
__m128 __fastcall read_44_as_m128(void* self, void* unused_edx);
__m128 __fastcall mul_74_x_54(void* self, void* unused_edx);

// --- 第三批：极简访问器 ---
void* __fastcall return_const_addr(void* self, void* unused_edx);

// --- 第四批 ---

// --- 第六批 ---

}  // namespace th185