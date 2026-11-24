#pragma once

/**
 * SSE 标量字段访问器（`movss` 读写）。
 *
 * ## 为什么返回 / 接受 __m128 而不是 float
 *
 * MSVC 在 x86 上：
 *   float 返回值  -> fld  dword [ecx]      （x87 栈，ST0）
 *   __m128 返回值 -> movss xmm0,[ecx]      （XMM 寄存器）
 *
 * 原版用后者，所以返回类型必须是 __m128。
 *
 * ## 为什么写入器要两个 __m128 参数
 *
 * 原版写入器是 `movss [ecx+0x18], xmm1` —— 用的是 **xmm1**，不是 xmm0。
 * x86 上第 1 个 __m128 参数占 xmm0、第 2 个占 xmm1，
 * 所以源码必须是「两个 __m128 参数」的形状，值落在第 2 个上。
 * 第 1 个参数原版是否使用**没有证据**，故保持匿名（不写 `(void)` 消警语句，
 * 匿名参数本身就不产生警告）。
 *
 * ## 为什么用 struct + 成员函数
 *
 * 原版是 C++ 类，`ecx` 就是 `this`，写入器天然是 thiscall：
 *
 *   成员函数   void S::store(__m128, __m128 v)  -> ecx=this, xmm0/xmm1  ✓ 无需哑参数
 *   自由函数   void __fastcall f(void*, __m128, __m128 v)  -> 多一个哑参数
 *
 * 读取器不能写成成员函数 —— 实测成员函数返回 __m128 会走**隐藏返回指针**
 * （sret：`mov eax,[ebp+8]; movss [eax+0x18],xmm0`），与原版的
 * `movss xmm0,[ecx+0x18]` 不符。所以读取器写成 **静态成员 + __fastcall**，
 * 只取对象指针一个参数。
 *
 * ## 字段偏移的见证
 *
 * 每个 `f_NN` 的偏移都直接来自该函数自己的机器码（disp8/disp32），
 * 见每个字段旁的见证地址。`gap_00_NN` 是**无证据的未解析区间**，
 * 只为把有证据的字段放到正确偏移上，不含任何猜测的字段。
 */

#include <xmmintrin.h>

/// +0x00 处是 float 的对象。见证 0x004415F0 / 0x00441610。
struct ObjSse00 {
    float f_00;   ///< +0x00，见证 0x004415F0（`movss [ecx],xmm1`）、0x00441610（`movss xmm0,[ecx]`）

    static __m128 __fastcall load_00(ObjSse00* self);
    void store_00(__m128, __m128 value);
};

/// +0x04 处是 float 的对象。见证 0x004415E0 / 0x00441600。
struct ObjSse04 {
    float f_00;   ///< +0x00 未解析（占位，无证据）
    float f_04;   ///< +0x04，见证 0x004415E0（`movss [ecx+4],xmm1`）、0x00441600（`movss xmm0,[ecx+4]`）

    static __m128 __fastcall load_04(ObjSse04* self);
    void store_04(__m128, __m128 value);
};

/// +0x18 处是 float 的对象。见证 0x00417780 / 0x00429A40。
struct ObjSse18 {
    char gap_00_18[0x18];  ///< +0x00..+0x18 未解析（无证据的区间）
    float f_18;            ///< +0x18，见证 0x00417780（`movss [ecx+0x18],xmm1`）、0x00429A40（`movss xmm0,[ecx+0x18]`）

    static __m128 __fastcall load_18(ObjSse18* self);
    void store_18(__m128, __m128 value);
};

/// +0x20 处是 float 的对象。见证 0x00429A00 / 0x00441330。
struct ObjSse20 {
    char gap_00_20[0x20];  ///< +0x00..+0x20 未解析（无证据的区间）
    float f_20;            ///< +0x20，见证 0x00429A00（`movss [ecx+0x20],xmm1`）、0x00441330（`movss xmm0,[ecx+0x20]`）

    static __m128 __fastcall load_20(ObjSse20* self);
    void store_20(__m128, __m128 value);
};

/// +0x24 处是 float 的对象。见证 0x004299F0 / 0x00441340。
struct ObjSse24 {
    char gap_00_24[0x24];  ///< +0x00..+0x24 未解析（无证据的区间）
    float f_24;            ///< +0x24，见证 0x004299F0（`movss [ecx+0x24],xmm1`）、0x00441340（`movss xmm0,[ecx+0x24]`）

    static __m128 __fastcall load_24(ObjSse24* self);
    void store_24(__m128, __m128 value);
};

/// +0x28 处是 float 的对象。见证 0x00441240。
struct ObjSse28 {
    char gap_00_28[0x28];  ///< +0x00..+0x28 未解析（无证据的区间）
    float f_28;            ///< +0x28，见证 0x00441240（`movss xmm0,[ecx+0x28]`）

    static __m128 __fastcall load_28(ObjSse28* self);
};

/// +0x2C 处是 float 的对象。见证 0x00441250 / 0x00441270。
struct ObjSse2C {
    char gap_00_2C[0x2C];  ///< +0x00..+0x2C 未解析（无证据的区间）
    float f_2C;            ///< +0x2C，见证 0x00441250（`movss xmm0,[ecx+0x2c]`）、0x00441270（`movss [ecx+0x2c],xmm1`）

    static __m128 __fastcall load_2C(ObjSse2C* self);
    void store_2C(__m128, __m128 value);
};

/// +0x34 处是 float 的对象。见证 0x00441320。
struct ObjSse34 {
    char gap_00_34[0x34];  ///< +0x00..+0x34 未解析（无证据的区间）
    float f_34;            ///< +0x34，见证 0x00441320（`movss [ecx+0x34],xmm1`）

    void store_34(__m128, __m128 value);
};

/// +0x38 处是 float 的对象。见证 0x00441310。
struct ObjSse38 {
    char gap_00_38[0x38];  ///< +0x00..+0x38 未解析（无证据的区间）
    float f_38;            ///< +0x38，见证 0x00441310（`movss [ecx+0x38],xmm1`）

    void store_38(__m128, __m128 value);
};

/// +0x6C 处是 float 的对象。见证 0x004341D0。
struct ObjSse6C {
    char gap_00_6C[0x6C];  ///< +0x00..+0x6C 未解析（无证据的区间）
    float f_6C;            ///< +0x6C，见证 0x004341D0（`movss [ecx+0x6c],xmm1`）

    void store_6C(__m128, __m128 value);
};

/// +0x74 处是 float 的对象。见证 0x004341E0。
struct ObjSse74 {
    char gap_00_74[0x74];  ///< +0x00..+0x74 未解析（无证据的区间）
    float f_74;            ///< +0x74，见证 0x004341E0（`movss [ecx+0x74],xmm1`）

    void store_74(__m128, __m128 value);
};

/// +0x78 处是 float 的对象。见证 0x004341F0。
struct ObjSse78 {
    char gap_00_78[0x78];  ///< +0x00..+0x78 未解析（无证据的区间）
    float f_78;            ///< +0x78，见证 0x004341F0（`movss [ecx+0x78],xmm1`）

    void store_78(__m128, __m128 value);
};
