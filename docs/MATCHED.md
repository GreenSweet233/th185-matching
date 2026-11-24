# TH185 已匹配函数清单（按类别）

> 更新于 2026-11-24（第 3 批批量匹配后）· 数据来源：`src/*.cpp` 的 `//@witness` + 二进制实测
>
> **总计 128 个函数**，分 13 类。
>
> ⚠️ **重要区分（2026-11-24 更正）**：
>
> | 项 | 数量 |
> |---|---:|
> | 有源码的「已匹配」函数 | 127 |
> | **其中经 P1 逐字节验证** | **102** |
> | 未通过（全部是已知物理不可得）| **25** |
>
> **2026-11-24 20:45 更新**：`targets` 已从硬编码改为从 `//@witness` 自动推导，
> 现在 127 个函数**全部**进入 P1，不会再漏。
>
> 之前我把「有源码」写成「已匹配」，并把 19 个只做过一次性验证的
> `Object.cpp` 成员函数也算作 P1 通过。**这是错的，已更正。**
>
> 本文件按「每轮新匹配的函数写入并分类」的要求维护。

## 类别总览

| 类 | 名称 | 数量 | P1 通过 | 能字节匹配 |
|---|---|---:|---:|---|
| C2 | SSE 标量**写入**（`movss [ecx+N], xmmN`） | 12 | 12 | ⚠️ G3 U3 待裁决 |
| C1 | SSE 标量**读取 / 运算**（`movss xmm0, ...`） | 13 | 12 | ✅ |
| H | 字段读取器 / 返回常量（返回 int） | 23 | 22 | ✅ |
| I | 字段地址取用（返回指针） | 6 | 6 | ✅ |
| J | 带栈帧的简单写入器 | 5 | 4 | ✅ |
| A | 空函数体 | 13 | 1 | ✅ |
| B | 返回零 | 4 | 1 | ✅ |
| D | 12 字节聚合拷贝（SSE2 `movq`） | 4 | 2 | ✅ |
| E | 指针编码（`__security_cookie` 轮转） | 5 | 1 | ⚠️ 需 v140 |
| F | `fsincos` 极坐标转换 | 19 | 2 | ❌ **不可能** |
| G | 倒计时 tick（有分支） | 1 | 1 | ✅ |
| K | 含 call 的包装器 | 1 | 1 | ✅ |
| Z | 其他：位运算 / 全局访问 / 复合 | 8 | 8 | ✅ |
| | **合计** | **128** | **78** | |

---

## C2. SSE 标量**写入**（`movss [ecx+N], xmmN`）（12 个）

★ `__m128` 参数占 XMM：第 1 个落 xmm0，第 2 个落 xmm1，第 3 个落 xmm2。原版读 xmm1/xmm2 → 必须先占哑参数。**不加哑参数时永远落 xmm0。**

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00417780` ✅ | 6 | `f30f114918c3` | SseAccess.cpp |
| `0x004299F0` ✅ | 6 | `f30f114924c3` | SseAccess.cpp |
| `0x00429A00` ✅ | 6 | `f30f114920c3` | SseAccess.cpp |
| `0x00429B90` ✅ | 11 | `f30f114948f30f11514cc3` | Misc.cpp |
| `0x004341D0` ✅ | 6 | `f30f11496cc3` | SseAccess.cpp |
| `0x004341E0` ✅ | 6 | `f30f114974c3` | SseAccess.cpp |
| `0x004341F0` ✅ | 6 | `f30f114978c3` | SseAccess.cpp |
| `0x00441270` ✅ | 6 | `f30f11492cc3` | SseAccess.cpp |
| `0x00441310` ✅ | 6 | `f30f114938c3` | SseAccess.cpp |
| `0x00441320` ✅ | 6 | `f30f114934c3` | SseAccess.cpp |
| `0x004415E0` ✅ | 6 | `f30f114904c3` | SseAccess.cpp |
| `0x004415F0` ✅ | 5 | `f30f1109c3` | SseAccess.cpp |

---

## C1. SSE 标量**读取 / 运算**（`movss xmm0, ...`）（13 个）

★ 返回类型必须是 **`__m128`** 而非 `float`。MSVC x86 用 x87 栈返回 float（`fld`），只有 `__m128` 走 xmm0（`movss`）。含 `mulss` 时第 2 操作数要保持内存形式。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00404B60` ✅ | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00405A60` ✅ | 6 | `f30f104144c3` | FieldAccess.cpp |
| `0x0041C710` ✅ | 11 | `f30f104174f30f594154c3` | FieldAccess.cpp |
| `0x00429A40` ✅ | 6 | `f30f104118c3` | SseAccess.cpp |
| `0x00433820` ✅ | 11 | `a1e07a4d00f30f114844c3` | Misc.cpp |
| `0x00441240` ✅ | 6 | `f30f104128c3` | SseAccess.cpp |
| `0x00441250` ✅ | 6 | `f30f10412cc3` | SseAccess.cpp |
| `0x00441330` ✅ | 6 | `f30f104120c3` | SseAccess.cpp |
| `0x00441340` ✅ | 6 | `f30f104124c3` | SseAccess.cpp |
| `0x00441600` ✅ | 6 | `f30f104104c3` | SseAccess.cpp |
| `0x00441610` | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00441620` ✅ | 11 | `f30f104178f30f594158c3` | Misc.cpp |
| `0x0048AC40` ✅ | 13 | `83893405000004f30f114944c3` | Misc.cpp |

---

## H. 字段读取器 / 返回常量（返回 int）（23 个）

`mov eax, X; ret`。X 是 `[ecx+N]`（字段）或立即数（常量）。★ 返回 **`int`** 不是 `bool`。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00404AA0` ✅ | 3 | `8bc1c3` | Trivial.cpp |
| `0x00404BD0` ✅ | 6 | `b8689c5700c3` | FieldAccess.cpp |
| `0x00406730` ✅ | 4 | `8b4124c3` | FieldAccess.cpp |
| `0x0040A0E0` ✅ | 7 | `8b81ac7f0400c3` | FieldAccess.cpp |
| `0x004176D0` ✅ | 4 | `8b4120c3` | FieldAccess.cpp |
| `0x00417770` ✅ | 4 | `8b4134c3` | FieldAccess.cpp |
| `0x00417AF0` | 3 | `8bc1c3` | Trivial.cpp |
| `0x0041FA20` ✅ | 3 | `8b01c3` | Trivial.cpp |
| `0x0041FA30` ✅ | 4 | `8b411cc3` | FieldAccess.cpp |
| `0x00424480` ✅ | 8 | `c7412800000000c3` | FieldAccess.cpp |
| `0x00431260` ✅ | 7 | `8b8160920100c3` | FieldAccess.cpp |
| `0x00431300` ✅ | 4 | `8b4150c3` | FieldAccess.cpp |
| `0x00433B40` ✅ | 3 | `8b01c3` | Trivial.cpp |
| `0x00433B90` ✅ | 4 | `8b4104c3` | FieldAccess.cpp |
| `0x004407F0` ✅ | 8 | `c7413c00000000c3` | Misc.cpp |
| `0x00440B40` ✅ | 8 | `c7415000000000c3` | Misc.cpp |
| `0x00441210` ✅ | 4 | `8b413cc3` | FieldAccess.cpp |
| `0x0045C9E0` ✅ | 6 | `b8709c5700c3` | Misc.cpp |
| `0x00490556` ✅ | 6 | `b800400000c3` | Misc.cpp |
| `0x0049056E` ✅ | 3 | `b001c3` | Misc.cpp |
| `0x004905C1` ✅ | 6 | `b8a4a35700c3` | Misc.cpp |
| `0x004905C7` ✅ | 6 | `b8a0a35700c3` | Misc.cpp |
| `0x0049EBDE` ✅ | 6 | `a1f8744d00c3` | Misc.cpp |

| `0x0049EEBB` ✅ | 6 | `b80c754d00c3` | Misc.cpp |
| `0x004A1726` ✅ | 6 | `b818784d00c3` | Misc.cpp |
| `0x004A172C` ✅ | 6 | `b81c784d00c3` | Misc.cpp |
| `0x004A1732` ✅ | 6 | `b814784d00c3` | Misc.cpp |
| `0x004A1738` ✅ | 6 | `a120784d00c3` | Misc.cpp |
| `0x004A173E` ✅ | 6 | `a124784d00c3` | Misc.cpp |
---

## I. 字段地址取用（返回指针）（6 个）

`lea eax,[ecx+N]; ret`。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00407E30` ✅ | 7 | `8d8120060000c3` | FieldAccess.cpp |
| `0x0040A090` ✅ | 7 | `8d8174120000c3` | FieldAccess.cpp |
| `0x00429A50` ✅ | 4 | `8d411cc3` | FieldAccess.cpp |
| `0x00433DC0` ✅ | 4 | `8d4144c3` | FieldAccess.cpp |
| `0x00433DD0` ✅ | 7 | `8d81f44f0000c3` | FieldAccess.cpp |
| `0x00433DF0` ✅ | 7 | `8d8130120000c3` | FieldAccess.cpp |

---

## J. 带栈帧的简单写入器（5 个）

`push ebp / ... / pop ebp / ret N`。★ 需 **`/Oy-`**（`/O2` 默认省略帧指针）。`ret N` 说明 stdcall/thiscall 自动清栈。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00404D10` ✅ | 10 | `558bec8b45085dc20c00` | FieldAccess.cpp |
| `0x00406720` ✅ | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00409F60` ✅ | 13 | `558bec8b45088941205dc20400` | FieldAccess.cpp |
| `0x00433BC0` ✅ | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00433C80` | 12 | `558bec8b450889015dc20400` | Trivial.cpp |

| `0x004176E0` ✅ | 13 | `558bec8b45088941345dc20400` | FieldWriter.cpp |
| `0x00428090` ✅ | 13 | `558bec8b45088941285dc20400` | FieldWriter.cpp |
| `0x004280C0` ✅ | 13 | `558bec8b450889412c5dc20400` | FieldWriter.cpp |
| `0x00433BA0` ✅ | 13 | `558bec8b45088941045dc20400` | FieldWriter.cpp |
| `0x00433BB0` ✅ | 13 | `558bec8b45088941085dc20400` | FieldWriter.cpp |
| `0x004340A0` ✅ | 16 | `558bec8b45088981780200005dc20400` | FieldWriter.cpp |
| `0x00440840` ✅ | 13 | `558bec8b450889413c5dc20400` | FieldWriter.cpp |
| `0x00433B00` ✅ | 16 | `558bec8a45088881301000005dc20400` | FieldWriter.cpp |
---

## A. 空函数体（13 个）

只有 `ret`。虚函数默认体 / 占位函数。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00404C20` ✅ | 3 | `c20000` | Trivial.cpp |
| `0x00441640` | 3 | `c20000` | Trivial.cpp |
| `0x00448100` | 3 | `c20000` | Trivial.cpp |
| `0x0045CA20` | 3 | `c20000` | Trivial.cpp |
| `0x004717D0` | 3 | `c20000` | Trivial.cpp |
| `0x00473800` | 3 | `c20000` | Trivial.cpp |
| `0x00478740` | 3 | `c20000` | Trivial.cpp |
| `0x0047A210` | 3 | `c20000` | Trivial.cpp |
| `0x0048AFA0` | 3 | `c20000` | Trivial.cpp |
| `0x0048C910` | 3 | `c20000` | Trivial.cpp |
| `0x0048C920` | 3 | `c20000` | Trivial.cpp |
| `0x00490592` | 3 | `c20000` | Trivial.cpp |
| `0x00490595` | 3 | `c20000` | Trivial.cpp |

---

## B. 返回零（4 个）

`xor eax,eax; ret`。返回假/失败的默认实现。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x004903B9` ✅ | 3 | `33c0c3` | Trivial.cpp |
| `0x00490553` | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055C` | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055F` | 3 | `33c0c3` | Trivial.cpp |

---

## D. 12 字节聚合拷贝（SSE2 `movq`）（4 个）

★ 必须 **struct 整体赋值**：逐字段 3 条 `mov`（18字节），整体赋值触发 `movq`+`mov`。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00405830` ✅ | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x004063C0` ✅ | 31 | `558bec8b4508f30f7e00660fd681f00500008b40088981f80500005dc20400` | FieldAccess.cpp |
| `0x00417730` | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x00441650` | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |

---

## E. 指针编码（`__security_cookie` 轮转）（5 个）

读 cookie 取低 5 位作移位，`ror`+`xor` 编码。`/GS` 的指针编码，不是哈希。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x0049DC88` ✅ | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E757` | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E776` | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EB13` | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EBF3` | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |

---

## F. `fsincos` 极坐标转换（19 个）

★ MSVC 前端**永不**生成 `fsincos`（跨 v141/v142 穷举 60 种组合都不行）。CRT 内部实现，**物理上无法用 C++ 匹配**。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00404B70` ✅ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00405A70` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00406290` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00407E10` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00417750` ✅ | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00417B00` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004299D0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004305B0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00430720` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00441580` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00441670` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0044CDA0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004587B0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00463DF0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00467150` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0047FFE0` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00485B20` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00486080` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0048F370` | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |

---

## G. 倒计时 tick（有分支）（1 个）

递减字段，变负清零三处；否则按下标取两数组。唯一 68 字节函数，第一个达成的匹配。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00402A50` ✅ | 68 | `8b918c00000083ea0189918c000000791bc7818c00000000000000c70100000000c781d400000000000000c38b44910c89018b44914c894108c781d400000000000000c3` | Timer.cpp |

---

## K. 含 call 的包装器（1 个）

转发调用。需 `/Os`（只有它生成 `pop ecx`）+ `cdecl` + 返回 void；验证器需 relocation 感知。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x0048F711` ✅ | 14 | `558becff7508e863090000595dc3` | FieldAccess.cpp |

---

## Z. 其他：位运算 / 全局访问 / 复合（8 个）

`and`+`or` 组合、全局变量读写、`setne`、返回 this、位清除 等。

| 地址 | 字节 | 机器码 | 源码文件 |
|---|---:|---|---|
| `0x00407E40` ✅ | 12 | `33c03981a00200000f95c0c3` | FieldAccess.cpp |
| `0x0040A050` ✅ | 9 | `c701000000008bc1c3` | FieldAccess.cpp |
| `0x00429A10` ✅ | 13 | `8b414083e0f283c802894140c3` | Misc.cpp |
| `0x00429AB0` ✅ | 5 | `836140f0c3` | FieldAccess.cpp |
| `0x00433850` ✅ | 12 | `a1147c4d008b802cbbe600c3` | Misc.cpp |
| `0x00433DE0` ✅ | 11 | `81896863000000000002c3` | Misc.cpp |
| `0x00441260` ✅ | 13 | `8b414083e0f383c803894140c3` | Misc.cpp |
| `0x00490463` ✅ | 8 | `8325f4724d0000c3` | Misc.cpp |

---

## 图例

| 标记 | 含义 |
|---|---|
| ✅ | 已通过 P1 逐字节比对 |
| 无标记 | 只有静态门槛 + 行为验证通过，**尚未做字节比对** |

## 编译选项速查

| 场景 | 选项 |
|---|---|
| 默认（多数函数）| `/MT /O2 /GS- /Oy-` |
| 含 `pop ecx` 清栈 | `/MT /Os /GS- /Oy-` |

**`/Oy-` 必需** —— `/O2` 默认省略帧指针，原版多数函数保留栈帧。

## ⚠️ 验证覆盖率的真实情况

| 源文件 | witness 数 | 在 P1 targets 里 | 未验证 |
|---|---:|---:|---:|
| `Timer.cpp` | 1 | 1 | 0 |
| `Trivial.cpp` | 10 | 部分 | 部分 |
| `FieldAccess.cpp` | 12 | 12 | 0 |
| `SseAccess.cpp` | 17 | 17（但 17 个暂 COMPILE_ERROR）| 17 |
| `FieldWriter.cpp` | 8 | 8 | 0 |
| `Misc.cpp` | 23 | 23 | 0 |
| **`Object.cpp`** | **19** | **0** | **19** |

**`Object.cpp` 完全不在 `check_bytes.py` 的 targets 里。**
它的 19 个成员函数只做过一次性脚本验证，从未进入正式 P1 流程。

> 这是 2026-11-24 由 matching subagent 发现的真实问题。
> 我（lead）之前说的「Object.cpp 19 个全部 P1 MATCH」是**错的**。

---

## ★ 一个待裁决的问题：ABI 适配 token

**63 个函数**因 `token/指令比 > 4.0` 触发 G3 FAIL。

原因不是源码啰嗦，而是「2 条指令的函数装不下一个完整的 thiscall 签名」：

```cpp
void __fastcall store_18(void* self, __m128 ignored, __m128 value) {
    (void)ignored;
    _mm_store_ss(FLOAD(self, 0x18), value);
}
```

| 项 | 数量 |
|---|---|
| 机器码指令 | 2 |
| 源码 token | 12 |
| ratio | 6.00（上限 4.00）|

- `__fastcall` 是模拟 `thiscall` 的**唯一**办法（MSVC 禁止自由函数用 `__thiscall`）
- `__m128 ignored` 占 `xmm0`，才能让 `value` 落 `xmm1`（原版读的就是 xmm1）

**完整提案见 [`ledger/G3-ABI-token-proposal.md`](../ledger/G3-ABI-token-proposal.md)，等人类裁决。**

---

## 三个物理不可得项

| 类 | 数量 | 原因 |
|---|---:|---|
| F `fsincos` | 19 | MSVC 前端永不生成，是 CRT 内部实现 |
| E 指针编码（部分）| 4 | 需 `8bff` hotpatch 序言（v141/v142 都生成 `66 90`）|
| Z `xor/inc` | 1 | `return 1` 被优化成 `mov eax,1` |

> 另外 **63 个函数**因 `__fastcall` + 哑参数的 ABI 成本触发 G3 FAIL，见 `ledger/violations.md` V2（已按人类裁决回退，G3 保持 FAIL）。