#include "Batch26.hpp"

// ===========================================================================
// 1. 返回 64 位上下界（低 / 高）
// ===========================================================================
//
// 两个实例，只有常量不同：
//
// ## 0x0049B59D（23 字节）
//
// ```asm
// 80790400        cmp  byte [ecx+4], 0    ; flag == 0 ?
// 7409            je   zero
// 83c8ff          or   eax, -1            ; 返回 0x000FFFFF_FFFFFFFF
// baffff0f00      mov  edx, 0xFFFFF
// c3              ret
// zero:
// b8ffff7f00      mov  eax, 0x7FFFFF      ; 返回 0x00000000_007FFFFF
// 33d2            xor  edx, edx
// c3              ret
// ```
//
// ## 0x0049BC66（23 字节）
//
// 同结构，常量不同：`0x1FFFFF` / `0xFFFFFF`。
//
// ★★ 关键：返回 `unsigned long long`（64 位，`eax` + `edx`）
//
//   目标是**两个独立出口**（各自 `c3`），不是 `cmov`。
//
//   | 写法 | 生成 |
//   |---|---|
//   | `if (flag == 0) return 小; return 大;` | `cmov`（16 字节，单出口）|
//   | **`if (flag != 0) return 大; return 小;`** | **两出口（23 字节）★** |
//
//   分支顺序很关键：**先写 != 0 返回大值**，编译器才生成「跳转到小值块」的两出口形态。
//
//@witness func=0x0049B59D kind=thiscall
unsigned long long QRangeA::bounds() {
    if (flag != 0) {
        return 0xFFFFFFFFFFFFFull;
    }
    return 0x7FFFFFull;
}

//@witness func=0x0049BC66 kind=thiscall
unsigned long long QRangeB::bounds() {
    if (flag != 0) {
        return 0x1FFFFFFFFFFFFFull;
    }
    return 0xFFFFFFull;
}