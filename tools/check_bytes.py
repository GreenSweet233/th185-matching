#!/usr/bin/env python3
r"""P1 字节级匹配验证器：编译源码，与 /orig 的 .text 逐字节比对。

## 工作方式

  1. 从 src/ 里找带 //@witness func=0xNNN 的函数
  2. 用 MSVC v142 + 指定 flags 编译成 .obj
  3. 解析 COFF 取 .text 段
  4. 与原版对应地址的字节比对

## 编译器来源（不需要管理员权限）

  D:\MSVC142\14.29.30133  从 ProgramData 缓存里的 .vsix 解压而来
  见 evidence/P1-first-byte-match.json

## 退出码
  0 = 全部字节相等
  1 = 有差异
  2 = 环境错误
"""

import argparse
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_common
import check_witness as cw

MSVC_ROOT = r"D:\MSVC142\14.29.30133"
CL = os.path.join(MSVC_ROOT, "bin", "Hostx86", "x86", "cl.exe")
# INCLUDE 路径：MSVC 的头 + Windows SDK 的 UCRT/shared/um
#
# ★ 只用 MSVC 的 include 会在 <math.h> 上失败 ——
#   math.h 在 Windows SDK 的 ucrt 目录里，不在 MSVC 工具集里。
WINSDK = r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0"
INCLUDE_DIRS = [
    os.path.join(MSVC_ROOT, "include"),
    os.path.join(WINSDK, "ucrt"),
    os.path.join(WINSDK, "shared"),
    os.path.join(WINSDK, "um"),
]
LIB = os.path.join(MSVC_ROOT, "lib", "x86")
BIN = os.path.join(MSVC_ROOT, "bin", "Hostx86", "x86")


def parse_coff_text(path):
    """从 COFF 目标文件里取代码段字节。"""
    raw = open(path, "rb").read()
    machine, nsec, tds, symptr, nsym, optsz, chars = struct.unpack_from("<HHIIIHH", raw, 0)
    strtab_off = symptr + nsym * 18
    off = 20 + optsz
    best = None
    for i in range(nsec):
        s = raw[off:off+40]
        nb = s[0:8]
        if nb.startswith(b"/"):
            try:
                noff = int(nb[1:].rstrip(b"\0"))
                end = raw.index(b"\0", strtab_off + noff)
                name = raw[strtab_off + noff:end].decode("ascii", "replace")
            except Exception:
                name = "?"
        else:
            name = nb.rstrip(b"\0").decode("ascii", "replace")
        vsize, vaddr, rawsize, rawptr, relptr, lnptr, nrel, nln, flags = struct.unpack_from(
            "<IIIIIIHHI", s, 8)
        if name.startswith(".text") and (best is None or rawsize > best[1]):
            best = (name, rawsize, raw[rawptr:rawptr + rawsize])
        off += 40
    return best


def compile_cpp(source_text, flags, workdir):
    """编译一段 C++，返回 .obj 路径。"""
    cpp = os.path.join(workdir, "t.cpp")
    obj = os.path.join(workdir, "t.obj")
    with open(cpp, "w", encoding="utf-8") as fh:
        fh.write(source_text)
    env = dict(os.environ)
    env["INCLUDE"] = os.pathsep.join(INCLUDE_DIRS)
    env["LIB"] = LIB
    env["PATH"] = BIN + os.pathsep + env.get("PATH", "")
    cmd = [CL, "/nologo", "/c"] + flags + ["/Fo" + obj, cpp]
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                           errors="replace", timeout=120, env=env, cwd=workdir)
    except (subprocess.TimeoutExpired, OSError) as exc:
        return None, "cl.exe failed: %s" % exc
    if not os.path.isfile(obj):
        return None, (p.stdout or "") + (p.stderr or "")
    return obj, ""




def parse_coff_relocs(path, section_bytes):
    """解析 COFF 重定位表，返回需要屏蔽的字节区间列表。

    ## 为什么需要这个

    `call rel32` 在未链接的 .obj 里位移是占位值（0 或 -5），只有链接后才是真实值。
    但我们比对的是**单个函数**，不需要真的链接 —— 只需知道「这 4 个字节是重定位占位，
    不能参与比对」，把它们排除掉即可。

    ## 返回

    区间列表 [(start, size), ...]，size 由重定位类型决定：
      REL32 / REL32_1..5  -> 4 字节
      ADDR32              -> 4 字节
      其他                -> 4 字节（保守）
    """
    raw = open(path, "rb").read()
    _machine, nsec, _tds, symptr, nsym, optsz, _chars = struct.unpack_from(
        "<HHIIIHH", raw, 0)
    off = 20 + optsz
    ranges = []
    for _ in range(nsec):
        s = raw[off:off+40]
        if len(s) < 40:
            break
        _vs, _va, rs, _rp, relp, _lnp, nrel, _nln, _fl = struct.unpack_from(
            "<IIIIIIHHI", s, 8)
        # 只关心本次实际取用的那段代码所在节（size 匹配的）
        if rs == len(section_bytes) and nrel > 0:
            for k in range(nrel):
                ro = relp + k * 10
                try:
                    va, _symidx, rtype = struct.unpack_from("<IIH", raw, ro)
                except struct.error:
                    break
                ranges.append((va, 4))
        off += 40
    return ranges


def expand_project_includes(text, include_dir, seen=None, depth=0):
    """把 `#include "X.hpp"` 就地展开成 X.hpp 的内容（递归）。

    ## 为什么需要（BUG FIX C）

    本脚本原来只处理 `#include <...>`（系统头），**完全忽略** `#include "..."`
    （工程头）—— 行被直接丢掉。后果是工程头之间无法复用：

        想按 AGENTS.md §3「类型只允许在 /include 定义」把结构体布局集中到
        Object.hpp，让 SseAccess.hpp 去 `#include "Object.hpp"`，
        结果 Object.hpp 的内容根本进不了翻译单元，编译报 "未定义的类型"。

    于是每个 hpp 只能各自重复一份布局 —— 而这正是 G1「同一 struct 在多个 TU
    里定义必须一致」要防的漂移。

    修法：把引号形式的工程头就地展开。只认 include/ 下真实存在的文件，
    其余（如 <cmath>）不动。

    ## 防环

    seen 记录已展开过的绝对路径，重复时返回空串，避免 A->B->A 无限递归。
    """
    import re as _re
    if seen is None:
        seen = set()
    if depth > 8:
        return text
    out = []
    for ln in text.split("\n"):
        m = _re.match(r'\s*#\s*include\s+"([^"]+)"\s*$', ln)
        if not m:
            out.append(ln)
            continue
        rel = m.group(1).replace("\\", "/")
        cand = os.path.join(include_dir, rel)
        if not os.path.isfile(cand):
            # 工程头找不到：原样保留会被编译器报错，但那是真实错误，不掩盖。
            out.append(ln)
            continue
        ap = os.path.abspath(cand)
        if ap in seen:
            continue
        seen.add(ap)
        try:
            with open(cand, encoding="utf-8", errors="replace") as fh:
                inner = fh.read()
        except OSError:
            out.append(ln)
            continue
        out.append(expand_project_includes(inner, include_dir, seen, depth + 1))
    return "\n".join(out)


def hpp_namespace_chain(hpp_text):
    """返回 hpp 在**文件作用域**上依次打开的 namespace 名列表。

    ## 为什么需要（BUG FIX B）

    本脚本拼翻译单元时，把 hpp 的声明放在前面、把从 .cpp 抠出的**函数定义**
    放在后面。若 hpp 里的 struct 包在 `namespace th185 { ... }` 中，
    而函数定义是 `int Obj00::get_00() {...}`（hpp 的 namespace 包裹没有一并带过来），
    MSVC 会报：

        error C2653: "Obj00": 不是类或命名空间名称
        error C2065: "f_00": 未声明的标识符

    修法：把函数定义放回 hpp 当初打开的同一串 namespace 里。

    ## 口径

    只收「在花括号深度 0 处」出现的 `namespace X {`。
    深度用逐行累加花括号估算（hpp 里不会有字符串/注释干扰到根层结构）。
    """
    # 用「栈」记录当前打开的 namespace，并保留**遇到的最深一链**。
    #
    # 旧版只在花括号深度 0 处收集，结果是 `namespace th185 { namespace misc {`
    # 只识别出 th185，漏掉 misc —— 函数被塞进错误的命名空间，报 C2653。
    # 本项目确实存在两层嵌套（Misc.hpp 的 th185::misc、FieldWriter.hpp 的 th185::fw），
    # 所以必须按栈处理，不能只看最外层。
    stack = []
    opened = []  # (name, depth_when_opened)
    deepest = []
    depth = 0
    for ln in hpp_text.split("\n"):
        s = ln.split("//")[0]
        m = re.match(r"\s*namespace\s+([A-Za-z_]\w*)\s*\{", s)
        if m:
            stack.append(m.group(1))
            opened.append((m.group(1), depth))
            if len(stack) > len(deepest):
                deepest = list(stack)
        depth += s.count("{") - s.count("}")
        # 弹出已经关闭的 namespace
        while opened and depth <= opened[-1][1]:
            opened.pop()
            stack.pop()
    return deepest


def bytes_match_with_relocs(ours, orig, relocs):
    """比较两段字节，忽略重定位占位区。

    返回 (是否相等, 差异描述)。
    """
    if len(ours) != len(orig):
        return False, "length differs: %d vs %d" % (len(ours), len(orig))
    masked = bytearray(len(ours))
    for start, size in relocs:
        for k in range(size):
            if start + k < len(masked):
                masked[start + k] = 1
    diffs = []
    for idx in range(len(ours)):
        if masked[idx]:
            continue
        if ours[idx] != orig[idx]:
            diffs.append(idx)
    if not diffs:
        return True, "%d 字节中 %d 字节是重定位占位（已忽略）" % (len(ours), sum(masked))
    return False, "非重定位区差异在: %s" % diffs[:8]

def original_bytes(sections, image_base, va, size):
    for name, s in sections.items():
        lo = image_base + s["va"]
        if lo <= va < lo + len(s["data"]):
            off = va - lo
            return s["data"][off:off + size]
    return None


def main():
    ap = argparse.ArgumentParser(description="P1 byte-level match")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    ap.add_argument("--exe", default=None)
    # ★ 默认值必须含 /Oy- —— 原版多数函数保留栈帧，而 /O2 默认省略。
    #   见 evidence/P1-flags-breakthrough.json
    ap.add_argument("--flags", default="/MT /O2 /GS- /Oy-")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--write-dashboard", action="store_true")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    if not os.path.isfile(CL):
        print("[FAIL] 找不到 cl.exe: %s" % CL)
        print("       见 evidence/P1-first-byte-match.json 的解包步骤")
        return 2

    exe = pe_common.find_exe(root, args.exe)
    if not exe:
        print("[FAIL] 找不到原版 th185.exe")
        return 1

    pe, sections = pe_common.load_pe(exe)
    image_base = pe.OPTIONAL_HEADER.ImageBase
    md = pe_common.make_disassembler()
    insns = pe_common.scan_text(sections, image_base, md)

    print("=== P1 byte-level match ===")
    print("  compiler: %s" % CL)
    print("  flags   : %s" % args.flags)
    print()

    flags = args.flags.split()
    results = []

    # ------------------------------------------------------------------
    # 目标列表：**从 //@witness 声明自动推导**，不再手工维护。
    #
    # 为什么改（2026-11-24）：
    #   旧版是一张手写的 71 项白名单。于是出现了一个隐蔽的漏洞 ——
    #   源码里声明了 witness、但忘了加进这张表的函数，**从未被字节比对过**，
    #   却在 dashboard / docs/MATCHED.md 里被算作「已匹配」。
    #   实测：127 个有源码的函数里有 57 个不在表内
    #   （Trivial.cpp 41 个 + Object.cpp 16 个），全部未验证。
    #   手工维护的清单必然会与源码漂移，改成自动推导后这类漏检不可能再发生。
    #
    # 附带修掉一处更隐蔽的问题：旧代码会在编译前**改写源码**
    # （need_this=True 时自动插入 __fastcall）。那意味着被验证的不是仓库里的代码。
    # 现在一律不改写，调用约定必须由源码自己写清楚。
    # ------------------------------------------------------------------

    # 每个地址的编译 flags 覆盖（只有极少数函数需要 /Os，见 toolchain.lock 规则 5）
    FLAG_OVERRIDES = {
        "0x0048F711": "/MT /Os /GS- /Oy-",
    }

    def _func_name_after(lines, marker_idx, fn_hint=None):
        """从 witness 标记行往下找函数定义，取出函数名。

        取「定义行里 "(" 之前最后一个标识符」——对
            int Obj00::get_00() { ... }
            void* return_579C68() { ... }
        都成立。
        """
        for j in range(marker_idx + 1, min(marker_idx + 20, len(lines))):
            s = lines[j].strip()
            if not s or s.startswith("//"):
                continue
            if "(" not in s:
                continue
            head = s.split("(", 1)[0]
            # ★ 必须返回 "Class::name" 而不是光秃秃的 "name"。
            #
            # 踩过的坑：FieldWriter 里有 8 个 `set`（ObjFw04::set、ObjFw08::set ...），
            # 旧代码只取最后一个标识符 -> 全都返回 "set" -> 定位时统统命中文件里
            # 第一个 `set(`，于是 8 个函数比的其实是同一段源码，
            # 除第一个外全部报 DIFF（而且差异看起来像"偏移错了"，极具误导性）。
            m_q = re.search(r"([A-Za-z_]\w*)\s*::\s*([A-Za-z_]\w*)\s*$", head)
            if m_q:
                return m_q.group(1) + "::" + m_q.group(2)
            ids = re.findall(r"[A-Za-z_]\w*", head)
            if not ids:
                continue
            # 去掉 __fastcall / __stdcall / __cdecl 这类关键字
            ids = [x for x in ids if not x.startswith("__")]
            if ids:
                return ids[-1]
        return fn_hint

    decls = cw.parse_witnesses(os.path.join(root, "src"))

    targets = []
    _seen = set()
    for _addr in sorted(decls):
        if _addr.startswith("<file:"):
            continue
        _item = next((it for it in decls[_addr] if it["kind"] == "func"), None)
        if _item is None:
            continue
        _src_file = os.path.basename(_item["file"])
        if not _src_file.endswith(".cpp"):
            continue
        try:
            with open(_item["file"], encoding="utf-8", errors="replace") as _fh:
                _lines = _fh.read().split("\n")
        except OSError:
            continue
        _fn = _func_name_after(_lines, _item["line"] - 1)
        if not _fn:
            print("  (warn: cannot infer function name for %s in %s)"
                  % (_addr, _src_file))
            continue
        _key = (_addr, _src_file, _fn)
        if _key in _seen:
            continue
        _seen.add(_key)
        targets.append((_addr, int(_addr, 16), _src_file, _fn, False,
                        FLAG_OVERRIDES.get(_addr)))

    print("  targets : %d（由 //@witness 自动推导）" % len(targets))
    print()
    for label, va, src_name, fn_name, need_this, *rest in targets:
        extra_flags = rest[0] if rest else None
        lo, hi = cw.function_extent(label, insns)
        if lo is None:
            results.append({"func": label, "status": "ERROR", "error": "no extent"})
            continue
        orig = original_bytes(sections, image_base, lo, hi - lo)

        src_path = os.path.join(root, "src", src_name)
        if not os.path.isfile(src_path):
            results.append({"func": label, "status": "SKIP", "error": "no " + src_name})
            continue
        with open(src_path, encoding="utf-8") as fh:
            src = fh.read()

        # 提取单个函数：定位**定义行**。
        #
        # ★ 2026-11-24 重写。旧版是「先在 //@witness 注释附近找、再全文兜底」，
        #   但兜底那条会在多个同名重载里命中第一个：
        #   FieldWriter 有 8 个 set()，于是 7 个函数比的是别人的源码。
        #   现在直接用**限定名**（Class::name）定位定义行，不再靠“附近”。
        lines = src.split("\n")
        idx = None
        import re as _re2

        def _is_definition(ln):
            """该行是否是 fn_name 的定义（而不是调用/声明）。"""
            if "//" in ln.split(fn_name)[0] if fn_name in ln else False:
                return False
            if not _re2.search(r"\b" + _re2.escape(fn_name) + r"\s*\(", ln):
                return False
            # 调用语句以 ; 结尾；定义后面跟 { 或换行后的 {
            stripped = ln.strip()
            if stripped.endswith(";"):
                return False
            return True

        for j, ln in enumerate(lines):
            if _is_definition(ln):
                idx = j
                break

        if idx is None:
            results.append({"func": label, "status": "SKIP",
                            "error": "cannot locate %s in %s" % (fn_name, src_name)})
            continue

        # 从函数定义起，做花括号配平
        depth = 0
        started = False
        body_lines = []
        for j in range(idx, len(lines)):
            ln = lines[j]
            body_lines.append(ln)
            depth += ln.count("{") - ln.count("}")
            if "{" in ln:
                started = True
            if started and depth == 0:
                break

        func_text = "\n".join(body_lines)

        # ★ 2026-11-24 移除「编译前自动插入 __fastcall」。
        #
        # 旧代码在 need_this=True 时会把 `void f(...)` 改写成
        # `void __fastcall f(...)` 再编译 —— 这意味着 **被比对的不是仓库里的代码**。
        # 一个源码里没写 __fastcall 的函数，也可能因此「匹配成功」。
        # 调用约定是源码事实，必须由源码自己写清楚，工具不得代劳。

        # 前置需要的定义：
        #   1) hpp 里的 constexpr / static const 常量
        #   2) cpp 里的匿名 namespace 块（辅助函数 / 辅助 struct）
        #   3) 外部符号声明（extern "C" 之类）
        prefix = ""
        hpp_path = os.path.join(root, "include",
                                src_name.replace(".cpp", ".hpp"))
        if os.path.isfile(hpp_path):
            with open(hpp_path, encoding="utf-8") as fh:
                for ln in fh.read().split("\n"):
                    s = ln.strip()
                    if s.startswith("constexpr") or s.startswith("static const"):
                        prefix += ln + "\n"
                    # ★ 2026-11-24 补：extern 变量声明也要带进来
                    #
                    # Misc.cpp 用 `extern unsigned g_at_4D74F8;` 声明全局变量，
                    # 定义在 .cpp 里。只提取 constexpr 会导致该变量未声明 →
                    # COMPILE_ERROR。
                    elif s.startswith("extern ") and s.endswith(";"):
                        prefix += ln + "\n"

        # 抠出所有 namespace { ... } 块（辅助定义）
        depth = 0
        in_anon = False
        anon = []
        for ln in lines:
            if not in_anon and ln.strip().startswith("namespace {") and ln.strip().endswith("{"):
                in_anon = True
                depth = 1
                anon.append(ln)
                continue
            if in_anon:
                anon.append(ln)
                depth += ln.count("{") - ln.count("}")
                if depth == 0:
                    in_anon = False
        prefix += "\n".join(anon) + "\n"

        # 外部符号声明（extern "C" 单行声明）
        for ln in lines:
            s = ln.strip()
            if s.startswith("extern \"C\"") and s.endswith(";") and "{" not in s:
                prefix += ln + "\n"

        body = prefix + "\n" + func_text
        body = "\n".join(ln for ln in body.split("\n")
                          if not ln.strip().startswith("#include")
                          and not ln.strip().startswith("#pragma")
                          and "//@witness" not in ln)

        # ★ 只补标准库头（不补工程头，那会引入完整依赖链）
        #
        # 之前完全滤掉 #include 导致 std::sin/std::cos 找不到；
        # 但全保留又会把 "Trivial.hpp" 带进来从而导致 0/17 全崩。
        # 折中：只保留 <...> 形式的系统头。
        sys_includes = [ln for ln in lines
                        if ln.strip().startswith("#include <")]

        # ★ 2026-11-24 补：工程头里的系统 include 也要带进来
        #
        # 例子：SseAccess.cpp 只 `#include "SseAccess.hpp"`，
        # 而 `<xmmintrin.h>` 在那个 .hpp 里。若只扫 .cpp，
        # `_mm_load_ss` 就找不到声明，整批函数全失败。
        hpp_for_inc = os.path.join(
            root, "include", src_name.replace(".cpp", ".hpp"))
        if os.path.isfile(hpp_for_inc):
            with open(hpp_for_inc, encoding="utf-8") as fh:
                for ln in fh.read().split("\n"):
                    s2 = ln.strip()
                    if s2.startswith("#include <") and ln not in sys_includes:
                        sys_includes.append(ln)

        if sys_includes:
            body = "\n".join(sys_includes) + "\n" + body

        # ★ 成员函数（`Cls::name`）需要 struct 定义
        #
        # 如果提取出的函数体里有 `::`，说明是成员函数，
        # 必须把 hpp 里的 struct/class 定义一并带进来，否则编译不过。
        # ★ 判据必须严格：只认「标识符::标识符(」这种成员函数签名，
        # 不能只看有没有 "::" —— Misc.cpp 的函数体里含 "th185::misc::" 命名空间，
        # 会被误判成成员函数，把整个 hpp 拼进来导致编译错误。
        import re as _re3
        # ★ 判据：只看**函数定义行**是否形如 `Ret Cls::name(`。
        # 不能扫整个 body —— Misc.cpp 里 `return th185::misc::g_v;` 也含 `::`，
        # 会被误判，把整个 hpp 拼进来导致编译错误。
        is_member = _re3.search(
            r"\b[A-Za-z_]\w*::[A-Za-z_]\w*\s*\("
            , func_text.split("\n")[0]) is not None
        if is_member and hpp_for_inc and os.path.isfile(hpp_for_inc):
            with open(hpp_for_inc, encoding="utf-8") as fh:
                hpp_text = fh.read()
            # BUG FIX C：把 `#include "..."` 工程头就地展开，
            # 否则 hpp 之间无法复用结构体定义（只能各自复制一份布局）。
            hpp_text = expand_project_includes(
                hpp_text, os.path.join(root, "include"))
            # 展开后可能带出新的系统头（如展开 Object.hpp 后出现 <xmmintrin.h>）
            for _ln in hpp_text.split("\n"):
                if _ln.strip().startswith("#include <") and _ln not in sys_includes:
                    sys_includes.append(_ln)
            decl_lines = []
            for ln in hpp_text.split("\n"):
                s2 = ln.strip()
                if s2.startswith("#pragma") or s2.startswith("#include"):
                    continue
                decl_lines.append(ln)

            # ---- BUG FIX A: declarations must come AFTER #include ----
            #
            # Old code did `body = decl_lines + body`, but `body` already
            # began with `#include <...>`, so the final TU order was:
            #     <struct decls using __m128>      <- BEFORE the include
            #     #include <xmmintrin.h>            <- too late
            # MSVC then reported C2061 (unknown __m128) / C2143, and the
            # whole member-function branch failed to compile.
            includes_part = ("\n".join(sys_includes) + "\n") if sys_includes else ""
            if includes_part and body.startswith(includes_part):
                tail = body[len(includes_part):]
            else:
                includes_part, tail = "", body

            # ---- BUG FIX B: put the definition back in the hpp namespace ----
            #
            # hpp declares `namespace th185 { struct Obj00 {...}; }` while
            # func_text is `int Obj00::get_00() {...}` with no namespace.
            # Splicing them verbatim gives C2653 "Obj00 is not a class or
            # namespace name".
            ns_chain = hpp_namespace_chain(hpp_text)
            if ns_chain:
                open_ns = "".join("namespace %s {\n" % n for n in ns_chain)
                close_ns = "".join("}\n" for _ in ns_chain)
                wrapped = open_ns + func_text + "\n" + close_ns
            else:
                wrapped = func_text

            # 保留 prefix（匿名 namespace 辅助定义 / extern 声明 / constexpr），
            # 它原本在 body 的 include 段之后、func_text 之前。
            if func_text in tail:
                prefix_part = tail.split(func_text, 1)[0]
            else:
                prefix_part = ""
            body = includes_part + "\n".join(decl_lines) + "\n" + prefix_part + wrapped

        use_flags = (extra_flags.split() if extra_flags else flags)
        with tempfile.TemporaryDirectory() as td:
            obj, err = compile_cpp(body, use_flags, td)
            if not obj:
                results.append({"func": label, "status": "COMPILE_ERROR", "error": err[:500]})
                continue
            parsed = parse_coff_text(obj)
            # relocation 表也要在临时目录销毁前读取
            _tmp_relocs = parse_coff_relocs(obj, parsed[2]) if parsed else []
            if not parsed:
                results.append({"func": label, "status": "COFF_ERROR"})
                continue
            _, _, code = parsed

        # ★ 用 relocation 感知的比较：call rel32 在未链接的 .obj 里是占位值，
        #   不能参与比对。这部分字节在链接时才被填成真实位移。
        relocs = _tmp_relocs
        same, detail = bytes_match_with_relocs(code, orig, relocs)
        entry = {
            "func": label,
            "status": "MATCH" if same else "DIFF",
            "orig_len": len(orig),
            "ours_len": len(code),
            "relocs": len(relocs),
            "detail": detail,
        }
        if not same:
            n = 0
            while n < min(len(code), len(orig)) and code[n] == orig[n]:
                n += 1
            entry["common_prefix"] = n
            entry["orig"] = orig.hex()
            entry["ours"] = code.hex()
        results.append(entry)

        shown = (" [%s]" % extra_flags) if extra_flags else ""
        print("  %-14s %-8s  orig=%d ours=%d %s%s" % (
            label, entry["status"], len(orig), len(code),
            "" if same else "(prefix %d)" % entry.get("common_prefix", 0), shown))

    print()
    n_match = sum(1 for r in results if r["status"] == "MATCH")
    n_total = len(results)
    print("  结果: %d / %d 字节相等" % (n_match, n_total))
    if n_match < n_total:
        for r in results:
            if r["status"] != "MATCH":
                print("  [FAIL] %s: %s" % (r["func"], r["status"]))
                if r.get("error"):
                    print("         %s" % r["error"][:200])
                if r.get("orig"):
                    print("         orig: %s" % r["orig"][:120])
                    print("         ours: %s" % r["ours"][:120])

    status = "PASS" if n_match == n_total and n_total > 0 else "FAIL"
    print()
    print("[%s] P1" % status)

    if args.write_dashboard:
        write_dashboard(root, results, status)

    return 0 if status == "PASS" else 1


def write_dashboard(root, results, status):
    path = os.path.join(root, "dashboard", "metrics.json")
    try:
        with open(path, encoding="utf-8") as fh:
            m = json.load(fh)
    except (OSError, json.JSONDecodeError):
        m = {}
    n_match = sum(1 for r in results if r["status"] == "MATCH")
    m["byte_match"] = {
        "status": status,
        "matched": n_match,
        "verified": len(results),
        "_note": "只覆盖能单独编译验证的函数；其余仍只有静态门槛",
    }
    m["updated_at"] = __import__("datetime").datetime.now().isoformat(timespec="seconds")
    try:
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(m, fh, indent=2, ensure_ascii=False)
    except OSError as exc:
        print("  (warn: %s)" % exc)


if __name__ == "__main__":
    sys.exit(main())