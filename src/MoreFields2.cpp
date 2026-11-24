#include "MoreFields2.hpp"

namespace th185 {

// ---------------------------------------------------------------------------
// 外部数据对象与外部函数的**定义**（地址由二进制见证给出，链接后由链接器定位）
// ---------------------------------------------------------------------------
extern const char g_data_4B2124 = 0;
extern const char g_data_4B2120 = 0;
extern const char g_data_4B213C = 0;
extern const char g_data_4B2134 = 0;
extern const char g_data_4B217C = 0;
extern const char g_data_4B2174 = 0;
extern const char g_data_4B2194 = 0;
extern const char g_data_4B218C = 0;

float g_4D0FE0 = 0.0f;
void* g_p_4D7C14 = nullptr;
void* g_p_4D7AF4 = nullptr;

// ===========================================================================
// 通知桩：push imm / call / pop ecx / ret
// ===========================================================================

//@witness func=0x004A23A7 kind=cdecl
void notify_six() { notify_4A2533(kNotifySix); }

//@witness func=0x004A417D kind=cdecl
void notify_one() { notify_4A402B(kNotifyOne); }

//@witness func=0x004A6B75 kind=cdecl
void notify_eight() { notify_4A2533(kNotifyEight); }

// ===========================================================================
// 四参注册调用
// ===========================================================================

//@witness func=0x004A0C5F kind=cdecl
void register_slot0() {
    register_4A0D78(kSlot0, &g_data_4B2124, &g_data_4B2120, &g_data_4B2124);
}

//@witness func=0x004A0C79 kind=cdecl
void register_slot1() {
    register_4A0D78(kSlot1, &g_data_4B213C, &g_data_4B2134, &g_data_4B213C);
}

//@witness func=0x004A0C93 kind=cdecl
void register_slot20() {
    register_4A0D78(kSlot20, &g_data_4B217C, &g_data_4B2174, &g_data_4B217C);
}

//@witness func=0x004A0CAD kind=cdecl
void register_slot22() {
    register_4A0D78(kSlot22, &g_data_4B2194, &g_data_4B218C, &g_data_4B2194);
}

// ===========================================================================
// 三参调用
// ===========================================================================

//@witness func=0x0049DEDF kind=cdecl
void forward_pair2() { forward3_49DD94(0, 2, 1); }

//@witness func=0x0049DEEE kind=cdecl
void forward_pair0() { forward3_49DD94(0, 0, 1); }

// ===========================================================================
// 构造函数
// ===========================================================================

//@witness func=0x00429BF0 kind=thiscall
ObjCtor460* ObjCtor460::construct() {
    init_ctor(this, 0, kCtorSize460);
    return this;
}

//@witness func=0x00434200 kind=thiscall
ObjCtor460B* ObjCtor460B::construct() {
    init_ctor(this, 0, kCtorSize460);
    return this;
}

//@witness func=0x00434280 kind=thiscall
ObjCtor45C* ObjCtor45C::construct() {
    init_ctor(this, 0, kCtorSize45C);
    return this;
}

// ===========================================================================
// 全局指针写入
// ===========================================================================

//@witness func=0x00433830 kind=thiscall
void ObjStoreVia4D7C14::store(void* value) {
    *reinterpret_cast<void**>(static_cast<char*>(g_p_4D7C14) + 0xE6BB2C) = value;
}

//@witness func=0x00434060 kind=thiscall
void ObjStoreVia4D7AF4::store(void* value) {
    *reinterpret_cast<void**>(static_cast<char*>(g_p_4D7AF4) + 0x8C) = value;
}

// ===========================================================================
// 三参写入
// ===========================================================================

//@witness func=0x00492832 kind=cdecl
void store_third(int* base, int unused, int value) { base[2] = value; }

// ===========================================================================
// float 写入
// ===========================================================================

//@witness func=0x00430710 kind=cdecl
void store_global_float(__m128, __m128 value) {
    _mm_store_ss(&g_4D0FE0, value);
}

//@witness func=0x00434270 kind=thiscall
void ObjSse7B4::store(__m128, __m128 value) { _mm_store_ss(&f_7B4, value); }

// ===========================================================================
// 字段读取 / 清零
// ===========================================================================

//@witness func=0x00433B90 kind=thiscall
int ObjGet04::get_04() { return f_04; }

//@witness func=0x004410E0 kind=thiscall
void ObjClearA4::clear(int unused) { f_A4 = 0; }

// ===========================================================================
// 数组元素写入
// ===========================================================================

//@witness func=0x00433BD0 kind=thiscall
void ObjArr00::set_at(int index, int value) { f_00[index] = value; }

// ===========================================================================
// 指针挑选
// ===========================================================================

//@witness func=0x0049F0F1 kind=cdecl
void* pick_or_fallback() {
    char* picked = static_cast<char*>(get_4A0FC0());
    if (picked == nullptr) {
        return reinterpret_cast<void*>(kFallback4C8A98);
    }
    return picked + kPick14Offset;
}

}  // namespace th185