#include "MoreFields.hpp"

namespace th185 {

// ---------------------------------------------------------------------------
// 全局变量定义。地址由二进制见证给出，最终布局由链接器决定。
// 未链接时地址是占位值，比对时由 relocation 屏蔽。
// ---------------------------------------------------------------------------
extern int g_57A3A8 = 0;
extern int g_4C8A60 = 0;
extern void* g_p_4D7B08 = nullptr;
extern void* g_base_529E88 = nullptr;
extern Pair32 g_pair_573E98 = {0, 0};

// ===========================================================================
// 带栈帧的 thiscall 写入器
// ===========================================================================

//@witness func=0x004408D0 kind=thiscall
void ObjFw40::set(int value) { f_40 = value; }

//@witness func=0x00440B90 kind=thiscall
void ObjFw50::set(int value) { f_50 = value; }

//@witness func=0x004410C0 kind=thiscall
void ObjFw24::set(int value) { f_24 = value; }

// ===========================================================================
// 12 字节聚合拷贝
// ===========================================================================

//@witness func=0x00429A20 kind=thiscall
void ObjVec34::copy_from(const Triple32* source) { f_34 = *source; }

//@witness func=0x00434250 kind=thiscall
void ObjVec7A4::copy_from(const Triple32* source) { f_7A4 = *source; }

//@witness func=0x00440BA0 kind=thiscall
void ObjVec30::copy_from(const Triple32* source) { f_30 = *source; }

//@witness func=0x00440BC0 kind=thiscall
void ObjVec24::copy_from(const Triple32* source) { f_24 = *source; }

//@witness func=0x00440BE0 kind=thiscall
void ObjVec18::copy_from(const Triple32* source) { f_18 = *source; }

//@witness func=0x00440C00 kind=thiscall
void ObjVec0C::copy_from(const Triple32* source) { f_0C = *source; }

// ===========================================================================
// 从指针取值写入字段
// ===========================================================================

//@witness func=0x004280A0 kind=thiscall
void ObjPtr04::set_from(const int* source) { f_04 = *source; }

//@witness func=0x004280B0 kind=thiscall
void ObjPtr00::set_from(const int* source) { f_00 = *source; }

//@witness func=0x00430590 kind=thiscall
void ObjPtr0C::set_from(const int* source) { f_0C = *source; }

//@witness func=0x004305A0 kind=thiscall
void ObjPtr08::set_from(const int* source) { f_08 = *source; }

// ===========================================================================
// 数组元素写入
// ===========================================================================

//@witness func=0x00433BE0 kind=thiscall
void ObjArr54::set_at(int index, int value) { f_54[index] = value; }

// ===========================================================================
// 位掩码判定
// ===========================================================================

//@witness func=0x00431270 kind=thiscall
int ObjBits08::is_mode20() { return (f_08 & kModeMask) == kModeValue; }

//@witness func=0x00441630 kind=thiscall
int ObjBits40::is_two() { return (f_40 & kKindMask) == kKindValue; }

// ===========================================================================
// 带上限的自增
// ===========================================================================

//@witness func=0x004410D0 kind=thiscall
void ObjCounter20::inc() {
    if (f_20 < kCounterMax) {
        ++f_20;
    }
}

// ===========================================================================
// 位翻转
// ===========================================================================

//@witness func=0x00433B70 kind=thiscall
void ObjToggle18::set_low_bit(int flip) {
    f_18 = (f_18 & ~1u) | (static_cast<unsigned>(flip) & 1u);
}

// ===========================================================================
// 字段自增
// ===========================================================================

//@witness func=0x00441220 kind=thiscall
void ObjAddDC::add(int delta) { f_DC += delta; }

//@witness func=0x00441230 kind=thiscall
void ObjAddD8::add(int delta) { f_D8 += delta; }

// ===========================================================================
// SSE 双写
// ===========================================================================

//@witness func=0x00433C60 kind=thiscall
void ObjSseC4C8::store(__m128, __m128 first, __m128 second) {
    _mm_store_ss(&f_C4, first);
    _mm_store_ss(&f_C8, second);
}

// ===========================================================================
// 指针推进 + 置标志
// ===========================================================================

//@witness func=0x0049C51C kind=thiscall
void ObjScan::skip_star() {
    if (*p_08 == kUtf16Asterisk) {
        p_08 += 1;
        f_18 = 1;
    }
}

// ===========================================================================
// 8 字节聚合整块赋值
// ===========================================================================

//@witness func=0x004408B0 kind=thiscall
void ObjPair00::set_pair(const Pair32* source) { f_00 = *source; }

//@witness func=0x00440850 kind=thiscall
void ObjPair18::load(int flags) { f_18 = g_pair_573E98; }

//@witness func=0x00440870 kind=thiscall
void ObjPair10::load(int flags) { f_10 = g_pair_573E98; }

// ===========================================================================
// 全局表按字段索引取值
// ===========================================================================

//@witness func=0x0048AEC0 kind=thiscall
void* ObjIdx1C::at() {
    return reinterpret_cast<Big*>(g_base_529E88)->table[f_1C];
}

}  // namespace th185