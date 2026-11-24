#pragma once

/**
 * 第 5 批匹配：通知桩 / 四参调用 / 全局指针写入 / 构造函数。
 *
 * ## 本批全部来自「已有形状族的新成员」
 *
 * 判据与第 4 批一致：先用 tools/check_bytes.py 的 compile_cpp 单点快跑，
 * 通过后才写入 src/。本批 21 个候选逐个探测，21 个全部一次通过。
 *
 * ## 一个必须记住的负面结果
 *
 * 另有 4 个同族成员（0x0049D72A / 0x004A7C50 / 0x0049DEFD / 0x0049DF39）
 * 形状与本批几个函数**完全相同，只多一个前缀 8bff（mov edi,edi）**。
 *
 * 实测 24 种 flags 组合（/O1 /O2 /Od × /Oy- /Oy × /hotpatch × /Gy）：
 *   - 以 8bff 开头的：**0 个**
 *   - 以 6690 开头的：8 个（即所有 /hotpatch 组合）
 *
 * 结论与第 4 批登记的 E 类一致：v142 的 /hotpatch 选 6690 而非 8bff。
 * **这 4 个不写入 src/**（宁可 unmatched，不可 garbage match）。
 */

#include <xmmintrin.h>

// ---------------------------------------------------------------------------
// 外部数据对象与外部函数（地址 / 名字都来自二进制见证）
//
// ★ 全部放在 **th185 命名空间之外**。原因（实测踩过）：
//   tools/check_bytes.py 会把 hpp 里形如 `extern ...;` 的行**原样提到翻译单元
//   最前面**，提到的是全局作用域。若这些声明在 namespace 内，被提出来后就无法
//   解析（C2065 未声明的标识符）—— 本批 5 个函数因此编译失败过一次。
//   同时要求：行必须以 `;` 结尾、注释另起一行（提取条件是 startswith/endswith）。
// ---------------------------------------------------------------------------
/// 0x004B2124，见证 0x004A0C5F（`6824214b00`）。
extern const char g_data_4B2124;
/// 0x004B2120，见证 0x004A0C5F（`6820214b00`）。
extern const char g_data_4B2120;
/// 0x004B213C，见证 0x004A0C79。
extern const char g_data_4B213C;
/// 0x004B2134，见证 0x004A0C79。
extern const char g_data_4B2134;
/// 0x004B217C，见证 0x004A0C93。
extern const char g_data_4B217C;
/// 0x004B2174，见证 0x004A0C93。
extern const char g_data_4B2174;
/// 0x004B2194，见证 0x004A0CAD。
extern const char g_data_4B2194;
/// 0x004B218C，见证 0x004A0CAD。
extern const char g_data_4B218C;

/// 0x004D0FE0，见证 0x00430710（`f30f110de00f4d00`）。
extern float g_4D0FE0;
/// 0x004D7C14，见证 0x00433830（`a1147c4d00`）。
extern void* g_p_4D7C14;
/// 0x004D7AF4，见证 0x00434060（`a1f47a4d00`）。
extern void* g_p_4D7AF4;

/// 0x004A2533：通知分发。见证 0x004A23A7 / 0x004A6B75 的 `call`。
extern "C" void notify_4A2533(int code);
/// 0x004A402B：通知分发（另一条）。见证 0x004A417D。
extern "C" void notify_4A402B(int code);
/// 0x004A0D78：四参注册调用。见证 0x004A0C5F 一族的 `call 0x4A0D78`。
///
/// ★ 形参顺序由 push 顺序反推（cdecl 从右往左压栈）：
///   0x004A0C5F 的机器码是 `push 0x4B2124 / push 0x4B2020 / push 0x4B2124 / push 0`，
///   最先压栈的是**最后一个**形参。所以第 1 个形参是那个 int，后面才是三个指针。
///   我第一次写成 (ptr, ptr, ptr, int) 时字节完全不符（实测 26 vs 26 但全错位）。
extern "C" void register_4A0D78(int slot, const void* a, const void* b, const void* c);

/// 0x0049DD94：三参转发。见证 0x0049DEDF / 0x0049DEEE 的 `call 0x49DD94`。
extern "C" void forward3_49DD94(int a, int b, int c);
/// 0x004A0FC0：取指针。见证 0x0049F0F1（`e83a0b0000`）。
extern "C" void* get_4A0FC0();
/// 0x004A3B54：构造/初始化。见证 0x00429BF0 一族（`e860770600` 等）。
extern "C" void init_ctor(void* self, int flag, int size);

namespace th185 {

// ---------------------------------------------------------------------------
// 常量（G4 / AGENTS.md §7：魔法数必须具名）
// ---------------------------------------------------------------------------

/// 0x004A0C5F 一族传的第 4 个实参（int）。见证 `6a00` / `6a01` / `6a14` / `6a16`。
constexpr int kSlot0 = 0;
constexpr int kSlot1 = 1;
constexpr int kSlot20 = 0x14;
constexpr int kSlot22 = 0x16;

/// 0x004A23A7 一族传给 0x004A2533 的通知码。见证 `6a06` / `6a01` / `6a08`。
constexpr int kNotifySix = 6;
constexpr int kNotifyOne = 1;
constexpr int kNotifyEight = 8;

/// 0x00429BF0 一族传给构造函数的尺寸。见证 `6860040000`(0x460) / `685c040000`(0x45C)。
constexpr int kCtorSize460 = 0x460;
constexpr int kCtorSize45C = 0x45C;

/// 0x0049F0F1 的兜底地址与偏移。见证 `b8988a4c00` / `83c014`。
constexpr unsigned kFallback4C8A98 = 0x4C8A98u;
constexpr int kPick14Offset = 0x14;

// (外部声明见文件顶部)

// ---------------------------------------------------------------------------
// 通知桩：push imm / call / pop ecx / ret
//
// ★ 用 `pop ecx` 而不是 `add esp,4` —— 这是 /O2 对单参 cdecl 调用的选择。
//   写成显式的 `push` `call`（不带参数直接传常量）才会得到这个形状。
// ---------------------------------------------------------------------------
/// 0x004A23A7（`6a06 e885010000 59 c3`）。
void notify_six();
/// 0x004A417D（`6a01 e8a7feffff 59 c3`）。
void notify_one();
/// 0x004A6B75（`6a08 e8b7b9ffff 59 c3`）。
void notify_eight();

// ---------------------------------------------------------------------------
// 四参注册调用：push a / push b / push c / push slot / call / add esp,0x10 / ret
//
// ★ 形参顺序由 push 顺序反推：最后一个 push 的是第一个实参。
//   0x004A0C5F 的 push 序是 4B2124, 4B2020, 4B2124, 0
//   -> 实参顺序 (0, 4B2124, 4B2020, 4B2124)
// ---------------------------------------------------------------------------
/// 0x004A0C5F。
void register_slot0();
/// 0x004A0C79。
void register_slot1();
/// 0x004A0C93。
void register_slot20();
/// 0x004A0CAD。
void register_slot22();

// ---------------------------------------------------------------------------
// 三参调用：push c / push b / push a / call / add esp,0xC / ret
// ---------------------------------------------------------------------------
/// 0x0049DEDF（`6a01 6a02 6a00 e8aafeffff 83c40c c3`）。
///
/// ★ push 序 1,2,0 反推 → 实参 (0, 2, 1)。
void forward_pair2();
/// 0x0049DEEE（`6a01 6a00 6a00 e89bfeffff 83c40c c3`）。
///
/// ★ push 序 1,0,0 → 实参 (0, 0, 1)。
void forward_pair0();

// ---------------------------------------------------------------------------
// 构造函数：push size / mov esi,ecx / push 0 / push this / call / add esp,0xC /
//            mov eax,esi / pop esi / ret
// ---------------------------------------------------------------------------
/// 0x00429BF0（`56 6860040000 8bf1 6a00 56 e860770600 83c40c 8bc6 5e c3`）。
struct ObjCtor460 {
    /// 构造并返回自身。
    ObjCtor460* construct();
};

/// 0x00434200（同形，尺寸同为 0x460）。
struct ObjCtor460B {
    ObjCtor460B* construct();
};

/// 0x00434280（尺寸 0x45C）。
struct ObjCtor45C {
    ObjCtor45C* construct();
};

// ---------------------------------------------------------------------------
// 全局指针写入：mov eax,[全局指针] / mov ecx,[ebp+8] / mov [eax+DISP],ecx / ret 4
//
// ★ 必须是 **thiscall**（ret 4）—— 写成自由函数会变成 ret，字节不符。
// ---------------------------------------------------------------------------
/// 0x00433830（DISP = 0xE6BB2C）。
struct ObjStoreVia4D7C14 {
    void store(void* value);
};

/// 0x00434060（DISP = 0x8C）。
struct ObjStoreVia4D7AF4 {
    void store(void* value);
};

// ---------------------------------------------------------------------------
// 三参写入：mov eax,[ebp+8] / mov ecx,[ebp+0x10] / mov [eax+8],ecx / ret
//
// ★ cdecl（末尾 c3，调用方清栈），且第 2 个形参完全不使用 —— 必须保留它，
//   否则第 3 个实参落到 [ebp+0xC]，字节不符（实测差 1 字节）。
// ---------------------------------------------------------------------------
/// 0x00492832。
void store_third(int* base, int unused, int value);

// ---------------------------------------------------------------------------
// 全局 float 写入：movss [0x4D0FE0], xmm1; ret
//
// ★ 必须两个 __m128 形参：第 1 个占 xmm0，值才落 xmm1。
// ---------------------------------------------------------------------------
/// 0x00430710（`f30f110de00f4d00 c3`）。
void store_global_float(__m128, __m128 value);

// ---------------------------------------------------------------------------
// 字段 float 写入：movss [ecx+0x7B4], xmm1; ret
// ---------------------------------------------------------------------------
/// +0x7B4。0x00434270（`f30f1189b4070000 c3`）。
struct ObjSse7B4 {
    char gap_00_7B4[0x7B4];  ///< +0x00..+0x7B4 未解析（无证据的区间）
    float f_7B4;             ///< +0x7B4，见证 0x00434270

    void store(__m128, __m128 value);
};

// ---------------------------------------------------------------------------
// 字段读取 / 清零（补）：形状已在 H 类证明过
// ---------------------------------------------------------------------------
/// +0x04。0x00433B90（`8b4104 c3`）。
struct ObjGet04 {
    int f_00;  ///< +0x00 未解析（无证据的区间）
    int f_04;  ///< +0x04，见证 0x00433B90

    int get_04();
};

/// +0xA4。0x004410E0（`c781a400000000000000 c20400`）。
struct ObjClearA4 {
    char gap_00_A4[0xA4];  ///< +0x00..+0xA4 未解析（无证据的区间）
    int f_A4;              ///< +0xA4，见证 0x004410E0

    /// ★ 形参必须保留（ret 4 = thiscall 清 4 字节栈），虽然函数体不用它。
    void clear(int unused);
};

// ---------------------------------------------------------------------------
// 数组元素写入（补）：mov [ecx+N],edx / ret 8
// ---------------------------------------------------------------------------
/// 0x00433BD0（`558bec 8b4508 8b550c 891481 5d c20800`）。
///
/// ★ 首字段是数组而不是单个 int —— `f_00[index] = value` 才生成 `mov [ecx+eax*4],edx`。
struct ObjArr00 {
    int f_00[1];  ///< +0x00 起的 int 数组，见证 0x00433BD0

    void set_at(int index, int value);
};

// ---------------------------------------------------------------------------
// 指针挑选：call / test / jne / mov eax,兜底 / ret / add eax,N / ret
// ---------------------------------------------------------------------------
/// 0x0049F0F1（`e83a0b0000 85c0 7506 b8988a4c00 c3 83c014 c3`）。
void* pick_or_fallback();

}  // namespace th185