#pragma once

/**
 * 第 7 批匹配：全局单例析构守卫 / 虚表槽位分派。
 *
 * ## 两类形状
 *
 * 1. **全局单例析构守卫**（6 个，17 字节）：
 *    `mov ecx,[g] / test ecx,ecx / je skip / push ecx / call dtor / skip: ret`
 *    全局指针非空则调用它的析构。返回值必须是 void —— 原版没有 `xor eax,eax`。
 * 2. **虚表槽位分派**（3 个，39 字节）：
 *    `mov eax,[esi+0x5E0] / test / je skip / mov eax,[eax*4+TBL] / mov edx,imm /
 *    call eax / skip: mov [esi+0x494],imm`
 *    ★ 第二实参走 **edx** 而不是栈，所以被调方必须声明成 `__fastcall`。
 *      写成 `__stdcall` 会生成 `6a02`（push imm8）而不是 `ba02000000`（mov edx,imm32），
 *      长度差 3 字节 —— 这是本批第一版 DIFF 的原因。
 */

// ---------------------------------------------------------------------------
// 外部数据对象（必须放 namespace 外，见 MoreFields2.hpp 的同类注释）
// ---------------------------------------------------------------------------

/// 0x004D7AE0，见证 0x0042A1E0（`8b0de07a4d00`）。
extern void* g_singleton_4D7AE0;
/// 0x004D7AF0，见证 0x00432600。
extern void* g_singleton_4D7AF0;
/// 0x004D7AF4，见证 0x00434C30（`8b0df47a4d00`）。
extern void* g_singleton_4D7AF4;
/// 0x004D7B08，见证 0x00442630（`8b0d087b4d00`）。
extern void* g_singleton_4D7B08;
/// 0x004D7C14，见证 0x0044BA20。
extern void* g_singleton_4D7C14;
/// 0x004D7C4C，见证 0x00469AF0。
extern void* g_singleton_4D7C4C;

/// 0x004B5CF8，函数指针表。见证 0x0041C6B0 的 `8b0485f85c4b00`
/// （`mov eax,[eax*4+0x4b5cf8]`）。
extern void* g_dispatch_table_4B5CF8[];

/// 被析构对象的公共形态。
///
/// ★ 必须放在 \`namespace th185\` **之外**。
///   check_bytes.py 对成员函数会把 hpp 的声明行拼进翻译单元，
///   但 \`Destructible\` 在 .cpp 里被引用时是按**全局名**解析的；
///   若它只在 namespace 内，临时翻译单元会报 C2061（语法错误：标识符）。
struct Destructible {
    void destroy(Destructible* self);
};

namespace th185 {

// ---------------------------------------------------------------------------
// 1. 全局单例析构守卫
//
// ★ 被调方是 thiscall（单参，ecx 传 this），源码里写成成员函数即可。
//   写成自由函数 + __stdcall 会得到 `a1`（mov eax,moffs）而不是 `8b0d`（mov ecx,[mem]），
//   因为 MSVC 对「无 this 的指针全局」用 eax 装载，对有 this 的用 ecx。
// ---------------------------------------------------------------------------

/// 全局量 `0x004D7AE0` 非空则析构。
struct SingletonGuard4D7AE0 {
    void release_if_present();
};
/// 全局量 `0x004D7AF0` 非空则析构。
struct SingletonGuard4D7AF0 {
    void release_if_present();
};
/// 全局量 `0x004D7AF4` 非空则析构。
struct SingletonGuard4D7AF4 {
    void release_if_present();
};
/// 全局量 `0x004D7B08` 非空则析构。
struct SingletonGuard4D7B08 {
    void release_if_present();
};
/// 全局量 `0x004D7C14` 非空则析构。
struct SingletonGuard4D7C14 {
    void release_if_present();
};
/// 全局量 `0x004D7C4C` 非空则析构。
struct SingletonGuard4D7C4C {
    void release_if_present();
};

// ---------------------------------------------------------------------------
// 2. 虚表槽位分派
// ---------------------------------------------------------------------------

/// 槽位分派对象：索引在 +0x5E0，结果写 +0x494。
struct DispatchTarget {
    char gap_00_494[0x494];
    int result;   ///< +0x494，写入被分派的模式号
    char gap_498_5E0[0x5E0 - 0x498];
    int slot;     ///< +0x5E0，函数指针表下标
    void apply();
};

/// 模式 2 分派器。
struct DispatchModeTwo {
    char gap_00_494[0x494];
    int result;
    char gap_498_5E0[0x5E0 - 0x498];
    int slot;
    void apply();
};
/// 模式 3 分派器。
struct DispatchModeThree {
    char gap_00_494[0x494];
    int result;
    char gap_498_5E0[0x5E0 - 0x498];
    int slot;
    void apply();
};
/// 模式 4 分派器。
struct DispatchModeFour {
    char gap_00_494[0x494];
    int result;
    char gap_498_5E0[0x5E0 - 0x498];
    int slot;
    void apply();
};

}  // namespace th185