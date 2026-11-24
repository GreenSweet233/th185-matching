#include "Batch8.hpp"

namespace th185 {
namespace b8 {

// ===========================================================================
// 1. 常量返回 —— 优化级决定形态
// ===========================================================================
//
// ★ 重要发现：`return 1` 的机器码取决于优化级
//
//     /O2:  b8 01000000 c3        mov eax,1 ; ret
//     /O1:  33 c0 40 c3           xor eax,eax ; inc eax ; ret
//
// 我此前在 docs/STATUS.md 里记过「return 1 被优化成 mov eax,1，物理不可得」——
// **那条记录是错的**。原版这些函数用的是 /O1，可以精确复现。
// 已按 FLAG_OVERRIDES 机制为这两个函数单独指定 /O1。

//@witness func=0x00490258 kind=thiscall
int __fastcall return_one(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return 1;
}

// `return 0` 在 /O2 下就是 33c0c3，与 /O1 相同，无需 override。
//@witness func=0x004903B9 kind=thiscall
int __fastcall return_zero(void* self, void* unused_edx) {
    (void)self;
    (void)unused_edx;
    return 0;
}

}  // namespace b8
}  // namespace th185