#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert
"""What fm1-dock's build will check, tried first on a computer without the FM-1's toolchain: the module compiled
for a 32-bit RISC-V (clang, rv32im, no FPU), then
  - nothing undefined but memcpy / memset and the like (a 64-bit division or a float would ask for a helper)
  - its RAM within a module's 272 KiB and its image within a .dock's 272 KiB
  - no constant in the code and no 16-bit-aligned word in its tables that reads as an address in the chip's ROM
    (0xFFC00000..0xFFCFFFFF): fm1-dock's build refuses those
It is a stand-in: the real checks are tools/hbmod.py's, with JieLi's compiler.

  DOCK=/path/to/fm1-dock python3 tools/precheck.py      (needs clang with the RISC-V target, llvm-objdump)"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCK = Path(os.environ.get("DOCK", ROOT.parent / "fm1-dock")).resolve()
OUT = ROOT / "build" / "chk"
ALLOWED = {"memcpy", "memset", "memmove", "memcmp", "strlen"}       # modules/lib/hb_lib.c
RAM_MAX = FILE_MAX = 0x44000


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    obj = OUT / "fizzy.o"
    run("clang", "--target=riscv32-unknown-elf", "-march=rv32im", "-mabi=ilp32", "-O2", "-ffunction-sections",
        "-fdata-sections", "-fno-builtin", "-ffreestanding", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
        "-Wno-unused-parameter", "-Wno-missing-field-initializers", f"-I{DOCK}/firmware/dock", f"-I{DOCK}/modules/lib",
        "-c", str(ROOT / "fizzyfm" / "fizzyfm.c"), "-o", str(obj))
    errors = []
    undef = set(run("llvm-nm", "-u", str(obj)).split()[1::2]) - ALLOWED
    if undef:
        errors.append(f"needs {sorted(undef)}: a 64-bit division, a float or a libc call")
    text = data = bss = 0
    consts = []
    for m in re.finditer(r"^\s*\d+\s+(\.\S+)\s+([0-9a-f]+)", run("llvm-objdump", "-h", str(obj)), re.M):
        name, size = m.group(1), int(m.group(2), 16)
        if name.startswith((".text", ".rodata", ".srodata", ".hb_desc")):
            text += size
        elif name.startswith((".data", ".sdata")):
            data += size
        elif name.startswith((".bss", ".sbss")):
            bss += size
        if name.startswith((".rodata", ".srodata", ".data", ".sdata")):
            consts.append(name)
    if data + bss > RAM_MAX:
        errors.append(f"RAM {data + bss} B > {RAM_MAX}")
    if text + data > FILE_MAX - 4096:
        errors.append(f"image {text + data} B: over a .dock's {FILE_MAX} B")
    rom = 0
    for ln in run("llvm-objdump", "-d", str(obj)).splitlines():      # a 32-bit constant is a lui (+ addi) here
        m = re.search(r"\blui\s+\w+, (\S+)", ln)
        if m and 0xFFC00 <= int(m.group(1), 0) <= 0xFFD00:
            rom += 1
            errors.append(f"a constant in the code reads as a ROM address: {ln.strip()}")
    for name in consts:
        tmp = OUT / "sec.bin"
        run("llvm-objcopy", "-O", "binary", f"--only-section={name}", str(obj), str(tmp))
        d = tmp.read_bytes()
        for i in range(0, len(d) - 3, 2):
            v = struct.unpack_from("<I", d, i)[0]
            if 0xFFC00000 <= v < 0xFFD00000:
                rom += 1
                errors.append(f"{name}+{i:#x}: the word {v:#010x} reads as a ROM address")
    # JieLi's compiler (clang 4) folds a clamp's bound and a scaling by a power of two after it into one constant,
    # where this clang does not: every bound over 16 bits is tried against every such scaling here, in the source
    for f in sorted((ROOT / "fizzyfm").glob("ap_*.h")) + [ROOT / "fizzyfm" / "fizzyfm.c"]:
        for ln_no, ln in enumerate(f.read_text().splitlines(), 1):
            for m in re.finditer(r"clampi\([^;]*?,\s*-\s*(\d+)\s*,\s*(\d+)\s*\)", ln):
                b = int(m.group(1))
                bad = [k for k in range(13) if b > 32768 and 3145729 <= b << k <= 4194304]
                if bad:
                    rom += 1
                    errors.append(f"{f.name}:{ln_no}: the bound {b} times {1 << bad[0]} reads as a ROM address")
    print(f"code + constants {text} B, .data {data} B, .bss {bss} B (RAM {data + bss} of {RAM_MAX}), "
          f"{rom} ROM-like constants")
    for e in errors[:20]:
        print("  FAIL", e)
    sys.exit(1 if errors else 0)


if __name__ == "__main__":
    main()
