#include "Batch11.hpp"

// ===========================================================================
// 1. 拷贝两个 int（Pair 整体赋值）
// ===========================================================================
//
// 原版字节：
//   558bec           push ebp / mov ebp,esp
//   8b4508           mov  eax, [ebp+8]      ; 源指针
//   8b10             mov  edx, [eax]        ; s->a
//   8b4004           mov  eax, [eax+4]      ; s->b
//   895108           mov  [ecx+8], edx
//   89410c           mov  [ecx+0xc], eax
//   5d               pop  ebp
//   c20400           ret  4
//
// ★ 必须用 `Pair 整体赋值`（`f = *s`），不能逐字段写。
//   逐字段 `x = s[0]; y = s[1];` 生成的是：
//     8b5508 8b02 894108 8b4204 89410c
//              ^^^^           ^^^^^^     用 edx 作基址（mov edx,[ebp+8]）
//   目标用的是 `8b4508`（eax 作基址），只有整体赋值才这样。
//
//@witness func=0x00440890 kind=thiscall
struct Pair2 {
    int a;
    int b;
};

struct ObjPair {
    char p[8];
    Pair2 f;

    void copy_from(const Pair2* source);
};

void ObjPair::copy_from(const Pair2* source) {
    f = *source;
}