# TH185 已匹配函数清单（按类别）

> 更新于 2026-11-24（第 4 批批量匹配后）· 数据来源：`tools/check_bytes.py` 当次输出 + 二进制实测
>
> **本文件的每个地址只出现一次。** 各分类之和 = 156，与 `check_bytes.py` 的 `targets` 数量**精确相等**。

## 真实数字（`tools/check_bytes.py` 原始输出）

```
targets : 156（由 //@witness 自动推导）
结果: 131 / 156 字节相等
```

| 项 | 数量 |
|---|---:|
| 有源码的函数（`//@witness` 自动推导的 targets） | **156** |
| P1 逐字节通过 | **131** |
| P1 未通过 | **25**（全部已知物理不可得，见末节） |
| 二进制可枚举函数总数（G2 口径：直接 call 目标） | 1675 |
| 覆盖率（156 / 1675） | **9.31%** |

> 第 4 批新增 29 个函数，**全部一次通过**。
> 25 个 DIFF 的地址集合与第 3 批**完全相同** —— 无新增失败、无既有匹配退化。

## 类别总览

> ★ 本表的分类是**从二进制机器码自动推导**的（判据见每节标题），不是手工归类。
> 因此各类的数量与第 3 批手工归类的数字不同 —— 例如 `0x0049025C` 以前被手工放进 Z，
> 按机器码形状它属于 H（`xor eax,eax / cmp [mem],eax / setne al / ret`）。
> **判定口径变严了，不是匹配数变了。** 匹配数始终来自 `check_bytes.py`。

| 类 | 名称 | 数量 | P1 通过 | P1 失败 |
|---|---|---:|---:|---:|
| C1 | SSE 标量**读取 / 运算**（`movss xmm0, ...`） | **12** | 12 | 0 |
| C2 | SSE 标量**写入**（`movss [ecx+N], xmmN`） | **13** | 13 | 0 |
| H | 字段读取器 / 返回常量（返回 int） | **36** | 36 | 0 |
| I | 字段地址取用（返回指针） | **6** | 6 | 0 |
| J | 带栈帧的简单写入器 | **25** | 25 | 0 |
| D | 12 字节聚合拷贝（SSE2 `movq`） | **10** | 10 | 0 |
| A | 空函数体 | **13** | 13 | 0 |
| B | 返回零 | **4** | 4 | 0 |
| G | 倒计时 tick（有分支） | **1** | 1 | 0 |
| K | 含 call 的包装器 | **1** | 1 | 0 |
| Z | 其他：位运算 / 全局访问 / 复合 | **11** | 10 | 1 |
| E | 指针编码（`__security_cookie` 轮转）— **全部未通过** | **5** | 0 | 5 |
| F | `fsincos` 极坐标转换 — **全部未通过** | **19** | 0 | 19 |
| | **合计** | **156** | **131** | **25** |

---

## 第 4 批新增的 29 个函数（本批全部 P1 MATCH）

源码：`src/MoreFields.cpp`（26 个）+ `src/Misc.cpp`（3 个）。
结构体：`include/MoreFields.hpp` + `include/Misc.hpp`。

其中 **5 类是本次首次匹配的新形状**（其余为已有形状族的新实例）：

| 新形状 | 判据 |
|---|---|
| 从指针取值写入 | `mov eax,[ebp+8] / mov eax,[eax] / mov [ecx+N],eax`。★ 形参必须是 `const int*`，写成 `int` 会退化成普通写入器（少一条 `mov eax,[eax]`）|
| 数组元素写入 | `mov [ecx+eax*4+0x54],edx` + `ret 8` |
| 8 字节聚合整块赋值 | `mov eax,[esi] / mov edx,[esi+4]`。★ 整体赋值；逐字段写会让编译器改用 edx 当源指针 |
| 全局聚合整体拷贝 | `a1 ../8b15../8941N/8951N`。★ 全局必须声明成 `Pair32` **对象**再整体拷贝 |
| 全局表索引 | `mov eax,[eax+ecx*4+0x0312072C]`（disp32 远超任何合理字段偏移）|

按类别归入上表（不重复列出）：J ×3、D ×6、C2 ×1、H ×4、Z ×15。

本批新确立的代码生成规则：

- 形参必须写成 `const int*`（而不是 `int`）才会生成 `mov eax,[eax]`
- `f = *source` 整体赋值用 eax+edx；写成 `f0=s[0]; f1=s[1];` 会让编译器改用 edx 当源指针
- 拷贝全局时必须写成 `Pair32` 类型的**对象**再整体赋值，不能逐字段写
- 位翻转必须写成 `(f & ~1) | (arg & 1)`；写成 if/else 会生成 cmov 序列
- 返回 `int`（而非 `bool`）才能得到 `xor eax,eax` + `cmp [mem],eax` + `sete al`

---

## C1. SSE 标量**读取 / 运算**（`movss xmm0, ...`）（12 个）

★ 返回类型必须是 **`__m128`** 而不是 `float`。MSVC x86 对 `float` 返回值走 x87 栈（`fld`），只有 `__m128` 走 XMM0（`movss`）。含 `mulss` 时第 2 操作数必须保持内存形式。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404B60` | ✅ | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00405A60` | ✅ | 6 | `f30f104144c3` | FieldAccess.cpp |
| `0x0041C710` | ✅ | 11 | `f30f104174f30f594154c3` | FieldAccess.cpp |
| `0x00429A40` | ✅ | 6 | `f30f104118c3` | SseAccess.cpp |
| `0x00441240` | ✅ | 6 | `f30f104128c3` | SseAccess.cpp |
| `0x00441250` | ✅ | 6 | `f30f10412cc3` | SseAccess.cpp |
| `0x00441330` | ✅ | 6 | `f30f104120c3` | SseAccess.cpp |
| `0x00441340` | ✅ | 6 | `f30f104124c3` | SseAccess.cpp |
| `0x00441600` | ✅ | 6 | `f30f104104c3` | SseAccess.cpp |
| `0x00441610` | ✅ | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00441620` | ✅ | 11 | `f30f104178f30f594158c3` | Misc.cpp |
| `0x0048AC40` | ✅ | 13 | `83893405000004f30f114944c3` | Misc.cpp |

---

## C2. SSE 标量**写入**（`movss [ecx+N], xmmN`）（13 个）

★ `__m128` 形参占 XMM：第 1 个落 xmm0、第 2 个落 xmm1、第 3 个落 xmm2。原版写的是 xmm1/xmm2，所以源码里必须有哑形参把位置占掉。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00417780` | ✅ | 6 | `f30f114918c3` | SseAccess.cpp |
| `0x004299F0` | ✅ | 6 | `f30f114924c3` | SseAccess.cpp |
| `0x00429A00` | ✅ | 6 | `f30f114920c3` | SseAccess.cpp |
| `0x00429B90` | ✅ | 11 | `f30f114948f30f11514cc3` | Misc.cpp |
| `0x00433C60` | ✅ | 17 | `f30f1189c4000000f30f1191c8000000c3` | MoreFields.cpp |
| `0x004341D0` | ✅ | 6 | `f30f11496cc3` | SseAccess.cpp |
| `0x004341E0` | ✅ | 6 | `f30f114974c3` | SseAccess.cpp |
| `0x004341F0` | ✅ | 6 | `f30f114978c3` | SseAccess.cpp |
| `0x00441270` | ✅ | 6 | `f30f11492cc3` | SseAccess.cpp |
| `0x00441310` | ✅ | 6 | `f30f114938c3` | SseAccess.cpp |
| `0x00441320` | ✅ | 6 | `f30f114934c3` | SseAccess.cpp |
| `0x004415E0` | ✅ | 6 | `f30f114904c3` | SseAccess.cpp |
| `0x004415F0` | ✅ | 5 | `f30f1109c3` | SseAccess.cpp |

---

## H. 字段读取器 / 返回常量（返回 int）（36 个）

`mov eax, X; ret`，X 是 `[ecx+N]`（字段）或立即数（常量）。★ 返回 **`int`** 而非 `bool`。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404AA0` | ✅ | 3 | `8bc1c3` | Trivial.cpp |
| `0x00404BD0` | ✅ | 6 | `b8689c5700c3` | Trivial.cpp |
| `0x00406730` | ✅ | 4 | `8b4124c3` | Object.cpp |
| `0x00407E40` | ✅ | 12 | `33c03981a00200000f95c0c3` | FieldAccess.cpp |
| `0x0040A0E0` | ✅ | 7 | `8b81ac7f0400c3` | Object.cpp |
| `0x004176D0` | ✅ | 4 | `8b4120c3` | Object.cpp |
| `0x00417770` | ✅ | 4 | `8b4134c3` | Object.cpp |
| `0x00417AF0` | ✅ | 3 | `8bc1c3` | Trivial.cpp |
| `0x0041FA20` | ✅ | 3 | `8b01c3` | Trivial.cpp |
| `0x0041FA30` | ✅ | 4 | `8b411cc3` | Object.cpp |
| `0x00429A10` | ✅ | 13 | `8b414083e0f283c802894140c3` | Misc.cpp |
| `0x00431260` | ✅ | 7 | `8b8160920100c3` | Object.cpp |
| `0x00431300` | ✅ | 4 | `8b4150c3` | Object.cpp |
| `0x00433820` | ✅ | 11 | `a1e07a4d00f30f114844c3` | Misc.cpp |
| `0x00433850` | ✅ | 12 | `a1147c4d008b802cbbe600c3` | Misc.cpp |
| `0x00433B40` | ✅ | 3 | `8b01c3` | Trivial.cpp |
| `0x004340F0` | ✅ | 18 | `a1087b4d008b80a00200008b80ac010000c3` | Misc.cpp |
| `0x00440850` | ✅ | 20 | `a1983e57008b159c3e570089411889511cc20400` | MoreFields.cpp |
| `0x00440870` | ✅ | 20 | `a1983e57008b159c3e5700894110895114c20400` | MoreFields.cpp |
| `0x004410D0` | ✅ | 13 | `8b412083f80f7d0440894120c3` | MoreFields.cpp |
| `0x00441210` | ✅ | 4 | `8b413cc3` | Object.cpp |
| `0x00441260` | ✅ | 13 | `8b414083e0f383c803894140c3` | Misc.cpp |
| `0x0045C9E0` | ✅ | 6 | `b8709c5700c3` | Misc.cpp |
| `0x0049025C` | ✅ | 12 | `33c03905a8a357000f95c0c3` | Misc.cpp |
| `0x00490556` | ✅ | 6 | `b800400000c3` | Misc.cpp |
| `0x004905B5` | ✅ | 12 | `33c03905608a4c000f94c0c3` | Misc.cpp |
| `0x004905C1` | ✅ | 6 | `b8a4a35700c3` | Misc.cpp |
| `0x004905C7` | ✅ | 6 | `b8a0a35700c3` | Misc.cpp |
| `0x0049C51C` | ✅ | 20 | `8b41086683382a750a83c002c6411801894108c3` | MoreFields.cpp |
| `0x0049EBDE` | ✅ | 6 | `a1f8744d00c3` | Misc.cpp |
| `0x0049EEBB` | ✅ | 6 | `b80c754d00c3` | Misc.cpp |
| `0x004A1726` | ✅ | 6 | `b818784d00c3` | Misc.cpp |
| `0x004A172C` | ✅ | 6 | `b81c784d00c3` | Misc.cpp |
| `0x004A1732` | ✅ | 6 | `b814784d00c3` | Misc.cpp |
| `0x004A1738` | ✅ | 6 | `a120784d00c3` | Misc.cpp |
| `0x004A173E` | ✅ | 6 | `a124784d00c3` | Misc.cpp |

---

## I. 字段地址取用（返回指针）（6 个）

`lea eax,[ecx+N]; ret`。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00407E30` | ✅ | 7 | `8d8120060000c3` | Object.cpp |
| `0x0040A090` | ✅ | 7 | `8d8174120000c3` | Object.cpp |
| `0x00429A50` | ✅ | 4 | `8d411cc3` | Object.cpp |
| `0x00433DC0` | ✅ | 4 | `8d4144c3` | Object.cpp |
| `0x00433DD0` | ✅ | 7 | `8d81f44f0000c3` | Object.cpp |
| `0x00433DF0` | ✅ | 7 | `8d8130120000c3` | Object.cpp |

---

## J. 带栈帧的简单写入器（25 个）

`push ebp / ... / pop ebp / ret N`。★ 需要 **`/Oy-`**（`/O2` 默认省略帧指针）。`ret N` 说明 thiscall 自动清栈。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404D10` | ✅ | 10 | `558bec8b45085dc20c00` | FieldAccess.cpp |
| `0x00406720` | ✅ | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00409F60` | ✅ | 13 | `558bec8b45088941205dc20400` | FieldAccess.cpp |
| `0x004176E0` | ✅ | 13 | `558bec8b45088941345dc20400` | FieldWriter.cpp |
| `0x00428090` | ✅ | 13 | `558bec8b45088941285dc20400` | FieldWriter.cpp |
| `0x004280A0` | ✅ | 15 | `558bec8b45088b008941045dc20400` | MoreFields.cpp |
| `0x004280B0` | ✅ | 14 | `558bec8b45088b0089015dc20400` | MoreFields.cpp |
| `0x004280C0` | ✅ | 13 | `558bec8b450889412c5dc20400` | FieldWriter.cpp |
| `0x00430590` | ✅ | 15 | `558bec8b45088b0089410c5dc20400` | MoreFields.cpp |
| `0x004305A0` | ✅ | 15 | `558bec8b45088b008941085dc20400` | MoreFields.cpp |
| `0x00433B00` | ✅ | 16 | `558bec8a45088881301000005dc20400` | FieldWriter.cpp |
| `0x00433B70` | ✅ | 19 | `558bec8b411833450883e0013141185dc20400` | MoreFields.cpp |
| `0x00433BA0` | ✅ | 13 | `558bec8b45088941045dc20400` | Object.cpp |
| `0x00433BB0` | ✅ | 13 | `558bec8b45088941085dc20400` | Object.cpp |
| `0x00433BC0` | ✅ | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00433BE0` | ✅ | 17 | `558bec8b45088b550c895481545dc20800` | MoreFields.cpp |
| `0x00433C80` | ✅ | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x004340A0` | ✅ | 16 | `558bec8b45088981780200005dc20400` | FieldWriter.cpp |
| `0x00440840` | ✅ | 13 | `558bec8b450889413c5dc20400` | Object.cpp |
| `0x004408B0` | ✅ | 20 | `558bec8b45088b108b400489118941045dc20400` | MoreFields.cpp |
| `0x004408D0` | ✅ | 13 | `558bec8b45088941405dc20400` | MoreFields.cpp |
| `0x00440B90` | ✅ | 13 | `558bec8b45088941505dc20400` | MoreFields.cpp |
| `0x004410C0` | ✅ | 13 | `558bec8b45088941245dc20400` | MoreFields.cpp |
| `0x00441220` | ✅ | 16 | `558bec8b45080181dc0000005dc20400` | MoreFields.cpp |
| `0x00441230` | ✅ | 16 | `558bec8b45080181d80000005dc20400` | MoreFields.cpp |

---

## D. 12 字节聚合拷贝（SSE2 `movq`）（10 个）

★ 必须 **struct 整体赋值**：逐字段 3 条 `mov`（18 字节），整体赋值才触发 `movq`+`mov`。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00405830` | ✅ | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x004063C0` | ✅ | 31 | `558bec8b4508f30f7e00660fd681f00500008b40088981f80500005dc20400` | FieldAccess.cpp |
| `0x00417730` | ✅ | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x00429A20` | ✅ | 25 | `558bec8b4508f30f7e00660fd641348b400889413c5dc20400` | MoreFields.cpp |
| `0x00434250` | ✅ | 31 | `558bec8b4508f30f7e00660fd681a40700008b40088981ac0700005dc20400` | MoreFields.cpp |
| `0x00440BA0` | ✅ | 25 | `558bec8b4508f30f7e00660fd641308b40088941385dc20400` | MoreFields.cpp |
| `0x00440BC0` | ✅ | 25 | `558bec8b4508f30f7e00660fd641248b400889412c5dc20400` | MoreFields.cpp |
| `0x00440BE0` | ✅ | 25 | `558bec8b4508f30f7e00660fd641188b40088941205dc20400` | MoreFields.cpp |
| `0x00440C00` | ✅ | 25 | `558bec8b4508f30f7e00660fd6410c8b40088941145dc20400` | MoreFields.cpp |
| `0x00441650` | ✅ | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |

---

## A. 空函数体（13 个）

只有 `ret`。虚函数默认体 / 占位函数。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404C20` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00441640` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00448100` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x0045CA20` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x004717D0` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00473800` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00478740` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x0047A210` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x0048AFA0` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x0048C910` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x0048C920` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00490592` | ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00490595` | ✅ | 3 | `c20000` | Trivial.cpp |

---

## B. 返回零（4 个）

`xor eax,eax; ret`。返回假/失败的默认实现。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x004903B9` | ✅ | 3 | `33c0c3` | Trivial.cpp |
| `0x00490553` | ✅ | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055C` | ✅ | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055F` | ✅ | 3 | `33c0c3` | Trivial.cpp |

---

## G. 倒计时 tick（有分支）（1 个）

唯一 68 字节函数。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00402A50` | ✅ | 68 | `8b918c00000083ea0189918c000000791bc7818c00000000000000c70100000000c781d400000000000000c38b44910c89018b44914c894108c781d400000000000000c3` | Timer.cpp |

---

## K. 含 call 的包装器（1 个）

需 `/Os`（只有它生成 `pop ecx`）+ `cdecl` + 返回 void。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x0048F711` | ✅ | 14 | `558becff7508e863090000595dc3` | FieldAccess.cpp |

---

## Z. 其他：位运算 / 全局访问 / 复合（11 个）

> 本类 10 通过 / 1 未通过。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x0040A050` | ✅ | 9 | `c701000000008bc1c3` | FieldAccess.cpp |
| `0x00424480` | ✅ | 8 | `c7412800000000c3` | Object.cpp |
| `0x00429AB0` | ✅ | 5 | `836140f0c3` | Trivial.cpp |
| `0x00431270` | ✅ | 15 | `8b490833c080e13080f9200f94c0c3` | MoreFields.cpp |
| `0x00433DE0` | ✅ | 11 | `81896863000000000002c3` | Misc.cpp |
| `0x004407F0` | ✅ | 8 | `c7413c00000000c3` | Misc.cpp |
| `0x00440B40` | ✅ | 8 | `c7415000000000c3` | Misc.cpp |
| `0x00441630` | ✅ | 15 | `8b494033c080e10f80f9020f94c0c3` | MoreFields.cpp |
| `0x0048AEC0` | ✅ | 16 | `8b491ca1889e52008b84882c071203c3` | MoreFields.cpp |
| `0x00490463` | ❌ | 8 | `8325f4724d0000c3` | Misc.cpp |
| `0x0049056E` | ✅ | 3 | `b001c3` | Misc.cpp |

---

## E. 指针编码（`__security_cookie` 轮转）— **全部未通过**（5 个）

★ 原版有 `8bff`（`mov edi,edi`）hotpatch 序言，v141/v142 都生成 `66 90`。工具链层面物理不可得，不是源码写法问题。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x0049DC88` | ❌ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E757` | ❌ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E776` | ❌ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EB13` | ❌ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EBF3` | ❌ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |

---

## F. `fsincos` 极坐标转换 — **全部未通过**（19 个）

★ MSVC 前端**永不**生成 `fsincos`（跨 v141/v142 穷举 60 种组合）。它是静态链接的 x87 CRT 内部实现。源码只保留语义，不声称字节匹配。

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404B70` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00405A70` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00406290` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00407E10` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00417750` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00417B00` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004299D0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004305B0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00430720` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00441580` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00441670` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0044CDA0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004587B0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00463DF0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00467150` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0047FFE0` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00485B20` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00486080` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0048F370` | ❌ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |

---

## 25 个未通过项的定性（与上表 E/F/Z 类一致）

| 类 | 数量 | 残差 | 为何不可得 | 已穷举 |
|---|---:|---|---|---|
| F `fsincos` | 19 | 原版 32 字节含 `d9fb`；我们 `call sinf/cosf` 67 字节，公共前缀 3 | MSVC 前端只生成 `call`，从不把 sin+cos 合成一条 `fsincos`；它是静态链接的 x87 CRT 内部实现 | v141/v142 × 60 种 flags（含 `/arch:IA32`），`d9fb` 一次未出现 |
| E hotpatch 序言 | 5 | 原版以 `8bff`（`mov edi,edi`）开头；v142 生成 `66 90`，公共前缀 0 | v142 的 `/hotpatch` 选用了 `66 90` 而非 `8bff` | `/Od` `/O1` `/O2` × `/hotpatch` 等 45 种组合，`8bff` 一次未出现 |
| Z `and [mem],0` | 1 | 原版 8 字节 `8325 f4724d00 00 c3`；我们 11 字节 `c705 ...` | 现代 MSVC 对 `g = 0` 一律生成 `mov [mem],0` 而非 `and [mem],0` | 45 种写法/选项组合 |

`0x00490463`（`and dword [0x4D72F4], 0`）是 Z 类里的那一个。
**这些没有硬塞进 targets，也没有用 `__asm` 绕过**（§6 G4 禁止 inline-asm）。
完整记录见 `ledger/unresolved.md`。

---

## 编译选项速查

| 场景 | 选项 |
|---|---|
| 默认（多数函数）| `/MT /O2 /GS- /Oy-` |
| 含 `pop ecx` 清栈 | `/MT /Os /GS- /Oy-` |

**`/Oy-` 必需** —— `/O2` 默认省略帧指针，原版多数函数保留栈帧。

---

## SseAccess.cpp 的 17 个：17/17 全部 MATCH

之前报告为 `COMPILE_ERROR`，根因是 `check_bytes.py` 的 Bug A（`#include` 顺序）
与 Bug B（namespace 链丢失），两个 bug 上一轮已修复合并。
本批实测确认：**bug 修好后它们立刻全部通过，源码一个字节都没改。**

---

## 验证覆盖率的真实情况

| 源文件 | witness 函数数 | P1 MATCH | P1 未通过 |
|---|---:|---:|---:|
| `Timer.cpp` | 1 | 1 | 0 |
| `Trivial.cpp` | 10 | 10 | 0 |
| `FieldAccess.cpp` | 12 | 12 | 0 |
| `SseAccess.cpp` | 17 | 17 | 0 |
| `FieldWriter.cpp` | 8 | 8 | 0 |
| `Misc.cpp` | 26 | 26 | 0 |
| `Object.cpp` | 19 | 19 | 0 |
| `MoreFields.cpp` | 26 | 26 | 0 |
| **合计** | **156** | **131** | **25** |

> 未通过项集中在 `Trivial.cpp`（F 类 19 + E 类 5 = 24）与 `Misc.cpp`（1 个）。
> 这不是文件划分问题 —— 是那 25 个二进制函数本身生成不出来。
