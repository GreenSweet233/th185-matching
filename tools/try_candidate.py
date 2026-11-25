#!/usr/bin/env python3
r"""候选函数验证工具（复用 check_bytes 的解析器）。

## 为什么必须存在

2026-11-25 我犯了两个错误，导致 5 个本来能匹配的候选全被误判成 DIFF：

1. **手工截取 target 长度** —— 只截到 `or [eax+0x78],8`，
   漏掉尾部 `8be55dc3`（`mov esp,ebp / pop ebp / ret`）。
2. **自己重写 relocation 解析** —— 没用 `check_bytes.parse_coff_relocs`，
   于是绝对地址占位符（`00000000`）没被屏蔽，全部 DIFF。

**自己重写一遍 = 引入第二个可能有 bug 的实现，
而且它的 bug 会伪装成「候选不匹配」，比直接报错更难发现。**

## 本工具强制做的事

- target 长度：**从反汇编走到 `ret` 为止**，不手写
- relocation：**直接用 `check_bytes.parse_coff_relocs`**
- 比对：**直接用 `check_bytes.bytes_match_with_relocs`**

## 用法

```
python tools/try_candidate.py 0x004A09F7 --src "extern int a; int f(){...}"
python tools/try_candidate.py 0x004A09F7 --src-file /tmp/cand.cpp
```

会依次试 `/O2 /O1 /Od /Ox /Os` 五种优化级，任一匹配就报告。
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pe_common
import check_bytes as CB
import capstone

MSVC = r"D:\MSVC142\14.29.30133"
CL = os.path.join(MSVC, "bin", "Hostx86", "x86", "cl.exe")
WINSDK = r"C:\Program Files (x86)\Windows Kits\10\Include\10.0.19041.0"
INCS = [
    os.path.join(MSVC, "include"),
    os.path.join(WINSDK, "ucrt"),
    os.path.join(WINSDK, "shared"),
    os.path.join(WINSDK, "um"),
]


def make_env():
    env = dict(os.environ)
    env["INCLUDE"] = os.pathsep.join(INCS)
    env["LIB"] = os.path.join(MSVC, "lib", "x86")
    env["PATH"] = (os.path.join(MSVC, "bin", "Hostx86", "x86")
                   + os.pathsep + os.environ.get("PATH", ""))
    env["_CL_"] = ""
    return env


def compile_obj(src, flags, workdir):
    cpp = os.path.join(workdir, "cand.cpp")
    obj = os.path.join(workdir, "cand.obj")
    with open(cpp, "w", encoding="utf-8") as fh:
        fh.write(src)
    proc = subprocess.run(
        [CL, "/nologo", "/c"] + flags + ["/Fo" + obj, cpp],
        capture_output=True, text=True, encoding="utf-8",
        errors="replace", env=make_env(), cwd=workdir, timeout=180)
    if not os.path.isfile(obj):
        import re
        errs = re.findall(r"error C\d+: [^\n]+", proc.stdout or "")
        return None, (errs[0] if errs else (proc.stdout or "")[:200])
    return obj, ""


def text_bytes(obj_path):
    """取 .obj 里最大的 .text 段字节。"""
    raw = open(obj_path, "rb").read()
    _m, nsec, _tds, _sp, _ns, optsz, _ch = struct.unpack_from("<HHIIIHH", raw, 0)
    off = 20 + optsz
    best = None
    for _ in range(nsec):
        s = raw[off:off + 40]
        if len(s) < 40:
            break
        name = s[0:8].rstrip(b"\0").decode("ascii", "replace")
        _vs, _va, rs, rp, _relp, _lnp, _nrel, _nln, _fl = struct.unpack_from(
            "<IIIIIIHHI", s, 8)
        if name.startswith(".text") and (best is None or rs > best[1]):
            best = (name, rs, raw[rp:rp + rs])
        off += 40
    return best[2] if best else None


def target_bytes(sections, image_base, va):
    """从 va 反汇编到 ret（含）为止 —— 不手写长度。"""
    for _name, s in sections.items():
        lo = image_base + s["va"]
        if lo <= va < lo + len(s["data"]):
            off = va - lo
            blob = s["data"][off:off + 400]
            md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
            for ins in md.disasm(blob, va):
                if ins.mnemonic == "ret":
                    end = ins.address - va + ins.size
                    return blob[:end]
            return blob[:32]
    return None


def main():
    ap = argparse.ArgumentParser(description="try one candidate against the binary")
    ap.add_argument("va", help="target function address, e.g. 0x004A09F7")
    ap.add_argument("--src", default=None, help="C++ source text")
    ap.add_argument("--src-file", default=None, help="path to C++ source")
    ap.add_argument("--root", default=os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument("--all-opts", action="store_true",
                    help="try /O1 too (default: /O2 /O1)")
    args = ap.parse_args()

    va = int(args.va, 16)
    if args.src_file:
        with open(args.src_file, encoding="utf-8") as fh:
            src = fh.read()
    elif args.src:
        src = args.src
    else:
        ap.error("need --src or --src-file")

    exe = pe_common.find_exe(os.path.abspath(args.root))
    _pe, sections = pe_common.load_pe(exe)
    image_base = _pe.OPTIONAL_HEADER.ImageBase

    target = target_bytes(sections, image_base, va)
    if target is None:
        print("[FAIL] 0x%08X not inside any section" % va)
        return 2

    print("target 0x%08X  len=%d  %s" % (va, len(target), target.hex()))

    opts = ["/O2", "/O1"]
    if args.all_opts:
        opts = ["/O2", "/O1", "/Od", "/Ox", "/Os"]

    best = None
    for opt in opts:
        with tempfile.TemporaryDirectory() as td:
            obj, err = compile_obj(src, ["/MT", opt, "/GS-", "/Oy-"], td)
            if obj is None:
                print("  %-4s COMPILE_ERROR  %s" % (opt, err[:100]))
                continue
            ours = text_bytes(obj)
            if ours is None:
                print("  %-4s no .text" % opt)
                continue
            relocs = CB.parse_coff_relocs(obj, ours)
            ok, detail = CB.bytes_match_with_relocs(ours, target, relocs)
            if ok:
                print("  %-4s ★ MATCH   reloc=%d  %s" % (opt, len(relocs), detail))
                return 0
            if len(ours) == len(target):
                if best is None:
                    best = (opt, ours)
            print("  %-4s DIFF  len=%d  %s" % (opt, len(ours), detail[:80]))

    if best:
        print()
        print("最接近的 [%s]：" % best[0])
        print("  ours  : %s" % best[1].hex())
        print("  target: %s" % target.hex())
    print()
    print("[NO MATCH]")
    return 1


if __name__ == "__main__":
    sys.exit(main())