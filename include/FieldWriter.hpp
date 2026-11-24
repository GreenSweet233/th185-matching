#pragma once

/**
 * 带栈帧的字段写入器（`push ebp / mov eax,[ebp+8] / mov [ecx+N],eax / ret 4`）。
 *
 * ## 关键规则
 *
 * ★ **必须用 /Oy-** —— /O2 默认省略帧指针，但原版这些函数保留栈帧。
 * ★ `ret 4` 说明是 thiscall（MSVC 自动清理 1 个栈参数）。
 *
 * ## 为什么写成成员函数
 *
 * 原版是 thiscall，`ecx` 就是 this。成员函数天然如此：
 *
 *   成员函数   void S::set(int v)                    -> 558bec 8b4508 894134 5d c20400  ✓
 *   自由+哑参  void __fastcall f(void*, void*, int)  -> 同上，但多一个哑参数
 *   自由无哑参 void __fastcall f(void*, int)         -> 895134c3  ✗ 少了栈帧
 *
 * 第三行是关键：**没有哑参数时，第 2 个参数 v 会占 edx 而不是栈**，
 * 于是生成 `mov [ecx+0x34],edx`，与原版不符。
 * 成员函数没有这个问题（this 占 ecx，第一个真参数直接走栈）。
 */

namespace th185 {
namespace fw {

/// +0x04 处是 int 的对象。见证 0x00433BA0。
struct ObjFw04 {
    char gap_00_04[0x04];  ///< +0x00..+0x04 未解析（无证据的区间）
    int f_04;              ///< +0x04，见证 0x00433BA0

    void set(int value);
};

/// +0x08 处是 int 的对象。见证 0x00433BB0。
struct ObjFw08 {
    char gap_00_08[0x08];  ///< +0x00..+0x08 未解析（无证据的区间）
    int f_08;              ///< +0x08，见证 0x00433BB0

    void set(int value);
};

/// +0x28 处是 int 的对象。见证 0x00428090。
struct ObjFw28 {
    char gap_00_28[0x28];  ///< +0x00..+0x28 未解析（无证据的区间）
    int f_28;              ///< +0x28，见证 0x00428090

    void set(int value);
};

/// +0x2C 处是 int 的对象。见证 0x004280C0。
struct ObjFw2C {
    char gap_00_2C[0x2C];  ///< +0x00..+0x2C 未解析（无证据的区间）
    int f_2C;              ///< +0x2C，见证 0x004280C0

    void set(int value);
};

/// +0x34 处是 int 的对象。见证 0x004176E0。
struct ObjFw34 {
    char gap_00_34[0x34];  ///< +0x00..+0x34 未解析（无证据的区间）
    int f_34;              ///< +0x34，见证 0x004176E0

    void set(int value);
};

/// +0x278 处是 int 的对象。见证 0x004340A0。
struct ObjFw278 {
    char gap_00_278[0x278];  ///< +0x00..+0x278 未解析（无证据的区间）
    int f_278;               ///< +0x278，见证 0x004340A0

    void set(int value);
};

/// +0x3C 处是 int 的对象。见证 0x00440840。
struct ObjFw3C {
    char gap_00_3C[0x3C];  ///< +0x00..+0x3C 未解析（无证据的区间）
    int f_3C;              ///< +0x3C，见证 0x00440840

    void set(int value);
};

/// +0x1030 处是 byte 的对象。见证 0x00433B00。
///
/// ★ 参数类型必须是 char：传 int 会生成 `mov [ecx+0x1030],eax`（4 字节写），
///   原版是 `mov [ecx+0x1030],al`（1 字节写）。
struct ObjFw1030 {
    char gap_00_1030[0x1030];  ///< +0x00..+0x1030 未解析（无证据的区间）
    char f_1030;               ///< +0x1030，见证 0x00433B00

    void set(char value);
};

}  // namespace fw
}  // namespace th185
