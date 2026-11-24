# TH185 已匹配函数清单（按类别）

> 更新于 2026-11-24（第 6 批批量匹配后）· 数据来源：`tools/check_bytes.py` 当次输出 + 二进制实测
>
> **本文件的每个地址只出现一次。** 各分类之和 = 194，与 `check_bytes.py` 的 `targets` 数量**精确相等**。

## 真实数字（`tools/check_bytes.py` 原始输出）

```
targets : 194（由 //@witness 自动推导）
结果: 169 / 194 字节相等
```

| 项 | 数量 |
|---|---:|
| 有源码的函数（`//@witness` 自动推导的 targets） | **194** |
| P1 逐字节通过 | **169** |
| P1 未通过 | **25**（全部已知物理不可得，见末节） |
| 二进制可枚举函数总数（G2 口径：直接 call 目标） | 1675 |
| 覆盖率（194 / 1675） | **11.58%** |

> 第 6 批新增 **17** 个函数（`src/Batch6.cpp`），**全部通过**。
> 25 个 DIFF 的地址集合与第 5 批**完全相同** --- 无新增失败、无既有匹配退化。
> P1 从 `152 / 177`（85.9%）提升到 `169 / 194`（87.1%）。

## 类别总览

> 本表的分类是**从二进制机器码自动推导**的（判据见每节标题），不是手工归类。

| 类 | 名称 | 数量 | P1 通过 | P1 失败 |
|---|---|---:|---:|---:|
| A | 空函数体 / 返回 this | **15** | 15 | 0 |
| B | 返回零 | **4** | 4 | 0 |
| C1 | SSE 标量读取 / 运算 | **14** | 14 | 0 |
| C2 | SSE 标量写入 | **16** | 16 | 0 |
| D | 12 字节聚合拷贝（SSE2 `movq`） | **10** | 10 | 0 |
| G | 有分支的推进 / 查询 | **17** | 17 | 0 |
| H | 字段读取器 / 返回常量 | **51** | 51 | 0 |
| I | 字段地址取用（返回指针） | **6** | 6 | 0 |
| J | 带栈帧的简单写入器 | **22** | 22 | 0 |
| K | 含 call 的函数 | **14** | 14 | 0 |
| Z | 其他 --- 未通过 | **1** | 0 | 1 |
| E | 指针编码（`8bff` 序言）--- 全部未通过 | **5** | 0 | 5 |
| F | `fsincos` 极坐标 --- 全部未通过 | **19** | 0 | 19 |
| | **合计** | **194** | **169** | **25** |

---

## 第 6 批新增的 17 个函数（全部 P1 MATCH）

源码 `src/Batch6.cpp`，结构体 `include/Batch6.hpp`。

| 地址 | 类 | 字节 | 函数 |
|---|---|---:|---|
| `0x0041C7B0` | G | 37 | `InputMaskQuery::any_of_first_pair` |
| `0x00431570` | G | 37 | `InputMaskQueryAlternate::any_of_second_pair` |
| `0x0048AE00` | C1 | 44 | `ResourceProbe44::read` |
| `0x0048AE30` | C1 | 44 | `ResourceProbe54::read` |
| `0x0048AE60` | C1 | 44 | `ResourceProbe58::read` |
| `0x00409C40` | G | 94 | `CounterB4::advance_capped` |
| `0x00417940` | G | 97 | `Counter9C::advance_capped` |
| `0x004306A0` | G | 97 | `CounterAC::advance_capped` |
| `0x004410F0` | G | 97 | `CounterA4::advance_capped` |
| `0x00447D80` | G | 97 | `CounterA8::advance_capped` |
| `0x00447DF0` | G | 97 | `CounterA0::advance_capped` |
| `0x00447E60` | G | 94 | `Counter90::advance_capped` |
| `0x00447EC0` | G | 94 | `Counter98::advance_capped` |
| `0x00447F20` | G | 97 | `Counter94::advance_capped` |
| `0x00447F90` | G | 97 | `Counter8C::advance_capped` |
| `0x00448000` | G | 97 | `Counter88::advance_capped` |
| `0x00463D90` | G | 94 | `CounterB0::advance_capped` |

### 三类形状

| 形状族 | 数量 | 说明 |
|---|---:|---|
| 分桶计数推进器 `advance_capped` | 12 | 上限钳制 + 负数归零 + 满 100 进一 |
| 全局掩码查询 `any_of_*_pair` | 2 | 两个全局掩码量的 OR |
| 资源浮点查询 `read` | 3 | find 命中读 +0x44/+0x54/+0x58，未命中读兜底 |

### 本批新确立 / 复用的代码生成规则

1. **`constexpr` 常量不能放 hpp --- 若该 hpp 里有成员函数，会 C2374 重定义。**
   `check_bytes.py` 拼翻译单元时对成员函数会做两件事：
   (a) 把 hpp 里所有 `constexpr` / `static const` 行抽进 prefix；
   (b) 又把整个 hpp 的声明行拼进去。常量因此出现两次。
   非成员函数只走 (a)，所以历史文件的 hpp 常量一直没暴露这个问题。
   **修法**：常量放进 `.cpp` 的匿名 `namespace { ... }`（只被抽一次）。
2. **hpp 里的 `struct` 必须放在 `namespace th185` 内，`extern` 全局量必须放在 namespace 外。**
   `check_bytes.py` 用 `hpp_namespace_chain()` 把 `.cpp` 的函数定义包回 hpp 的 namespace；
   若 struct 在全局而定义在 `th185` 里，clang 视为两个不同类型，G4 报
   `cannot define or redeclare ... because namespace 'th185'`。
   本批第一次 G4 FAIL 就是这一条。
3. **返回 `int` 与 `bool` 走不同路径**（STATUS.md 3.2 规则 3 复用）：
   掩码查询写 `bool` 得 `b001`/`32c0`，写 `int` 才是原版的 `b801000000`/`33c0`。
4. **返回 `__m128` 而非 `float`；且必须「静态成员 + `__fastcall` 单参」**（STATUS.md 3.3 复用）：
   `float` 走 x87 `fld`；普通成员函数返回 `__m128` 走 sret（隐藏返回指针）。
5. **绝对地址操作数（`test dword [0x4CE400],eax`）的见证类型是 `disp` 而不是 `imm`。**
   方括号内的位移与方括号外的立即数在 `check_witness.py` 里分属两个集合，
   写错会导致 G2 `witness_not_found`。本批第一次 G2 FAIL 就是这一条（4 处）。
6. **「赋值表达式作返回值」与「先算 `int next` 再赋值返回」生成同样字节**：
   两种写法实测都是 `lea eax,[edi+1] / cmp ecx,0x63 / cmovne eax,edi / mov [esi+OFF],eax`。
   说明这是 `/O2` 对该模式的稳定产物，不依赖具体写法。

### 本批的负面结果（不写入 src/）

**441 个无源码函数带 `8bff`（`mov edi,edi`）前缀，v142 无法生成。**
本批对全部 1498 个无源码函数做了前缀统计：

| 范围 | 总数 | 带 `8bff` | 可匹配池 |
|---|---:|---:|---:|
| 全部无源码函数 | 1498 | 441 (29.4%) | 1057 |
| **13-30 指令区间** | 415 | 168 (40.5%) | 247 |
| 13-30 且无分支 | 116 | 47 (40.5%) | 69 |

实测 9 种 flags 组合（`/O1 /O2 /Od` x `/Oy-` x `/hotpatch` x `/Gy` x `/Zi`）：
`/hotpatch` 一律生成 `6690`，**`8bff` 命中 0 次**。与第 4/5 批登记的 E 类结论一致。
**这些函数不写入 `src/`**（宁可 unmatched，不可 garbage match）。

---

## A. 空函数体 / 返回 this（15 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404AA0` | OK | 3 | `8bc1c3` | Trivial.cpp |
| `0x00404C20` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00417AF0` | OK | 3 | `8bc1c3` | Trivial.cpp |
| `0x00441640` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00448100` | OK | 3 | `c20000` | Trivial.cpp |
| `0x0045CA20` | OK | 3 | `c20000` | Trivial.cpp |
| `0x004717D0` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00473800` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00478740` | OK | 3 | `c20000` | Trivial.cpp |
| `0x0047A210` | OK | 3 | `c20000` | Trivial.cpp |
| `0x0048AFA0` | OK | 3 | `c20000` | Trivial.cpp |
| `0x0048C910` | OK | 3 | `c20000` | Trivial.cpp |
| `0x0048C920` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00490592` | OK | 3 | `c20000` | Trivial.cpp |
| `0x00490595` | OK | 3 | `c20000` | Trivial.cpp |

---

## B. 返回零（4 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x004903B9` | OK | 3 | `33c0c3` | Trivial.cpp |
| `0x00490553` | OK | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055C` | OK | 3 | `33c0c3` | Trivial.cpp |
| `0x0049055F` | OK | 3 | `33c0c3` | Trivial.cpp |

---

## C1. SSE 标量读取 / 运算（14 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404B60` | OK | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00405A60` | OK | 6 | `f30f104144c3` | FieldAccess.cpp |
| `0x0041C710` | OK | 11 | `f30f104174f30f594154c3` | FieldAccess.cpp |
| `0x00429A40` | OK | 6 | `f30f104118c3` | SseAccess.cpp |
| `0x00441240` | OK | 6 | `f30f104128c3` | SseAccess.cpp |
| `0x00441250` | OK | 6 | `f30f10412cc3` | SseAccess.cpp |
| `0x00441330` | OK | 6 | `f30f104120c3` | SseAccess.cpp |
| `0x00441340` | OK | 6 | `f30f104124c3` | SseAccess.cpp |
| `0x00441600` | OK | 6 | `f30f104104c3` | SseAccess.cpp |
| `0x00441610` | OK | 5 | `f30f1001c3` | Trivial.cpp |
| `0x00441620` | OK | 11 | `f30f104178f30f594158c3` | Misc.cpp |
| `0x0048AE00` | OK | 44 | `568bf1578b3d889e52008bcfff36e88df6ffff85c0750d8906f30f1087280100005f5ec3f30f1040445f5ec3` | Batch6.cpp |
| `0x0048AE30` | OK | 44 | `568bf1578b3d889e52008bcfff36e85df6ffff85c0750d8906f30f1087380100005f5ec3f30f1040545f5ec3` | Batch6.cpp |
| `0x0048AE60` | OK | 44 | `568bf1578b3d889e52008bcfff36e82df6ffff85c0750d8906f30f10873c0100005f5ec3f30f1040585f5ec3` | Batch6.cpp |

---

## C2. SSE 标量写入（16 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00417780` | OK | 6 | `f30f114918c3` | SseAccess.cpp |
| `0x004299F0` | OK | 6 | `f30f114924c3` | SseAccess.cpp |
| `0x00429A00` | OK | 6 | `f30f114920c3` | SseAccess.cpp |
| `0x00429B90` | OK | 11 | `f30f114948f30f11514cc3` | Misc.cpp |
| `0x00433820` | OK | 11 | `a1e07a4d00f30f114844c3` | Misc.cpp |
| `0x00433C60` | OK | 17 | `f30f1189c4000000f30f1191c8000000c3` | MoreFields.cpp |
| `0x004341D0` | OK | 6 | `f30f11496cc3` | SseAccess.cpp |
| `0x004341E0` | OK | 6 | `f30f114974c3` | SseAccess.cpp |
| `0x004341F0` | OK | 6 | `f30f114978c3` | SseAccess.cpp |
| `0x00434270` | OK | 9 | `f30f1189b4070000c3` | MoreFields2.cpp |
| `0x00441270` | OK | 6 | `f30f11492cc3` | SseAccess.cpp |
| `0x00441310` | OK | 6 | `f30f114938c3` | SseAccess.cpp |
| `0x00441320` | OK | 6 | `f30f114934c3` | SseAccess.cpp |
| `0x004415E0` | OK | 6 | `f30f114904c3` | SseAccess.cpp |
| `0x004415F0` | OK | 5 | `f30f1109c3` | SseAccess.cpp |
| `0x0048AC40` | OK | 13 | `83893405000004f30f114944c3` | Misc.cpp |

---

## D. 12 字节聚合拷贝（SSE2 `movq`）（10 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00405830` | OK | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x004063C0` | OK | 31 | `558bec8b4508f30f7e00660fd681f00500008b40088981f80500005dc20400` | FieldAccess.cpp |
| `0x00417730` | OK | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |
| `0x00429A20` | OK | 25 | `558bec8b4508f30f7e00660fd641348b400889413c5dc20400` | MoreFields.cpp |
| `0x00434250` | OK | 31 | `558bec8b4508f30f7e00660fd681a40700008b40088981ac0700005dc20400` | MoreFields.cpp |
| `0x00440BA0` | OK | 25 | `558bec8b4508f30f7e00660fd641308b40088941385dc20400` | MoreFields.cpp |
| `0x00440BC0` | OK | 25 | `558bec8b4508f30f7e00660fd641248b400889412c5dc20400` | MoreFields.cpp |
| `0x00440BE0` | OK | 25 | `558bec8b4508f30f7e00660fd641188b40088941205dc20400` | MoreFields.cpp |
| `0x00440C00` | OK | 25 | `558bec8b4508f30f7e00660fd6410c8b40088941145dc20400` | MoreFields.cpp |
| `0x00441650` | OK | 24 | `558bec8b4508f30f7e00660fd6018b40088941085dc20400` | Trivial.cpp |

---

## G. 有分支的推进 / 查询（17 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00402A50` | OK | 68 | `8b918c00000083ea0189918c000000791bc7818c00000000000000c70100000000c781d400000000000000c38b44910c89018b44914c894108c781d400000000000000c3` | Timer.cpp |
| `0x00409C40` | OK | 94 | `568bf1578bbeb400000083ff647e125fc786b400000064000000b8640000005ec385ff790f5fc786b40000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986b40000005ec3` | Batch6.cpp |
| `0x00417940` | OK | 97 | `568bf1578bbe9c00000081ffe70300007e125fc7869c000000e7030000b8e70300005ec385ff790f5fc7869c0000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f89869c0000005ec3` | Batch6.cpp |
| `0x0041C7B0` | OK | 37 | `558bec8b4508850500e44c00750e8505fce34c00750633c05dc20400b8010000005dc20400` | Batch6.cpp |
| `0x004306A0` | OK | 97 | `568bf1578bbeac00000081ffe70300007e125fc786ac000000e7030000b8e70300005ec385ff790f5fc786ac0000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986ac0000005ec3` | Batch6.cpp |
| `0x00431570` | OK | 37 | `558bec8b4508850500e44c00750e8505f8e34c00750633c05dc20400b8010000005dc20400` | Batch6.cpp |
| `0x004410D0` | OK | 13 | `8b412083f80f7d0440894120c3` | MoreFields.cpp |
| `0x004410F0` | OK | 97 | `568bf1578bbea400000081ffe70300007e125fc786a4000000e7030000b8e70300005ec385ff790f5fc786a40000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986a40000005ec3` | Batch6.cpp |
| `0x00447D80` | OK | 97 | `568bf1578bbea800000081ffe70300007e125fc786a8000000e7030000b8e70300005ec385ff790f5fc786a80000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986a80000005ec3` | Batch6.cpp |
| `0x00447DF0` | OK | 97 | `568bf1578bbea000000081ffe70300007e125fc786a0000000e7030000b8e70300005ec385ff790f5fc786a00000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986a00000005ec3` | Batch6.cpp |
| `0x00447E60` | OK | 94 | `568bf1578bbe9000000083ff647e125fc7869000000064000000b8640000005ec385ff790f5fc786900000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986900000005ec3` | Batch6.cpp |
| `0x00447EC0` | OK | 94 | `568bf1578bbe9800000083ff647e125fc7869800000064000000b8640000005ec385ff790f5fc786980000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986980000005ec3` | Batch6.cpp |
| `0x00447F20` | OK | 97 | `568bf1578bbe9400000081ffe70300007e125fc78694000000e7030000b8e70300005ec385ff790f5fc786940000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986940000005ec3` | Batch6.cpp |
| `0x00447F90` | OK | 97 | `568bf1578bbe8c00000081ffe70300007e125fc7868c000000e7030000b8e70300005ec385ff790f5fc7868c0000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f89868c0000005ec3` | Batch6.cpp |
| `0x00448000` | OK | 97 | `568bf1578bbe8800000081ffe70300007e125fc78688000000e7030000b8e70300005ec385ff790f5fc786880000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986880000005ec3` | Batch6.cpp |
| `0x00463D90` | OK | 94 | `568bf1578bbeb000000083ff017e125fc786b000000001000000b8010000005ec385ff790f5fc786b00000000000000033c05ec3b81f85eb518bcff7efc1fa058bc2c1e81f03c26bc0642bc88d470183f9630f45c75f8986b00000005ec3` | Batch6.cpp |
| `0x0049C51C` | OK | 20 | `8b41086683382a750a83c002c6411801894108c3` | MoreFields.cpp |

---

## H. 字段读取器 / 返回常量（51 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404BD0` | OK | 6 | `b8689c5700c3` | Trivial.cpp |
| `0x00404D10` | OK | 10 | `558bec8b45085dc20c00` | FieldAccess.cpp |
| `0x00406730` | OK | 4 | `8b4124c3` | Object.cpp |
| `0x00407E40` | OK | 12 | `33c03981a00200000f95c0c3` | FieldAccess.cpp |
| `0x0040A050` | OK | 9 | `c701000000008bc1c3` | FieldAccess.cpp |
| `0x0040A0E0` | OK | 7 | `8b81ac7f0400c3` | Object.cpp |
| `0x004176D0` | OK | 4 | `8b4120c3` | Object.cpp |
| `0x00417770` | OK | 4 | `8b4134c3` | Object.cpp |
| `0x0041FA20` | OK | 3 | `8b01c3` | Trivial.cpp |
| `0x0041FA30` | OK | 4 | `8b411cc3` | Object.cpp |
| `0x00424480` | OK | 8 | `c7412800000000c3` | Object.cpp |
| `0x00429A10` | OK | 13 | `8b414083e0f283c802894140c3` | Misc.cpp |
| `0x00429AB0` | OK | 5 | `836140f0c3` | Trivial.cpp |
| `0x00430710` | OK | 9 | `f30f110de00f4d00c3` | MoreFields2.cpp |
| `0x00431260` | OK | 7 | `8b8160920100c3` | Object.cpp |
| `0x00431270` | OK | 15 | `8b490833c080e13080f9200f94c0c3` | MoreFields.cpp |
| `0x00431300` | OK | 4 | `8b4150c3` | Object.cpp |
| `0x00433830` | OK | 21 | `558beca1147c4d008b4d0889882cbbe6005dc20400` | MoreFields2.cpp |
| `0x00433850` | OK | 12 | `a1147c4d008b802cbbe600c3` | Misc.cpp |
| `0x00433B00` | OK | 16 | `558bec8a45088881301000005dc20400` | FieldWriter.cpp |
| `0x00433B40` | OK | 3 | `8b01c3` | Trivial.cpp |
| `0x00433B90` | OK | 4 | `8b4104c3` | MoreFields2.cpp |
| `0x00433DE0` | OK | 11 | `81896863000000000002c3` | Misc.cpp |
| `0x00434060` | OK | 21 | `558beca1f47a4d008b4d0889888c0000005dc20400` | MoreFields2.cpp |
| `0x004340F0` | OK | 18 | `a1087b4d008b80a00200008b80ac010000c3` | Misc.cpp |
| `0x004407F0` | OK | 8 | `c7413c00000000c3` | Misc.cpp |
| `0x00440850` | OK | 20 | `a1983e57008b159c3e570089411889511cc20400` | MoreFields.cpp |
| `0x00440870` | OK | 20 | `a1983e57008b159c3e5700894110895114c20400` | MoreFields.cpp |
| `0x00440B40` | OK | 8 | `c7415000000000c3` | Misc.cpp |
| `0x004410E0` | OK | 13 | `c781a400000000000000c20400` | MoreFields2.cpp |
| `0x00441210` | OK | 4 | `8b413cc3` | Object.cpp |
| `0x00441220` | OK | 16 | `558bec8b45080181dc0000005dc20400` | MoreFields.cpp |
| `0x00441230` | OK | 16 | `558bec8b45080181d80000005dc20400` | MoreFields.cpp |
| `0x00441260` | OK | 13 | `8b414083e0f383c803894140c3` | Misc.cpp |
| `0x00441630` | OK | 15 | `8b494033c080e10f80f9020f94c0c3` | MoreFields.cpp |
| `0x0045C9E0` | OK | 6 | `b8709c5700c3` | Misc.cpp |
| `0x0048AEC0` | OK | 16 | `8b491ca1889e52008b84882c071203c3` | MoreFields.cpp |
| `0x0049025C` | OK | 12 | `33c03905a8a357000f95c0c3` | Misc.cpp |
| `0x00490556` | OK | 6 | `b800400000c3` | Misc.cpp |
| `0x0049056E` | OK | 3 | `b001c3` | Misc.cpp |
| `0x004905B5` | OK | 12 | `33c03905608a4c000f94c0c3` | Misc.cpp |
| `0x004905C1` | OK | 6 | `b8a4a35700c3` | Misc.cpp |
| `0x004905C7` | OK | 6 | `b8a0a35700c3` | Misc.cpp |
| `0x00492832` | OK | 14 | `558bec8b45088b4d108948085dc3` | MoreFields2.cpp |
| `0x0049EBDE` | OK | 6 | `a1f8744d00c3` | Misc.cpp |
| `0x0049EEBB` | OK | 6 | `b80c754d00c3` | Misc.cpp |
| `0x004A1726` | OK | 6 | `b818784d00c3` | Misc.cpp |
| `0x004A172C` | OK | 6 | `b81c784d00c3` | Misc.cpp |
| `0x004A1732` | OK | 6 | `b814784d00c3` | Misc.cpp |
| `0x004A1738` | OK | 6 | `a120784d00c3` | Misc.cpp |
| `0x004A173E` | OK | 6 | `a124784d00c3` | Misc.cpp |

---

## I. 字段地址取用（返回指针）（6 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00407E30` | OK | 7 | `8d8120060000c3` | Object.cpp |
| `0x0040A090` | OK | 7 | `8d8174120000c3` | Object.cpp |
| `0x00429A50` | OK | 4 | `8d411cc3` | Object.cpp |
| `0x00433DC0` | OK | 4 | `8d4144c3` | Object.cpp |
| `0x00433DD0` | OK | 7 | `8d81f44f0000c3` | Object.cpp |
| `0x00433DF0` | OK | 7 | `8d8130120000c3` | Object.cpp |

---

## J. 带栈帧的简单写入器（22 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00406720` | OK | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00409F60` | OK | 13 | `558bec8b45088941205dc20400` | FieldAccess.cpp |
| `0x004176E0` | OK | 13 | `558bec8b45088941345dc20400` | FieldWriter.cpp |
| `0x00428090` | OK | 13 | `558bec8b45088941285dc20400` | FieldWriter.cpp |
| `0x004280A0` | OK | 15 | `558bec8b45088b008941045dc20400` | MoreFields.cpp |
| `0x004280B0` | OK | 14 | `558bec8b45088b0089015dc20400` | MoreFields.cpp |
| `0x004280C0` | OK | 13 | `558bec8b450889412c5dc20400` | FieldWriter.cpp |
| `0x00430590` | OK | 15 | `558bec8b45088b0089410c5dc20400` | MoreFields.cpp |
| `0x004305A0` | OK | 15 | `558bec8b45088b008941085dc20400` | MoreFields.cpp |
| `0x00433B70` | OK | 19 | `558bec8b411833450883e0013141185dc20400` | MoreFields.cpp |
| `0x00433BA0` | OK | 13 | `558bec8b45088941045dc20400` | Object.cpp |
| `0x00433BB0` | OK | 13 | `558bec8b45088941085dc20400` | Object.cpp |
| `0x00433BC0` | OK | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x00433BD0` | OK | 16 | `558bec8b45088b550c8914815dc20800` | MoreFields2.cpp |
| `0x00433BE0` | OK | 17 | `558bec8b45088b550c895481545dc20800` | MoreFields.cpp |
| `0x00433C80` | OK | 12 | `558bec8b450889015dc20400` | Trivial.cpp |
| `0x004340A0` | OK | 16 | `558bec8b45088981780200005dc20400` | FieldWriter.cpp |
| `0x00440840` | OK | 13 | `558bec8b450889413c5dc20400` | Object.cpp |
| `0x004408B0` | OK | 20 | `558bec8b45088b108b400489118941045dc20400` | MoreFields.cpp |
| `0x004408D0` | OK | 13 | `558bec8b45088941405dc20400` | MoreFields.cpp |
| `0x00440B90` | OK | 13 | `558bec8b45088941505dc20400` | MoreFields.cpp |
| `0x004410C0` | OK | 13 | `558bec8b45088941245dc20400` | MoreFields.cpp |

---

## K. 含 call 的函数（14 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00429BF0` | OK | 23 | `5668600400008bf16a0056e86077060083c40c8bc65ec3` | MoreFields2.cpp |
| `0x00434200` | OK | 23 | `5668600400008bf16a0056e850d1050083c40c8bc65ec3` | MoreFields2.cpp |
| `0x00434280` | OK | 23 | `56685c0400008bf16a0056e8d0d0050083c40c8bc65ec3` | MoreFields2.cpp |
| `0x0048F711` | OK | 14 | `558becff7508e863090000595dc3` | FieldAccess.cpp |
| `0x0049DEDF` | OK | 15 | `6a016a026a00e8aafeffff83c40cc3` | MoreFields2.cpp |
| `0x0049DEEE` | OK | 15 | `6a016a006a00e89bfeffff83c40cc3` | MoreFields2.cpp |
| `0x0049F0F1` | OK | 19 | `e83a0b000085c07506b8988a4c00c383c014c3` | MoreFields2.cpp |
| `0x004A0C5F` | OK | 26 | `6824214b006820214b006824214b006a00e81901000083c410c3` | MoreFields2.cpp |
| `0x004A0C79` | OK | 26 | `683c214b006834214b00683c214b006a01e8ff00000083c410c3` | MoreFields2.cpp |
| `0x004A0C93` | OK | 26 | `687c214b006874214b00687c214b006a14e8e500000083c410c3` | MoreFields2.cpp |
| `0x004A0CAD` | OK | 26 | `6894214b00688c214b006894214b006a16e8cb00000083c410c3` | MoreFields2.cpp |
| `0x004A23A7` | OK | 9 | `6a06e88501000059c3` | MoreFields2.cpp |
| `0x004A417D` | OK | 9 | `6a01e8a7feffff59c3` | MoreFields2.cpp |
| `0x004A6B75` | OK | 9 | `6a08e8b7b9ffff59c3` | MoreFields2.cpp |

---

## Z. 其他 --- 未通过（1 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00490463` | DIFF | 8 | `8325f4724d0000c3` | Misc.cpp |

---

## E. 指针编码（`8bff` 序言）--- 全部未通过（5 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x0049DC88` | DIFF | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E757` | DIFF | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049E776` | DIFF | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EB13` | DIFF | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |
| `0x0049EBF3` | DIFF | 31 | `8bff558beca14c8a4c0083e01f6a20592bc88b4508d3c833054c8a4c005dc3` | Trivial.cpp |

---

## F. `fsincos` 极坐标 --- 全部未通过（19 个）

| 地址 | 状态 | 字节 | 机器码 | 源文件 |
|---|---|---:|---|---|
| `0x00404B70` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00405A70` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00406290` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00407E10` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00417750` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00417B00` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004299D0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004305B0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00430720` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00441580` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00` | Trivial.cpp |
| `0x00441670` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0044CDA0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x004587B0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00463DF0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00467150` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0047FFE0` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00485B20` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x00486080` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |
| `0x0048F370` | DIFF | 32 | `558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800` | Trivial.cpp |

---

## 25 个未通过项的定性

| 类别 | 数量 | 性质 |
|---|---:|---|
| `fsincos`（F 类） | 19 | x87 复合指令只在 CRT 内部手写使用，MSVC 前端从不内联成 `fsincos` |
| `8bff` 序言（E 类，cookie 编码） | 5 | v142 的 `/hotpatch` 生成 `6690`，从不出 `8bff`；实测 24 种组合 0 命中 |
| `and [mem],0`（Z 类） | 1 | 现代 MSVC 对 `g = 0` 生成 `mov [mem],0`，不生成 `and [mem],0` |

**这 25 个跨 v141/v142 穷举确认不可得，不是技术问题，也不是还没做。**

## 验证覆盖率的真实情况

- `check_bytes.py`（P1）：**169 / 194**，25 个 DIFF 全部物理不可得。
- `check_behavior.py`（G2+）：51 / 51 PASS（用 unicorn 执行**原版机器码**，与 Python 独立重算比对）。
- 注意：**这两个数字不同量纲** --- G2+ 只证明语义，P1 才证明字节。

