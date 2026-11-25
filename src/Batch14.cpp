#include "Batch14.hpp"

// ===========================================================================
// 1. 整除判断（a==b 时返回 0，否则看 b % d 是否为 0）
// ===========================================================================
//
// 原版字节（33 字节）：
//   558bec           push ebp / mov ebp,esp
//   8b4104           mov  eax, [ecx+4]      ; b
//   3b01             cmp  eax, [ecx]        ; b == a ?
//   7411             je   done
//   99               cdq
//   f77d08           idiv [ebp+8]          ; b / d
//   85d2             test edx, edx         ; 余数 == 0 ?
//   7509             jne  done
//   b801000000       mov  eax, 1
//   5dc20400         pop ebp / ret 4
//   done:
//   33c0             xor  eax, eax
//   5dc20400         pop ebp / ret 4
//
// ★ 关键：「先算余数存局部变量」才能让编译器把两个 return 0 合并到末尾：
//
//   ```cpp
//   if (b == a) return 0;
//   int m = b % d;        // ★ 存进局部变量
//   if (m != 0) return 0; // 与上面的 return 0 共用同一个出口
//   return 1;
//   ```
//
//   直接写 `return (b % d) == 0;` 会让编译器选择「就近展开」而不是尾部合并。
//
//@witness func=0x004299A0 kind=thiscall
int DivCheck::divisible(int d) {
    if (b == a) {
        return 0;
    }
    const int m = b % d;
    if (m != 0) {
        return 0;
    }
    return 1;
}