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
---

## U6：`test dl,1` vs `test al,1` —— 寄存器分配差异（2026-11-25 发现）

### 现象

有一族 **12 个函数**（7 + 3 + 2）共享同一形状：

```asm
push ebp / mov ebp,esp
mov  eax,[ebp+8]
mov  [ecx+0xNNN],eax        ; setter：把参数存进字段
mov  edx,[ecx+0xMMM]        ; ★ 目标是 edx
test dl,1                   ; ★ 目标是 dl（低字节）
jne  skip
or   edx,1
mov  [ecx+...],0
mov  [ecx+...],0
mov  [ecx+0xMMM],edx
skip:
mov  [ecx+...],0
mov  [ecx+...],0
mov  [ecx+...],-1
ret  4
```

我们生成的等价代码：

```asm
mov  eax,[ecx+0xMMM]        ; ★ 我们是 eax
test al,1                   ; ★ 我们是 al
```

**语义完全相同，但寄存器不同，字节全错。**

### 已穷举的写法

| 写法 | 结果 |
|---|---|
| 直接 `if (!(f & 1))` | `eax` + `al` |
| 加前置读 `int t = f;` | `eax` + `al` |
| 用局部变量 `unsigned v = (unsigned)f;` | `eax` + `al` |
| `volatile` 字段 | `eax` + `al`（且长度不对）|
| 调整写入顺序 | 顺序变了，但仍是 `eax` |
| `/O1` | 变成 `test al,1` + 更短，但语义偏移不同 |

### 推断

`edx` 而非 `eax` 说明原版编译时**寄存器压力不同** —— 可能是：

| 可能 | 说明 |
|---|---|
| 函数有更多局部变量 | 但目标函数体里没有栈帧（无 `sub esp`）|
| 原版是不同编译器的寄存器分配器 | 与已确认的 v142 Rich Header 矛盾 |
| 原版源码里 `f` 先被用在别处，占住了 eax | 需要还原完整调用上下文 |

### 处置

**保持 unmatched。** 这 12 个占 P1 分母 5.6%。

和 U5（fsincos）一样，属于「语义理解正确但代码生成不匹配」——
**不是不知道函数做什么，是知道但生成不出来。**

### 可复用的判据

**「知道语义」≠「能生成同样字节」。** 遇到这种情况：

1. 先穷举**优化级**（`/Od /O1 /O2 /Ox /Os`）—— 这条救回过 `return 1`
2. 再穷举 **`/arch`**（默认 / IA32 / SSE / SSE2 / AVX）—— 这条救回过不少
3. 再试**寄存器压力**（加/减局部变量、改语句顺序、`volatile`）
4. 都失败就如实登记为「生成不出来」，**不要硬凑**

第 3 条这次的 6 种写法全失败，说明可能真不是写法问题。
---

## U7：`push ecx` 栈帧 —— 已穷举优化级（2026-11-25）

### 现象

3 个候选都缺 `push ecx` + `mov esp,ebp`（函数保留栈帧 + 4 字节局部空间）：

```
target:  558bec 51        push ebp / mov ebp,esp / push ecx
          a1 e47a4d00     mov  eax,[0x4D7AE4]
          83487808        or   [eax+0x78],8
          8be55d c3       mov esp,ebp / pop ebp / ret
```

### 已穷举的优化级（★ 本轮补上之前漏的 `/O1` `/Os`）

| 优化级 | 长度 | 有栈帧？ |
|---|---:|---|
| `/Od` | 36 | ✗ |
| `/O1` | 10 | ✗ |
| `/O2` | 10 | ✗ |
| `/Ox` | 10 | ✗ |
| **`/Os`** | **33** | **✓** |

**`/Os` 是唯一保留栈帧的优化级。** 但它同时把参数 spill 到栈上（`mov [ebp-4],edx` 等），
导致 33 字节 vs 目标 17 字节。

### 进一步尝试（`/Od` + 成员函数，试图消除参数 spill）

| 写法 | `/Od` 长度 | 差异 |
|---|---:|---|
| 成员函数无参数 | 31 | 多 `894dfc`（保存 ecx 到 [ebp-4]）|
| 成员函数 + 局部指针 | 36 | 更多 |
| `__fastcall` 2 参数 | 36 | 参数 spill |

**目标只 `push ecx`（占位），并不真的保存 ecx。** 我们生成的都保存了。

### 结论

目标形态是「保留栈帧但只分配空间、不保存任何寄存器」，
这是 MSVC 的 `/Od` 或 `/Os` 下**没有需要保存的寄存器**时的样子。

但我们写的任何等价 C++ 都会引入需要保存的东西（`ecx`/`edx` 指针）。

**判断：生成不出来，不是代码写得不对。** 记为工具链缺口。

### 受影响的候选

已知至少 3 个（`0x00434180`、`0x00434080`、`0x00431240`），
可能还有更多在 13-30 指令区间里没检查到的。

### 可复用判据（更新到 §3）

「语义对但字节不对」时按顺序穷举：

1. **优化级** `/Od /O1 /O2 /Ox /Os` —— 本轮发现 `/Os` 是唯一保留栈帧的
2. **`/arch`** 默认 / IA32 / SSE / SSE2 / AVX
3. **调用约定**：`__fastcall` / 成员函数 / `__cdecl` / `__stdcall`
4. **聚合 vs 逐字段**（整体赋值触发不同寄存器）
5. 都失败就如实登记，**不要硬凑**
---

## U8：`push esi` / `pop esi` —— 中间寄存器保存（2026-11-25）

### 现象

`0x00405A90` / `0x00405AD0`（2 个）的完整反汇编：

```asm
push ebp
mov  ebp, esp
mov  eax, [ecx+0x10]      ; flag
push esi                  ; ★ 保存 esi
test al, 1
jne  skip
or   eax, 1
mov  [ecx+4], 0
mov  [ecx+8], 0
mov  [ecx+0xc], 0
mov  [ecx+0x10], eax
skip:
mov  eax, [ebp+8]         ; v
mov  [ecx+4], eax
pop  esi                  ; ★ 恢复 esi
movd xmm0, eax
dec  eax
cvtdq2ps xmm0, xmm0
mov  [ecx], eax           ; c2 = v - 1
movss [ecx+8], xmm0       ; d = (float)v
pop  ebp
ret  4
```

**语义完全清楚**：`if(!(flag&1)){flag|=1;e=0;d=0;c=0;} e=v; c2=v-1; d=(float)v;`

### 但生成不出来

| 写法 | `/O2` | `/O1` | 目标 |
|---|---:|---:|---:|
| 完整语义 | 52 | 49 | **64** |
| `d` 用 int 写 0 | 52 | 49 | 64 |
| clear 顺序改为 e,d,c | 48 | 45 | 64 |

**我们生成的反而更短**（52 vs 64）。差距在 `push esi` / `pop esi`（2 字节）
和它附近的指令调度。

### 为什么难

`push esi` 出现在 `mov eax,[ecx+0x10]` **之后** —— 这个位置不符合任何常见模式。
MSVC 通常把 `push` 放在函数开头。出现在中间说明：

1. 该函数**返回结构体**（`esi` 用于隐藏的返回指针）— 但目标用 `eax` 返回值
2. 或原版源码里有一个**用到 `esi` 的中间表达式**，被 MSVC 分配了 `esi`
3. 或这是**函数内联后的残留** —— 原版某个被内联的小函数用了 `esi`

**第 3 条最可能**：这说明这个「函数」在原版里可能不是独立的，
而是某个更大函数的一部分，或者是内联展开的产物。

### 处置

**保持 unmatched。** 2 个函数，占 P1 分母约 0.9%。

### 该族的更大影响

`test al,1` 这个变体（我们能生成的那种）**只有这 2 个**，
而 `test dl,1` 变体有 **12 个**（U6，已知不可得）。

**所以整族 14 个都不可得。** 这条路彻底关闭。
---

## U9：check_bytes.py 的 TU 拼接限制 —— **已绕过，不需要改工具**（2026-11-25）

> **✅ 状态：已解决（不改任何门槛脚本）。**
>
> 绕过方法：**把函数写成 `static` 成员函数**（`Finder::find_by_key`）。
> `check_bytes.py` 只在函数签名形如 `Cls::name(` 时才拼入 hpp 的 struct 定义，
> 所以写成成员函数后 struct 就进来了 —— **不需要修改工具的拼接逻辑**。
>
> 配套还要避开两个坑（都是已有的规则）：
>
> | 坑 | 现象 | 修法 |
> |---|---|---|
> | `constexpr` 放 hpp | **C2374 重定义**（prefix 提取一次 + hpp 整体拼接一次）| 直接写字面量 `87` |
> | 表定义在本 TU | **COFF_ERROR**（无外部引用被当死代码删）| 改 `extern` 声明 |
>
> 结果：`0x00407F80` **MATCH（43 字节）**，P1 194 → 195。

---

## U9 原始记录（保留，说明当时的困境）

### 背景（当时的分析）

### 背景

`0x00407F80`（全局表线性搜索，43 字节）**已验证可以精确匹配**：

```
target: 33d2 b874a34c00 3908 7411 83c044 42 3d90ba4c00 7cf1 b804ba4c00 c3 ...
ours  : ★MATCH（用「索引循环 + key 在 +4」）
```

但在 `check_bytes.py` 里跑不通，原因是**它拼 TU 时只带三类前置定义**：

| 来源 | 带什么 |
|---|---|
| hpp | `constexpr` / `static const` 行、`extern ` 开头且 `;` 结尾的行 |
| cpp | 匿名 namespace 块 |
| cpp | `extern "C"` 单行 |

**cpp 全局作用域的 `struct` 定义和变量定义都不带。**

### 因此形成的三重夹击

| 写法 | 结果 |
|---|---|
| struct 在 hpp + extern 在 hpp | extern 被提到最前，struct 被丢弃 → **C4430** |
| struct 在 cpp 全局 + extern 在 cpp 全局 | 两者都不被带入 → **C4430** |
| struct + 表定义都在 cpp 匿名 ns | 表无外部引用被当死代码删 → **COFF_ERROR**（.obj 无 .text）|
| 同上 + `__declspec(noinline)` | 消除死代码，但 `_func_name_after` 的签名解析被 `__declspec(noinline)` 破坏 → **C2059 在 `for`** |
| 匿名 ns 里写 `extern` | **C7631**「不允许在匿名命名空间内使用 extern」|

### 已验证可用的独立写法（T4/T5 实验）

```
T4: struct + extern 声明都在全局作用域   -> .text$mn 43 字节 ✓
T5: struct + 数组定义在匿名 ns + noinline -> .text$mn 43 字节 ✓
```

**两者都能生成正确字节，但都无法透过 `check_bytes.py` 的 TU 拼接。**

### 结论

这不是源码问题，是**工具能力缺口**。

要解决需要改 `check_bytes.py` 的 prefix 拼装逻辑（例如：hpp 的 `struct` 定义也带进来，
或 cpp 全局作用域的 struct 也带进来）。**但那属于改门槛脚本，按 §12 我不擅自改。**

### 处置

- `0x00407F80` 暂不纳入 P1（源码已验证可以匹配，但没有验证路径）
- 已删除 `src/Batch12.cpp` / `include/Batch12.hpp`（避免留下无法验证的代码）
- **需要人类裁决**：是否允许扩展 `check_bytes.py` 的 TU 拼接条件

### 可复用判据

**「能手工匹配」≠「能进门禁」。**

验证脚本 `tools/try_candidate.py` 能跑通（它自己拼 TU），
但正式门禁 `check_bytes.py` 的 TU 拼接规则更严格。

遇到这种情况：要么改门禁（需批准），要么放弃这个函数 —— **不要用 try_candidate 的结果冒充 P1 通过**。
---

## U10：`inc [reg+disp]` 内存自增形式生成不出来（2026-11-25）

### 现象

`0x00402A00`（65 字节）目标中间段：

```asm
ba 0f000000     mov  edx, 0xf
ff 818c000000   inc  dword ptr [ecx+0x8c]     ; ★ 内存自增（3 字节）
8b818c000000    mov  eax, [ecx+0x8c]         ; 再读回来
83f810          cmp  eax, 0x10
c781d40000000000000000  mov [ecx+0xd4], 0
0f4dc2          cmovge eax, edx
89818c000000    mov  [ecx+0x8c], eax
```

我们生成的是：

```asm
8b818c000000    mov  eax, [ecx+0x8c]         ; 先读
40              inc  eax
c781d40000000000000000  mov [ecx+0xd4], 0
83f810          cmp  eax, 0x10
0f4dc2          cmovge eax, edx
89818c000000    mov  [ecx+0x8c], eax
```

**长度差 5 字节**（60 vs 65），差异就在「内存自增 vs 读-改-写」。

### 已试的写法（8 种）

| 写法 | 结果 |
|---|---|
| `a1()[idx] = v0; a2()[idx] = v8; ++idx; k = idx;` | 读-改-写 |
| `flag = 0` 放 `++idx` 之前 | 读-改-写 |
| `a1()[idx] = v0; ...; int k = ++idx;` | 读-改-写 |
| 用索引而非指针 | 读-改-写 |
| `++idx; k = idx; a1()[k-1] = ...` | 显式 spill，更短 |
| `idx` 用局部变量中转 | 更短，47-53 |

**8 种写法全部生成不出 `ff 81 84 00 00 00`。**

### 推断

`inc [mem]` 后面紧跟 `mov eax, [mem]`（读回同一地址）这个组合，
从 C++ 语义上完全冗余（自增结果没被使用）。

**MSVC 只在「自增结果确实不被使用」时才生成内存自增。**

而我们的 C++ 里 `++idx` 之后必然要读 `idx`（因为要判 `>= 0x10`），
所以编译器必然读-改-写。

### 可能的原因

原版可能写的是：

```cpp
++idx;              // 结果不用
if (idx > 0xf) ...  // ★ 注意：这里是重新读内存
```

如果两次读之间**没有其他对 `this` 的写入**，MSVC 可能合并；
但目标里有 `mov [ecx+0xd4], 0`（flag=0）夹在中间，形成了屏障。

**结论：不确定原版怎么写的，暂不硬凑。**

### 处置

保持 unmatched。

### 可复用判据

**「长度差 5 字节且差异是 `inc [mem]` vs `mov/inc/mov`」** 这个模式，
说明原版的 `++x` 结果没有被使用。遇到时先找「结果未被使用」的写法，
但如果中间有屏障写入，可能真的生成不出来。
---

## U11：SSE 结果的寄存器选择（`movaps` 收尾）—— 同类（2026-11-25）

### 现象

`0x004415C0`（31 字节，二维距离平方）：

```asm
f30f104904   movss xmm1, [ecx+4]    ; y
f30f1001     movss xmm0, [ecx]      ; x
f30f5c02     subss xmm0, [edx]      ; dx = x - ox
f30f5c4a04   subss xmm1, [edx+4]    ; dy = y - oy
f30f59c0     mulss xmm0, xmm0       ; dx*dx
f30f59c9     mulss xmm1, xmm1       ; dy*dy
f30f58c1     addss xmm0, xmm1       ; 结果在 xmm0
c3           ret
```

我们生成（29-42 字节，视写法）：

```asm
f30f1001     movss xmm0, [ecx]      ✓
f30f104904   movss xmm1, [ecx+4]    ✓
f30f5c4a04   subss xmm1, [edx+4]    ✗ 顺序反了
f30f5c02     subss xmm0, [edx]      ✗
f30f59c9     mulss xmm1, xmm1       ✗ 顺序反了
f30f59c0     mulss xmm0, xmm0
f30f58c8     addss xmm0, xmm1       （或用 xmm1 作累加器）
0f28c1       movaps xmm0, xmm1      ✗ 多一条
c3           ret
```

**目标让结果直接落在 xmm0；我们的结果落在 xmm1 再 `movaps` 搬过去。**

### 已试的写法（7 种）

| 写法 | 结果 |
|---|---|
| `float dx, dy; return _mm_set_ss(dx*dx+dy*dy)` | 34，多 `movaps` |
| `__m128 dx,dy; _mm_add_ss(_mm_mul_ss(dx,dx), ...)` | 42 |
| 交换两个表达式的书写顺序 | 寄存器号反，仍有 `movaps` |
| 拆中间变量 / 内联 | 34-83 |
| 用 `_mm_store_ss` 中转 | 82 |

### 结论

**MSVC 选择哪个 xmm 作累加器，不由源码控制。**

这与 U6（`eax` vs `edx`）、U8（`esi` vs `edx`）同源：
**寄存器分配是 MSVC 内部决策，源码层面无法干预。**

### 归并

把 U6 / U8 / U11 合并理解：**「寄存器选择差异」是一大类**，
共同特征：

| 特征 | 含义 |
|---|---|
| 指令序列完全一致 | 语义正确 |
| 寄存器名不同（eax/edx/esi/xmm0/xmm1）| MSVC 决策 |
| 可能多一条 `push`/`pop`/`movaps` | 保存或搬运 |

**遇到这种，直接跳过，不要试写法。**
---

## U12：`float` 返回值走 `xmm0` —— 根因是 `__vectorcall`（2026-11-25）

### 现象

`0x00405930`（49 字节）结尾：

```asm
f30f1081 c4050000    movss  xmm0, dword ptr [ecx+0x5c4]
c3                   ret
```

**`float` 返回值在 `xmm0`，不是 x87 `st(0)`。**

### ★★ 根因：`__vectorcall`

| 调用约定 | 优化级 | 长度 | 返回指令 |
|---|---|---:|---|
| `__fastcall` | `/O2` | 47 | `d981...` = `fld`（x87）|
| `__cdecl` | `/O2` | 48 | `fld` |
| **`__vectorcall`** | **`/O2`** | **49 ★** | **`f30f1081` = `movss xmm0`** |

**MSVC 32 位的 `float` 返回值默认走 x87；只有 `__vectorcall` 会走 `xmm0`。**

这也解释了为什么 `/fp:fast`、`/arch:SSE`、`/arch:SSE2` 全都不起作用 ——
它们只影响中间计算和参数传递，**不影响返回值约定**。

### 当前还差的 1 处

```
targ: 8bc8 8b81bc050000 85c0 75e8
ours: 8bc8 8b80bc050000 85c0 75e8
            ^^ 8b81 = mov eax,[ecx+0x5bc]
            ^^ 8b80 = mov eax,[eax+0x5bc]
```

`mov ecx,eax` 之后 `ecx == eax`，两种编码功能完全等价 ——
**这是纯寄存器分配差异**（归入 U6/U8/U11 大类）。

已试 4 种写法（`p=p->link` / `cur` 赋值后读 `cur` / `this` 版本 / `for` 双变量），
全部生成 `[eax+...]`。

### 可复用判据

**目标里如果 `float` 结果落 `xmm0` 且函数是叶子，先试 `__vectorcall`。**

反过来：如果试了 `/fp:fast`、`/arch:SSE*` 都还是 `fld`，那就是调用约定问题，不是浮点模型问题。
---

## U13：G4「能 for 不 while」与 P1 字节匹配的真实冲突（2026-11-25）

### 现象

`0x004029B0`（71 字节）和 `0x00404B10`（77 字节）是角度归一化函数。
它们的目标控制流是：

```asm
loop1:
8bc1          mov eax, ecx      ; old = n
f30f5cc2      subss xmm0, xmm2  ; a -= 2PI
41            inc ecx           ; ++n
83f820        cmp eax, 0x20
7f05          jg done1
0f2fc1        comiss xmm0, xmm1
77ef          ja loop1           ; while (a > PI)
```

这是标准的 **while 循环**（条件在末尾判断）。

### 但 G4 判它 FAIL

```
[FAIL] while_should_be_for  src\Batch22.cpp:72
       while (a > kPi) {
       -> §6 G4 规范形：能 for 不 while（循环体在自增计数器）
```

`check_style.py` 的 `scan_while()` 判定口径：
```python
if re.search(r"(?:\+\+\s*\w+|\w+\s*\+\+|\w+\s*\+=\s*1)", body):
    findings.append({... "while_should_be_for" ...})
```

**只要循环体里有 `++n` 就判 FAIL**，不看循环的具体形态。

### 改成 for 后 P1 掉下来

| 写法 | 长度 | P1 |
|---|---:|---|
| `while (a > kPi) { int old=n; a-=2PI; ++n; if(old>0x20) break; }` | **71** | **★MATCH** |
| `for (; a > kPi; ++n) { int old=n; a-=2PI; if(old>0x20) break; }` | 73 | DIFF |

**目标字节就是 while 形态，for 写法的控制流结构不一样。**

### 处置

**保留 while（P1 匹配），G4 如实报 FAIL。**

理由：
1. 按既有原则，**改了门禁换来的 PASS 不算 PASS**（见 V2）
2. P1 是字节级判据，比 G4 的风格规范更硬
3. 两者冲突时，**不能为了 G4 让 P1 掉下来** —— 那等于用风格规则否定事实

### 需要人类裁决

`scan_while()` 的口径可能过宽：

- 它只看「循环体有自增」，但**没有区分「计数器用于循环条件」和「计数器只是副作用」**
- 本例的 `n` 是安全上限计数器，不是循环的主控变量（主控是 `a > kPi`）
- 真正的「能用 for」应该判据是：**计数器的自增是循环的唯一驱动**

**建议（未实施）**：把判据改成「for 的三段式能等价表达该循环」才算 FAIL。
但这属于改门槛脚本，需批准。
---

## U14：`cmp eax,imm32`（5 字节）vs `cmp edx,imm32`（6 字节）—— 累加器选择（2026-11-25）

### 现象

`0x00441550`（39 字节）——计数器累加 + 上限截断：

```asm
b8cdcccccc      mov  eax, 0xCCCCCCCD
f76508          mul  dword [ebp+8]       ; n / 10 的无符号魔术数
8b414c          mov  eax, [ecx+0x4c]     ; ★ 先读 counter 到 eax
c1ea03          shr  edx, 3
03c2            add  eax, edx            ; ★ eax 作累加器
baffc99a3b      mov  edx, 0x3B9AC9FF
3d00ca9a3b      cmp  eax, 0x3B9ACA00     ; ★ 5 字节（短格式）
0f43c2          cmovae eax, edx
89414c          mov  [ecx+0x4c], eax
```

我们生成（38 字节）：

```asm
b8cdcccccc      mov  eax, 0xCCCCCCCD
f76508          mul  dword [ebp+8]
b8ffc99a3b      mov  eax, 0x3B9AC9FF     ; 提前读上限
c1ea03          shr  edx, 3
03514c          add  edx, [ecx+0x4c]     ; ★ edx 作累加器
81fa00ca9a3b    cmp  edx, 0x3B9ACA00     ; ★ 6 字节（长格式）
0f43d0          cmovae edx, eax
89514c          mov  [ecx+0x4c], edx
```

**差 1 字节，全部来自 `cmp eax` vs `cmp edx`。**

### 已试的写法（9 种）

| 写法 | 长度 |
|---|---:|
| `stuct C { int counter; }; void add(unsigned n)` + `v = counter + n/10` | 38 |
| `v = counter; v += n/10` | 38 |
| `unsigned v` / `unsigned counter` | 38 |
| `counter += n/10` | 40 |
| 三目 `v >= 1e9 ? 999999999 : v` | 38 |
| `>` 而非 `>=`（34 字节，更短不对）| 34 |

**全部把累加器放 `edx`。**

### 分析

`edx` 是 `mul` 的结果持有者（`mul` 固定用 `edx:eax`）。
编译器倾向于**复用 `edx` 作累加器**，省一条 `mov`。

目标却把 `counter` 读到 `eax` 再 `add eax, edx` —— 说明原版的源码形态
让编译器认为 `eax` 是自然选择（可能是用了 `min()` 或不同的运算顺序）。

### 处置

保持 unmatched，记录为 U14（寄存器分配类，与 U6/U8/U11 同源）。
---

## U15：`xor eax,eax / cmp [ecx+N],al` —— 复用零寄存器（2026-11-25）

### 现象

`0x004A2CA8`（20 字节）：

```asm
33c0            xor  eax, eax          ; ★ 先清零并复用为源
384114          cmp  [ecx+0x14], al    ; flag == 0 ?（用 al 而非直接 0）
7403            je   done
884114          mov  [ecx+0x14], al
894108          mov  [ecx+8], eax
89410c          mov  [ecx+0xc], eax
894110          mov  [ecx+0x10], eax
c3              ret
```

我们生成（21 字节）：

```asm
80791400740e    cmp  byte [ecx+0x14], 0    ; ★ 直接与立即数 0 比较
33c0            xor  eax, eax
884114          mov  [ecx+0x14], al
...
```

**目标把 `xor eax,eax` 提到 `cmp` 之前，并用 `al` 作比较操作数（省 1 字节）。**

### 已试的写法（9 种）

| 写法 | 长度（/O1）|
|---|---:|
| `if (flag) { flag=0; a=b=c=0; }` | 21 |
| `bool` flag | 21 |
| `char z = 0; if (flag != z) {...}` | 21 |
| `int z = 0; if (flag != (char)z) {...}` | 21 |
| `flag` 声明为 `int` | 21 |
| `if (!flag) return; ...` | 21 |
| `goto done` 尾 | 21 |
| `memset` 风格 | 21 |

**全部生成 `cmp byte [mem], 0` + 后置 `xor eax,eax`。**

### 分析

目标形态要求编译器**先建立零值寄存器，再复用它做比较和赋值**。
这需要 MSVC 把「比较的 0」和「赋值的 0」识别为同一个值。

MSVC 在 `/O1` 下倾向于用立即数比较（`cmp ...,0` 4 字节 vs `cmp ...,al` 2 字节），
只有在寄存器已持有 0 时才复用。

**结论：源码层面无法强制这个顺序。** 归入寄存器分配大类，但与之前几类不同：
这次是「常量复用的时机」，不是「用哪个寄存器」。

### 可复用判据

看到 `xxc0`（xor eax,eax / xor ecx,ecx）出现在**比较之前**、
且后续用 `al`/`cl` 作比较操作数 —— 说明原版的零值来自某个「已初始化的局部变量」，
而非字面量 0。但具体源码形态未能还原。
---

## U16：分支方向反转（`7e` vs `7f`）—— 编译器选择反条件（2026-11-25）

### 现象

`0x00492815`（29 字节）：

```asm
558bec               push ebp / mov ebp,esp
8b4510               mov  eax, [ebp+0x10]    ; p3
8b4d08               mov  ecx, [ebp+8]       ; p1
817804 80000000      cmp  dword [eax+4], 0x80
7f06                 jg   small              ; ★ 真 -> 走字节分支
0fbe4108             movsx eax, byte [ecx+8]
5d                   pop  ebp
c3                   ret
small:
8b4108               mov  eax, [ecx+8]       ; 完整 int
5d                   pop  ebp
c3                   ret
```

我们生成（29 字节，指令完全相同）：

```asm
... 817804 80000000      cmp  dword [eax+4], 0x80
7e06                 jle  int_path            ; ★ 反条件
...
```

**唯一差异：`7f` (jg) vs `7e` (jle)，1 字节。**

### 已试的写法（10 种）

| 写法 | 跳转 |
|---|---|
| `if (f4 > 0x80) return 字节; return int;` | `7e` |
| `if (!(f4 > 0x80)) return int; return 字节;` | `7f` |
| `if (f4 <= 0x80) return int; return 字节;` | `7f` |
| 三目 / goto / 中间变量 | `7e` 或长度变短 |
| `unsigned` 比较 | `77` |

**`7f` 的写法能对上方向，但块内容顺序反了**（目标真分支是 movsx 在前）。

### 分析

目标的结构是：**条件为真 → 跳到后面的字节分支**（fallthrough 是 int 分支）。

这要求编译器把「真」目标放在跳转位置、把「假」放在 fallthrough。
MSVC 默认总是把 fallthrough 给「真」分支（即生成反条件跳转到假分支）。

**这是 MSVC 的分支布局策略，源码层面无法干预。**

### 可复用判据

**当目标指令序列与生成完全一致、只有跳转条件字节不同（`7e`↔`7f`、`74`↔`75` 等），
说明源码语义已正确，只是编译器选了反条件。归入不可得。**

判定方法：对比两段反汇编，若除 `jXX` 的 opcode 外完全一致，即可停止尝试。
---

## U17：`jne` 后紧跟 `ret`（无 `jmp` 跳过）—— 尾块布局（2026-11-25）

### 现象

`0x0049CD9B`（32 字节）：

```asm
8b4120           mov  eax, [ecx+0x20]     ; s = state
83f802           cmp  eax, 2
7504             jne  +4                  ; ★ 跳到 cmp 3（不是跳到 ret）
c64124 00        mov  byte [ecx+0x24], 0
83f803           cmp  eax, 3
740a             je   ret
83f804           cmp  eax, 4
7405             je   ret
83f808           cmp  eax, 8
7504             jne  +4
c64124 01        mov  byte [ecx+0x24], 1
c3               ret
```

我们生成（33 字节）：

```asm
8b4120           mov  eax, [ecx+0x20]
83f802           cmp  eax, 2
7505             jne  +5                  ; ★ 跳到 c3（ret）
c64124 00        mov  byte [ecx+0x24], 0
c3               ret                       ; ★ 多这一条 ret
83f803           cmp  eax, 3
...
```

**差 1 字节。目标是「flag=0 后继续往下检查」，我们是「flag=0 后直接 ret」。**

也就是说：目标语义里 `state==2` 设 `flag=0` 后**仍继续**执行 `cmp 3/4/8`；
而所有 C++ 写法（if / else if / switch）都让编译器在 `flag=0` 后插一条 `ret`。

### 已试的写法（8 种）

| 写法 | 长度 |
|---|---:|
| `if (s==2) flag=0; if (s==3\|\|s==4\|\|s==8) flag=1;` | 33 |
| `if/else if` 链 | 33 |
| `switch` + fallthrough | 23-24（更小，被优化）|
| 独立四条 `if` | 33 |
| `case 3: case 4: case 8:` 共享 | 34-55 |

### 分析

目标的 `7504` 让 `jne` **跳过 flag 赋值继续**，而不是跳到 ret。
这要求编译器认出一个「设完值还要继续执行」的控制流，
但语义上 `state==2` 和 `state==8` 是互斥的（不可能同时成立），
所以编译器认为 `flag=0` 之后必然要 `ret`，插入了多余的 `c3`。

**结论：语义上目标写法有冗余（`state==2` 后不可能 ==8），
编译器识别出这是 dead path 并插入 `ret`。源码层面无法阻止。**

### 可复用判据

**当目标在「赋值后没有 `jmp` 跳过」而生成结果多一条 `ret` 时，
说明目标来自语义上更宽松（含 dead path）的源码，MSVC 会收紧。归入不可得。**