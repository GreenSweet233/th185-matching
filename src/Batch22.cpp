#include "Batch22.hpp"

namespace {
constexpr float kPi = 3.1415927410125732f;
constexpr float k2Pi = 6.2831854820251465f;
constexpr float kNegPi = -3.1415927410125732f;
}  // namespace

// ===========================================================================
// 1. 角度归一化到 (-PI, PI]
// ===========================================================================
//
// 共享常量（.rdata 实测）：
//   0x4BC81C =  3.1415927410125732f   (+PI)
//   0x4BC850 =  6.2831854820251465f   (2PI)
//   0x4BCA34 = -3.1415927410125732f   (-PI)
//
// ## 0x004029B0（71 字节）：float 进、float 出
//
// ```asm
// f30f100d1cc84b00   movss xmm1, [0x4BC81C]   ; +PI
// 33c9               xor  ecx, ecx
// 0f2fc1             comiss xmm0, xmm1
// f30f101550c84b00   movss xmm2, [0x4BC850]   ; 2PI
// 7611               jbe  done1
// loop1:
// 8bc1               mov  eax, ecx            ; ★ 保存旧 n
// f30f5cc2           subss xmm0, xmm2
// 41                 inc  ecx
// 83f820             cmp  eax, 0x20           ; ★ 用旧 n 判断
// 7f05               jg   done1
// 0f2fc1             comiss xmm0, xmm1
// 77ef               ja   loop1
// done1:
// f30f100d34ca4b00   movss xmm1, [0x4BCA34]   ; -PI
// 0f2fc8             comiss xmm1, xmm0
// 7611               jbe  done2
// loop2:
// 8bc1               mov  eax, ecx
// f30f58c2           addss xmm0, xmm2
// 41                 inc  ecx
// 83f820             cmp  eax, 0x20
// 7f05               jg   done2
// 0f2fc8             comiss xmm1, xmm0
// 77ef               ja   loop2
// done2:
// c3                 ret
// ```
//
// ★★ 三个关键：
//
//   1. **`__vectorcall`**（`float` 参数在 `xmm0`，返回也在 `xmm0`）
//   2. **`int old = n` 必须在自增之前保存**
//      ```cpp
//      while (a > PI) { int old = n; a -= 2PI; ++n; if (old > 0x20) break; }
//      ```
//      目标用 `mov eax,ecx` 保存旧值、`cmp eax,0x20` 判断旧值。
//      如果写成 `while (a > PI && n <= 0x20)`，MSVC 会在循环**入口**判断，字节不同（67 vs 71）。
//   3. **两个 while 共享同一个 `n`**（不重置）
//
// ## 0x00404B10（77 字节）：float 进（xmm1）、写回 [ecx]、返回 ecx
//
// 同一个核心，区别：
//   - 输入在 `xmm1`（`xmm0` 是第 1 个向量参数，未用）
//   - 末尾 `movss [ecx], xmm1` + `mov eax, ecx` —— **返回 `this` 指针**
//
// 所以签名是 `float* proc(float unused, float in)`，返回传入的 `self`。
//
//@witness func=0x004029B0 kind=thiscall
float __vectorcall AngleBox::normalize(float a) {
    int n = 0;
    while (a > kPi) {
        const int old = n;
        a -= k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    while (a < kNegPi) {
        const int old = n;
        a += k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    return a;
}

//@witness func=0x00404B10 kind=thiscall
float* __vectorcall AngleBox::normalize_store(float unused, float in) {
    (void)unused;
    int n = 0;
    float a = in;
    while (a > kPi) {
        const int old = n;
        a -= k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    while (a < kNegPi) {
        const int old = n;
        a += k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    float* const out = reinterpret_cast<float*>(this);
    *out = a;
    return out;
}

//@witness func=0x00429A60 kind=thiscall
void __vectorcall AngleBox::normalize_to_field(float unused, float in) {
    (void)unused;
    int n = 0;
    float a = in;
    while (a > kPi) {
        const int old = n;
        a -= k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    while (a < kNegPi) {
        const int old = n;
        a += k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    field_1c = a;
}

//@witness func=0x00404AB0 kind=thiscall
float* __vectorcall AngleBox::normalize_add_store(float unused, float in) {
    (void)unused;
    float* const p = &value;
    int n = 0;
    float a = in + *p;
    while (a > kPi) {
        const int old = n;
        a -= k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    while (a < kNegPi) {
        const int old = n;
        a += k2Pi;
        ++n;
        if (old > 0x20) {
            break;
        }
    }
    *p = a;
    return p;
}