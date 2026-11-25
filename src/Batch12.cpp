#include "Batch12.hpp"

// ===========================================================================
// 1. 全局表线性搜索（43 字节，叶子函数 —— 没有 push ebp）
// ===========================================================================
//
//   33d2                xor  edx, edx           ; i = 0
//   b8 74a34c00         mov  eax, &table[0].key  ; (reloc)
//   loop:
//   3908                cmp  [eax], ecx
//   7411                je   found
//   83c044              add  eax, 0x44
//   42                  inc  edx
//   3d 90ba4c00         cmp  eax, &table[87]     ; (reloc)
//   7cf1                jl   loop
//   b8 04ba4c00         mov  eax, &sentinel      ; (reloc)
//   c3                  ret
//   found:
//   8bc2 c1e004 03c2    mov eax,edx / shl eax,4 / add eax,edx   ; i*17
//   8d0485 70a34c00     lea  eax, [eax*4+&table]                ; (reloc)
//   c3                  ret
//
// ★ 四条经验（每一层都是一个坑，缺一不可）：
//
//   1. **key 在 struct 的 +4**
//      比较用 `&table[0].key`（table+4），返回用 `&table[i]`（table+0），差 4 字节。
//      只看比较指令会漏掉这一点，必须同时看返回值怎么算的。
//
//   2. **索引循环而非指针循环**
//      for (int i = 0; i < 87; ++i) { if (t[i].key == k) return &t[i]; }   // 43 字节 ✓
//      for (Entry* p = t; p < end; ++p) { if (p->key == k) return p; }      // 37 字节 ✗
//      目标有 `inc edx` 和返回时的 `i*17*4` —— 说明是索引循环。
//
//   3. **必须写成 static 成员函数**（`Finder::find_by_key`）
//      check_bytes.py 只在函数签名形如 `Cls::name(` 时才把 hpp 的 struct 定义
//      拼进 TU。写成自由函数 -> `Entry` 类型未知 -> C4430。
//
//   4. **`kEntryCount` 必须直接写字面量 87，不能用 constexpr**
//      constexpr 会被 check_bytes.py 的 prefix 单独提取一次，
//      同时因为成员函数路径整个 hpp 又被拼接一次 -> **C2374 重定义**。
//      （这条与 Batch8 踩过的「constexpr 不能放 hpp」是同一个坑）
//
//@witness func=0x00407F80 kind=thiscall
//@witness disp 0x4ca374
//@witness disp 0x4cba90
//@witness disp 0x4cba04
//@witness disp 0x4ca370
Entry* __fastcall Finder::find_by_key(unsigned key) {
    for (int i = 0; i < 87; ++i) {
        if (g_table[i].key == static_cast<int>(key)) {
            return &g_table[i];
        }
    }
    return &g_sentinel;
}