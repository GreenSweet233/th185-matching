# /orig — 原始二进制（只读）

**AGENTS.md §0 第 5 条：/orig 只读。**

原版 `th185.exe` **不入库**（版权 + 体积）。本目录在开发者机器上应放：

- `th185.exe` — v1.00a 原版，哈希见 `toolchain.lock` 的 `target.sha256`
- `*.obj` — 若将来能从链接期信息提取

校验方式：`th185_web/scripts/verify-target.mjs`。

## 为什么这里是空的

原版文件由使用者在本地提供。仓库只保存**哈希与指纹**（`toolchain.lock`、`th185_web/target.json`），
不保存二进制本体。这是 AGENTS.md 之外的既有约定（见归档的旧规约第 1 条铁律）。
