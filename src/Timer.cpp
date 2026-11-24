#include "Timer.hpp"

namespace th185 {

namespace {

// 取 +offset 处的 32 位有符号字段。
//
// 用函数而不是就地强转，是为了让「按偏移访问」这件事只出现在一个地方：
// 将来 struct 布局确定后，只需要改这一个函数，不必逐处改调用点。
// ★ 用宏而非 inline 函数：实测只有文本替换能让偏移折叠进 disp8。
//   见 evidence/P3-macro-required-for-disp.json
#define FIELD_AT(self, offset) \
    reinterpret_cast<int*>(static_cast<char*>(self) + (offset))

}  // namespace

//@witness func=0x00402A50 kind=thiscall
//@witness disp 0x8C
//@witness disp 0xD4
//@witness disp 0x0C
//@witness disp 0x4C
void __fastcall tick_timer(void* self, void* unused_edx) {
    (void)unused_edx;
    const int countdown = *FIELD_AT(self, kTimerCountdownOffset) - 1;
    *FIELD_AT(self, kTimerCountdownOffset) = countdown;
    if (countdown < 0) {
        *FIELD_AT(self, kTimerCountdownOffset) = 0;
        *FIELD_AT(self, kTimerFieldAOffset) = 0;
        *FIELD_AT(self, kTimerFieldBOffset) = 0;
        return;
    }
    // 非负分支：用递减后的 countdown 作下标，读两个数组元素写进两个字段。
    //
    // 0x00402A7C  mov eax, [ecx + edx*4 + 0x0C]
    // 0x00402A80  mov [ecx], eax
    // 0x00402A82  mov eax, [ecx + edx*4 + 0x4C]
    // 0x00402A86  mov [ecx+8], eax
    // 0x00402A89  mov [ecx+0xD4], 0
    //
    // 两个数组基址是 +0x0C 和 +0x4C，间距 0x40 = 16 * 4 —— 各自 16 项 int32。
    const int index = countdown;
    *FIELD_AT(self, kTimerFieldAOffset) =
        *FIELD_AT(self, kTimerArrayAOffset + index * 4);
    *FIELD_AT(self, kTimerFieldCOffset) =
        *FIELD_AT(self, kTimerArrayBOffset + index * 4);
    *FIELD_AT(self, kTimerFieldBOffset) = 0;
}

}  // namespace th185