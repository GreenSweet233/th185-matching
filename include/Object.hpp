#pragma once

/**
 * TH185 对象的内存布局（部分）。
 *
 * ## 设计原则
 *
 * **只声明有二进制见证的字段**，字段之间用 `gap_XX` 占位并注明「未解析」。
 * 不编造字段名、不假设类型 —— 每个 `int f_XX` 都对应一条实测的机器码。
 *
 * ## 为什么用 struct + 成员函数，而不用「自由函数 + 偏移常量」
 *
 * 原版是 C++ 类，`ecx` 就是 `this`。写成成员函数后：
 *
 *   1. 编译器自动用 `ecx` 传 `this`，**不需要 `__fastcall` 伪装**
 *   2. **不需要哑参数**（`unused_edx` / `ignored`）
 *   3. 源码 token 数大幅下降，G3 自然合规
 *   4. 源码形态更接近原版真实写法
 *
 * 实测：`int C::get() { return f; }` → `8b01c3`（`mov eax,[ecx]; ret`）
 * 与 `int __fastcall get(void* s, void* d)` 生成的字节完全相同，但 token 从 12 降到 6。
 */

namespace th185 {

/// 0x00433B40 / 0x0041FA20 等：字段在 +0x00 的对象。
struct Obj00 {
    int f_00;   ///< +0x00，见证 0x00433B40（`mov eax,[ecx]`）

    int get_00();
};

/// 字段在 +0x04 / +0x08 的对象（0x00433BA0 / 0x00433BB0）。
struct Obj04 {
    int f_00;   ///< +0x00
    int f_04;   ///< +0x04，见证 0x00433BA0
    int f_08;   ///< +0x08，见证 0x00433BB0

    void set_04(int v);
    void set_08(int v);
};

/// 字段在 +0x1c 的对象（0x0041FA30 / 0x00429A50）。
struct Obj1C {
    char gap_00_1C[0x1C];   ///< +0x00..+0x1C 未解析
    int f_1C;               ///< +0x1C，见证 0x0041FA30

    int get_1C();
    int* addr_1C();
};

/// 字段在 +0x20 的对象（0x004176D0）。
struct Obj20 {
    char gap_00_20[0x20];   ///< +0x00..+0x20 未解析
    int f_20;               ///< +0x20，见证 0x004176D0

    int get_20();
};

/// 字段在 +0x24 的对象（0x00406730）。
struct Obj24 {
    char gap_00_24[0x24];   ///< +0x00..+0x24 未解析
    int f_24;               ///< +0x24，见证 0x00406730

    int get_24();
};

/// 字段在 +0x28 的对象（0x00424480）。
struct Obj28 {
    char gap_00_28[0x28];   ///< +0x00..+0x28 未解析
    int f_28;               ///< +0x28，见证 0x00424480

    void clear_28();
};

/// 字段在 +0x34 的对象（0x00417770）。
struct Obj34 {
    char gap_00_34[0x34];   ///< +0x00..+0x34 未解析
    int f_34;               ///< +0x34，见证 0x00417770

    int get_34();
};

/// 字段在 +0x3C 的对象（0x00441210 / 0x00440840）。
struct Obj3C {
    char gap_00_3C[0x3C];   ///< +0x00..+0x3C 未解析
    int f_3C;               ///< +0x3C，见证 0x00441210

    int get_3C();
    void set_3C(int v);
};

/// 字段在 +0x44 的对象（0x00433DC0）。
struct Obj44 {
    char gap_00_44[0x44];   ///< +0x00..+0x44 未解析
    int f_44;               ///< +0x44，见证 0x00433DC0

    int* addr_44();
};

/// 字段在 +0x50 的对象（0x00431300）。
struct Obj50 {
    char gap_00_50[0x50];   ///< +0x00..+0x50 未解析
    int f_50;               ///< +0x50，见证 0x00431300

    int get_50();
};

/// 字段在 +0x620 的对象（0x00407E30）。
struct Obj620 {
    char gap_00_620[0x620]; ///< +0x00..+0x620 未解析
    int f_620;              ///< +0x620，见证 0x00407E30

    int* addr_620();
};

/// 字段在 +0x1274 的对象（0x0040A090）。
struct Obj1274 {
    char gap_00_1274[0x1274]; ///< +0x00..+0x1274 未解析
    int f_1274;               ///< +0x1274，见证 0x0040A090

    int* addr_1274();
};

/// 字段在 +0x1230 的对象（0x00433DF0）。
struct Obj1230 {
    char gap_00_1230[0x1230]; ///< +0x00..+0x1230 未解析
    int f_1230;               ///< +0x1230，见证 0x00433DF0

    int* addr_1230();
};

/// 字段在 +0x4FF4 的对象（0x00433DD0）。
struct Obj4FF4 {
    char gap_00_4FF4[0x4FF4]; ///< +0x00..+0x4FF4 未解析
    int f_4FF4;               ///< +0x4FF4，见证 0x00433DD0

    int* addr_4FF4();
};

/// 字段在 +0x19260 的对象（0x00431260）。
struct Obj19260 {
    char gap_00_19260[0x19260]; ///< +0x00..+0x19260 未解析
    int f_19260;                ///< +0x19260，见证 0x00431260

    int get_19260();
};

/// 字段在 +0x47FAC 的对象（0x0040A0E0）。
struct Obj47FAC {
    char gap_00_47FAC[0x47FAC]; ///< +0x00..+0x47FAC 未解析
    int f_47FAC;                ///< +0x47FAC，见证 0x0040A0E0

    int get_47FAC();
};

}  // namespace th185