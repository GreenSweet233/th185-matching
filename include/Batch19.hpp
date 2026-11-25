#pragma once

/**
 * 第 19 批：大结构体逐字段拷贝（含 8 字节整数）。
 *
 * ## ★★ 四条关键，缺一不可
 *
 * ### 1. `movq xmm0,[mem]` 只能通过 intrinsic 生成
 *
 * | 写法 | 生成 | 长度 |
 * |---|---|---:|
 * | `long long` + 普通赋值 | `mov eax,[src] / mov edx,[src+4]` | 118 |
 * | `__m64` 字段 | MMX `0f6f`（mm 寄存器）| 100 |
 * | **`_mm_loadl_epi64` / `_mm_storel_epi64`** | **`f30f7e` / `660fd6`** | **106 ★** |
 *
 * ### 2. 8 字节块用 `char[8]`，不能用 `long long` 或 `#pragma pack(1)`
 *
 * 目标里 `+0xc`（非 8 的倍数）也有 8 字节拷贝 —— 需要 1 字节对齐。
 *
 * - `long long` 默认 8 字节对齐 -> 字段被推到 `+0x10`（字节不对）
 * - `#pragma pack(1)` 能对齐，但 **`#pragma` 行会被 check_bytes.py 过滤掉** -> C2143
 * - **`char[8]` 天然 1 字节对齐，且不需要 pragma** ✓
 *
 * ### 3. 必须逐字段写
 *
 * 整体赋值 `*dst = *src` 被优化成 `rep movsd`（/O1）或 `movups`（/O2）。
 *
 * ### 4. 必须写成 static 成员函数
 *
 * `check_bytes.py` 只在签名形如 `Cls::name(` 时才拼入 hpp 的 struct 定义。
 */

struct CopyBlock {
    char m0[8];   ///< +0x00（alignment 1）
    int  i0;      ///< +0x08
    char m1[8];   ///< +0x0c（未对齐，靠 char[8] 保证）
    int  i1;      ///< +0x14
    int  i2;      ///< +0x18
    int  i3;      ///< +0x1c
    int  i4;      ///< +0x20
    int  i5;      ///< +0x24
    int  i6;      ///< +0x28
    int  i7;      ///< +0x2c
    int  i8;      ///< +0x30
    char m2[8];   ///< +0x34
    int  i9;      ///< +0x3c
    int  i10;     ///< +0x40
};

struct Copier {
    static CopyBlock* __fastcall copy_big(CopyBlock* dst, void* unused_edx, const CopyBlock* src);
};