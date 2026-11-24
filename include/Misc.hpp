#pragma once

#include <xmmintrin.h>

/**
 * 杂项：返回常量、清字段、全局读写、位操作。
 *
 * ## 三种形态，对应三种原版形状
 *
 * 1. **不碰 ecx 的** -> 普通无参数函数。
 *    原版 `b8 XXXXXXXX c3`（`mov eax,imm32; ret`）根本不读 ecx，
 *    所以它不是 thiscall，写成普通函数即可，不需要任何占位参数。
 *
 * 2. **碰 ecx 的** -> 成员函数。
 *    原版 `mov eax,[ecx]` 这类，ecx 就是 this。
 *
 * 3. **只碰全局变量 / 全局指针的** -> 普通函数（可能带 __m128 参数）。
 *    原版 `a1 XXXXXXXX c3`（`mov eax,ds:[addr]`）不读 ecx。
 *
 * ## 全局变量
 *
 * 定义在本文件的 .cpp 里，靠 extern 声明产生重定位；
 * 未链接时地址是 0，比对时由 relocation 屏蔽处理。
 */

namespace th185 {
namespace misc {

// ---- 全局变量（地址来自二进制见证）----
extern unsigned g_at_4D74F8;
extern unsigned g_at_4D72F4;
extern void* g_p_kitchen;
extern unsigned g_at_4D7820;
extern unsigned g_at_4D7824;

// ---------------------------------------------------------------------------
// 形态 1：不碰 ecx —— 普通函数
// ---------------------------------------------------------------------------
/// 0x0045C9E0（`b8 709c5700 c3`）。
void* return_579C70();
/// 0x00490556（`b8 00400000 c3`）。
unsigned return_4000();
/// 0x004905C1（`b8 a4a35700 c3`）。
void* return_57A3A4();
/// 0x004905C7（`b8 a0a35700 c3`）。
void* return_57A3A0();
/// 0x0049056E（`b0 01 c3` = `mov al,1`）。返回 bool 才生成 `mov al` 而非 `mov eax`。
bool return_true();

/// 0x0049EBDE（`a1 f8744d00 c3`）。
unsigned read_global_4D74F8();
/// 0x004A1738（`a1 20784d00 c3`）。
unsigned read_global_4D7820();
/// 0x004A173E（`a1 24784d00 c3`）。
unsigned read_global_4D7824();
/// 0x00490463（`83 25 f4724d00 00 c3` = `and dword [addr],0`）。
void zero_global_4D72F4();

/// 0x0049EEBB（`b8 0c754d00 c3`）。
void* return_addr_4D750C();
/// 0x004A1726（`b8 18784d00 c3`）。
void* return_addr_4D7818();
/// 0x004A172C（`b8 1c784d00 c3`）。
void* return_addr_4D781C();
/// 0x004A1732（`b8 14784d00 c3`）。
void* return_addr_4D7814();

/// 0x00433850（`a1 147c4d00 8b80 2cbbe600 c3`）。不碰 ecx。
void* load_via_global();

// ---------------------------------------------------------------------------
// 形态 3：只碰全局指针 + xmm 参数
// ---------------------------------------------------------------------------
/// 0x00433820（`a1 e07a4d00 f30f114844 c3`）。
/// 不碰 ecx；值是第 2 个 __m128 参数（xmm1）。
void __fastcall store_44_via_global(__m128, __m128 value);

// ---------------------------------------------------------------------------
// 形态 2：碰 ecx —— 成员函数
// ---------------------------------------------------------------------------
/// 清 +0x3C。0x004407F0（`c7 41 3C 00000000 c3`）。
struct ObjClear3C {
    char gap_00_3C[0x3C];  ///< +0x00..+0x3C 未解析（无证据的区间）
    int f_3C;              ///< +0x3C，见证 0x004407F0

    void clear();
};

/// 清 +0x50。0x00440B40（`c7 41 50 00000000 c3`）。
struct ObjClear50 {
    char gap_00_50[0x50];  ///< +0x00..+0x50 未解析（无证据的区间）
    int f_50;              ///< +0x50，见证 0x00440B40

    void clear();
};

/// 置位 +0x6368 的 bit 25。0x00433DE0（`81 89 68630000 00000002 c3`）。
struct ObjBit6368 {
    char gap_00_6368[0x6368];  ///< +0x00..+0x6368 未解析（无证据的区间）
    unsigned f_6368;           ///< +0x6368，见证 0x00433DE0

    void set_bit();
};

/// +0x40 处的标志位。0x00429A10 / 0x00441260。
struct ObjFlags40 {
    char gap_00_40[0x40];  ///< +0x00..+0x40 未解析（无证据的区间）
    unsigned f_40;         ///< +0x40，见证 0x00429A10 / 0x00441260

    void set_mask_f2_or_2();
    void set_mask_f3_or_3();
};

/// +0x44 / +0x48 / +0x4C / +0x534 / +0x58 / +0x78 处的对象。
/// 这几个偏移的字段都来自同一个类族（同一批 SSE 访问器），
/// 但它们是否真的属于**同一个**类没有证据，故只按函数各自需要声明。
struct ObjSse44 {
    char gap_00_44[0x44];  ///< +0x00..+0x44 未解析（无证据的区间）
    float f_44;            ///< +0x44，见证 0x0048AC40
    char gap_48_534[0x534 - 0x48];  ///< +0x48..+0x534 未解析（无证据的区间）
    unsigned f_534;                 ///< +0x534，见证 0x0048AC40

    void set_534_and_store_44(__m128, __m128 value);
};

/// +0x48 / +0x4C 两个相邻 float。0x00429B90。
struct ObjSse484C {
    char gap_00_48[0x48];  ///< +0x00..+0x48 未解析（无证据的区间）
    float f_48;            ///< +0x48，见证 0x00429B90
    float f_4C;            ///< +0x4C，见证 0x00429B90

    void store_both(__m128, __m128 v1, __m128 v2);
};

/// +0x58 / +0x78。0x00441620（`f30f104178 f30f594158 c3`）。
struct ObjMul7858 {
    char gap_00_58[0x58];  ///< +0x00..+0x58 未解析（无证据的区间）
    float f_58;            ///< +0x58，见证 0x00441620
    char gap_5C_78[0x78 - 0x5C];  ///< +0x5C..+0x78 未解析（无证据的区间）
    float f_78;                   ///< +0x78，见证 0x00441620

    /// ★ 静态成员 + __fastcall（同 ObjMul7454::mul 的理由：返回值走 xmm0 而非 sret）。
    static __m128 __fastcall mul(ObjMul7858* self);
};

}  // namespace misc
}  // namespace th185
