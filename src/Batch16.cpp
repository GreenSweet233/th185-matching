#include "Batch16.hpp"

// ===========================================================================
// 辅助定义（匿名 namespace —— check_bytes.py 会作为 prefix 带进 TU）
// ===========================================================================
//
// ★ 为什么 struct 必须在这里而不是 hpp：
//   这两个函数是**自由函数**（不是成员函数），check_bytes.py 只在函数签名
//   形如 `Cls::name(` 时才拼入 hpp 的 struct 定义。自由函数的话 hpp 不带进来，
//   会 C2065「未声明的标识符」。
//
// ★★ 为什么必须是自由函数而不是成员函数：
//   成员函数生成 `8b411c`（直接 `[ecx+0x1c]`）—— 少了取全局那一步。
//   目标生成 `a1 <addr> / 8b401c`（先 `mov eax,[全局]` 再取 `[eax+0x1c]`）。
//   所以原版这两个函数**不是成员函数**，靠全局单例指针访问链表。
namespace {

struct InnerA {
    int unknown_0;   ///< +0x00：本函数未读
    int key;         ///< +0x04
};
struct NodeA {
    InnerA* inner;
    NodeA* next;
};

struct InnerB {
    char unknown_pre[0x683c];   ///< +0x00..+0x683c：本函数未读
    int key;                    ///< +0x683c
};
struct NodeB {
    InnerB* inner;
    NodeB* next;
};

NodeA* list_a_head() {
    return *reinterpret_cast<NodeA**>(g_p_4D7AB8 + 0x1c);
}
NodeB* list_b_head() {
    return *reinterpret_cast<NodeB**>(g_p_4D7AF4 + 0x4f38);
}

}  // namespace

// ===========================================================================
// 1. 链表遍历查找
// ===========================================================================
//
// ## 0x00417790（47 字节）
//
// ```asm
// 558bec             push ebp / mov ebp,esp
// a1 b87a4d00        mov  eax, [0x4D7AB8]        ; ★ 全局指针
// 8b401c             mov  eax, [eax+0x1c]        ; head
// 85c0               test eax,eax
// 7411               je   notfound
// loop:
// 8b5508             mov  edx, [ebp+8]          ; key
// 8b08               mov  ecx, [eax]            ; node->inner
// 395104             cmp  [ecx+4], edx
// 740d               je   found
// 8b4004             mov  eax, [eax+4]          ; p = p->next
// 85c0               test eax,eax
// 75f2               jne  loop
// notfound:
// 33c0               xor  eax,eax
// 5dc20400           pop ebp / ret 4
// found:
// b801000000         mov  eax, 1
// 5dc20400           pop ebp / ret 4
// ```
//
// ## 0x0040A0A0（64 字节）
//
// 同一模式，区别：
//   - 先判参数非 0（`test edx,edx / je notfound`）
//   - 链表头在 `[g_4D7AF4 + 0x4f38]`
//   - key 在 `inner + 0x683c`
//
//@witness func=0x00417790 kind=thiscall
int __fastcall list_a_contains(void* self, void* unused_edx, int key) {
    (void)self;
    (void)unused_edx;
    for (NodeA* p = list_a_head(); p != nullptr; p = p->next) {
        if (p->inner->key == key) {
            return 1;
        }
    }
    return 0;
}

//@witness func=0x0040A0A0 kind=thiscall
int __fastcall list_b_contains(void* self, void* unused_edx, int key) {
    (void)self;
    (void)unused_edx;
    if (key == 0) {
        return 0;
    }
    for (NodeB* p = list_b_head(); p != nullptr; p = p->next) {
        if (p->inner->key == key) {
            return 1;
        }
    }
    return 0;
}