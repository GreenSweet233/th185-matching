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

    # 每个条目：(标签, 地址, 源文件, 函数名, 额外前缀, 额外flags)
    # 额外前缀用于把 __fastcall 模拟的声明也带进去
    targets = [
        ("0x00402A50", 0x00402A50, "Timer.cpp", "tick_timer", True),
        ("0x00404AA0", 0x00404AA0, "Trivial.cpp", "return_this", True),
        ("0x00404C20", 0x00404C20, "Trivial.cpp", "do_nothing", False),
        ("0x0041FA20", 0x0041FA20, "Trivial.cpp", "read_first_field", True),
        ("0x00404B70", 0x00404B70, "Trivial.cpp", "polar_to_vec2", True),
        ("0x004903B9", 0x004903B9, "Trivial.cpp", "return_zero", False),
        ("0x00405830", 0x00405830, "Trivial.cpp", "copy_vec3", True),
        ("0x00406720", 0x00406720, "Trivial.cpp", "set_first_field", True),
        ("0x00417750", 0x00417750, "Trivial.cpp", "polar_to_vec2_scaled", True),
        ("0x0049DC88", 0x0049DC88, "Trivial.cpp", "encode_value", False),
        ("0x00404D10", 0x00404D10, "FieldAccess.cpp", "first_of_three", False),
        ("0x00407E40", 0x00407E40, "FieldAccess.cpp", "is_field_2A0_set", True),
        ("0x0040A050", 0x0040A050, "FieldAccess.cpp", "clear_first_and_return_this", True),
        ("0x00409F60", 0x00409F60, "FieldAccess.cpp", "set_field_20", True),
        ("0x00433BC0", 0x00433BC0, "FieldAccess.cpp", "set_first_field", True),
        ("0x004063C0", 0x004063C0, "FieldAccess.cpp", "copy_vec3_to_5F0", True),
        ("0x00404B60", 0x00404B60, "FieldAccess.cpp", "read_first_as_m128", True),
        ("0x00405A60", 0x00405A60, "FieldAccess.cpp", "read_44_as_m128", True),
        ("0x0041C710", 0x0041C710, "FieldAccess.cpp", "mul_74_x_54", True),
        ("0x00406730", 0x00406730, "FieldAccess.cpp", "get_field_24", True),
        ("0x004176D0", 0x004176D0, "FieldAccess.cpp", "get_field_20", True),
        ("0x00429A50", 0x00429A50, "FieldAccess.cpp", "addr_of_1C", True),
        ("0x00407E30", 0x00407E30, "FieldAccess.cpp", "addr_of_620", True),
        ("0x00424480", 0x00424480, "FieldAccess.cpp", "clear_field_28", True),
        ("0x00404BD0", 0x00404BD0, "FieldAccess.cpp", "return_const_addr", True),
        ("0x00429AB0", 0x00429AB0, "FieldAccess.cpp", "clear_low_nibble_at_40", True),
        ("0x0040A090", 0x0040A090, "FieldAccess.cpp", "addr_of_1274", True),
        ("0x00417770", 0x00417770, "FieldAccess.cpp", "get_field_34", True),
        ("0x00433B90", 0x00433B90, "FieldAccess.cpp", "get_field_04", True),
        ("0x00433DC0", 0x00433DC0, "FieldAccess.cpp", "addr_of_44", True),
        ("0x00431300", 0x00431300, "FieldAccess.cpp", "get_field_50", True),
        ("0x00431260", 0x00431260, "FieldAccess.cpp", "get_field_19260", True),
        ("0x0040A0E0", 0x0040A0E0, "FieldAccess.cpp", "get_field_47FAC", True),
        # ★ 这个函数需要 /Os（生成 pop ecx 而非 add esp,4）
        ("0x0048F711", 0x0048F711, "FieldAccess.cpp", "forward_to_cpu_probe", False, "/MT /Os /GS- /Oy-"),
        ("0x0041FA30", 0x0041FA30, "FieldAccess.cpp", "get_field_1C_int", True),
        ("0x00433B40", 0x00433B40, "FieldAccess.cpp", "get_field_00", True),
        ("0x00441210", 0x00441210, "FieldAccess.cpp", "get_field_3C", True),
        ("0x00433DD0", 0x00433DD0, "FieldAccess.cpp", "addr_of_4FF4", True),
        ("0x00433DF0", 0x00433DF0, "FieldAccess.cpp", "addr_of_1230", True),
        ("0x00429A40", 0x00429A40, "SseAccess.cpp", "load_18", True),
        ("0x00441240", 0x00441240, "SseAccess.cpp", "load_28", True),
        ("0x00441250", 0x00441250, "SseAccess.cpp", "load_2C", True),
        ("0x00441330", 0x00441330, "SseAccess.cpp", "load_20", True),
        ("0x00441340", 0x00441340, "SseAccess.cpp", "load_24", True),
        ("0x00441600", 0x00441600, "SseAccess.cpp", "load_04", True),
        ("0x004415F0", 0x004415F0, "SseAccess.cpp", "store_00", True),
        ("0x004415E0", 0x004415E0, "SseAccess.cpp", "store_04", True),
        ("0x00417780", 0x00417780, "SseAccess.cpp", "store_18", True),
        ("0x004299F0", 0x004299F0, "SseAccess.cpp", "store_24", True),
        ("0x00429A00", 0x00429A00, "SseAccess.cpp", "store_20", True),
        ("0x00441270", 0x00441270, "SseAccess.cpp", "store_2C", True),
        ("0x00441320", 0x00441320, "SseAccess.cpp", "store_34", True),
        ("0x00441310", 0x00441310, "SseAccess.cpp", "store_38", True),
        ("0x004341D0", 0x004341D0, "SseAccess.cpp", "store_6C", True),
        ("0x004341E0", 0x004341E0, "SseAccess.cpp", "store_74", True),
        ("0x004341F0", 0x004341F0, "SseAccess.cpp", "store_78", True),
    ]

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

        # 提取单个函数：从 //@witness func= 之后到下一个 //@witness 或文件尾
        lines = src.split("\n")
        idx = None
        import re as _re2
        for k, ln in enumerate(lines):
            if "//@witness func=" in ln and fn_name in "\n".join(lines[max(0,k):k+18]):
                for j in range(k + 1, min(k + 18, len(lines))):
                    if _re2.search(r"\b" + _re2.escape(fn_name) + r"\s*\(", lines[j]):
                        idx = j
                        break
                if idx is not None:
                    break
        # 兜底：全文搜函数定义
        if idx is None:
            for j, ln in enumerate(lines):
                if _re2.search(r"\b" + _re2.escape(fn_name) + r"\s*\(", ln) and "//" not in ln.split(fn_name)[0]:
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

        # 如果是 __fastcall 模拟，需要把调用约定写进去（源文件里可能没写）
        if need_this and "__fastcall" not in func_text:
            # 在函数名前的返回类型后插入 __fastcall
            import re as _re
            func_text = _re.sub(r"(\n)([A-Za-z_][\w\s\*]*?)\s+" + fn_name + r"\s*\(",
                                r"\1\2 __fastcall " + fn_name + "(", func_text, count=1)

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