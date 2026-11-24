#pragma once

/**
 * 第 6 批匹配：分桶计数推进器 / 全局掩码查询 / 资源浮点查询。
 *
 * ## 本批三类形状
 *
 * 1. **分桶计数推进器**（12 个）：`cur > cap` 截断、`cur < 0` 归零、
 *    否则按 100 一档「满档进一」。12 个成员字段偏移不同（0x88..0xB4），
 *    是两个上限值（100 / 999，另有一个上限 1）的幂集。
 * 2. **全局掩码查询**（2 个）：`return (gA & m) || (gB & m);` 写成 int 返回
 *    才是 `b801000000` / `33c0`；写 bool 会变成 `b001` / `32c0`（实测）。
 * 3. **资源浮点查询**（3 个）：先 `find(handle)`，命中读对象 +0x44/+0x54/+0x58，
 *    未命中把 handle 清 0 并读系统对象 +0x128/+0x138/+0x13C 的兜底值。
 *    返回值必须是 `__m128`，普通 `float` 会走 x87（`fld`）而非 `movss`。
 *
 * ## 类型约定
 *
 * 未解析区间一律命名 `gap_起_止`（与 FieldWriter.hpp / Misc.hpp 一致）。
 * G4 黑名单禁止 pad/dummy/unused 命名的数组，gap 不在黑名单里。
 */

#include <xmmintrin.h>

// ---------------------------------------------------------------------------
// 具名常量放在 Batch6.cpp 的匿名 namespace 里，**不放这里**。
//
// ★ 踩过的坑（本批 17 个函数第一次全部 COMPILE_ERROR，错误 C2374 重定义）：
//   tools/check_bytes.py 拼翻译单元时，对**成员函数**会做两件事：
//     (a) 把 hpp 里所有 `constexpr` / `static const` 行抽出来放进 prefix
//     (b) 又把整个 hpp 的声明行（decl_lines）也拼进去
//   于是 hpp 里的 constexpr 出现两次 -> C2374。
//   非成员函数不会触发（只走 (a)），所以历史文件里的 hpp 常量一直没事。
//   修法：常量移到 .cpp 的匿名 namespace（check_bytes 会把 `namespace { ... }`
//   整块抽进 prefix，只抽一次）。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 全局掩码源（地址来自二进制见证）
// ---------------------------------------------------------------------------

/// 0x004CE400。见证 0x0041C7B0 的 `850500e44c00`（test dword [0x4ce400],eax）。
extern unsigned g_input_mask_4CE400;
/// 0x004CE3FC。见证 0x0041C7B0 的 `8505fce34c00`。
extern unsigned g_input_mask_4CE3FC;
/// 0x004CE3F8。见证 0x00431570 的 `8505f8e34c00`。
extern unsigned g_input_mask_4CE3F8;

// ---------------------------------------------------------------------------
// 资源系统（find 返回实体，未命中返回空）
// ---------------------------------------------------------------------------

/// 资源实体：数值位于 +0x44 / +0x54 / +0x58。
struct ResourceEntry44 {
    char gap_00_44[0x44];
    float value;
};
/// 资源实体：数值位于 +0x54。
struct ResourceEntry54 {
    char gap_00_54[0x54];
    float value;
};
/// 资源实体：数值位于 +0x58。
struct ResourceEntry58 {
    char gap_00_58[0x58];
    float value;
};

/// 资源系统（单例，指针在 0x00529E88）。
struct ResourceSystem {
    char gap_00_128[0x128];
    float fallback_128;
    float fallback_12C;
    float fallback_130;
    float fallback_134;
    float fallback_138;
    float fallback_13C;
    void* find(int id);
};

/// 0x00529E88。见证 0x0048AE00 的 `8b3d889e5200`。
extern ResourceSystem* g_resource_system;

// ---------------------------------------------------------------------------
// ★ 以下是类定义，必须放在 \`namespace th185\` 里。
//
//   原因（本批第一次 G4 FAIL 就是它）：
//     check_bytes.py 用 \`hpp_namespace_chain()\` 取 hpp 在文件作用域打开的 namespace，
//     把 .cpp 里的函数定义包回同一串 namespace。.cpp 写的是
//     \`namespace th185 { int InputMaskQuery::f() {...} }\`，
//     如果 hpp 的 struct 在**全局**作用域，clang 看到的是两个不同类型
//     （\`::InputMaskQuery\` vs \`th185::InputMaskQuery\`），报：
//       error: cannot define or redeclare 'any_of_first_pair' here
//              because namespace 'th185'
//     G4 直接 FAIL。MoreFields2.hpp 也是把 struct 放在 namespace th185 里，
//     本文件与之一致。
//
//   而 \`extern\` 全局量**必须留在 namespace 外**（见 MoreFields2.hpp §注释）。
// ---------------------------------------------------------------------------

namespace th185 {

// ---------------------------------------------------------------------------
// 1. 全局掩码查询（thiscall，ret 4）
// ---------------------------------------------------------------------------

/// `(g_input_mask_4CE400 & mask) || (g_input_mask_4CE3FC & mask)`。
struct InputMaskQuery {
    int any_of_first_pair(int mask);
};

/// `(g_input_mask_4CE400 & mask) || (g_input_mask_4CE3F8 & mask)`。
/// 与上一个并存成两种方法（全局量不同，形状相同）。
struct InputMaskQueryAlternate {
    int any_of_second_pair(int mask);
};

// ---------------------------------------------------------------------------
// 2. 资源浮点查询（静态成员 + __fastcall，返回值走 xmm0）
// ---------------------------------------------------------------------------

/// +0x44，兜底 +0x128。
struct ResourceProbe44 {
    int handle;
    static __m128 __fastcall read(ResourceProbe44* self);
};

/// +0x54，兜底 +0x138。
struct ResourceProbe54 {
    int handle;
    static __m128 __fastcall read(ResourceProbe54* self);
};

/// +0x58，兜底 +0x13C。
struct ResourceProbe58 {
    int handle;
    static __m128 __fastcall read(ResourceProbe58* self);
};

// ---------------------------------------------------------------------------
// 3. 分桶计数推进器（12 个类，各自持有计数于不同偏移）
// ---------------------------------------------------------------------------

/// `+0xB4`，上限 `kCapHundred`，见证 `0x00409C40`。
struct CounterB4 {
    char gap_00_B4[0xB4];
    int count;
    int advance_capped();
};

/// `+0x9C`，上限 `kCapNineHundredNinetyNine`，见证 `0x00417940`。
struct Counter9C {
    char gap_00_9C[0x9C];
    int count;
    int advance_capped();
};

/// `+0xAC`，上限 `kCapNineHundredNinetyNine`，见证 `0x004306A0`。
struct CounterAC {
    char gap_00_AC[0xAC];
    int count;
    int advance_capped();
};

/// `+0xA4`，上限 `kCapNineHundredNinetyNine`，见证 `0x004410F0`。
struct CounterA4 {
    char gap_00_A4[0xA4];
    int count;
    int advance_capped();
};

/// `+0xA8`，上限 `kCapNineHundredNinetyNine`，见证 `0x00447D80`。
struct CounterA8 {
    char gap_00_A8[0xA8];
    int count;
    int advance_capped();
};

/// `+0xA0`，上限 `kCapNineHundredNinetyNine`，见证 `0x00447DF0`。
struct CounterA0 {
    char gap_00_A0[0xA0];
    int count;
    int advance_capped();
};

/// `+0x90`，上限 `kCapHundred`，见证 `0x00447E60`。
struct Counter90 {
    char gap_00_90[0x90];
    int count;
    int advance_capped();
};

/// `+0x98`，上限 `kCapHundred`，见证 `0x00447EC0`。
struct Counter98 {
    char gap_00_98[0x98];
    int count;
    int advance_capped();
};

/// `+0x94`，上限 `kCapNineHundredNinetyNine`，见证 `0x00447F20`。
struct Counter94 {
    char gap_00_94[0x94];
    int count;
    int advance_capped();
};

/// `+0x8C`，上限 `kCapNineHundredNinetyNine`，见证 `0x00447F90`。
struct Counter8C {
    char gap_00_8C[0x8C];
    int count;
    int advance_capped();
};

/// `+0x88`，上限 `kCapNineHundredNinetyNine`，见证 `0x00448000`。
struct Counter88 {
    char gap_00_88[0x88];
    int count;
    int advance_capped();
};

/// `+0xB0`，上限 `kCapOne`，见证 `0x00463D90`。
struct CounterB0 {
    char gap_00_B0[0xB0];
    int count;
    int advance_capped();
};
}  // namespace th185
