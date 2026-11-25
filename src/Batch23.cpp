#include "Batch23.hpp"

// ===========================================================================
// 1. 角度归一化：两参数相加后归一到 (-PI, PI]
// ===========================================================================
//
// 原版字节（82 字节，叶子函数）：
//   f30f10151cc84b00   movss  xmm2, [0x4BC81C]   ; +PI
//   f30f58c1           addss  xmm0, xmm1         ; ★ xmm0 = x + y
//   f30f100d50c84b00   movss  xmm1, [0x4BC850]   ; 2PI
//   33c9               xor    ecx, ecx
//   0f2fc2             comiss xmm0, xmm2
//   7616               jbe    done1
//   0f1f440000         nop    dword [eax+eax]
// loop1:
//   8bc1               mov    eax, ecx
//   f30f5cc1           subss  xmm0, xmm1
//   41                 inc    ecx
//   83f820             cmp    eax, 0x20
//   7f05               jg     done1
//   0f2fc2             comiss xmm0, xmm2
//   77ef               ja     loop1
// done1:
//   f30f101534ca4b00   movss  xmm2, [0x4BCA34]   ; -PI
//   0f2fd0             comiss xmm2, xmm0
//   7613               jbe    done2
//   6690               nop
// loop2:
//   8bc1               mov    eax, ecx
//   f30f58c1           addss  xmm0, xmm1
//   41                 inc    ecx
//   83f820             cmp    eax, 0x20
//   7f05               jg     done2
//   0f2fd0             comiss xmm2, xmm0
//   77ef               ja     loop2
// done2:
//   c3                 ret
//
// ★★ 关键：这是**自由函数 + `__vectorcall`**，不是成员函数
//
//   `__vectorcall` 的参数分配：
//     ecx/edx   = 前两个**整数**参数
//     xmm0..xmm3 = 前四个**向量/浮点**参数
//
//   目标是 `addss xmm0, xmm1` —— 两个 float 参数分别落在 `xmm0` 和 `xmm1`。
//
//   如果写成成员函数 `float f(float x, float y)`，`this` 会占用 `ecx`（整数槽），
//   两个 float 仍是 `xmm0`/`xmm1` —— 但 MSVC 会插入一条多余的 `movss xmm0,[mem]`
//   把参数搬来搬去（85 字节 vs 82）。
//
//   写成自由函数 `float __vectorcall f(void* self, float x, float y)`：
//   `self` 占 `ecx`（不用），两个 float 直接落 `xmm0`/`xmm1`，**82 字节 ✓**
//
//@witness func=0x00402950 kind=free
float __vectorcall angle_add_normalize(void* self, float x, float y) {
    (void)self;
    float a = x + y;
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