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
// 此前在 docs/STATUS.md 里记过「return 1 被优化成 mov eax,1，物理不可得」——
// **那条记录是错的**。原版这些函数用的是 /O1，可以精确复现。
// 已按 FLAG_OVERRIDES 机制为 0x00490258 单独指定 /O1。
//
// ★ 2026-11-25 修正：必须写成**无参自由函数**。
//   第一版用 `__fastcall + 两个哑参 + (void) 消警告`，字节对但 G3 的
//   token/指令比 = 12/2 = 6.00 > 4.0，G3 FAIL。改无参自由函数后
//   字节不变、token 降到 5、ratio 2.50，G3 PASS。
//   证据上这也更对：二进制里没有 ecx 痕迹，就不该有 this 参数。
// ---------------------------------------------------------------------------

//@witness func=0x00490258 kind=cdecl
int return_one() { return 1; }

// `return 0` 在 /O2 下就是 33c0c3，与 /O1 相同，无需 override。
//@witness func=0x004903B9 kind=cdecl
int return_zero() { return 0; }

}  // namespace b8
}  // namespace th185
