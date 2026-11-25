# Unresolved 清单（AGENTS.md §8 第 4 条）

格式：每个未匹配函数一条，必须写残差 diff 与假设。禁止留空。

## 单函数未匹配（25 个，全部已定性为物理不可得）

> 2026-11-24 第 4 批后更新。这 25 个不是「还没做」，是**已穷举确认生成不出来**。
> 每次 P1 的 DIFF 地址集合都恰好等于这张表 —— 若将来多出任何地址，说明是新问题。

### 类别 1：`fsincos`（19 个）— 不可得

`0x00404B70 0x00405A70 0x00406290 0x00407E10 0x00417750 0x00417B00 0x004299D0
0x004305B0 0x00430720 0x00441580 0x00441670 0x0044CDA0 0x004587B0 0x00463DF0
0x00467150 0x0047FFE0 0x00485B20 0x00486080 0x0048F370`

| 项 | 内容 |
|---|---|
| 残差 | 原版 32 字节含 `d9fb`（`fsincos`）；我们 `call sinf/cosf` 序列 67 字节，公共前缀 3 字节（`558bec`）|
| 假设 | 原版静态链接了 x87 版 CRT，`fsincos` 由 CRT 内部手写汇编产生 |
| 已穷举 | v141 / v142 × 60 种 flags 组合（含 `/arch:IA32`），`d9fb` **一次未出现** |
| 为何不可得 | MSVC 前端只生成 `call`，从不把 sin+cos 合成一条 `fsincos` |
| 是否可用 `__asm` | ❌ 违反 AGENTS.md §6 G4 inline-asm 禁令，未采用 |

证据：`evidence/P9-fsincos-definitive.json`、`evidence/shapes-fsincos-and-cookie.json`

### 类别 2：hotpatch 序言 `8bff`（5 个）— 不可得

`0x0049DC88 0x0049E757 0x0049E776 0x0049EB13 0x0049EBF3`

| 项 | 内容 |
|---|---|
| 残差 | 原版 31 字节以 `8bff`（`mov edi,edi`）开头；v142 生成 `66 90`（双字节 nop），公共前缀 0 |
| 假设 | 原版用 `/hotpatch`，而 v142 的 `/hotpatch` 选用了 `66 90` 而非 `8bff` |
| 已穷举 | `/Od` `/O1` `/O2` ×`/hotpatch` 等 45 种组合 —— `8bff` 一次未出现 |
| 备注 | 另有 `6a20 59`（`push 0x20 / pop ecx`）也无法在 v142 下同时满足 |

证据：`evidence/P4-hotpatch-and-encoderemain.json`

### 类别 4：hotpatch 序言 `8bff` 的第二批确认（4 个同族，2026-11-24 第 5 批复测）

`0x0049D72A 0x004A7C50 0x0049DEFD 0x0049DF39`

这四个是第 5 批匹配函数的**同族成员**：机器码形状完全一致，只多一个 `8bff`（`mov edi,edi`）前缀。

| flags 组合（24 种）| 以 `8bff` 开头 | 以 `6690` 开头 |
|---|---:|---:|
| `/O1 /O2 /Od` × `/Oy- /Oy` × `/hotpatch` × `/Gy` | **0** | **8**（全部 `/hotpatch` 组合）|

**这是对类别 2 的独立复现** —— 换了一批函数、换了一套 flags 枚举，结论一致：
v142 的 `/hotpatch` 选 `66 90` 而非 `8bff`。

处置：**不写入 src/**（宁可 unmatched，不可 garbage match）。

### 类别 3：`and dword [mem], 0`（1 个）— 不可得

`0x00490463`

| 项 | 内容 |
|---|---|
| 残差 | 原版 8 字节 `8325 f4724d00 00 c3`；我们 11 字节 `c705 00000000 00000000 c3` |
| 假设 | 原版编译器选了 `and [mem],0`（8 字节）这条更短的编码 |
| 已穷举 | 45 种写法/选项组合，v142 一律生成 `mov [mem],0` |

证据：`evidence/P4-hotpatch-and-encoderemain.json`

## 局部 unresolved（函数已通门槛，但语义未定）

### 0x00402A50 ≠ 字段布局

- **状态**：Phase A 语义重建完成，G1/G2/G3/G4 全过
- **已确定**：递减 [ecx+0x8C]；归负时清 [ecx+0x8C] / [ecx+0x00] / [ecx+0xD4]
- **未确定**：这三个偏移之间的 0x04..0x8B、0x90..0xD3 是什么
- **处置**：不定义 struct（避免 pad 无见证），用具名偏移常量 + void*
- **解除条件**：等更多函数确认这些偏移之间的成员后，在 /include 定义正式 struct 并锁进 lock.json

## 已确定的二进制事实（不是 unresolved，是证据）

### 9 个虚函数槽（call [ecx+N]）

从 592 个 `[ecx+N]` 偏移里，按访问方式筛出 9 个**按虚表槽调用**的：

| 偏移 | 总访问 | call_indirect | 说明 |
|---|---:|---:|---|
| `0xE4` | 162 | **156** | 最热的虚函数槽 |
| `0x10C` | 87 | 86 | |
| `0x48` | 36 | 19 | 还有 float 读写，可能是带数据的槽 |
| `0x164` | 22 | 17 | |
| `0x14C` | 16 | 10 | |
| `0xAC` | 15 | 8 | |
| `0x114` | 14 | 11 | |
| `0x94` | 9 | 5 | |
| `0x104` | 5 | 3 | |

证据文件：`evidence/offsets-ecx.json`（592 个偏移的完整统计）

**注意**：这些是「某个类」的槽偏移，但**不同类的同偏移会被合并统计**。
要确定具体类的布局，必须限定在单个函数范围内（G2 的 `facts_for_range` 就是这么做的）。

### 语义类别分布（121 个 >=5 次访问的偏移）

```
read_write    82 个    ← 普通可读写字段
float_field   23 个    ← 浮点字段（movss/addss 等）
read_only     23 个
write_only    11 个
vtable_slot    9 个    ← 见上表
bit_flags      5 个    ← and/or/test 为主
state_value    4 个    ← cmp 为主
```

### 5 个位标志字

判据：`and`/`or`/`xor`/`test` 占比 >= 30% 且 >= 5 次。
这类偏移是**状态机的标志位**，典型用法 `and eax, 0xNN` 后 `test`。

### 4 个状态值

判据：`cmp` 占比 >= 60% 且 >= 5 次。
如 `0x544`（122 次 cmp），只被比较、极少写——**这是从别处传入的当前状态**，
用来与自身状态比较做分支。

## 当前全局阻塞（非单函数）

### 阻塞 1（P0）：~~编译器三档 build 并存~~ **已降级为 PARTIALLY_RESOLVED**

> ★ 2026-11-24 修正：27412/31328/31332 是 **MSVC productId**，不是 build 号。
> 此前的「三档 VS2019 并存」判断建立在一个字段读反的 bug 上。
> 真实情况：VS2017 15.x（2712，237 单元）+ VS2019 16.x（3132，170 单元）两个世代。
> 详见 `evidence/rich-header-correction.json`。
>
> 下面保留原始（已被推翻的）记录，不删除。

#### 原始记录（已推翻）

原版 Rich Header 解出三档 C++ 编译器：

```
build 27412 (VS2019 16.0.x)  237 个编译单元
build 31328 (VS2019 16.8)     89 个编译单元
build 31332 (VS2019 16.9)     81 个编译单元
```

本机实测：

```
MSVC found: 14.44.35207 (VS2022 Community)
C:\Program Files (x86)\Microsoft Visual Studio\2019  目录存在但为空
```

**后果**：

- G5 的两个要求（多优化级行为一致 / 扰动后 .text 不变）**都无法执行**
- P1（.text diff=0）**无法验证**

**不采取的规避手段**：用 clang 或 VS2022 冒充。§2 明令禁止为凑匹配改编译器，
且用别的编译器测扰动稳定性，测的是那个编译器而非目标。

**解除条件**：安装 VS2019 且包含 build 27412/31328/31332 对应的 MSVC 工具集；
或人类按 §8 第 5 条裁决改变主目标（§13 允许，但需显式批准）。

### 阻塞 2（P2）：clang-format 不可用 —— 影响 G4

G4 要求「过 clang-format」。全盘搜索本机无独立 clang-format（只有 emsdk 里 SDL 的构建脚本）。
脚本每次运行打印 FORMAT_UNAVAILABLE，不假装通过。

**解除条件**：安装 LLVM（含 clang-format.exe）。

### 阻塞 3（P3）：G6 三项要求 UNSUPPORTED

| 要求 | 原因 |
|---|---|
| RTTI 类名比对 | 原版 /GR- 编译，`.rdata` 里 `.?AV...@@` 0 命中 |
| .pdata 函数边界 | PE32 x86 无 .pdata（那是 x64 异常表） |
| 多反编译器交叉 | 本机无 Ghidra / IDA / retdec，只有 capstone |

**已能做的部分**：vtable 结构提取（117 个 / 2164 槽），
并用「87 个 22 槽 vtable ↔ 87 张卡」做了交叉印证。

### 阻塞 4：G2 反向覆盖率 0 / 1678

不是阻塞，是进度：目前只重建了 1 个函数（0x00402A50），
它的见证全部成立，但整个 .text 有 1678 个 call 目标待解释。
覆盖率会随函数逐个匹配单调上升。
---

## U5：`fsincos` 19 个函数 —— 已穷举确认（2026-11-25 更新）

### 目标字节

```
558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d0cd958048be55dc20800   (ret 8, 2 参数)
558bec51894dfc8b45fcd94508d9fbd84d0cd918d84d10d958048be55dc20c00   (ret 0xC, 3 参数)
```

反汇编：

```asm
push ebp / mov ebp,esp / push ecx / mov [ebp-4],ecx
mov  eax,[ebp-4]
fld  dword [ebp+8]      ; angle
fsincos                 ; ★ st(0)=sin, st(1)=cos
fmul dword [ebp+0xC]    ; sin * radius
fstp dword [eax]        ; self->x
fmul dword [ebp+0xC]    ; cos * radius
fstp dword [eax+4]      ; self->y
```

**这是极坐标→直角坐标转换，`fsincos` 一次算出 sin 和 cos。**

### 本轮新增的穷举（之前只测过版本，这次加了 `/arch`）

| 维度 | 已测 |
|---|---|
| 编译器版本 | v141 / v142 / v143（之前）|
| **`/arch`** | **(默认) / IA32 / SSE / SSE2 / AVX（本轮）** |
| 优化级 | `/Od` `/O1` `/O2` `/Ox` `/Os` |
| 源码形态 | `sinf(a)*r` + `cosf(a)*r`、中间变量、double 版 |

**25 种组合，全部不生成 `d9fb`。**

### 新证据：CRT 库里确实有 `fsincos`

```
libcmt.lib 里 33 处 d9fb
符号表里有 ___libm_sse2_sincos_ / ___libm_sse2_sincosf_ / ___libm_sse2_sincosf4_
```

但它们是 **SSE2 实现**（用 xmm 寄存器 + 多项式逼近），不是 x87 `fsincos`。
显式声明 `__libm_sse2_sincosf_` 并调用，生成的仍是 `call` + `fmov`，不是 `d9fb`。

### 结论

**`fsincos` 是 x87 指令，MSVC 前端从不主动生成它。**

原版那 32 字节只能是以下之一：

| 可能 | 评价 |
|---|---|
| 手写内联汇编 `_asm { fsincos }` | 最可能，但那是汇编不是 C++ |
| 更老的编译器（VC6 / VS2003 的 x87 数学库）| 与已确认的 v142 Rich Header 矛盾 |
| 手工编写的 x87 汇编 stub | 同第一条 |

### 处置

**保持 unmatched，不硬凑。**

这 19 个函数占 P1 分母的 8.8%（19/216），是当前最大的单一失败类别。
但它们**不影响** G1/G3/G4/G2+ 的判定，只影响 P1 的完成度。

如果将来确认原版用了内联汇编，正确做法是在 `src/` 里写 `__asm` 块 ——
但那属于「用汇编写 C++」，需要在 AGENTS.md 层面裁决是否允许。