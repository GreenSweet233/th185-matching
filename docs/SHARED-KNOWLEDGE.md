# TH185 共享知识库（两个仓库共用）

> 本文件放在 `th185-matching/docs/`，但**两个仓库都要读**。
>
> | 仓库 | 定位 | 谁负责 |
> |---|---|---|
> | `th185` | 功能还原（可读 C++ + web） | recon agent |
> | `th185-matching` | 字节级匹配（diff=0） | matching agent |
>
> **写入规则**：各自只写自己的仓库。本节内容由两边共同维护，
> 任何一方发现错误都可以改，但要在提交信息里说明。

---

## 一、已确认的工具链事实

### 编译器

```
MSVC v142 19.29.30159 (x86)
位置：D:\MSVC142\14.29.30133
来源：从 C:\ProgramData\Microsoft\VisualStudio\Packages 的 .vsix 解压
      ★ 不需要安装器，不需要管理员权限
```

**已实测确认这就是原版编译器**（Rich Header prodId `0x6B14` 命中，build 序号 257/259/260/261 全部吻合）。

### 头文件路径

```
D:\MSVC142\14.29.30133\include
C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\ucrt
C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\shared
C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\um
```

> `<math.h>` 在 **UCRT**（SDK）里，不在 MSVC 工具集里。只用 MSVC 的 include 会失败。

### 编译选项

| 场景 | 选项 |
|---|---|
| 默认 | `/MT /O2 /GS- /Oy-` |
| 含 `pop ecx` 清栈（极少数）| `/MT /Os /GS- /Oy-` |

**`/Oy-` 必需** —— `/O2` 默认省略帧指针，原版多数函数保留栈帧。

---

## 二、五条 MSVC 代码生成规则（全部实测得出）

| # | 规则 | 证据 |
|---|---|---|
| 1 | **`/Oy-` 保留栈帧** | `/O2` 默认省略；`/Od` 引入多余局部变量；`/Ob0` 无效 |
| 2 | **12 字节聚合类型必须 struct 整体赋值** | 逐字段 3 条 `mov`（18字节）；整体赋值触发 SSE2 `movq`+`mov` |
| 3 | **返回 `int` 与 `bool` 走不同路径** | `int` 用 `xor eax,eax`+`cmp [mem],eax`；`bool` 用 `cmp [mem],0` |
| 4 | **只有宏能让偏移折叠进 disp8** | `inline`/`template` 生成 `add`；只有 `#define` 生成 `[ecx+0x44]` |
| 5 | **优化级可能是 `/Os` 而非 `/O2`** | `pop ecx` vs `add esp,4` 是优化级差异，不是版本差异 |

---

## 三、★ 最重要的发现：原版用 `__m128` 而非 `float`

```asm
原版: f30f1001c3   movss xmm0,[ecx]    ← SSE 返回
我们: d901c3       fld  dword [ecx]    ← x87 返回（MSVC 对 float 的默认）
```

**根因**：原版返回类型是 `__m128`，不是 `float`。
MSVC x86 上用 x87 栈返回 float，**只有返回 `__m128` 才走 XMM0**。

**参数同理**：`__m128` 参数占 XMM 寄存器（第 1 个 xmm0，第 2 个 xmm1，第 3 个 xmm2）。

---

## 四、★ 第二重要的发现：原版是 C++ 类，写成员函数

```cpp
// ✗ 错：token 12，G3 ratio 6.00
int __fastcall get(void* self, void* unused_edx) {
    (void)unused_edx;
    return *(int*)self;
}

// ✓ 对：token 6，G3 ratio 3.00，字节完全相同
int Obj00::get() { return f_00; }
```

**成员函数天然是 `thiscall`（`ecx` = `this`），不需要 `__fastcall` 伪装，不需要哑参数。**

---

## 五、已实测的「物理不可得」项

| 项 | 数量 | 原因 |
|---|---:|---|
| `fsincos` 函数 | 19 | MSVC 编译器前端**永不**生成该指令（跨 v141/v142 穷举 60 种组合都不行），它是 CRT 内部实现 |
| `encode_value` | 1 | 需 `8bff` hotpatch 序言，v141/v142 都生成 `66 90` |
| `and [mem],0` | 1 | 现代 MSVC 对 `g_v = 0` 生成 `mov`，不生成 `and` |
| `xor eax,eax; inc eax` | 1 | `return 1` 被优化成 `mov eax,1` |
| G6 三项 | 3 | `/GR-` 无 RTTI、PE32 无 `.pdata`、无第二个反汇编器 |

---

## 六、结构布局证据（`Object.hpp`）

`th185-matching/include/Object.hpp` 有 16 个 struct，字段偏移**全部来自二进制见证**：

| struct | 字段偏移 | witness 地址 |
|---|---|---|
| `Obj00` | +0x00 | 0x00433B40（`mov eax,[ecx]`）|
| `Obj04` | +0x00 / +0x04 / +0x08 | 0x00433BA0 / 0x00433BB0 |
| `Obj1C` | +0x1C | 0x0041FA30 / 0x00429A50 |
| `Obj20` | +0x20 | 0x004176D0 |
| `Obj24` | +0x24 | 0x00406730 |
| `Obj28` | +0x28 | 0x00424480 |
| `Obj34` | +0x34 | 0x00417770 |
| `Obj3C` | +0x3C | 0x00441210 / 0x00440840 |
| `Obj44` | +0x44 | 0x00433DC0 |
| `Obj50` | +0x50 | 0x00431300 |
| `Obj620` | +0x620 | 0x00407E30 |
| `Obj1230` | +0x1230 | 0x00433DF0 |
| `Obj1274` | +0x1274 | 0x0040A090 |
| `Obj4FF4` | +0x4FF4 | 0x00433DD0 |
| `Obj19260` | +0x19260 | 0x00431260 |
| `Obj47FAC` | +0x47FAC | 0x0040A0E0 |

**这些偏移是硬的（有机器码见证），但字段的「业务含义」还不知道。**

> **recon 侧如果从 ECL/存档/卡牌数据推出了 +0x20 之类是什么，请写进 `th185_web/docs/`，
> 并在提交信息里标注「给 matching 侧的结构语义证据」。**

---

## 七、当前数字（2026-11-24）

### th185-matching

```
匹配函数    128 / 1675（7.6%）
P1 字节验证  66 / 71（93.0%）
G1 PASS | G3 FAIL(46) | G4 PASS | G2+ PASS(51/51)
G2 INCOMPLETE | G5 INCOMPLETE | G6 INCOMPLETE
```

### th185

```
197 提交（拆分前）
th185_web/ 37k 行，26 篇逆向文档
```

---

## 八、两个仓库的历史分家记录

2026-11-24 拆分，原因：**两边约束互相拖累**。

| 事项 | 规模 | 追求 |
|---|---|---|
| 功能还原 | 37,456 行 | 可读、行为等价 |
| 字节级匹配 | 5,401 行（当时）| `diff = 0` |

字节匹配要求 `__fastcall` + 哑参数 + 宏折叠等技巧，这些让功能还原的代码难读；
而功能还原的可读性要求又让字节匹配无法达成。

**现在两边并行推进，共享结构布局与函数语义的发现。**

---

## 九、给两个 agent 的协作约定

| 约定 | 说明 |
|---|---|
| **不跨仓库写入** | recon 只写 `th185`，matching 只写 `th185-matching` |
| **可跨仓库读** | 两边都可以读对方的产出 |
| **共享发现要落盘** | 不要只在对话里说，写进文件 |
| **不信任本文件** | 数字会过时，**先自己跑一遍核实** |