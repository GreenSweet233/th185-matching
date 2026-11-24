#include "Misc.hpp"

namespace th185 {
namespace misc {

// 全局变量定义。地址由二进制见证给出，实际布局由链接器决定。
unsigned g_at_4D74F8 = 0;
unsigned g_at_4D72F4 = 0;

namespace {
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

}  // namespace misc
}  // namespace th185