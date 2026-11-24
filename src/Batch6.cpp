#include "Batch6.hpp"

// ---------------------------------------------------------------------------
// 具名常量 + 外部数据对象定义
//
// ★ 常量必须放在**匿名 namespace** 里，不能放 hpp。
//   原因见 Batch6.hpp 顶部注释（C2374 重定义）。
//   check_bytes.py 会把 `namespace { ... }` 整块抽进翻译单元的 prefix，只抽一次。
// ---------------------------------------------------------------------------

namespace {
/// 分桶进一的档宽。见证 0x00409C40 的 `6bc064`（imul eax,eax,0x64）。
constexpr int kRolloverDivisor = 100;
/// 档内最后一个值。见证 `83f963`（cmp ecx,0x63）。
constexpr int kRolloverRemainder = 99;
/// 上限 100。见证 `83ff64` / `b864000000`。
constexpr int kCapHundred = 100;
/// 上限 999。见证 `81ffe7030000` / `b8e7030000`。
constexpr int kCapNineHundredNinetyNine = 999;
/// 上限 1。见证 0x00463D90 的 `83ff01` / `b801000000`。
constexpr int kCapOne = 1;
}  // namespace

namespace th185 {

unsigned g_input_mask_4CE400 = 0;
unsigned g_input_mask_4CE3FC = 0;
unsigned g_input_mask_4CE3F8 = 0;
ResourceSystem* g_resource_system = nullptr;

// ===========================================================================
// 1. 全局掩码查询
//
// ★ 返回类型必须是 `int` 而不是 `bool`。
//   实测：写 `bool` 生成 `b001` / `32c0`（mov al,1 / xor al,al），
//   原版是 `b801000000` / `33c0`（mov eax,1 / xor eax,eax）。
//   这是 STATUS.md 3.2 规则 3「返回 int 与 bool 走不同路径」的又一实例。
//
// ★ 两个全局量的先后顺序决定 jne 的跳向，不能对调。
// ===========================================================================

//@witness func=0x0041C7B0 kind=thiscall
//@witness disp 0x4CE400
//@witness disp 0x4CE3FC
int InputMaskQuery::any_of_first_pair(int mask) {
    unsigned m = static_cast<unsigned>(mask);
    if ((g_input_mask_4CE400 & m) != 0u) return 1;
    if ((g_input_mask_4CE3FC & m) != 0u) return 1;
    return 0;
}

//@witness func=0x00431570 kind=thiscall
//@witness disp 0x4CE400
//@witness disp 0x4CE3F8
int InputMaskQueryAlternate::any_of_second_pair(int mask) {
    unsigned m = static_cast<unsigned>(mask);
    if ((g_input_mask_4CE400 & m) != 0u) return 1;
    if ((g_input_mask_4CE3F8 & m) != 0u) return 1;
    return 0;
}

// ===========================================================================
// 2. 资源浮点查询
//
// ★ 必须返回 `__m128` 而不是 `float`（见 STATUS.md 3.3 最大发现）：
//   float 走 x87 `fld dword [ecx]`，只有 __m128 才走 `movss xmm0,[ecx]`。
//
// ★ 必须是「静态成员 + __fastcall 单参」而不是普通成员函数：
//   普通成员函数返回 __m128 会走 sret（隐藏返回指针），
//   生成 `mov eax,[ebp+8] / movss [eax+..],xmm0 / ret 4`，与原版不符。
//   此结论与 SseAccess.cpp 的读取器一致，是同一规则的复用。
// ===========================================================================

//@witness func=0x0048AE00 kind=thiscall
//@witness call 0x48A4A0
//@witness disp 0x44
//@witness disp 0x128
__m128 __fastcall ResourceProbe44::read(ResourceProbe44* self) {
    ResourceSystem* sys = g_resource_system;
    ResourceEntry44* entry =
        static_cast<ResourceEntry44*>(sys->find(self->handle));
    if (entry == nullptr) {
        self->handle = 0;
        return _mm_load_ss(&sys->fallback_128);
    }
    return _mm_load_ss(&entry->value);
}

//@witness func=0x0048AE30 kind=thiscall
//@witness call 0x48A4A0
//@witness disp 0x54
//@witness disp 0x138
__m128 __fastcall ResourceProbe54::read(ResourceProbe54* self) {
    ResourceSystem* sys = g_resource_system;
    ResourceEntry54* entry =
        static_cast<ResourceEntry54*>(sys->find(self->handle));
    if (entry == nullptr) {
        self->handle = 0;
        return _mm_load_ss(&sys->fallback_138);
    }
    return _mm_load_ss(&entry->value);
}

//@witness func=0x0048AE60 kind=thiscall
//@witness call 0x48A4A0
//@witness disp 0x58
//@witness disp 0x13C
__m128 __fastcall ResourceProbe58::read(ResourceProbe58* self) {
    ResourceSystem* sys = g_resource_system;
    ResourceEntry58* entry =
        static_cast<ResourceEntry58*>(sys->find(self->handle));
    if (entry == nullptr) {
        self->handle = 0;
        return _mm_load_ss(&sys->fallback_13C);
    }
    return _mm_load_ss(&entry->value);
}

// ===========================================================================
// 3. 分桶计数推进器
//
// 统一语义（12 个类只有字段偏移与上限不同）：
//   1) cur > cap  -> 钳到 cap，返回 cap
//   2) cur < 0    -> 归零，返回 0
//   3) 否则      -> 若 cur % 100 == 99 则 +1，否则不变；写回并返回
//
// ★ 原版序列是 `lea eax,[edi+1] / cmp ecx,0x63 / cmovne eax,edi /
//   mov [esi+OFF],eax` —— 先算候选值、比较、条件移动，最后存回。
//   实测「赋值表达式作返回值」与「先算 int next 再赋值返回」生成同样字节，
//   说明这是 /O2 对该模式的稳定产物，不依赖具体写法。
// ===========================================================================

//@witness func=0x00409C40 kind=thiscall
//@witness disp 0xB4
//@witness imm 0x64
//@witness imm 0x63
int CounterB4::advance_capped() {
    int current = count;
    if (current > kCapHundred) {
        count = kCapHundred;
        return kCapHundred;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00417940 kind=thiscall
//@witness disp 0x9C
//@witness imm 0x64
//@witness imm 0x63
int Counter9C::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x004306A0 kind=thiscall
//@witness disp 0xAC
//@witness imm 0x64
//@witness imm 0x63
int CounterAC::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x004410F0 kind=thiscall
//@witness disp 0xA4
//@witness imm 0x64
//@witness imm 0x63
int CounterA4::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447D80 kind=thiscall
//@witness disp 0xA8
//@witness imm 0x64
//@witness imm 0x63
int CounterA8::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447DF0 kind=thiscall
//@witness disp 0xA0
//@witness imm 0x64
//@witness imm 0x63
int CounterA0::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447E60 kind=thiscall
//@witness disp 0x90
//@witness imm 0x64
//@witness imm 0x63
int Counter90::advance_capped() {
    int current = count;
    if (current > kCapHundred) {
        count = kCapHundred;
        return kCapHundred;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447EC0 kind=thiscall
//@witness disp 0x98
//@witness imm 0x64
//@witness imm 0x63
int Counter98::advance_capped() {
    int current = count;
    if (current > kCapHundred) {
        count = kCapHundred;
        return kCapHundred;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447F20 kind=thiscall
//@witness disp 0x94
//@witness imm 0x64
//@witness imm 0x63
int Counter94::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00447F90 kind=thiscall
//@witness disp 0x8C
//@witness imm 0x64
//@witness imm 0x63
int Counter8C::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00448000 kind=thiscall
//@witness disp 0x88
//@witness imm 0x64
//@witness imm 0x63
int Counter88::advance_capped() {
    int current = count;
    if (current > kCapNineHundredNinetyNine) {
        count = kCapNineHundredNinetyNine;
        return kCapNineHundredNinetyNine;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

//@witness func=0x00463D90 kind=thiscall
//@witness disp 0xB0
//@witness imm 0x64
//@witness imm 0x63
int CounterB0::advance_capped() {
    int current = count;
    if (current > kCapOne) {
        count = kCapOne;
        return kCapOne;
    }
    if (current < 0) {
        count = 0;
        return 0;
    }
    return count = ((current % kRolloverDivisor) == kRolloverRemainder)
                       ? current + 1
                       : current;
}

}  // namespace th185
