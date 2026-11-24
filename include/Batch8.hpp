#pragma once

/**
 * 第 8 批：常量返回。
 *
 * 这两个函数本身极简，价值在于它们**推翻了一条错误记录**：
 * `return 1` 并非「物理不可得」，只是需要 `/O1` 而非 `/O2`。
 */

namespace th185 {
namespace b8 {

int __fastcall return_one(void* self, void* unused_edx);
int __fastcall return_zero(void* self, void* unused_edx);

}  // namespace b8
}  // namespace th185