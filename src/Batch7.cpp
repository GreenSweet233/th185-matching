#include "Batch7.hpp"

namespace {
/// 分派模式号。见证 0x0041C6B0 的 `ba02000000`（mov edx,2）。
constexpr int kModeTwo = 2;
/// 见证 0x0041C6E0 的 `ba03000000`。
constexpr int kModeThree = 3;
/// 见证 0x0044480A0 的 `ba04000000`。
constexpr int kModeFour = 4;
}  // namespace

namespace th185 {

void* g_singleton_4D7AE0 = nullptr;
void* g_singleton_4D7AF0 = nullptr;
void* g_singleton_4D7AF4 = nullptr;
void* g_singleton_4D7B08 = nullptr;
void* g_singleton_4D7C14 = nullptr;
void* g_singleton_4D7C4C = nullptr;
void* g_dispatch_table_4B5CF8[1] = {nullptr};

// ===========================================================================
// 0. 被析构对象的公共形态
//
// ★ 声明成 struct 且成员函数接收显式 this 指针 —— 这样 MSVC 生成的
//   `push ecx / call` 与原版一致（thiscall 但 this 也进实参）。
// ---------------------------------------------------------------------------

// ===========================================================================
// 1. 全局单例析构守卫
//
// ★ 为什么把全局量强制转成「带虚析构的类指针」再调用：
//   原版是 `mov ecx,[g] / test ecx,ecx / je / push ecx / call 0x434AA0 / ret`。
//   `push ecx` 而不是 `mov ecx,<this>` 说明被调方是 **cdecl/thiscall 且 this 走栈**，
//   也就是普通函数 + 指针实参；同时全局量用 `ecx` 装载说明它被当成 this。
//   两者同时成立 = 「thiscall 成员函数，但实参列表里也显式传了 this」。
//   源码用 `p->destroy(p)` 精确复现这一点。
// ===========================================================================

//@witness func=0x0042A1E0 kind=thiscall
//@witness disp 4D7AE0
void SingletonGuard4D7AE0::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7AE0);
    if (p != nullptr) {
        p->destroy(p);
    }
}

//@witness func=0x00432600 kind=thiscall
//@witness disp 4D7AF0
void SingletonGuard4D7AF0::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7AF0);
    if (p != nullptr) {
        p->destroy(p);
    }
}

//@witness func=0x00434C30 kind=thiscall
//@witness disp 4D7AF4
void SingletonGuard4D7AF4::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7AF4);
    if (p != nullptr) {
        p->destroy(p);
    }
}

//@witness func=0x00442630 kind=thiscall
//@witness disp 4D7B08
void SingletonGuard4D7B08::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7B08);
    if (p != nullptr) {
        p->destroy(p);
    }
}

//@witness func=0x0044BA20 kind=thiscall
//@witness disp 4D7C14
void SingletonGuard4D7C14::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7C14);
    if (p != nullptr) {
        p->destroy(p);
    }
}

//@witness func=0x00469AF0 kind=thiscall
//@witness disp 4D7C4C
void SingletonGuard4D7C4C::release_if_present() {
    Destructible* p = reinterpret_cast<Destructible*>(g_singleton_4D7C4C);
    if (p != nullptr) {
        p->destroy(p);
    }
}

// ===========================================================================
// 2. 虚表槽位分派
//
// ★ 第二实参必须是 __fastcall 的 edx：
//   原版 `ba02000000`（mov edx,2）。若声明成 __stdcall 会生成 `6a02`（push imm8），
//   差 3 字节。这是本批第一版 DIFF 的原因。
//
// ★ 表项类型是 `void*[]`，取出来强转成函数指针再调；
//   原版 `8b0485 f85c4b00` 正是 `mov eax,[eax*4+0x4B5CF8]`（比例变址 + 绝对位移）。
// ===========================================================================

//@witness func=0x0041C6B0 kind=thiscall
//@witness disp 0x494
//@witness disp 0x5E0
void DispatchModeTwo::apply() {
    int index = slot;
    if (index != 0) {
        void (__fastcall* handler)(void*, int) =
            reinterpret_cast<void (__fastcall*)(void*, int)>(
                g_dispatch_table_4B5CF8[index]);
        handler(this, kModeTwo);
    }
    result = kModeTwo;
}

//@witness func=0x0041C6E0 kind=thiscall
//@witness disp 0x494
//@witness disp 0x5E0
void DispatchModeThree::apply() {
    int index = slot;
    if (index != 0) {
        void (__fastcall* handler)(void*, int) =
            reinterpret_cast<void (__fastcall*)(void*, int)>(
                g_dispatch_table_4B5CF8[index]);
        handler(this, kModeThree);
    }
    result = kModeThree;
}

//@witness func=0x004480A0 kind=thiscall
//@witness disp 0x494
//@witness disp 0x5E0
void DispatchModeFour::apply() {
    int index = slot;
    if (index != 0) {
        void (__fastcall* handler)(void*, int) =
            reinterpret_cast<void (__fastcall*)(void*, int)>(
                g_dispatch_table_4B5CF8[index]);
        handler(this, kModeFour);
    }
    result = kModeFour;
}

}  // namespace th185