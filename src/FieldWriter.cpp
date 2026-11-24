#include "FieldWriter.hpp"

namespace th185 {
namespace fw {

namespace {
#define INT_AT(self, offset) \
    reinterpret_cast<int*>(static_cast<char*>(self) + (offset))

// 取 +offset 处的 byte 地址。★ 用宏的原因同 INT_AT（disp8 折叠）。
#define BYTE_AT(self, offset) \
    reinterpret_cast<char*>(static_cast<char*>(self) + (offset))
}  // namespace

//@witness func=0x004176E0 kind=thiscall
void __fastcall set_34(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x34) = v;
}

//@witness func=0x00428090 kind=thiscall
void __fastcall set_28(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x28) = v;
}

//@witness func=0x004280C0 kind=thiscall
void __fastcall set_2C(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x2C) = v;
}

//@witness func=0x00433BA0 kind=thiscall
void __fastcall set_04(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x04) = v;
}

//@witness func=0x00433BB0 kind=thiscall
void __fastcall set_08(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x08) = v;
}

//@witness func=0x004340A0 kind=thiscall
void __fastcall set_278(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x278) = v;
}

//@witness func=0x00440840 kind=thiscall
void __fastcall set_3C(void* self, void* unused_edx, int v) {
    (void)unused_edx;
    *INT_AT(self, 0x3C) = v;
}

//@witness func=0x00433B00 kind=thiscall
void __fastcall set_1030_byte(void* self, void* unused_edx, char v) {
    (void)unused_edx;
    *BYTE_AT(self, 0x1030) = v;
}

}  // namespace fw
}  // namespace th185