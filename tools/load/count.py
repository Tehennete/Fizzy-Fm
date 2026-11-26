#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert
"""How many instructions a block of Fizzy-Fm's sound takes, on a 32-bit RISC-V stand-in (rv32im, clang -O2), run in
Unicorn. fm1-emu charges one instruction per cycle of 240 MHz: a block of 256 frames has 5.8 ms, 1 392 000 of them,
and fm1-dock's rule is to stay under half. The FM-1's chip is another instruction set and has a flash cache, so this
is an estimate of the order, not a measurement: the real number is fm1-dock's LOAD on the unit.

  DOCK=/path/to/fm1-dock python3 tools/load/count.py      (needs clang, ld.lld, pip install unicorn)"""
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

from unicorn import Uc, UC_ARCH_RISCV, UC_MODE_RISCV32, UC_HOOK_BLOCK
from unicorn.riscv_const import UC_RISCV_REG_A0, UC_RISCV_REG_A1, UC_RISCV_REG_RA, UC_RISCV_REG_SP

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DOCK = Path(os.environ.get("DOCK", ROOT.parent / "fm1-dock")).resolve()
OUT = ROOT / "build" / "load"
BUDGET = 1_392_000


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    tgt = ["--target=riscv32-unknown-elf", "-march=rv32im", "-mabi=ilp32"]
    cf = ["-O2", "-ffunction-sections", "-fdata-sections", "-fno-builtin", "-ffreestanding", "-Wall",
          "-Wno-unused-function", f"-I{DOCK}/firmware/dock", f"-I{DOCK}/modules/lib"]
    subprocess.run(["clang", *tgt, *cf, "-c", str(HERE / "harness.c"), "-o", str(OUT / "harness.o")], check=True)
    subprocess.run(["clang", *tgt, *cf, "-c", str(DOCK / "modules/lib/hb_lib.c"), "-o", str(OUT / "lib.o")], check=True)
    subprocess.run(["ld.lld", "-T", str(HERE / "link.ld"), str(OUT / "harness.o"), str(OUT / "lib.o"), "-o",
                    str(OUT / "harness.elf"), "--gc-sections", "-e", "x_init", "--undefined=x_preset", "--undefined=x_set",
                    "--undefined=x_note", "--undefined=x_render", "--undefined=x_table", "--undefined=x_draw",
                    "--undefined=x_np", "--undefined=peak"], check=True)
    syms = subprocess.run(["llvm-nm", str(OUT / "harness.elf")], check=True, capture_output=True, text=True).stdout
    return {ln.split()[2]: int(ln.split()[0], 16) for ln in syms.splitlines() if len(ln.split()) == 3}


class Machine:
    def __init__(self, sym):
        self.sym = sym
        self.uc = Uc(UC_ARCH_RISCV, UC_MODE_RISCV32)
        self.uc.mem_map(0x00100000, 0x80000)
        self.uc.mem_map(0x00400000, 0x80000)
        self.uc.mem_map(0x00900000, 0x10000)                 # the stack
        self.uc.mem_map(0x00F00000, 0x1000)                  # where calls return to
        elf = (OUT / "harness.elf").read_bytes()
        phoff, phentsize, phnum = struct.unpack_from("<I", elf, 28)[0], *struct.unpack_from("<HH", elf, 42)
        for i in range(phnum):
            typ, off, vaddr, _, filesz, _, _, _ = struct.unpack_from("<8I", elf, phoff + i * phentsize)
            if typ == 1 and filesz:
                self.uc.mem_write(vaddr, elf[off:off + filesz])
        self.count = 0
        self.uc.hook_add(UC_HOOK_BLOCK, self._block)

    def _block(self, uc, addr, size, _):
        self.count += size // 4

    def call(self, name, *args):
        for reg, a in zip((UC_RISCV_REG_A0, UC_RISCV_REG_A1), args):
            self.uc.reg_write(reg, a)
        self.uc.reg_write(UC_RISCV_REG_RA, 0x00F00000)
        self.uc.reg_write(UC_RISCV_REG_SP, 0x0090F000)
        self.count = 0
        self.uc.emu_start(self.sym[name], 0x00F00000)
        return self.count


def params():
    src = (ROOT / "fizzyfm" / "ap_def.h").read_text()
    body = src.split("#define AP_PARAMS(X)")[1].split("#define X(id")[0]
    osc = re.findall(r"X\(o##_(\w+),", src)
    env = re.findall(r"X\(e##_(\w+),", src)
    lfo = re.findall(r"X\(l##_(\w+),", src)
    mod = re.findall(r"X\(m##_(\w+),", src)
    fxp = re.findall(r"X\(f##_(\w+),", src)
    ids = []
    for m in re.finditer(r"AP_(OSC|ENV|LFO|MOD|FX)\(X, (\w+)\)|X\((\w+),", body):
        if m.group(1):
            ids += [f"{m.group(2)}_{x}" for x in {"OSC": osc, "ENV": env, "LFO": lfo, "MOD": mod, "FX": fxp}[m.group(1)]]
        else:
            ids.append(m.group(3))
    return {n: i for i, n in enumerate(ids)}


def main():
    sym = build()
    P = params()
    m = Machine(sym)
    m.call("x_init")
    assert m.call("x_np") >= 0 and len(P) == 132, len(P)
    fac = re.findall(r'\{"([^"]+)", FS_', (ROOT / "fizzyfm" / "ap_def.h").read_text())

    def play(label, preset, keys, sets=(), blocks=40):
        m.call("x_preset", preset)
        for k, v in sets:
            m.call("x_set", P[k], v)
        for _ in range(4):
            m.call("x_render")
        for k in keys:
            m.call("x_note", k, 110)
        worst = max(m.call("x_render") for _ in range(blocks))
        for k in keys:
            m.call("x_note", k, 0)
        for _ in range(400):                                 # let it ring out before the next one
            m.call("x_render")
        print(f"  {label:58s} {worst:9d}  {worst * 100 / BUDGET:5.1f} %")
        return worst

    chord8 = (36, 43, 48, 52, 55, 60, 64, 67)
    print(f"instructions per block of 256 frames (rv32im), and their share of 5.8 ms at 240 MHz ({BUDGET})")
    play("nothing sounds, no effect", 0, ())
    play("INIT, 1 note", 0, (60,))
    play("INIT, 8 notes (one oscillator each)", 0, chord8)
    FT = dict(LP24=4, LPHOT=5, VOWEL=16, COMB=17)
    FXT = dict(DRIVE=1, FOLD=2, CRUSH=3, COMP=4, EQ=5, FILTER=6, PHASER=7, FLANGER=8, CHORUS=9, DIMENS=10, TREM=11,
               PUMP=12, DELAY=13, REVERB=14)

    def rack(*names):
        out = []
        for i, nm in enumerate(names):
            out += [(f"FX{i + 1}_TYPE", FXT[nm]), (f"FX{i + 1}_A", 60), (f"FX{i + 1}_B", 60), (f"FX{i + 1}_C", 80)]
        return tuple(out)

    play("INIT, 8 notes, LP 24", 0, chord8, (("F_TYPE", FT["LP24"]),))
    play("INIT, 8 notes, VOWEL", 0, chord8, (("F_TYPE", FT["VOWEL"]),))
    play("INIT, 8 notes, COMB +", 0, chord8, (("F_TYPE", FT["COMB"]), ("F_RES", 90)))
    play("INIT, 8 notes + chorus + echo + reverb", 0, chord8, rack("CHORUS", "DELAY", "REVERB"))
    play("INIT, 8 notes + the 7 heaviest effects", 0, chord8,
         rack("PHASER", "DIMENS", "REVERB", "CHORUS", "FLANGER", "DELAY", "FILTER"))
    for name in ("SUPER STACK", "GLASS PAD", "STRINGS", "PWM PAD", "SLOW DRONE"):
        play(f"{name}, 8 keys held", fac.index(name), chord8)
    heavy = (("A_WMODE", 1), ("A_WARP", 90), ("B_WMODE", 5), ("B_WARP", 90), ("SUB_LVL", 80), ("NZ_LVL", 60),
             ("F_TYPE", FT["LPHOT"]), ("F_DRV", 60)) + rack("DRIVE", "PHASER", "CHORUS", "DIMENS", "COMP", "DELAY", "REVERB")
    print("the heaviest sounds: SYNC and MIRROR, sub, noise, LP HOT with drive, seven effects, 6 matrix slots, 8 keys held")
    worst = 0
    for ua, ub in ((1, 1), (2, 2), (3, 1), (5, 3), (5, 5)):
        worst = max(worst, play(f"  unison {ua} + {ub}", fac.index("GLASS PAD"), chord8,
                                (("A_UNI", ua - 1), ("B_UNI", ub - 1)) + heavy))
    print(f"one frame of a wavetable (a pass of the main loop while a table is built): {m.call('x_table')}")
    print(f"the whole screen drawn (the host's drawing not counted): {m.call('x_draw')}")
    if worst > BUDGET * 0.6:
        sys.exit("a sound takes more than 60 % of a block's time")


if __name__ == "__main__":
    main()
