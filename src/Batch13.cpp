#include "Batch13.hpp"

// ===========================================================================
// 1. 环形缓冲：读一个 word
// ===========================================================================
//
// 原版字节（26 字节，thiscall 成员函数）：
//   8bd1                mov  edx, ecx              ; edx = this
//   8b4a08              mov  ecx, [edx+8]          ; ecx = this->pos
//   3b4a04              cmp  ecx, [edx+4]          ; pos == limit ?
//   7506                jne  read
//   b8 ffff0000         mov  eax, 0xFFFF           ; 返回 0xFFFF（空）
//   c3                  ret
//   read:
//   0fb701              movzx eax, word ptr [ecx]  ; v = *pos
//   83c102              add  ecx, 2                ; ++pos
//   894a08              mov  [edx+8], ecx          ; this->pos = ecx
//   c3                  ret
//
// ★ 一条经验：这个函数**四种写法都能匹配**（/O2 / /O1 × 直接返回 / 中间变量）。
//   说明当语义足够简单时，源码写法自由度大，不必猜原版具体怎么写的。
//   我一开始还以为需要精确还原「中间变量」还是「*pos++」，实测两种都行。
//
//@witness func=0x0049BAE4 kind=thiscall
unsigned short BufReader::read_word() {
    if (pos == limit) {
        return 0xFFFF;
    }
    const unsigned short value = *pos;
    ++pos;
    return value;
}