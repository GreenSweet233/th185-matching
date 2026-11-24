# 从 TH16 借鉴的方法论（2026-11-24）

> 本机 `thvip/th16`（`johnthreekay/th16` 的本地克隆）是**已跑通的** TH16 字节级匹配项目：
> **1004 / 1199 函数字节一致（83.74%）**，26/26 虚表、201/202 全局数据、11828 帧行为零差异。
>
> TH16 与 TH185 同属小数点作世代，ZUN 的引擎架构（ECL / ANM / PBG4 / RNG）一脉相承。
> **它的方法论可以直接搬过来。**

---

## 一、★ 最重要的发现：它解决了工具链问题

我一直以为「需要装 VS2017」是硬阻塞。**实际上 th16 早就把工具链下好了**：

```
D:\Windows\GreenSweet\Game\Tasks\thvip\th16\prefix\
├─ vs/VC/Tools/MSVC/14.10.25017/     ← cl.exe 报 19.10.25017（v141 / VS2017 15.0）
├─ sdk/Program Files/Microsoft SDKs/Windows/v7.1A/
├─ sdk/Windows Kits/10/Include/10.0.10240.0/ucrt/
└─ dxsdk/
```

**INCLUDE 顺序**（照抄 `toolchain.py`）：

```python
INCLUDE = [
    MSVC / "include",
    KITS10 / "Include/10.0.10240.0/ucrt",
    SDK71A / "Include",
    DXSDK / "Include",
]
```

## 二、★★ 根本性的方法差异：整程序 LTCG，不是逐函数编译

### th16 的做法

```python
CFLAGS = ["/GS", "/GL", "/W3", "/Gy", "/Zc:wchar_t", "/O2", "/Oy-",
          "/Zc:inline", "/fp:precise", "/Zc:forScope", "/Gd", "/Oi", "/MT",
          "/EHsc", "/FC", "/WX-"]

LFLAGS = ["/LTCG", "/INCREMENTAL:NO", "/NXCOMPAT", "/DYNAMICBASE:NO",
          "/MACHINE:X86", "/SAFESEH", "/OPT:REF", "/OPT:NOICF",
          "/SUBSYSTEM:WINDOWS,5.01", "/DEBUG"]
```

**`/GL` + `/LTCG` = 整程序优化。** 代码生成发生在链接阶段，不是编译阶段。

### 我之前的做法（错的方向）

```python
# 逐函数编译成 .obj，然后对比 .obj 的 .text 段
cl /c /MT /O2 /GS- /Oy- single_function.cpp
```

### 为什么这个差异是根本性的

| 层面 | 逐函数编译（我） | 整程序 LTCG（th16） |
|---|---|---|
| 函数边界 | 编译期固定 | **链接期决定**（可跨 TU 内联） |
| 参数常量折叠 | 不会发生 | **会发生**（所有调用点都是常量时） |
| 栈槽保留 | 按函数体 | **按整程序调用图** |
| 相同函数合并 | 不会 | `/OPT:NOICF` 禁止，但我们没这个选项 |

### 实测证据

我用 th16 的完整 CFLAGS 编译，**加了 `/GL` 直接编译失败**（`/GL` 的 `.obj` 不含机器码，要等链接）。
这本身就证明了差异：**`/GL` 把代码生成推迟到链接期。**

## 三、★★★ `/OPT:NOICF` 解释了 fsincos 为什么有 19 份

th16 的 `build.py` 里有一句决定性的注释：

```python
# /OPT:NOICF: th16.exe keeps several sets of byte-identical functions
# (twelve copies of one fsincos helper) that identical COMDAT folding
# would have merged.
```

**th16 里有 12 份字节完全相同的 `fsincos` helper。**

我们的 TH185 有 **19 个 `fsincos` 函数**（17 个圆形版 + 2 个椭圆版）。
这个数量关系不是巧合 —— 都来自**同一份源码被多次实例化**（`/OPT:NOICF` 保留了重复）。

**推论**：那 19 个函数在原版里其实是**同一段代码的多份拷贝**。
只要写出正确的**一份**，其余 18 个自动匹配。

## 四、`/GS` 改变函数序言 —— 这解释了我纠结很久的现象

`compare.py` 里有一段注释：

```python
# with /GS the thunk begins earlier with a stack cookie check
# (mov edx,[esp+8]; lea eax,[edx+N]; mov ecx,[edx-M]; xor ecx,eax;
#  call __security_check_cookie)
```

**`/GS` 会在函数序言插入 cookie 检查。**

我一直用 `/GS-`（关闭），理由是「`0x00402A50` 函数体内 0 处引用 cookie」。
但那只说明**那个函数**没有数组/大局部变量，不代表整个程序用 `/GS-`。

**TH185 引用 `__security_cookie` 250 次、`__security_check_cookie` 被调用 306 次 → 原版是 `/GS`（开启）。**

### 但实测：`/GS` 对已匹配函数没影响

```
0x00402A50  GS- / GS+  都 MATCH（68 字节完全相同）
0x00406720  GS- / GS+  都 MATCH
0x0041FA20  GS- / GS+  都 MATCH
```

原因：`/GS` 只对**含数组或大局部变量**的函数插入 cookie。
我们已匹配的 36 个都是简单叶子/访问器，没有数组 → 不受影响。

**结论：现有 36 个匹配是可靠的。** 但当匹配到复杂函数（带局部数组的）时，
`/GS` 的差异会显现 —— 那时必须用 `/GS` 而不是 `/GS-`。

## 五、th16 的差异三分类（已验证的「不可匹配」类型）

`MATCHING-FRONTIER.md` 记录了剩余 195 个缺口的根因：

| 类别 | 说明 | 实例 |
|---|---|---|
| ① 编译器常量折叠 | 原版保留 `mulss xmm1,[__real@3f800000]`（乘 1.0f），我们的 `/fp:precise` 会折叠掉 | ZunTimer::operator-- |
| ② 栈帧与 cookie 槽位 | 帧大小和 cookie 位置由**局部变量集合整体**决定，调声明顺序/volatile 均无效 | create_stringf（cookie 在 [ebp-8] vs [ebp-4]）|
| ③ 寄存器分配与 CSE | 操作数落在 eax/esi 还是 edi/edx 属分配器决策，无法指定 | step_back |

**关键结论（th16 原话）**：

> 后两类是**整程序决策**，修好一个函数可能让另一个失配，**有些可能永远无法匹配**。

这与我们的 `fsincos` / `hotpatch` 同类。

## 六、th16 的全局开关扫描结果（可直接复用）

| 开关 | th16 结果 | 结论 |
|---|---|---|
| `/O2 /Oi /fp:precise /Ob2` | 1001/1199 | **最优** |
| `/fp:strict` | 874/1198 | 排除 |
| `/Oi-` | 908/1194 | 排除 |
| `/Ob3` | 1001/1199 | 无影响（LTCG 忽略）|
| 链接顺序按原版地址 | 1000/1199 | 排除 |
| `/d2SSAOptimizer-` | 969/1197 | 排除（SSA 优化器在原版中开启）|
| `#pragma inline_depth(255)` | 1001/1199 | 无影响 |

## 七、8 条可直接套用的源码形态规律

从 `docs/findings.md`（1906 行）提炼：

| # | 规律 |
|---|---|
| 1 | **`volatile` 技巧**：某些构造函数的所有赋值都被保留然后覆盖，唯一能复现的写法是通过 `volatile T*` 写入 |
| 2 | **`new T` 与 `new T()` 不同**：后者值初始化加 memset。ZUN 写 `new T` |
| 3 | **RTTI 名字必须用 ZUN 的**：`ThreadInf`/`EnemyInf` 等，因为名字存在 exe 里 |
| 4 | **跳转表 switch 的 case 块按源码顺序布局** → 可反推 ZUN 的 case 顺序 |
| 5 | **`DECOMP_NOINLINE` 宏**：让某个 UCRT 内联函数在整程序范围保持 out-of-line |
| 6 | **`#pragma loop(no_vector)`**：复现原版未向量化的循环 |
| 7 | **两个全局相加时 MSVC 先加载第二个** |
| 8 | **参数在所有调用点都是常量时会被折叠**，但保留栈槽，调用方用 `push ecx` 填充 |

## 八、可直接移植的工具

| th16 脚本 | 用途 | 对我们的价值 |
|---|---|---|
| `variants_search.py` | 自动尝试多种源码写法找匹配 | ★★★ 可直接用于批量匹配 |
| `quickdiff.py` | 秒级差分 | ★★★ 比 `check_bytes.py` 快得多 |
| `gap_report.py` | 已量化的缺口分类报告 | ★★★ 用于规划下一批目标 |
| `behavior_diff.py` | 比对 calls/constants/globals | ★★ 我们的 G2+ 可借鉴 |
| `sigscan.py` | 签名扫描找函数 | ★★ 可替代 `function_extent` |
| `check_unchanged.py` | 确认可读性修改不改代码 | ★ 对功能还原仓库更有用 |
| `replay_trace_native.py` | 帧级追踪 | ✗ TH185 无 Replay，不适用 |

## 九、我的实测结果（用 v141）

### `encode_value`（`0x0049DC88`）

```
v141 /O1 /GS- /Oy- /hotpatch:  6690 558bec 8b15 4c8a4c00 8bc2 83e01f 6a20 59 2bc8 ...
                               ^^^^ /hotpatch   ^^^^ mov edx（不是 a1）

v141 /Os /GS- /Oy- /hotpatch:  6690 558bec 5151 a1 4c8a4c00 ...  （有 a1 但栈帧太大）
原版:                          8bff 558bec      a1 4c8a4c00 83e01f 6a20 59 2bc8 ...
```

**结论：v141 的 `/hotpatch` 也生成 `66 90`，不是 `8b ff`。**
我上一轮「更早版本用 8bff」的推测**被推翻**。原版的 `8bff` 需要比 v141 更早的编译器。

### `fsincos`（`0x00404B70`）

v141 同样不生成 `fsincos`。**跨版本确认：MSVC 编译器前端永不合成该指令。**

## 十、下一步该做什么

| 优先级 | 行动 | 理由 |
|---|---|---|
| **P0** | 改用 th16 的整程序构建（`/GL` + `/LTCG` + `/GS`）| 这是根本方法差异，不改正则永远只能匹配叶子函数 |
| **P1** | 移植 `variants_search.py` | 自动化找写法，比手工试快得多 |
| **P2** | 移植 `quickdiff.py` | 迭代速度 |
| **P3** | 检查 TH185 是否真用了 `/GL` | 函数数 1675 > th16 的 1199，可能没有 |

> **最后这条很重要**：如果 TH185 没用 `/GL`，那逐函数编译仍然可行，
> 但 `/GS` 一定要开启（250 处 cookie 引用 + 306 次 check 调用已证明）。