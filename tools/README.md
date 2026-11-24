# /tools — 门槛脚本

**AGENTS.md §4：这六个脚本未实现前，禁止开始匹配。**

| 顺序 | 脚本 | 硬门 | 检查什么 |
|---|---|---|---|
| 1 | `check_types.py` | G1 | 全局类型一致性；所有访问进全局表，冲突即 FAIL |
| 2 | `check_witness.py` | G2 | 双向 100% 二进制见证；产出 `evidence/{func}.witness.json` |
| 3 | `check_style.py` | G4 | 规范形 + 黑名单（goto / 内联 asm / pad / 裸偏移强转 / 未登记魔法数） |
| 4 | `check_structure.py` | G3 | 循环数 vs CFG 回边数；分支不显著超二进制；可归约性 |
| 5 | `check_stability.py` | G5 | UBSan/ASan；-O0/-O2/-Os 行为一致；安全扰动后 .text 不变 |
| 6 | `check_evidence.py` | G6 | assert/RTTI/vtable/.pdata；多反编译器 ≥2 一致 |

## 第 7 个脚本（非门槛）：`check_bootstrap.py`

不是 G1–G6 之一，而是**元检查**：防止 V1 违规（Bootstrap 未完成就提交 match:）复发。

检查六件事：

1. 六个门槛脚本都存在
2. 六个都能被 `ast.parse` 解析
3. 六个都跑过（dashboard 里有 `_gN_last_run` 记录）

**不要求六门都 PASS**（G2/G5 因客观缺口无法 PASS），
只要求「六个都实现且都跑过」—— 这与 §0.1 的措辞一致。

```powershell
python tools/check_bootstrap.py   # 0 = 可以提交 match，1 = 禁止
```

## 输出契约

每个脚本必须：
- 退出码 0 = PASS，非 0 = FAIL
- stdout 打印机器可读的 PASS/FAIL + 原因 + 证据路径
- 结果写入 `/dashboard/metrics.json` 的对应字段

## 禁止

- 修改/禁用/绕过本目录脚本（AGENTS.md §12，等同任务失败）
- 关闭 sanitizer / 压 warning / 跳回归
