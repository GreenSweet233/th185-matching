#include "SseAccess.hpp"

namespace th185 {
namespace sse {

namespace {
// 取 +offset 处的 float 地址。
//
// ★ 必须是宏 —— inline 函数/template 都会生成 add 而非 disp8。
//   见 evidence/P3-macro-required-for-disp.json
#define FLOAD(self, offset) \
    reinterpret_cast<float*>(static_cast<char*>(self) + (offset))
}  // namespace

// ===========================================================================
// 读取器：__m128 返回，走 xmm0
// ===========================================================================

//@witness func=0x00429A40 kind=thiscall
__m128 __fastcall load_18(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x18));
}

//@witness func=0x00441240 kind=thiscall
__m128 __fastcall load_28(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x28));
}

//@witness func=0x00441250 kind=thiscall
__m128 __fastcall load_2C(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x2C));
}

//@witness func=0x00441330 kind=thiscall
__m128 __fastcall load_20(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x20));
}

//@witness func=0x00441340 kind=thiscall
__m128 __fastcall load_24(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x24));
}

//@witness func=0x00441600 kind=thiscall
__m128 __fastcall load_04(void* self, void* unused_edx) {
    (void)unused_edx;
    return _mm_load_ss(FLOAD(self, 0x4));
}

// ===========================================================================
// 写入器：第 2 个 __m128 参数落在 xmm1
// ===========================================================================

//@witness func=0x004415F0 kind=thiscall
void __fastcall store_00(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x0), value);
}

//@witness func=0x004415E0 kind=thiscall
void __fastcall store_04(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x4), value);
}

//@witness func=0x00417780 kind=thiscall
void __fastcall store_18(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x18), value);
}

//@witness func=0x004299F0 kind=thiscall
void __fastcall store_24(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x24), value);
}

//@witness func=0x00429A00 kind=thiscall
void __fastcall store_20(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x20), value);
}

//@witness func=0x00441270 kind=thiscall
void __fastcall store_2C(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x2C), value);
}

//@witness func=0x00441320 kind=thiscall
void __fastcall store_34(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x34), value);
}

//@witness func=0x00441310 kind=thiscall
void __fastcall store_38(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x38), value);
}

//@witness func=0x004341D0 kind=thiscall
void __fastcall store_6C(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x6C), value);
}

//@witness func=0x004341E0 kind=thiscall
void __fastcall store_74(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x74), value);
}

//@witness func=0x004341F0 kind=thiscall
void __fastcall store_78(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x78), value);
}

}  // namespace sse
}  // namespace th185