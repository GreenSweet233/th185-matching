#pragma once
// 倒计时状态机 —— Phase A 语义重建。
//
// ## 目标
//
// th185.exe @ 0x00402A50，函数体 0x00402A50..0x00402A93（共 83 字节）。
// thiscall：ecx = this，无参数，无返回值。6 个调用方。
//
// ## 反汇编（二进制见证）
//
//   0x00402A50  8b 91 8C 00 00 00   mov edx, [ecx+0x8C]
//   0x00402A56  83 EA 01            sub edx, 1
//   0x00402A59  89 91 8C 00 00 00   mov [ecx+0x8C], edx
//   0x00402A5F  79 1B               jns 0x00402A7C          <- 非负就跳到 L2
//   0x00402A61  c7 81 8C..00000000 mov dword [ecx+0x8C], 0
//   0x00402A6B  c7 01 00000000      mov dword [ecx+0x00], 0
//   0x00402A71  c7 81 D4..00000000 mov dword [ecx+0xD4], 0
//   0x00402A7B  c3                  ret                     <- 负分支在此返回
//   0x00402A7C  L2:
//   0x00402A7C  8b 44 91 0C        mov eax, [ecx+edx*4+0x0C]
//   0x00402A80  89 01               mov [ecx], eax
//   0x00402A82  8b 44 91 4C        mov eax, [ecx+edx*4+0x4C]
//   0x00402A86  89 41 08            mov [ecx+8], eax
//   0x00402A89  c7 81 D4..00000000 mov dword [ecx+0xD4], 0
//   0x00402A93  c3                  ret
//
// ## ★ 重要：0x00402A7C 不是独立函数
//
// 我最初把 `function_extent` 的结束点定在 0x00402A7C，以为那是下一个函数。
// 证据推翻了这个判断：
//
//   call 0x00402A50 : 6 个调用方
//   call 0x00402A7C : 0 个
//   指向 0x00402A7C 的跳转 : 只有 0x00402A5F 的 jns
//
// 一个从不被 call、只被本函数内的 jns 引用的地址，是**内部基本块**，不是函数。
//
// ## 语义
//
// 递减倒计时，然后按结果分两支：
//   countdown < 0  : 清三个字段（+0x8C / +0x00 / +0xD4）后返回
//   countdown >= 0 : 以 countdown 为下标，从 +0x0C 和 +0x4C 两个数组取值，
//                    分别写进 +0x00 和 +0x08，再清 +0xD4
//
// 两个数组间距 0x40 = 16*4，各自是 16 项 int32。
//
// ## 为什么这里没有 struct
//
// 本函数碰 +0x00 / +0x08 / +0x0C / +0x4C / +0x8C / +0xD4 六个偏移，
// 但 +0x0C 与 +0x4C 之间、+0x4C 与 +0x8C 之间是什么，**没有证据**。
//
// 按 AGENTS.md §0 第 3 条与 §8 第 1 条，不用 pad 成员凑偏移，
// 把偏移写成具名常量，布局标 unresolved。

namespace th185 {

// 二进制见证的字段偏移（不是推测）。
constexpr unsigned kTimerCountdownOffset = 0x8C;   // 倒计时（有符号 int32）
constexpr unsigned kTimerFieldAOffset = 0x00;      // 被写入
constexpr unsigned kTimerFieldCOffset = 0x08;      // 被写入（第二数组的目标）
constexpr unsigned kTimerArrayAOffset = 0x0C;      // int32[16] 数组基址
constexpr unsigned kTimerArrayBOffset = 0x4C;      // int32[16] 数组基址（间距 0x40）
constexpr unsigned kTimerFieldBOffset = 0xD4;      // 两支都清零

// 递减倒计时并按结果分支。
//
// ★ 调用约定：原版是 thiscall（ecx = this）。
//   MSVC 不允许对自由函数用 __thiscall，所以用 __fastcall + 哑的 edx 参数模拟：
//   x86 上 __fastcall 把第一个参数放 ecx、第二个放 edx，生成的代码与 thiscall 完全一致。
//   实测：用 cdecl 会多出 4 字节 `mov ecx,[esp+4]`。
void __fastcall tick_timer(void* self, void* unused_edx);

}  // namespace th185