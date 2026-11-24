# th185-matching

> 東方言 project 第 18.5 作 *弾幕狂々の黒市場* (TH185) 的**字节级匹配反编译**。
>
> 与 [`th185`](https://github.com/GreenSweet233/th185)（功能还原）**独立**：
> `th185` 追求可读、行为等价的 C++ 重写；本仓库追求编译产物与原版**逐字节相同**。

---

## 当前状态

| 指标 | 值 |
|---|---|
| 已匹配函数 | **80 / 1675**（4.78%）|
| 字节级验证 | **36 / 39**（92.3%）|
| 门禁 PASS | G1 / G3 / G4 |
| 门禁 INCOMPLETE | G2 / G5 / G6 |
| 行为验证（G2+）| 51 / 51 PASS |

详见 [`docs/STATUS.md`](docs/STATUS.md)（现状与决策）和 [`docs/MATCHED.md`](docs/MATCHED.md)（80 个函数的分类清单）。

---

## 快速开始

### 1. 准备原版二进制

原版 `th185.exe` **不入库**（版权 + 体积）。三种提供方式，按优先级：

```
① 命令行参数    python tools/check_bytes.py --exe /path/to/th185.exe
② 环境变量      set TH185_EXE=D:\path\to\th185.exe
③ 仓库内文件    orig/th185.exe
④ 本地配置      local.target.json 里的 { "gameDir": "..." }
```

校验：SHA-256 见 `toolchain.lock` 的 `target.sha256`。

### 2. 准备编译器

```
MSVC v142 19.29.30159 (x86)
解压到 D:\MSVC142\14.29.30133
```

**不需要安装器、不需要管理员权限** —— 微软把工具集打包成 `.vsix` 放在
`C:\ProgramData\Microsoft\VisualStudio\Packages`，解压即用。
详见 `evidence/P1-first-byte-match.json` 的 `packages_needed`。

还需要 Windows SDK 的 UCRT 头（`math.h` 在那里，不在 MSVC 工具集里）：
```
C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0\ucrt
```

### 3. 跑验证

```bash
python tools/check_types.py       # G1 类型一致性
python tools/check_witness.py     # G2 二进制见证（覆盖率）
python tools/check_structure.py   # G3 结构对齐
python tools/check_style.py       # G4 风格
python tools/check_stability.py   # G5 扰动稳定
python tools/check_evidence.py    # G6 独立证据
python tools/check_bytes.py       # P1 逐字节比对  ← 本仓库的核心
python tools/check_behavior.py    # G2+ 执行原版机器码比对
```

---

## 为什么需要两个额外的验证器

原规约的 G1-G6 是**静态门槛**，只查「形式对不对」。实测发现不够：

| 层次 | 验证器 | 查什么 |
|---|---|---|
| 形式 | G1-G6 | 反汇编见证、结构、签名、风格 |
| **语义** | **G2+** | 用 unicorn 执行**原版机器码**，与独立 Python 重算比对 |
| **产出** | **P1** | 编译源码，与 `.text` **逐字节**比对 |

**证据**：曾经有 9 个函数 G1/G2/G3/G4/G6 全过、G2+ 全过，但**字节 0% 匹配**。
三者缺一不可。

---

## 五条 MSVC 代码生成规则（全部实测得出）

| # | 规则 | 证据 |
|---|---|---|
| 1 | **`/Oy-` 保留栈帧** | `/O2` 默认省略帧指针；`/Od` 引入多余局部变量；`/Ob0` 无效。只有 `/Oy-` 精确命中 |
| 2 | **12 字节聚合类型必须 struct 整体赋值** | 逐字段 3 条 `mov`（18 字节）；整体赋值触发 SSE2 `movq`+`mov` |
| 3 | **返回 `int` 与 `bool` 走不同路径** | `int` → `xor eax,eax`+`cmp [mem],eax`；`bool` → `cmp [mem],0`。判据：有无 `33c0` |
| 4 | **只有宏能让偏移折叠进 disp8** | `inline` → `add eax,[ebp+0xC]`；`template` → `add eax,0x44`；**只有 `#define` 生成 `[ecx+0x44]`** |
| 5 | **优化级可能是 `/Os` 而非 `/O2`** | `pop ecx` vs `add esp,4` 是优化级差异，不是版本差异 |

## 最大发现：SSE 不是 float

```asm
原版: f30f1001c3   movss xmm0,[ecx]    ← SSE 返回
我们: d901c3       fld  dword [ecx]    ← x87 返回（MSVC 对 float 的默认）
```

**根因**：原版返回类型是 `__m128`，不是 `float`。
MSVC x86 上用 x87 栈返回 float，**只有返回 `__m128` 才走 XMM0**。

```cpp
__m128 __fastcall f(void* self, void* unused_edx) {
    return _mm_load_ss(FLOAT_AT(self, 0x44));
}
```

---

## 仓库结构

```
src/         字节匹配源码（3 个 .cpp，485 行）
include/     头文件（3 个 .hpp，332 行）
tools/       8 个验证器 + 公共库（约 4600 行）
evidence/    每次发现的证据（32 个 JSON）
docs/        STATUS.md（现状与决策）+ MATCHED.md（函数分类清单）
ledger/      违规登记 + 冲突提案 + unresolved
types/       全局类型表（db.json / lock.json）
orig/        原版二进制占位（不入库）
```

---

## 诚实边界

**这个仓库不追求「还原原始源码」** —— 编译是多对一映射，字节一致 ≠ 原像唯一。

已知的**物理不可得**项：

| 项 | 数量 | 原因 |
|---|---:|---|
| `fsincos` 函数 | 19 | MSVC 编译器前端永不生成该指令，它是 CRT 内部实现 |
| `encode_value` | 1 | 需 v141 的 `/hotpatch`（v142 生成 `66 90` 而非 `8b ff`）|
| G6 三项 | 3 | `/GR-` 无 RTTI、PE32 无 `.pdata`、无第二个反汇编器 |
| G5 | — | 编译器缺口 |

**宁可 unmatched，不可 garbage match。**

---

## 相关仓库

| 仓库 | 定位 |
|---|---|
| [`th185`](https://github.com/GreenSweet233/th185) | **功能还原** —— 可读、行为等价的 C++ 重写（37k 行）|
| `th185-matching`（本仓库）| **字节级匹配** —— 编译产物与原版逐字节相同（5k 行）|

---

## License

见 [LICENSE](LICENSE)。原版游戏资源版权归原作者（上海アリス幻樂団 / 製作者）所有。