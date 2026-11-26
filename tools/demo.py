#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A demo of APLAT's factory sounds, rendered on a computer by the module's own code (host/host.c): a script of
notes, key pressure, the wheel and a macro for host/run.sh.

  python3 tools/demo.py > build/demo.txt && DOCK=... host/run.sh build/demo.txt build/demo"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = re.findall(r'\{"([^"]+)", FS_', (ROOT / "aplat" / "ap_def.h").read_text())
lines, t, cur = [], 100, 1


def ev(s, at=None):
    lines.append((t if at is None else at, s))


def preset(name):
    global t, cur
    i = NAMES.index(name)
    if i != cur:
        ev(f"enc 6 {i - cur}")
    cur = i
    t += 450


def note(key, start, dur, vel=100):
    ev(f"on {key} {vel}", t + start)
    ev(f"off {key}", t + start + dur)


def chord(keys, start, dur, vel=100, strum=0):
    for i, k in enumerate(keys):
        note(k, start + i * strum, dur - i * strum, vel)


def ramp(kind, a, b, start, dur, steps=24, key=0):
    for i in range(steps + 1):
        v = round(a + (b - a) * i / steps)
        at = t + start + dur * i // steps
        if kind == "press":
            ev(f"midi 0xA0 {key} {v}", at)
        else:
            ev(f"midi 0xB0 {kind} {v}", at)


Am, F, C, G = (57, 60, 64, 69), (53, 57, 60, 65), (48, 55, 60, 64), (55, 59, 62, 67)

preset("SUPER STACK")
for i, ch in enumerate((Am, F, C, G)):
    chord(ch, i * 1300, 1200, 110)
ramp(16, 64, 127, 0, 2600)
ramp(16, 127, 30, 2600, 2600)
t += 6200

preset("GLASS PAD")
chord((50, 57, 60, 64, 65), 0, 5200, 90, strum=120)
for k in (50, 57, 60, 64, 65):
    ramp("press", 0, 127, 1800, 1800, key=k)
    ramp("press", 127, 0, 3700, 1200, key=k)
t += 8200

preset("ACID LINE")
line = [(36, 110), (36, 60), (48, 120), (36, 70), (39, 100), (36, 60), (46, 127), (48, 90),
        (36, 110), (36, 60), (51, 120), (48, 70), (43, 100), (36, 60), (46, 127), (34, 90)]
for bar in range(2):
    for i, (k, v) in enumerate(line):
        tie = i % 4 == 2                           # tied into the next step: it slides
        note(k, bar * 2000 + i * 125, 160 if tie else 95, v)
ramp(16, 0, 110, 0, 4000, 40)
t += 4800

preset("VELO PLUCK")
pat = [57, 64, 69, 72, 76, 72, 69, 64, 53, 60, 65, 69, 72, 69, 65, 60, 48, 55, 60, 64, 67, 64, 60, 55, 55, 62, 67, 71, 74, 71, 67, 62]
for i, k in enumerate(pat):
    note(k, i * 150, 140, 50 + (i * 37) % 78)
t += 6200

preset("PRESS CHOIR")
chord((48, 55, 60, 64), 0, 4200, 80)
for k in (48, 55, 60, 64):
    ramp("press", 0, 127, 1000, 1500, key=k)
    ramp("press", 127, 20, 2700, 1200, key=k)
t += 6200

preset("FM BASS")
for i, (k, v) in enumerate([(36, 120), (36, 70), (43, 100), (36, 127), (39, 80), (41, 110), (36, 70), (48, 127)] * 2):
    note(k, i * 250, 200, v)
t += 4600

preset("COLD BELLS")
for i, (k, v) in enumerate([(76, 90), (79, 70), (84, 100), (83, 60), (79, 80), (72, 110), (76, 70), (67, 90)]):
    note(k, i * 420, 400, v)
chord((48, 55), 0, 3300, 60)
t += 7000

preset("ARP ECHO")
chord((45, 52, 57, 60), 0, 2000, 100)
chord((41, 48, 53, 57), 2000, 2000, 100)
ev("midi 0xB0 123 0", t + 4100)
ramp(16, 20, 127, 0, 4000, 40)
t += 6500

preset("TINE KEYS")
for i, (ch, v) in enumerate(((C, 60), (Am, 85), (F, 105), (G, 125))):
    chord(ch, i * 1100, 1000, v, strum=25)
t += 5600

preset("SYNC LEAD")
mel = [(69, 400), (72, 200), (76, 600), (74, 200), (72, 200), (69, 800)]
at = 0
for k, d in mel:
    note(k, at, d + 60, 110)                       # overlapped: legato
    at += d
ramp(1, 0, 127, 600, 1500)
ramp(1, 127, 0, 2100, 600)
t += 4200

preset("SLOW DRONE")
chord((38, 45, 50, 57), 0, 6000, 90, strum=400)
t += 11000

for at, s in sorted(lines, key=lambda x: x[0]):
    print(at, s)
print(t, "end")
