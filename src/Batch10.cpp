#include "Batch10.hpp"

// ===========================================================================
// 1. 全局标志位比较（cookie 版本）
// ===========================================================================
//
// 原版字节：
//   8b0d 4c8a4c00    mov  ecx, [0x4C8A4C]      ; __security_cookie
//   33c0             xor  eax, eax
//   83c9 01          or   ecx, 1
//   390d 20754d00    cmp  [0x4D7520], ecx
//   0f94c0           sete al
//   c3               ret
//
// ★ `sete al` 前有 `xor eax,eax` —— 返回类型是 `int` 而非 `bool`。
//   写成 `bool` 会少 2 字节（没有 xor）。
//
//@witness func=0x004A09F7 kind=thiscall
//@witness disp 0x4c8a4c
//@witness disp 0x4d7520
int __fastcall is_global_cookie_flag(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return g_flag_4D7520 == (g_security_cookie | 1);
}

// ===========================================================================
// 2. 参数写入两处（全局 + 对象字段）
// ===========================================================================
//
// 原版字节：
//   558bec           push ebp / mov ebp,esp
//   a1 0c7b4d00      mov  eax, [0x4D7B0C]
//   8b4d 08          mov  ecx, [ebp+8]
//   890d 0c104d00    mov  [0x4D100C], ecx
//   8948 24          mov  [eax+0x24], ecx
//   5d               pop  ebp
//   c20400           ret  4
//
// ★ 顺序：先写全局、后写对象字段。
//
//@witness func=0x004411F0 kind=thiscall
//@witness disp 0x4d7b0c
//@witness disp 0x4d100c
void __fastcall set_param_to_two_places(void* self, void* unused_edx, int v) {
    (void)self;
    (void)unused_edx;
    g_last_4D100C = v;
    *reinterpret_cast<int*>(static_cast<char*>(g_ptr_4D7B0C) + 0x24) = v;
}