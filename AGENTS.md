# AGENTS.md — 自动化字节级匹配反编译规范（th185 特供）

> 最高指令：字节相等是门槛，不是目标。
> 任一硬门(G1–G6)不过的候选，diff 再小也拒。
> 宁可 unmatched，不可 garbage match。
> 禁止用合理性/可读性/一致性去换字节数。

人类几乎不审代码，只审 §11 仪表盘。你必须保证仪表盘每个数字都是真的。

---

## 路线切换声明（必读）

**本仓库自本轮起，目标由「功能还原重建」切换为「字节级匹配反编译」。**

切换前的工作（156 commit）是**行为等价重建**：C++/SDL3 → Emscripten/WASM，
运行在浏览器里，验收标准是「9/9 通关、faults=0、HUD 对齐」。
切换前规约的原文归档在 [`th185_web/docs/HISTORY-ROUTE-SWITCH.md`](th185_web/docs/HISTORY-ROUTE-SWITCH.md)，
其中第 3 条曾明确写着「本仓库不做字节级 matching」——该条自本文件生效之日起**作废**。

切换后果（务必认清，不要假装两者兼容）：

| | 重建（旧） | 匹配（新） |
|---|---|---|
| 产物 | WASM，可在浏览器跑 | 与原版逐字节相同的 PE32，跑在 Windows |
| 验收 | 9/9 通关、faults=0 | `.text` diff=0，终局整二进制链接匹配 |
| 编译器 | Emscripten（任意） | **必须锁死原版 MSVC 工具链** |
| 现有 64 个 .cpp / 14413 行 | 是资产（语义已解） | **不能直接搬**，需按 §5 Phase A 重写并过 G1–G6 |

**当前状态：Bootstrap 未完成，禁止开始匹配。** 见 §0.1。

---

## 0.1 本仓库的实现进度（诚实记录，禁止粉饰）

| 项 | 状态 |
|---|---|
| 规约（本文件） | ✅ 已写入 |
| `/tools` 六个门槛脚本 | ❌ **未实现**——按 §4，未实现前禁止开始匹配 |
| `/orig` 只读镜像 | ⚠️ 原版 `th185.exe` 未入库（.gitignore），仅 `verify-target.mjs` 校验哈希 |
| `toolchain.lock` | ❌ 未生成；已知的编译器事实见 §0.2 |
| `/types/db.json` + `lock.json` | ❌ 未建 |
| `/evidence` `/ledger` `/dashboard` | ❌ 未建 |
| 目录迁移（旧布局 → §3 布局） | ❌ 未做；现有代码在 `th185_web/` 下 |

**按 §4 实现顺序：G1→G2→G4→G3→G5→G6。在六个脚本全部跑通之前，
任何「match:」提交都视为违反 §12 反作弊条款。**

---

## 0.2 th185 目标二进制实测事实（可直接用于 toolchain.lock 与 G2 见证）

以下数字来自对本机 v1.00a 原版 `th185.exe` 的直接测量，不是推测。

### 文件与 PE 头

| 项 | 值 |
|---|---|
| 文件大小 | `883200` 字节 |
| SHA-256 | 见 `th185_web/target.json`（`verify-target.mjs` 校验） |
| 机器 | `0x014C` (i386 / PE32)，非 x64 |
| Optional magic | `0x010B` (PE32) |
| ImageBase | `0x00400000` |
| EntryPoint RVA | `0x0009003B` |
| TimeDateStamp | `1659144319` = 2022-07-30T01:25:19Z |
| PECheckSum | `0x000DE79F`（重算算法见归档旧规约的 3.2 节：`th185_web/docs/HISTORY-ROUTE-SWITCH.md`，必须整体跳过 4 字节） |
| 节数 | 4 |

### 节表（逐字节匹配的战场）

| 节 | VAddr | VSize | RawOff | RawSize | 特征 |
|---|---|---:|---:|---:|---|
| `.text` | `0x00001000` | `711114` | `0x000400` | `711168` | `0x60000020` CODE |
| `.rdata` | `0x000AF000` | `97948` | `0x0ADE00` | `98304` | `0x40000040` |
| `.data` | `0x000C7000` | `734132` | `0x0C5E00` | `65536` | `0xC0000040` |
| `.rsrc` | `0x0017B000` | `7168` | `0x0D5E00` | `7168` | `0x40000040` |

> `.data` 的 VSize(`734132`) 远大于 RawSize(`65536`)：差额是 **BSS 式未初始化数据**，
> 在文件里不存在，靠加载器清零。最终「整二进制链接匹配」包含这部分布局，
> 但文件级 diff 只在 `.text`/`.rdata`/`.rsrc` 的 RawSize 范围内可比。

### 编译器指纹（Rich Header，key = `0x0D2865FF`）

> ★★★ **2026-11-24 重大修正。本节此前整段是错的，原始错误记录保留在下方。**
>
> 旧版本把 `27412 / 31328 / 31332` 当作三个 build 号，得出「三档 VS2019 并存」的结论，
> 并把 `toolchain.lock` 标为 `BLOCKED`。**这个结论已被推翻。**
>
> **错因**：Rich Header 每个条目是两个 DWORD `(comp_id, count)`，其中
> `comp_id` 高 16 位 = **build**（真正的版本号），低 16 位 = **prodId**（产品线编号）。
> 旧解析把两个字段**读反了**，于是把 prodId 当成了 build。
>
> **怎么发现的**：准备安装 VS2019 之前，先查本机 MSVC 的 FileVersion ——
> `MSVC 14.44.35213.0 → FileVersion 19.44.35213.0`，第三段 `35213` 才是 build 号量级。
> 27412 显然不是这个量级，于是重解析。**本机就装着 VS2022，一眼就能看出来的事。**

**正确的表**（按 prodId 归组，build 是那些小数字）：

| prodId | 单元数 | minibuilds | 含义 |
|---|---:|---|---|
| **27412** | 237 | 257 / 259 / 260 / 261 / 262 | **VS2017 15.x MSVC**（productId 2712） |
| **31328** | 89 | 253 / 259 / 260 / 261 | **VS2019 16.x MSVC** |
| **31332** | 81 | 255 / 258 / 265 | **VS2019 16.x MSVC**（可能与 31328 同源） |
| 30729 | 7 | 131 / 147 | SDK / import lib |
| 30319 | 3 | 156 / 171 | SDK / import lib |
| 21202 | 2 | 156 | SDK / import lib |
| 50727 | 2 | 123 | SDK / import lib |
| 0 | 202 | 0 / 1 / 151 | 未标记 / 链接器自身 |

**修正后的结论（已由实测确认，见下方「工具链实测」）**：

- `toolchain.lock` 的 `compiler._status` 从 `BLOCKED` 改为 `VERIFIED_FOR_ONE_FUNCTION`。
- **P1 已可执行**：用 MSVC v142 (`19.29.30159`) + `/MT /O2 /GS- /Oy-` 对 17 个函数验证，
  13 个字节完全相等（76.5%）。
- `prodId` 的真实含义**仍未确定**：`27412=0x6B14`、`31328=0x7A60`、`31332=0x7A64` 的高字节
  `0x6B`/`0x7A` **不符合已知 MSVC 编译器编码**（`0xD1`–`0xDC` 系列 = VS2005–VS2022）。
  这些更可能是**工具类别**（链接器 / CVT / 资源编译器）的 prodId，而不是编译器版本。
- **「需要两个 VS 世代」这个推论已失效** —— 本机缓存里只有 `14.29` 和 `14.44`，**没有 VS2017 的 `14.16`**，
  而 v142 已经能匹配大部分函数。

### 工具链实测（2026-11-24）

```
编译器 : MSVC v142 19.29.30159 (x86)
来源   : 从 C:\ProgramData\Microsoft\VisualStudio\Packages 的 .vsix 解压
         到 D:\MSVC142\14.29.30133，**不需要安装器和管理员权限**
选项   : /MT /O2 /GS- /Oy-
验证器 : tools/check_bytes.py
结果   : 13 / 17 字节相等
```

### 三条从实测中得到的代码生成规则

| 规则 | 证据 |
|---|---|
| **`/Oy-` 保留栈帧** | 原版多数函数有 `push ebp / mov ebp,esp`，而 `/O2` 默认省略。`/Od` 会引入多余局部变量，`/Ob0` 无效，只有 `/Oy-` 精确命中 |
| **12 字节聚合类型必须 struct 整体赋值** | 逐字段赋值得 3 条 `mov`（18 字节）；`*(V3*)dst = *(const V3*)src` 得 SSE2 `movq`+`mov`，与原版一致 |
| **返回 `int` 与 `bool` 走不同路径** | `int` 用 `xor eax,eax` + `cmp [mem],eax`；`bool` 用 `cmp [mem],0`。判据：原版有无 `33c0` 前缀 |

### 两个未解（已标 UNRESOLVED）

| 函数 | 现象 | 已排查 |
|---|---|---|
| `0x00404B60` `read_first_float` | 原版 `movss xmm0,[ecx]`，MSVC 只生成 `fld dword [ecx]` | 14 种选项组合全部 `fld` |
| `0x00409C88` `encode_value` | 原版有 `mov edi,edi`（hotpatch 序言）+ `push 0x20/pop ecx` | 25 种组合 0 匹配 |
| `0x00404B70` 等 `fsincos` | 原版用 x87 `fsincos` 复合指令 | MSVC 前端不合成 `fsincos`；3 种写法均生成函数调用 |

证据：`evidence/rich-header-correction.json`

#### 原始错误记录（保留，不删）

<details><summary>旧的「三档 build 并存」表述 —— 已被推翻</summary>

```
| Product | build | count |
| Prod259 / Prod261 / Prod260 / Prod257 / Prod262 | 27412 | 33 / 164 / 27 / 12 / 1 |
| Prod253 / Prod260 / Prod259 / Prod261 | 31328 | 1 / 18 / 23 / 47 |
| Prod265 / Prod255 / Prod258 | 31332 | 79 / 1 / 1 |

「同一 EXE 内混用三档 C++ 编译器... 要复现，toolchain.lock 必须能同时提供三套 cl.exe/库，
  或证明其中两档对最终 .text 无影响。这一条没解决之前，不要开始 Phase B。」
```

上述表述的错误在于：`27412/31328/31332` 不是 build，是 prodId。

</details>

> **教训（写进规约）**：任何「看起来像版本号」的数字，在据此做出结构性判断
> （尤其是「需要 N 套工具链」「不可能」这类结论）之前，必须用一个**已知的样本**交叉验证。
> 本次代价：P1 验证挂了数十轮，错误结论还写进了 README 与 AGENTS.md 本身。

### 已验证的原版地址（来源：thprac，实战验证，可信度最高）

| 符号 | VA | 用途 |
|---|---|---|
| `CARD_DESC_LIST` | `0x004CA370` | 87 张卡表（68 字节/项） |
| `BLACK_MARKET_PTR` | `0x004D7AC4` | 黑市对象指针 |
| `ENEMY_MANAGER` | `0x004D7AF4` | 敌人管理器指针（`+0x324` 波次权重） |
| `GAME_THREAD_PTR` | `0x004D7B0C` | 游戏线程指针 |
| `SCALE_ADDR` | `0x00573DC0` | 缩放常量 |
| `AddCard` 调用点 | `0x00414F20` | Thiscall(ecx, cardId, 2) |
| `funds` | `0x004D106C` | 持久金钱，上限 999990 |
| `bulletMoney` | `0x004D1070` | 弹货，int32，两份副本 `+0x74` |

> 这些地址是 **Bootstrap 期唯一的可信锚点**，应优先写成 G2 的第一个见证用例。

---

## 0. 最高原则
1. 编译是多对一映射。字节一致 ≠ 还原原始代码。找"最可能的原像"，不是"任意原像"。
2. 目标函数是字典序(§1)，不是加权和。
3. 每个源码元素必须有证据(二进制见证或跨函数一致性)。作不出证就删掉或标 unresolved。
4. 禁止为通过检查而欺骗检查(§12)。
5. /orig、已锁定类型、工具链配置只读。

## 1. 目标函数(字典序，前项分胜负就不看后项)
- P0: G1–G6 全 PASS。任一 FAIL 直接淘汰，不看 diff。
- P1: 目标函数 .text diff=0；终局要求整二进制链接匹配。
- P2: 结构复杂度最小(循环/分支与二进制对齐，token/指令比低)。
- P3: 新增自由度最少(新常量/新类型/新声明顺序越少越好)。
- P4: 跨独立运行结构共识最高。
禁止 P0 与 P1 加权。G2 FAIL 但 diff=0 的候选，劣于全 PASS 但 diff=12 的候选。

## 2. 工具链(只读，钉死)
- 以 toolchain.lock 为准：编译器精确版本、flags、链接器、SDK 头文件 hash、镜像 digest、链接顺序。
- 禁止为凑匹配改编译器/flags/链接顺序/头文件。认为必须改 → 停下写 conflict proposal。
- 构建可复现：禁 __DATE__/__TIME__、路径嵌入、随机 PDB GUID。连编两次 hash 一致。
- /orig 只读。

## 3. 仓库布局
```
/orig       原始二进制+提取obj(只读)
/src        重建源码
/include    共享头文件，唯一允许定义 struct/enum/typedef 处
/types      db.json + lock.json
/evidence   witness map、assert提取、RTTI、vtable、指纹
/ledger     hack ledger、冲突提案、unresolved
/dashboard  指标输出
/tools      门槛脚本
```
- 类型只允许在 /include 定义并注册进 /types/db.json。禁 .cpp 就地造类型。
- 函数边界与二进制 1:1。禁拆/合函数、跨 TU 挪代码。

## 4. Bootstrap
/tools 门槛脚本未实现 → 先实现再匹配。禁止"先匹配着看看"。
实现顺序：G1→G2→G4→G3→G5→G6。
每脚本输出机器可读 PASS/FAIL+原因+证据，写入 /dashboard。

## 5. 工作流
两阶段，禁跳步：
- Phase A 语义重建：写人看的代码，行为等价、结构合理、命名有意义。此阶段不看 diff。
- Phase B 字节微调：只允许 §7 白名单编辑，每处登记二进制理由。
依赖顺序：叶子/简单函数先行 → 定 struct 与签名 → ≥2 独立函数确认即锁定 → 向上推，后来者服从锁定。
终局：整二进制链接匹配(含 .data/.rdata/vtable/reloc)。

## 6. 硬门 G1–G6(任一 FAIL 即拒)
G1 全局类型一致性 [tools/check_types.py]
- 所有访问(基类型,偏移,宽度,解释类型)进全局表，冲突即 FAIL。
- 锁定字段/签名不可被新函数推翻；要改走 conflict proposal。
- 签名与所有调用点一致。

G2 二进制见证 [tools/check_witness.py]
- 每个变量/常量/字段/call 必须映射到二进制 artifact(栈槽/立即数/偏移/调用目标/reloc)。
- 优化后消失且无见证(dummy/魔法pad) → FAIL。
- 反向覆盖：二进制每个栈槽/立即数/call 都被源码解释，覆盖率=100%。
- 产出 evidence/{func}.witness.json。

G3 结构对齐 [tools/check_structure.py]
- 源码循环数 ≈ 二进制 CFG 回边数；二进制有循环而源码全展开 → FAIL。
- 源码分支数不得显著超二进制(asm直译特征) → FAIL。
- 控制流必须可归约；irreducible loop → FAIL。
- token/指令比、魔法常量数设上限(tools/config.json)。

G4 规范形+黑名单 [tools/check_style.py]
- 提交前过 AST 归一化 + clang-format。
- 规范形：能 for 不 while；能下标不指针算术；能早返回不深嵌套；命名从词典。
- 黑名单(出现即 FAIL)：goto、内联asm、pad/dummy/unused、命名 v\d+/a\d+/temp\d+/sub_[0-9a-f]+、裸偏移强转、未登记魔法数(0/1/-1/sizeof派生/命名常量除外)、allowlist 外的优化 pragma/attribute。

G5 无UB+扰动稳定 [tools/check_stability.py]
- UBSan/ASan 开；-O0/-O2/-Os 各编一份跑同组随机输入，行为必须一致，否则 FAIL。
- 安全扰动(改注释/格式/重命名局部/字面量换constexpr)后 .text 必须不变，否则 FAIL。

G6 独立证据 [tools/check_evidence.py]
- assert/日志字符串提取的表达式与字段名必须采用。
- RTTI/vtable/.pdata 的类名/继承/函数边界必须吻合。
- 库/引擎指纹命中 → 用真实已知源码，禁编造。
- 多反编译器(Ghidra/IDA/retdec)类型/结构判断 ≥2 一致才采纳。

## 7. 编辑约束
Phase B 白名单(仅允许)：
- 独立语句顺序调整
- 从类型库选类型(不得新造)
- 有调用图证据的 inline 决策
- 规范形集合内的循环/分支写法
- 折叠后完全等价的常量表示
全局黑名单(任何阶段)：
- G4 全部条目；改锁定类型；改工具链/flags//orig；拆合函数/跨TU挪代码；加无见证元素；关 sanitizer/压 warning/改门槛脚本。
每处非常规编辑写 /ledger：改了什么/为什么/二进制见证/影响哪些门。

## 8. 卡住时强制行为
1. 不许加 dummy/pad/魔法常量。
2. 不许改 flags/编译器。
3. 不许静默改锁定类型。
4. 标 unresolved，记残差 diff 与假设，写 /ledger/unresolved。
5. 认为锁定类型错 → 提 conflict proposal 附 ≥2 独立函数证据，等裁决，不动手。
unmatched 永远优于 garbage match。

## 9. 每函数必产工件(缺一不可)
- /src/{func}.cpp(已过规范形)
- evidence/{func}.witness.json(双向100%)
- /ledger 对应条目(可空但必须存在)
- G1–G6 全 PASS 报告
- 安全扰动存活报告
- 推断名字标 inferred，不得冒充已知

## 10. 回归与提交
- 一次一个函数或一小内聚批次。
- 跑全量回归，已匹配函数不得退化。
- 附门槛报告全 PASS 才提交。
- 提交格式：
```
match: {func} [G1-G6:PASS] [diff:0] [witness:100%]
```
- 锁定类型变更单独提交，附 conflict proposal 编号。

## 11. 仪表盘(人类唯一审查对象)
每次运行更新 /dashboard/metrics.json：
- type_conflicts 类型冲突数：>0 停推进先解决
- witnessless_elements 无见证元素数：>0 拒收
- ub_reports sanitizer/跨优化级分歧数：>0 拒收
- perturbation_survival 扰动存活率：<100% 标脆弱
- hack_count_trend hack趋势：超线性增长→回滚
- cross_run_consensus 跨运行共识：低→标欠定
- unresolved_count 未匹配数：正常存在，不算失败
- coverage 已匹配/总数：单调上升
人类审数字不读 cpp。禁止粉饰。

## 12. 反作弊(等同任务失败)
- 改名绕 G4(如 v1→enemyIndex 但保留 goto 汤)
- 修改/禁用/绕过 /tools 脚本
- 关 sanitizer/压 warning/跳回归
- 伪造 witness/ledger/仪表盘
- 挪复杂度到头文件/其他TU/其他函数让当前函数变绿
- Phase A 未完成就进 Phase B 爬 diff
- 把"字节一致"宣称为"还原原始代码"

## 13. 诚实边界
- 名字/注释原则上不可恢复(除非 debug/assert/RTTI 证据)。不编造历史。
- 自动化能保证一致/可维护/行为正确，不能保证历史原貌。
- 若目的是可重编译/可mod，主目标可改"行为等价+全局一致"，字节降为验证信号。须人类显式批准，Agent 不得自切。

本文件受版本控制，修改需人类显式批准。Agent 不得自行放宽任何门槛。

---

## 附录 A：th185 特有的匹配风险（不是通用条款，是本作实测）

1. **三档编译器并存**（见 §0.2）。这是 P1 的第一个真障碍，必须先解。
2. **`.data` 有 668596 字节 BSS**。vtable / 函数指针表落在未初始化区，
   文件里看不到初值，需要靠 `.text` 里的构造函数写入序列反推。
3. **ECL / ANM / STD 是数据驱动**，格式已完全解出（见 `th185_web/docs/`）。
   匹配只需匹配**解释器代码**，不需要匹配数据文件——这是本作比 TH06 省力的地方。
4. **RNG 必须逐位一致**。波次从 110 条模板池抽取，调用次序差一步就分叉。
   RNG 实现应是最先匹配的函数之一（短、纯、可穷举验证）。
5. **无 Replay**。所以「同输入跑原版逐帧比对」这条路不存在，
   §6 G5 的「同组随机输入行为一致」是本作唯一可用的等价性杠杆。

## 附录 B：旧仓库资产的处置原则

切换前 156 个 commit 的语义成果（PBG4 解密、LZSS、ANM、ECL 114 文件解析、
卡表 87 项字段语义、双货币、魔方阵、黑市）**全部保留为 Phase A 的输入**：

- 语义已解的，直接写进 `/src` 的 Phase A 版本，不用重新逆向；
- 但**不得**因为「旧代码能跑」就跳过 G1–G6；
- 旧实现凡是有**标定值**（如 `PLAYER_BASE_DAMAGE = 4`、魔方阵半径 44、持续 45 帧）的，
  必须在 Phase A 阶段替换为**从原版二进制见证得到的真值**，否则 G2 直接 FAIL。

## 附录 C：本文件的变更纪律

- 本文件受版本控制；**修改需人类显式批准**。
- Agent 不得自行放宽任何门槛（G1–G6、§7 白名单、§12 反作弊）。
- 若发现某条款在本作物理上不可满足（例如三档编译器无法同时提供），
  按 §8 第 5 条提 **conflict proposal**，附 ≥2 独立证据，等人类裁决；
  **不得自行降门槛继续推进**。
