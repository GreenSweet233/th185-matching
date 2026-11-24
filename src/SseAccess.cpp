#include "SseAccess.hpp"

// ===========================================================================
// 读取器：静态成员 + __fastcall（只取对象指针），返回值走 xmm0
//
// 为什么不是普通成员函数：实测成员函数返回 __m128 走 sret（隐藏返回指针），
// 生成 `mov eax,[ebp+8]; movss [eax+0x18],xmm0; mov [ebp+8],eax; ret 4`，
// 与原版 `movss xmm0,[ecx+0x18]; ret` 不一致。
// ===========================================================================

//@witness func=0x004415F0 kind=thiscall
void ObjSse00::store_00(__m128, __m128 value) { _mm_store_ss(&f_00, value); }

//@witness func=0x004415E0 kind=thiscall
void ObjSse04::store_04(__m128, __m128 value) { _mm_store_ss(&f_04, value); }

//@witness func=0x00417780 kind=thiscall
void ObjSse18::store_18(__m128, __m128 value) { _mm_store_ss(&f_18, value); }

//@witness func=0x004299F0 kind=thiscall
void ObjSse24::store_24(__m128, __m128 value) { _mm_store_ss(&f_24, value); }

//@witness func=0x00429A00 kind=thiscall
void ObjSse20::store_20(__m128, __m128 value) { _mm_store_ss(&f_20, value); }

//@witness func=0x00441270 kind=thiscall
void ObjSse2C::store_2C(__m128, __m128 value) { _mm_store_ss(&f_2C, value); }

//@witness func=0x00441320 kind=thiscall
void ObjSse34::store_34(__m128, __m128 value) { _mm_store_ss(&f_34, value); }

//@witness func=0x00441310 kind=thiscall
void ObjSse38::store_38(__m128, __m128 value) { _mm_store_ss(&f_38, value); }

//@witness func=0x004341D0 kind=thiscall
void ObjSse6C::store_6C(__m128, __m128 value) { _mm_store_ss(&f_6C, value); }

//@witness func=0x004341E0 kind=thiscall
void ObjSse74::store_74(__m128, __m128 value) { _mm_store_ss(&f_74, value); }

//@witness func=0x004341F0 kind=thiscall
void ObjSse78::store_78(__m128, __m128 value) { _mm_store_ss(&f_78, value); }

// ===========================================================================
// 读取器
// ===========================================================================

//@witness func=0x00429A40 kind=thiscall
__m128 __fastcall ObjSse18::load_18(ObjSse18* self) { return _mm_load_ss(&self->f_18); }

//@witness func=0x00441240 kind=thiscall
__m128 __fastcall ObjSse28::load_28(ObjSse28* self) { return _mm_load_ss(&self->f_28); }

//@witness func=0x00441250 kind=thiscall
__m128 __fastcall ObjSse2C::load_2C(ObjSse2C* self) { return _mm_load_ss(&self->f_2C); }

//@witness func=0x00441330 kind=thiscall
__m128 __fastcall ObjSse20::load_20(ObjSse20* self) { return _mm_load_ss(&self->f_20); }

//@witness func=0x00441340 kind=thiscall
__m128 __fastcall ObjSse24::load_24(ObjSse24* self) { return _mm_load_ss(&self->f_24); }

//@witness func=0x00441600 kind=thiscall
__m128 __fastcall ObjSse04::load_04(ObjSse04* self) { return _mm_load_ss(&self->f_04); }
