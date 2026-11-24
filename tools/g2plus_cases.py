#!/usr/bin/env python3
r"""G2+ 的验证用例集：每个函数一个 verify_* 函数。

拆成独立文件是为了避免与 check_behavior.py 的引号嵌套。
每个 verify_* 返回 list[dict]，每项含 case / status，失败时附 binary 与 expected。
"""

import math
import struct


def _f2u(v):
    """float -> 32 位无符号（用于压栈）。"""
    return struct.unpack("<I", struct.pack("<f", v))[0]


def _u2f(b, off):
    """从字节串读 float。"""
    return struct.unpack_from("<f", b, off)[0]


def verify_polar_to_vec2(Runner, HEAP_BASE, sections, image_base):
    """0x00404B70（17 个同形状）：极坐标 -> Vec2（圆）。

        fld  dword [ebp+8]      ; angle
        fsincos                 ; st0=sin, st1=cos
        fmul dword [ebp+0xC]    ; * length
        fstp dword [eax]        ; x = sin*length
        fmul dword [ebp+0xC]    ; * length   <- 同一个参数
        fstp dword [eax+4]      ; y = cos*length

    x 用 sin、y 用 cos —— 不是常见的 (cos,sin)。必须用执行验证。
    """
    FUNC = 0x00404B70
    out = []
    # ★ 必须包含 sin != cos 的角度，否则无法区分 x=sin 与 x=cos
    for angle, length in [(0.0, 2.0), (0.7853981634, 1.0),
                          (1.5707963268, 3.0), (3.1415926536, 1.0),
                          (-0.5, 2.5), (0.3, 1.7), (2.0, 4.0)]:
        obj = bytearray(8)
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj,
                                     args=[_f2u(angle), _f2u(length)])
        name = "a=%.4f L=%.1f" % (angle, length)
        if err:
            out.append({"case": name, "status": "ERROR", "error": err})
            continue
        gx, gy = _u2f(after, 0), _u2f(after, 4)
        # ★ 实测修正：x 用 cos、y 用 sin（我最初写反了）
        ex, ey = math.cos(angle) * length, math.sin(angle) * length
        ok = abs(gx - ex) < 1e-5 and abs(gy - ey) < 1e-5
        e = {"case": name, "status": "PASS" if ok else "FAIL"}
        if not ok:
            e["binary"] = {"x": round(gx, 6), "y": round(gy, 6)}
            e["expected"] = {"x": round(ex, 6), "y": round(ey, 6)}
        out.append(e)
    return out


def verify_polar_scaled(Runner, HEAP_BASE, sections, image_base):
    """0x00417750（2 个）：极坐标 -> Vec2，x/y 用不同 length。

    与 0x00404B70 只差一处：第二次 fmul 用 [ebp+0x10] 而非 [ebp+0xC]。
    """
    FUNC = 0x00417750
    out = []
    for angle, lx, ly in [(0.5, 2.0, 3.0), (1.0, 1.0, 5.0), (0.0, 4.0, 4.0)]:
        obj = bytearray(8)
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj,
                                     args=[_f2u(angle), _f2u(lx), _f2u(ly)])
        name = "a=%.2f lx=%.1f ly=%.1f" % (angle, lx, ly)
        if err:
            out.append({"case": name, "status": "ERROR", "error": err})
            continue
        gx, gy = _u2f(after, 0), _u2f(after, 4)
        # ★ 实测修正：x 用 cos*lx、y 用 sin*ly
        ex, ey = math.cos(angle) * lx, math.sin(angle) * ly
        ok = abs(gx - ex) < 1e-5 and abs(gy - ey) < 1e-5
        e = {"case": name, "status": "PASS" if ok else "FAIL"}
        if not ok:
            e["binary"] = {"x": round(gx, 6), "y": round(gy, 6)}
            e["expected"] = {"x": round(ex, 6), "y": round(ey, 6)}
        out.append(e)
    return out


def verify_copy_vec3(Runner, HEAP_BASE, sections, image_base):
    """0x00405830（3 个）：拷贝 12 字节（Vec3）。"""
    FUNC = 0x00405830
    src_data = bytes(range(0x11, 0x11 + 12))
    obj = bytearray(b"\xEE" * 12)
    src_va = HEAP_BASE + 0x2000
    r = Runner(sections, image_base)
    r.uc.mem_write(src_va, src_data)
    eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj, args=[src_va])
    if err:
        return [{"case": "copy 12 bytes", "status": "ERROR", "error": err}]
    ok = (after == src_data)
    e = {"case": "copy 12 bytes", "status": "PASS" if ok else "FAIL"}
    if not ok:
        e["binary"] = after.hex()
        e["expected"] = src_data.hex()
    return [e]


def verify_set_first_field(Runner, HEAP_BASE, sections, image_base):
    """0x00406720（3 个）：写首字段。"""
    FUNC = 0x00406720
    out = []
    for val in (0, 0x12345678, 0xFFFFFFFF):
        obj = bytearray(b"\xAA" * 8)
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj, args=[val])
        name = "set 0x%08X" % val
        if err:
            out.append({"case": name, "status": "ERROR", "error": err})
            continue
        got = struct.unpack_from("<I", after, 0)[0]
        tail_ok = after[4:] == b"\xAA" * 4
        ok = (got == val and tail_ok)
        e = {"case": name, "status": "PASS" if ok else "FAIL"}
        if not ok:
            e["binary"] = "0x%08X tail=%s" % (got, after[4:].hex())
            e["expected"] = "0x%08X tail=aaaaaaaa" % val
        out.append(e)
    return out


def verify_timer_tick(Runner, HEAP_BASE, sections, image_base):
    """0x00402A50（0x00402A50..0x00402A93，68 字节）：倒计时状态机。

        countdown = *(int*)(this + 0x8C) - 1
        *(int*)(this + 0x8C) = countdown
        if countdown < 0:
            *(int*)(this + 0x8C) = 0
            *(int*)(this + 0x00) = 0
            *(int*)(this + 0xD4) = 0
            return
        *(int*)(this + 0x00) = *(int*)(this + 0x0C + countdown*4)
        *(int*)(this + 0x08) = *(int*)(this + 0x4C + countdown*4)
        *(int*)(this + 0xD4) = 0
    """
    FUNC = 0x00402A50
    OBJ = 0xD8
    out = []

    def make_obj(init_cd):
        o = bytearray(OBJ)
        struct.pack_into("<i", o, 0x8C, init_cd)
        struct.pack_into("<I", o, 0x00, 0xDEADBEEF)
        struct.pack_into("<I", o, 0x08, 0xFEEDFACE)
        struct.pack_into("<I", o, 0xD4, 0xCAFEBABE)
        for k in range(16):
            struct.pack_into("<i", o, 0x0C + k * 4, 0xA000 + k)
            struct.pack_into("<i", o, 0x4C + k * 4, 0xB000 + k)
        return o

    for init_cd, desc in [(5,  "non-negative: 5-1=4 -> A[4]/B[4]"),
                          (1,  "boundary: 1-1=0 -> A[0]/B[0]"),
                          (0,  "boundary: 0-1=-1 -> clear"),
                          (-1, "negative input"),
                          (15, "15-1=14 (last array index)"),
                          (16, "16-1=15 (array tail)"),
                          (30, "30-1=29 (beyond array)")]:
        obj = make_obj(init_cd)
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj)
        if err:
            out.append({"case": desc, "status": "ERROR", "error": err})
            continue
        exp = bytearray(obj)
        cd = init_cd - 1
        if cd < 0:
            struct.pack_into("<i", exp, 0x8C, 0)
            struct.pack_into("<I", exp, 0x00, 0)
            struct.pack_into("<I", exp, 0xD4, 0)
        else:
            struct.pack_into("<i", exp, 0x8C, cd)
            ia, ib2 = 0x0C + cd * 4, 0x4C + cd * 4
            va = struct.unpack_from('<i', obj, ia)[0] if ia + 4 <= OBJ else 0
            vb = struct.unpack_from('<i', obj, ib2)[0] if ib2 + 4 <= OBJ else 0
            struct.pack_into("<i", exp, 0x00, va)
            struct.pack_into("<i", exp, 0x08, vb)
            struct.pack_into("<I", exp, 0xD4, 0)
        expected = bytes(exp)
        ok = (after == expected)
        e = {"case": desc, "status": "PASS" if ok else "FAIL"}
        if not ok:
            for off in (0x00, 0x08, 0x8C, 0xD4):
                if after[off:off+4] != expected[off:off+4]:
                    e["binary"] = {"offset": "0x%02X" % off,
                                   "value": "0x%08X" % struct.unpack("<I", after[off:off+4])[0]}
                    e["expected"] = {"offset": "0x%02X" % off,
                                     "value": "0x%08X" % struct.unpack("<I", expected[off:off+4])[0]}
                    break
        out.append(e)
    return out

def verify_ret_zero(Runner, HEAP_BASE, sections, image_base):
    """0x00404C20（13 个同形状）：空函数 `ret 0`。

    ret 0 = stdcall/thiscall 带 0 字节参数清理，函数体为空。
    """
    FUNC = 0x00404C20
    obj = bytearray(b"\xAA" * 16)
    r = Runner(sections, image_base)
    eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj)
    if err:
        return [{"case": "empty body, no side effect", "status": "ERROR", "error": err}]
    ok = (after == bytes(obj))
    e = {"case": "empty body, no side effect", "status": "PASS" if ok else "FAIL"}
    if not ok:
        e["binary"] = after.hex()
        e["expected"] = bytes(obj).hex()
    return [e]


def verify_return_zero(Runner, HEAP_BASE, sections, image_base):
    """0x004903B9（4 个同形状）：`xor eax,eax; ret` = return 0。"""
    FUNC = 0x004903B9
    obj = bytearray(8)
    r = Runner(sections, image_base)
    eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj)
    if err:
        return [{"case": "returns 0", "status": "ERROR", "error": err}]
    ok = (eax == 0)
    e = {"case": "returns 0", "status": "PASS" if ok else "FAIL"}
    if not ok:
        e["binary"] = "eax=0x%08X" % eax
        e["expected"] = "eax=0x00000000"
    return [e]


def verify_encode_value(Runner, HEAP_BASE, sections, image_base):
    """0x0049DC88（5 个同形状）：MSVC /GS 指针编码。

        mov  eax, [0x4C8A4C]     ; __security_cookie
        and  eax, 0x1f
        push 0x20 / pop ecx      ; ecx = 32
        sub  ecx, eax            ; ecx = 32 - (cookie & 31)
        mov  eax, [ebp+8]        ; value
        ror  eax, cl
        xor  eax, [0x4C8A4C]
        ret

    ★ 关键：x86 的 ror 对 32 位操作数把 CL 按 5 位掩码。
      所以 shift=32 时实际旋转 0 次，等价于不旋转。
      这正是我上一轮靠人工推导修掉 UB 的那处 —— 现在实测验证。

    实测 cookie = 0xBB40E64E，cookie&31 = 14，shift = 18（不是 32）。
    但为覆盖边界，本测试会额外验证 shift=32 的等价公式。
    """
    FUNC = 0x0049DC88
    COOKIE_ADDR = 0x004C8A4C

    # 读 cookie（镜像已映射，可直接 mem_read）
    r0 = Runner(sections, image_base)
    cookie = struct.unpack('<I', r0.uc.mem_read(COOKIE_ADDR, 4))[0]
    shift = (32 - (cookie & 0x1F)) & 0x1F

    out = []

    def ror32(v, n):
        n &= 0x1F
        if n == 0:
            return v & 0xFFFFFFFF
        return ((v >> n) | (v << (32 - n))) & 0xFFFFFFFF

    # 用例 1：用真实 cookie（实测 0xBB40E64E，shift=18）
    # 用例 2-4：覆盖 cookie 构造 shift=32 的边界
    #
    # ★ shift_raw = 32 - (cookie & 31)，当 cookie & 31 == 0 时 shift_raw = 32。
    #   C++ 里 `v >> 32` 是 UB（移位量 >= 类型宽度），但 x86 的 ror 把 CL 掩码到 5 位，
    #   所以硬件行为是「旋转 0 次」。这正是我上一轮靠人工推导修掉 UB 的地方，
    #   现在用实测固化下来。
    scenarios = [
        ("real cookie", None),
        ("cookie &31==0 (shift_raw=32)", 0xBB40E640),
        ("cookie==0 (shift_raw=32)", 0x00000000),
        ("cookie==0xFFFFFFFF (shift=1)", 0xFFFFFFFF),
    ]

    for scen_name, cookie_override in scenarios:
        for val in (0, 1, 0x12345678, 0xFFFFFFFF, 0xDEADBEEF):
            obj = bytearray(4)
            r = Runner(sections, image_base)
            if cookie_override is not None:
                r.uc.mem_write(COOKIE_ADDR, struct.pack("<I", cookie_override))
            eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj, args=[val])
            name = "%s v=0x%08X" % (scen_name, val)
            if err:
                out.append({"case": name, "status": "ERROR", "error": err})
                continue

            eff_cookie = cookie_override if cookie_override is not None else cookie
            raw_shift = 32 - (eff_cookie & 0x1F)
            exp = ror32(val, raw_shift) ^ eff_cookie
            ok = (eax == exp)
            e = {"case": name, "status": "PASS" if ok else "FAIL"}
            if not ok:
                e["binary"] = "eax=0x%08X (cookie=0x%08X raw_shift=%d)" % (eax, eff_cookie, raw_shift)
                e["expected"] = "eax=0x%08X (cookie=0x%08X raw_shift=%d)" % (exp, eff_cookie, raw_shift)
            out.append(e)
    return out


def verify_read_first_float(Runner, HEAP_BASE, sections, image_base):
    """0x00404B60（2 个）：`movss xmm0,[ecx]; ret` = 读首字段为 float。"""
    FUNC = 0x00404B60
    out = []
    for fval in (0.0, 1.5, -3.25, 1e10):
        obj = bytearray(8)
        raw = struct.pack('<f', fval)
        obj[0:4] = raw
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj)
        name = "read float %.2f" % fval
        if err:
            out.append({"case": name, "status": "ERROR", "error": err})
            continue
        # xmm0 读不到，但函数无副作用，验证内存不变 + 不崩溃
        ok = (after == bytes(obj))
        out.append({"case": name, "status": "PASS" if ok else "FAIL"})
    return out


def verify_read_first_u32(Runner, HEAP_BASE, sections, image_base):
    """0x0041FA20（2 个）：`mov eax,[ecx]; ret` = 读首字段为 u32。"""
    FUNC = 0x0041FA20
    out = []
    for val in (0, 0x12345678, 0xFFFFFFFF):
        obj = bytearray(8)
        struct.pack_into('<I', obj, 0, val)
        r = Runner(sections, image_base)
        eax, after, err = r.run_leaf(FUNC, HEAP_BASE + 0x1000, obj)
        name = "read u32 0x%08X" % val
        if err:
            out.append({"case": name, "status": "ERROR", "error": err})
            continue
        ok = (eax == val)
        e = {"case": name, "status": "PASS" if ok else "FAIL"}
        if not ok:
            e["binary"] = "eax=0x%08X" % eax
            e["expected"] = "eax=0x%08X" % val
        out.append(e)
    return out


def verify_return_this(Runner, HEAP_BASE, sections, image_base):
    """0x00404AA0（2 个）：`mov eax,ecx; ret` = 返回 this。"""
    FUNC = 0x00404AA0
    obj = bytearray(8)
    r = Runner(sections, image_base)
    this_va = HEAP_BASE + 0x1000
    eax, after, err = r.run_leaf(FUNC, this_va, obj)
    if err:
        return [{"case": "returns this", "status": "ERROR", "error": err}]
    ok = (eax == this_va)
    e = {"case": "returns this", "status": "PASS" if ok else "FAIL"}
    if not ok:
        e["binary"] = "eax=0x%08X" % eax
        e["expected"] = "eax=0x%08X" % this_va
    return [e]

# 注册表：(名称, 函数地址, 验证器)
CASES = [
    ("0x00402A50 timer tick", 0x00402A50, "verify_timer_tick"),
    ("0x00404C20 ret 0 (empty)", 0x00404C20, "verify_ret_zero"),
    ("0x004903B9 return 0", 0x004903B9, "verify_return_zero"),
    ("0x0049DC88 encode_value (/GS)", 0x0049DC88, "verify_encode_value"),
    ("0x00404B60 read_first_float", 0x00404B60, "verify_read_first_float"),
    ("0x0041FA20 read_first_u32", 0x0041FA20, "verify_read_first_u32"),
    ("0x00404AA0 return_this", 0x00404AA0, "verify_return_this"),
    ("0x00404B70 polar (circle)", 0x00404B70, "verify_polar_to_vec2"),
    ("0x00417750 polar (scaled)", 0x00417750, "verify_polar_scaled"),
    ("0x00405830 copy_vec3", 0x00405830, "verify_copy_vec3"),
    ("0x00406720 set_first_field", 0x00406720, "verify_set_first_field"),
]