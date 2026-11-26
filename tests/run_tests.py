#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert
"""Fizzy-Fm's tests, on a computer: the module unchanged on host/host.c (fm1-dock's screen code under it), built with
the address and undefined-behaviour sanitizers, played by scripts; the sound and the screens are then checked.

  DOCK=/path/to/fm1-dock python3 tests/run_tests.py [-k NAME]      (needs cc, numpy, Pillow)

What it checks: the tuning, the folding back of harmonics on high notes, every factory sound (it sounds, it never
reaches full scale, it ends, nothing is left hanging), the unison's trade for notes, legato and glide, the
arpeggiator's rate from TEMPO and from MIDI clock, every page of the screen, saving, comparing, and a long run of
random knob turns, buttons and notes on random sounds. It cannot check what only the FM-1 can: the time a block
takes on the real chip, and the sound itself by ear."""
import argparse
import os
import re
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DOCK = Path(os.environ.get("DOCK", ROOT.parent / "fm1-dock")).resolve()
OUT = ROOT / "build" / "tests"
BIN = OUT / "fizzy_host_san"
FS = 44118
ENC_SELECT, ENC_ALGO, K1, K2, K3, K4, ENC_PRESETS = 0, 1, 2, 3, 4, 5, 6
BTN = dict(FX=2, SEL=3, ENV=4, LFO=5, EDIT=6, GLO=7, HOME=8, SAVE=9, ARP=10, SEQ=11, PLAY=12, REC=13)
OCT_DOWN, OCT_UP, HOME = 0, 1, 8
failures = []


def names_from_source():
    src = (ROOT / "fizzyfm" / "ap_def.h").read_text()
    fac = re.search(r"static const factory_t FACTORY\[\] = \{(.*?)\n\};", src, re.S).group(1)
    return re.findall(r'\{"([^"]+)",', fac)


FACTORY = names_from_source()
_src = (ROOT / "fizzyfm" / "ap_def.h").read_text()
MONO = [n for n, arr in re.findall(r'\{"([^"]+)", (FS_\w+)\}', _src)
        if re.search(r"P_MODE, MODE_(MONO|LEGATO)", re.search(arr + r"\[\] = \{(.*?)NP\};", _src, re.S).group(1))]
_pages = (ROOT / "fizzyfm" / "ap_ui.h").read_text().split("static const page_t PAGES[] = {")[1].split("};")[0]
PAGE_BTN = re.findall(r'\{"[^"]+", HB_BTN_(\w+),', _pages)     # the button of each page, in order
NPAGES = len(PAGE_BTN)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    cmd = ["cc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wextra",
           "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-missing-field-initializers", "-Werror",
           f"-I{DOCK}/firmware/dock", f"-I{DOCK}/firmware/src", f"-I{DOCK}/firmware/gen", "-o", str(BIN),
           str(ROOT / "host" / "host.c")]
    subprocess.run(cmd, check=True)


class Script:
    def __init__(self, name):
        self.name, self.lines, self.t = name, [], 10

    def at(self, t):
        self.t = t
        return self

    def wait(self, ms):
        self.t += ms
        return self

    def add(self, s):
        self.lines.append(f"{self.t} {s}")
        return self

    def preset(self, idx):                       # from the sound the module starts on (index 1), then by index
        cur = getattr(self, "cur", 1)
        if idx != cur:
            self.add(f"enc {ENC_PRESETS} {idx - cur}")
        self.cur = idx
        return self.wait(400)                    # its tables are built a frame a pass

    def on(self, key, vel=100):
        return self.add(f"on {key} {vel}")

    def off(self, key):
        return self.add(f"off {key}")

    def enc(self, e, clicks):
        return self.add(f"enc {e} {clicks}")

    def btn(self, name):
        return self.add(f"btn {BTN[name]}")

    def midi(self, st, d1=0, d2=0):
        return self.add(f"midi {st} {d1} {d2}")

    def shot(self, name):
        return self.add(f"shot {name}")

    def run(self, tail=200, env=None):
        p = OUT / f"{self.name}.txt"
        lines = sorted(self.lines, key=lambda ln: int(ln.split()[0]))      # (stable: the order written, per ms)
        p.write_text("\n".join(lines + [f"{self.t + tail} end"]) + "\n")
        r = subprocess.run([str(BIN), str(OUT / self.name), str(p)], capture_output=True, text=True,
                           env={**os.environ, **(env or {})})
        if r.returncode:
            fail(self.name, f"the host stopped ({r.returncode}): {r.stderr.strip()[-600:]}")
            return None, r.stdout
        w = wave.open(str(OUT / f"{self.name}.wav"))
        d = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2).astype(np.float64)
        return d, r.stdout


def fail(test, why):
    failures.append((test, why))
    print(f"  FAIL {test}: {why}")


def seg(d, t0, t1):
    return d[int(t0 * FS / 1000):int(t1 * FS / 1000)]


def rms(x):
    return float(np.sqrt(np.mean(np.square(x)))) if len(x) else 0.0


def pitch_of(x):
    """the fundamental, Hz: the strongest low peak of the spectrum, refined by its neighbours"""
    x = x * np.hanning(len(x))
    sp = np.abs(np.fft.rfft(x, 1 << 18))
    i = int(np.argmax(sp))
    a, b, c = np.log(sp[i - 1:i + 2] + 1e-9)
    return (i + 0.5 * (a - c) / (a - 2 * b + c)) * FS / (1 << 18)


def onsets(x):
    """how many notes start in x: its envelope (15 ms) rising through half its usual peak, after having been well under"""
    k = int(FS * 0.015)
    env = np.convolve(np.abs(x), np.ones(k) / k, "same")[::64]
    hi, lo = np.percentile(env, 90) * 0.5, np.percentile(env, 90) * 0.3
    n, armed = 0, True
    for v in env:
        if armed and v >= hi:
            n += 1
            armed = False
        elif v < lo:
            armed = True
    return n


# ---------------------------------------------------------------- the tests
def t_tuning():
    s = Script("tuning").preset(0)
    marks = []
    for key in (24, 36, 48, 60, 72, 84, 96, 108):
        t0 = s.t
        s.on(key).wait(700).off(key).wait(250)
        marks.append((key, t0))
    d, _ = s.run()
    if d is None:
        return
    for key, t0 in marks:
        f = pitch_of(seg(d, t0 + 150, t0 + 650)[:, 0])
        want = 440 * 2 ** ((key - 69) / 12) * 44117.647 / FS   # (the file says 44118 Hz, the codec is 44117.647)
        cents = 1200 * np.log2(f / want)
        print(f"  key {key}: {f:8.2f} Hz, {cents:+.2f} cents")
        if abs(cents) > 3:
            fail("tuning", f"key {key} is {cents:+.1f} cents off")


def t_alias():
    """a saw on high notes: what is not a harmonic of the note must stay far under the note"""
    s = Script("alias").preset(0)
    marks = []
    for key in (84, 91, 96, 103, 108):
        t0 = s.t
        s.on(key).wait(600).off(key).wait(200)
        marks.append((key, t0))
    d, _ = s.run()
    if d is None:
        return
    for key, t0 in marks:
        x = seg(d, t0 + 150, t0 + 550)[:, 0]
        x = x * np.hanning(len(x))
        sp = np.abs(np.fft.rfft(x)) ** 2
        fr = np.fft.rfftfreq(len(x), 1 / FS)
        f0 = pitch_of(x)
        harm = np.zeros(len(sp), bool)
        for h in range(1, int(FS / 2 / f0) + 1):
            harm |= np.abs(fr - h * f0) < 25
        other = 10 * np.log10(sp[~harm & (fr > 60)].sum() / sp[harm].sum() + 1e-12)
        print(f"  key {key}: everything else {other:6.1f} dB under the harmonics")
        if other > -40:
            fail("alias", f"key {key}: {other:.1f} dB of what is not the note")


def t_factory():
    chord = (48, 55, 60, 64, 67, 72)
    for idx, name in enumerate(FACTORY):
        s = Script(f"factory_{idx:02d}").preset(idx)
        t_note = s.t
        s.on(60, 100).wait(900).off(60).wait(500)
        t_chord = s.t
        for k in chord:
            s.on(k, 127)
        s.wait(1500)
        t_rel = s.t
        for k in chord:
            s.off(k)
        s.midi(0xB0, 123, 0)                     # (a latched arpeggio lets go too)
        s.wait(14000)
        d, out = s.run()
        if d is None:
            continue
        n, c = seg(d, t_note, t_note + 900), seg(d, t_chord, t_chord + 1500)
        tail = seg(d, s.t - 300, s.t + 200)
        peak = np.abs(d).max()
        over = int(re.search(r"over full scale (\d+)", out).group(1))
        dc = abs(np.mean(c))
        width = rms(c[:, 0] - c[:, 1]) / (rms(c[:, 0] + c[:, 1]) + 1e-9)
        npk, cpk = np.abs(n).max(), np.abs(c).max()
        print(f"  {idx + 1:2d} {name:12s} note peak {npk:6.0f} rms {rms(n):6.0f}  chord peak {cpk:6.0f} rms {rms(c):6.0f}  "
              f"width {width:4.2f}  dc {dc:4.0f}  tail {rms(tail):5.1f}")
        if not 3000 <= npk <= 20000:              # (a plucked sound has little rms: its peak is what is judged)
            fail(name, f"a single note peaks at {npk:.0f} (3000..20000 wanted)")
        if cpk < 6000 and name not in MONO:
            fail(name, f"the chord peaks at only {cpk:.0f}")
        if over or peak >= 32767:
            fail(name, f"reaches full scale ({over} samples over)")
        if rms(tail) > 3:
            fail(name, f"still sounds 14 s after the keys were let go (rms {rms(tail):.1f})")
        if dc > 120:
            fail(name, f"a DC offset of {dc:.0f}")
        # a flat top: several samples in a row at one value near the peak
        x = c[:, 0]
        flat = (np.abs(x) > 0.6 * np.abs(x).max()) & (np.abs(x) > 2000)
        run3 = flat[2:] & (x[2:] == x[1:-1]) & (x[1:-1] == x[:-2])
        if run3.mean() > 0.002 and name not in ("STEP DIGI", "TUNED NOISE"):   # (sounds that step on purpose)
            fail(name, f"flat tops in the chord ({run3.mean() * 100:.2f} %)")


def t_unison_budget():
    """SUPER STACK has 5 + 3 unison voices a note: fewer notes at once, the next key takes the oldest one's place"""
    s = Script("budget").preset(FACTORY.index("SUPER STACK"))
    for k in (48, 52, 55, 59, 62, 65, 69, 72):
        s.on(k).wait(120)
    s.wait(600).shot("play")
    for k in (48, 52, 55, 59, 62, 65, 69, 72):
        s.off(k)
    s.wait(3000)
    d, _ = s.run()
    if d is None:
        return
    if rms(seg(d, s.t - 200, s.t)) > 3:
        fail("budget", "notes left hanging")


def param_ids():
    """the parameters' numbers by name, from the source (they are also their NRPN numbers)"""
    src = (ROOT / "fizzyfm" / "ap_def.h").read_text()
    body = src.split("#define AP_PARAMS(X)")[1].split("#define X(id")[0]
    subs = {k: re.findall(r"X\(%s##_(\w+)," % v, src) for k, v in (("OSC", "o"), ("ENV", "e"), ("LFO", "l"), ("MOD", "m"), ("FX", "f"))}
    ids = []
    for m in re.finditer(r"AP_(OSC|ENV|LFO|MOD|FX)\(X, (\w+)\)|X\((\w+),", body):
        ids += [f"{m.group(2)}_{x}" for x in subs[m.group(1)]] if m.group(1) else [m.group(3)]
    return {n: i for i, n in enumerate(ids)}


P = param_ids()
_def = (ROOT / "fizzyfm" / "ap_def.h").read_text()
FILTERS = re.findall(r'"([^"]+)"', re.search(r"LS_FTYPE\[\] = \{(.*?)\};", _def, re.S).group(1))
EFFECTS = re.findall(r'"([^"]+)"', re.search(r"LS_FX\[\] = \{(.*?)\};", _def, re.S).group(1))


def t_filters():
    """every filter type on a chord, with resonance, SHAPE and a moving cutoff: it sounds, it stays in range, it ends"""
    chord = (45, 52, 57, 60, 64)
    for i, name in enumerate(FILTERS):
        if not i:
            continue
        s = Script(f"filter_{i:02d}").preset(0)
        for k, v in (("F_TYPE", i), ("F_CUT", 70), ("F_RES", 80), ("F_VAR", 50), ("F_ENV", 100), ("E2_D", 80), ("E2_S", 30),
                     ("A_UNI", 1), ("A_SPR", 100)):
            s.add(f"nrpn {P[k]} {v}")
        s.wait(50)
        t0 = s.t
        for k in chord:
            s.on(k, 110)
        for j in range(20):                       # the cutoff swept by hand while it sounds
            s.wait(60).add(f"nrpn {P['F_CUT']} {20 + j * 5}")
        t1 = s.t
        for k in chord:
            s.off(k)
        s.wait(3000)
        d, out = s.run()
        if d is None:
            continue
        x = seg(d, t0, t1)
        print(f"  {i:2d} {name:8s} peak {np.abs(x).max():6.0f} rms {rms(x):6.0f}  tail {rms(seg(d, s.t - 200, s.t)):4.1f}")
        if rms(x) < 150:
            fail(f"filter {name}", f"nearly silent (rms {rms(x):.0f})")
        if rms(seg(d, s.t - 200, s.t)) > 3:
            fail(f"filter {name}", "it does not end")
        if not np.isfinite(d).all() or np.abs(d).max() > 24001:
            fail(f"filter {name}", "out of range")


def t_effects():
    """every effect alone in slot 1, its knobs where they start and then at both ends; then all of them in turn in
    seven slots at once"""
    chord = (48, 55, 60, 64)
    def knobs(s, slot, t, a=None, b=None, c=None):
        s.add(f"nrpn {P[f'FX{slot}_TYPE']} {t}")
        for nm, v in (("A", a), ("B", b), ("C", c)):
            if v is not None:
                s.add(f"nrpn {P[f'FX{slot}_{nm}']} {v}")
    base = None
    for i, name in enumerate(EFFECTS):
        s = Script(f"fx_{i:02d}").preset(0)
        if i:
            s.btn("FX").enc(K1, i)                # picked on the panel: its knobs take their usual places
        s.wait(50)
        t0 = s.t
        for k in chord:
            s.on(k, 110)
        s.wait(900)
        if i:
            for a, b, c in ((0, 0, 127), (127, 127, 127), (127, 0, 64), (0, 127, 0)):
                knobs(s, 1, i, a, b, c)
                s.wait(300)
        for k in chord:
            s.off(k)
        s.wait(9000)
        d, out = s.run()
        if d is None:
            continue
        x = seg(d, t0 + 100, t0 + 900)
        if base is None:
            base = x.copy()
        diff = rms(x - base) / (rms(base) + 1e-9)
        print(f"  {i:2d} {name:8s} rms {rms(x):6.0f} peak {np.abs(d).max():6.0f}  differs from dry by {diff * 100:5.1f} %  "
              f"tail {rms(seg(d, s.t - 200, s.t)):4.1f}")
        if i and diff < 0.02 and name not in ("COMP", "EQ"):      # (at their starting places those two leave INIT as it is)
            fail(f"effect {name}", "it changes nothing")
        if rms(x) < 300:
            fail(f"effect {name}", "nearly silent")
        if rms(seg(d, s.t - 200, s.t)) > 3:
            fail(f"effect {name}", "it does not end")
        if np.abs(d).max() > 24001:
            fail(f"effect {name}", "out of range")
    s = Script("fx_all").preset(0)
    for k in chord:
        s.on(k, 110)
    for rot in range(len(EFFECTS)):               # seven slots, the effects moving through them
        for slot in range(1, 8):
            knobs(s, slot, (rot + slot) % len(EFFECTS), 90, 90, 100)
        s.wait(250)
    for k in chord:
        s.off(k)
    s.wait(1000)
    d, out = s.run()
    if d is not None and np.abs(d).max() > 24001:
        fail("effects", "out of range with seven at once")


def t_link():
    """the editor's link: everything asked for comes with the right sum; a parameter set by NRPN is set; a table of
    one's own arrives whole, is kept, and plays"""
    s = Script("link").preset(0)
    s.add("nrpn 2000 0").wait(400)
    s.add(f"nrpn {P['F_CUT']} 33").add(f"nrpn {P['X3_AMT']} 200").wait(20)
    s.enc(K1, 1).wait(50)                         # a knob turned on the FM-1: the editor is told
    rng = np.random.default_rng(5)
    spec = np.zeros(1024, int)
    for f in range(8):                            # a table: a saw that gets brighter
        for h in range(64):
            spec[(f * 64 + h) * 2] = int(9000 / (h + 1) ** (1.6 - 0.1 * f))
    spec[5], spec[7] = -60, -50                   # (two values the module must move out of -64..-49)
    s.add("nrpn 2003 1")
    tot = 0
    for v in spec:
        u = int(v) & 0xFFFF
        tot += u
        s.add(f"nrpn {3000 + (u >> 14)} {u & 0x3FFF}")
    s.add(f"nrpn 2005 {tot & 0x3FFF}").wait(100)
    s.add("nrpn 2003 2").add("nrpn 3000 5").add("nrpn 2005 5").wait(50)       # a broken one: refused
    s.add(f"nrpn {P['A_WAVE']} 65").wait(400)     # USER 2
    t0 = s.t
    s.on(57).wait(600).off(57).wait(2500)
    d, out = s.run()
    if d is None:
        return
    got = {int(a): int(b) for a, b in re.findall(r"NRPN out (\d+) = (\d+)", out.split("NRPN out 2011")[0])}
    summ = re.search(r"NRPN out 2011 = (\d+)", out)
    n_par = sum(1 for k in got if k < 132)
    print(f"  everything: {n_par} parameters, the name {''.join(chr(got.get(1000 + i, 0) or 32) for i in range(12)).strip()!r}")
    if n_par != 132 or not summ or int(summ.group(1)) != sum(v for k, v in got.items() if k < 132) & 0x3FFF:
        fail("link", "the dump or its sum is wrong")
    if "NRPN out 2012 = 1" not in out or "NRPN out 2012 = 0" not in out:
        fail("link", "the tables' answers (one whole, one broken) are not both there")
    if "bank 1 saved (6144 B)" not in out:
        fail("link", "the table was not kept")
    if f"NRPN out {P['M1']} = " not in out.split("NRPN out 2011")[1]:
        fail("link", "a knob turned on the FM-1 was not sent")
    x = seg(d, t0 + 100, t0 + 550)[:, 0]
    f0 = pitch_of(x)
    print(f"  the table of one's own plays: {f0:.1f} Hz, rms {rms(x):.0f}")
    if abs(f0 - 220) > 2 or rms(x) < 300:
        fail("link", "the user table does not play")


def t_guard():
    """a pretend chip on which every note takes 15 % of a block: of 8 keys, notes must go until the blocks are on
    time again (5 notes are 75 %), and on a fast chip nothing may change"""
    for pct, want in ((15, 5), (5, 8)):
        s = Script(f"guard_{pct}").preset(0)
        for k in (48, 52, 55, 59, 62, 65, 69, 72):
            s.on(k)
        s.wait(1500)
        d, out = s.run(env={"FIZZY_LOAD": str(pct)})
        if d is None:
            continue
        n = int(re.search(r"notes allowed at the end: (\d+)", out).group(1))
        print(f"  {pct} % a note: {n} notes left")
        if n != want:
            fail("guard", f"at {pct} % a note, {n} notes are allowed ({want} expected)")
        if rms(seg(d, 1000, 1400)) < 500:
            fail("guard", "silence")


def t_legato():
    s = Script("legato").preset(FACTORY.index("ACID LINE"))
    t0 = s.t
    s.on(48).wait(500).on(60).wait(700)          # tied: it glides up an octave, no new attack
    t1 = s.t
    s.off(48).off(60).wait(600)
    d, _ = s.run()
    if d is None:
        return
    f_a = pitch_of(seg(d, t0 + 250, t0 + 480)[:, 0])
    f_mid = pitch_of(seg(d, t0 + 505, t0 + 540)[:, 0])
    f_b = pitch_of(seg(d, t1 - 250, t1 - 20)[:, 0])
    print(f"  {f_a:.1f} Hz, gliding at {f_mid:.1f} Hz, then {f_b:.1f} Hz")
    if abs(1200 * np.log2(f_b / f_a) - 1200) > 15:
        fail("legato", f"the tied note is not an octave up ({f_a:.1f} -> {f_b:.1f} Hz)")
    if not f_a * 1.02 < f_mid < f_b * 0.98:
        fail("legato", "no glide between the tied notes")


def t_arp():
    """ARP ECHO: 1/16 notes at 120 BPM are 8 a second; with MIDI clock at 150 BPM, 10 a second"""
    idx = FACTORY.index("ARP ECHO")
    for name, bpm in (("arp_tempo", None), ("arp_midi", 150)):
        s = Script(name).preset(idx)
        s.btn("FX").enc(K4, -127).btn("FX").enc(K4, -127)          # the echo and the reverb off, to count the notes
        s.wait(50)
        if bpm:
            t, end = s.t, s.t + 5200
            step = 60000 / bpm / 24
            x = float(t)
            while x < end:
                s.lines.append(f"{int(x)} midi 0xF8 0 0")
                x += step
        s.wait(600)
        t0 = s.t
        for k in (48, 55, 60):
            s.on(k)
        s.wait(4000)
        s.shot("arp") if not bpm else None
        s.midi(0xB0, 123, 0).wait(1500)
        d, _ = s.run()
        if d is None:
            continue
        n = onsets(seg(d, t0, t0 + 4000)[:, 0])
        want = (bpm or 120) / 60 * 4 * 4
        print(f"  {name}: {n} notes in 4 s, {want:.0f} expected")
        if abs(n - want) > 2:
            fail(name, f"{n} notes in 4 s, {want:.0f} expected")


def t_pages():
    """every page: it draws, and a picture of each for the README"""
    s = Script("pages").preset(2)
    s.on(60).on(64).on(67).wait(300)
    for i in range(NPAGES):                   # each tap of a section's button: its next page (HOME twice: RANDOM)
        if i:
            s.btn(PAGE_BTN[i])
            if PAGE_BTN[i] == "HOME":
                s.wait(100).btn("HOME")
        s.wait(120).shot(f"page{i:02d}")
    s.off(60).off(64).off(67).wait(300)
    for b in BTN:
        if b not in ("SAVE", "REC"):
            s.btn(b).wait(60)
    d, out = s.run()
    if d is None:
        return
    m = re.search(r"most pixels sent in one draw\(\) (\d+)", out)
    print(f"  {NPAGES} pages; at most {m.group(1)} pixels sent in one pass of the main loop")
    if int(m.group(1)) > 240 * 96:
        fail("pages", "more than a canvas in one pass")


def t_save_compare():
    s = Script("save").preset(3)
    s.btn("ENV").enc(K2, 40).wait(100).shot("edited")       # FILTER: CUTOFF up, edited
    s.btn("SAVE").enc(ENC_PRESETS, 4).wait(100).shot("saveq")
    s.btn("REC").wait(100).shot("compare")                  # the stored one, the save still asked
    s.btn("REC").wait(50)
    s.btn("SAVE").wait(200).shot("saved")
    s.enc(ENC_PRESETS, -3).wait(400).enc(ENC_PRESETS, 3).wait(500).shot("back")
    d, out = s.run()
    if d is None:
        return
    if "bank saved (4608 B)" not in out:
        fail("save", "the user sounds were not written")


KINDS = ["ANY", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "BELL", "WEIRD", "CHAOS"]


def t_random():
    """the dice (fizzyfm/ap_rand.h). Thousands of throws: every parameter in its range, a name, the same seed the
    same sound, a mutation still a sound. A few of each kind played: none silent, none over the limiter, and how far
    apart their levels are. Then the panel: ROLL, back, forward, MUTATE, and the editor told each time."""
    dump = OUT / "rand_dump"
    subprocess.run(["cc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-Wall", "-Wextra",
                    "-Werror", "-Wno-unused-function", "-o", str(dump), str(ROOT / "tests" / "rand_dump.c")], check=True)
    seen, kinds = set(), set()
    for kind in range(len(KINDS)):
        for chaos in (0, 48, 127):
            r = subprocess.run([str(dump), "200", str(kind), str(chaos), "m"], capture_output=True, text=True)
            if r.returncode:
                fail("random", f"{KINDS[kind]}, chaos {chaos}: {r.stderr.strip()[-300:]}")
                return
            for ln in r.stdout.splitlines()[::2]:
                k, h = ln.split()
                b = bytes.fromhex(h)
                seen.add(b[132:].split(b"\0")[0])
                if kind == 0:
                    kinds.add(int(k))
                elif int(k) != kind:
                    fail("random", f"asked for {KINDS[kind]}, got {KINDS[int(k)]}")
    print(f"  {len(KINDS) * 3 * 200} throws and as many mutations: {len(seen)} names, ANY gave {len(kinds)} kinds")
    if len(kinds) != 7 or len(seen) < 150:
        fail("random", "ANY does not reach every kind, or the names are too few")
    # a few of each kind, as an editor would send them, played
    per, levels = 8, []
    for kind in range(1, len(KINDS)):
        pats = [bytes.fromhex(ln.split()[1]) for ln in
                subprocess.run([str(dump), str(per), str(kind), "64"], capture_output=True, text=True).stdout.splitlines()]
        s = Script(f"random_{kind}")
        marks = []
        for b in pats:
            for i in range(132):
                s.add(f"nrpn {i} {b[i]}")
            s.wait(420)
            notes = [60] if b[P["MODE"]] or kind == 1 else [60, 64, 67]
            for n in notes:
                s.on(n)
            marks.append(s.t)
            s.wait(700)
            for n in notes:
                s.off(n)
            s.wait(380)
        d, out = s.run()
        if d is None:
            return
        for j, t0 in enumerate(marks):
            x = seg(d, t0 + 10, t0 + 700)
            pk = float(np.abs(x).max())
            levels.append(pk)
            if pk < 500:
                fail("random", f"{KINDS[kind]}, seed {j + 1}: silent (peak {pk:.0f})")
            if pk > 24000:
                fail("random", f"{KINDS[kind]}, seed {j + 1}: over the limiter ({pk:.0f})")
    db = 20 * np.log10(np.array(levels) / np.median(levels))
    print(f"  {len(levels)} sounds played: peaks from {db.min():+.1f} to {db.max():+.1f} dB around the middle one, "
          f"8 in 10 within {np.percentile(db, 10):+.1f} .. {np.percentile(db, 90):+.1f} dB")
    if np.percentile(db, 10) < -9 or np.percentile(db, 90) > 9:
        fail("random", "the levels are too far apart")
    # the panel
    s = Script("random").preset(2)
    s.add("nrpn 2000 0").wait(900)                # an editor listens: every sound the panel makes is sent to it
    s.btn("HOME").wait(100).btn("HOME").wait(60).shot("random0")     # HOME twice: RANDOM
    s.btn("SEL").wait(900).btn("SEL").wait(900)   # two throws, the second over the first
    s.on(60).wait(300).off(60).wait(200).shot("random2")
    s.add(f"down {BTN['SEL']}").wait(700).add(f"up {BTN['SEL']}").wait(100).shot("kept")   # held: kept
    s.btn("SEL").wait(900)                        # a third throw, not kept
    s.enc(K1, -1).wait(900)                       # KEPT, back: the second (the third is gone)
    s.enc(K1, -1).wait(900).shot("random_first")  # the sound before the dice
    s.enc(K1, -1).wait(300)                       # no further
    s.enc(K4, 1).btn("SEL").wait(900)             # SEL THROWS: VARY. A variation of it
    s.add(f"down {BTN['SEL']}").wait(700).add(f"up {BTN['SEL']}").wait(100)     # kept: the second sound kept
    s.enc(K1, -1).wait(900)                       # back: the first kept
    d, out = s.run()
    if d is None:
        return
    sums = [int(v) for v in re.findall(r"NRPN out 2011 = (\d+)", out)]
    if len(sums) != 8:
        fail("random", f"{len(sums)} sounds reached the editor, 8 expected")
        return
    first, a, b, c, b2, first2, m, b3 = sums
    print(f"  the panel: throws {a} {b}, kept {b}, thrown {c}, back {b2}, the sound before {first2}, varied {m}, back {b3}")
    if not (b2 == b and b3 == b and first2 == first and m != first and len({first, a, b, c}) == 4):
        fail("random", "the throws, the sounds kept and the way back are wrong")
    if "KEPT 1" not in out and "KEPT" not in out:
        pass
    if float(np.abs(d).max()) > 24000:
        fail("random", "over the limiter")


def t_keys():
    """the FM-1's own keys: they play in the module's octave and go out as MIDI; OCT - / OCT + light and blink the
    faster the further; both at once: the middle; SELECT turns the tempo"""
    s = Script("keys").preset(0)
    s.add("nrpn 2000 0").wait(600)              # (an editor listens: the tempo is sent to it)
    s.add("key 7 100").wait(500).add("keyup 7").wait(400)            # C4
    t1 = s.t
    s.add(f"btn {OCT_UP}").wait(50).add("leds")
    s.add("key 7 100").wait(500).add("keyup 7").wait(400)            # C5
    s.add(f"btn {OCT_UP}").add(f"btn {OCT_UP}").wait(20)             # +3: blinking fast
    for _ in range(12):
        s.wait(45).add("leds")
    s.add("key 7 100").wait(150)                # held across a change of octave: it still lets go of its own note
    s.add(f"btn {OCT_DOWN}").wait(100).add("keyup 7").wait(400)
    s.add(f"down {OCT_DOWN}").add(f"down {OCT_UP}").add(f"up {OCT_DOWN}").add(f"up {OCT_UP}").wait(60).add("leds")
    s.enc(ENC_SELECT, 1).wait(150).enc(ENC_SELECT, 1).wait(100).shot("tempo")   # (slowly: a step a click)
    d, out = s.run()
    if d is None:
        return
    f0, f1 = pitch_of(seg(d, t1 - 800, t1 - 450)[:, 0]), pitch_of(seg(d, t1 + 100, t1 + 450)[:, 0])
    leds = [int(v, 16) for v in re.findall(r"leds ([0-9a-f]{4})", out)]
    blink = [v >> OCT_UP & 1 for v in leds[1:13]]
    print(f"  key C4 {f0:.1f} Hz, an octave up {f1:.1f} Hz; OCT + at +1 {leds[0] >> OCT_UP & 1}, at +3 {''.join(map(str, blink))}"
          f", both: {leds[-1] & 3}")
    if abs(f0 - 261.6) > 2 or abs(f1 - 523.3) > 4:
        fail("keys", "the keys are not in their octave")
    if not (leds[0] >> OCT_UP & 1) or len(set(blink)) != 2 or leds[-1] & 3:
        fail("keys", "the octave's lights are wrong")
    ons = re.findall(r"MIDI out 90 (\d+) 100", out)
    offs = re.findall(r"MIDI out 80 (\d+) 0", out)
    if ons != ["60", "72", "96"] or offs != ["60", "72", "96"]:
        fail("keys", f"the keys sent {ons} / {offs}")
    if f"NRPN out {P['TEMPO']} = 62" not in out:
        fail("keys", "SELECT did not turn the tempo")


def t_knobs():
    """a knob turned slowly goes a step a click; turned fast, further (eight clicks 10 ms apart: CUTOFF far); a list
    (the filter's type) a step a click however fast"""
    s = Script("knobs").preset(0)
    s.add("nrpn 2000 0").wait(600)
    s.btn("ENV").wait(100)                      # FILTER: TYPE, CUTOFF, ...
    for _ in range(4):
        s.enc(K2, -1).wait(150)                 # slowly: -4 (INIT's filter is open)
    for _ in range(8):
        s.enc(K2, -1).wait(10)                  # fast
    s.wait(300)
    for _ in range(6):
        s.enc(K1, 1).wait(10)                   # the type, fast: +6 still
    s.wait(300)
    d, out = s.run()
    if d is None:
        return
    cut = [int(v) for v in re.findall(rf"NRPN out {P['F_CUT']} = (\d+)", out)]
    typ = [int(v) for v in re.findall(rf"NRPN out {P['F_TYPE']} = (\d+)", out)]
    print(f"  CUTOFF {cut[0]} .. slowly {cut[4]} .. fast {cut[-1]}; TYPE {typ[0]} .. {typ[-1]}")
    if cut[0] - cut[4] != 4 or cut[4] - cut[-1] < 30 or typ[-1] - typ[0] != 6:
        fail("knobs", "the knobs do not speed up as they should")


def t_seq():
    """the sequencer: REC counts a bar in and records the keys (late by 30 ms: QUANTIZE puts them back on the
    sixteenths), the pattern plays them back; SEL undoes the pass and redoes it; the STEPS page writes steps from the
    keys; REC held clears; the patterns go to the flash once it stops"""
    s = Script("seq").preset(0)
    s.btn("SEQ").wait(200)
    t0 = s.t
    s.btn("REC")                                # a bar counted in (2 s at 120), then recording
    s.wait(2030)
    for k in (7, 11, 14, 19):                   # C4 E4 G4 C5 on the beats, 30 ms late
        s.add(f"key {k} 100").wait(200).add(f"keyup {k}").wait(300)
    s.btn("REC")                                # recording off (the second bar: played back)
    s.wait(t0 + 6100 - s.t).shot("pattern")
    s.btn("PLAY").wait(200)                     # stopped
    s.btn("SEL").wait(100)                      # undo: empty
    s.btn("PLAY").wait(2100).btn("PLAY").wait(200)
    t_undo = s.t - 2300
    s.btn("SEL").wait(100)                      # redo
    s.btn("SEQ").wait(200)                      # STEPS: step 1
    for k in (12, 16):                          # F4 then A4 on steps 1 and 2
        s.add(f"key {k} 100").wait(100).add(f"keyup {k}").wait(100)
    s.wait(200).shot("steps")
    t_play = s.t
    s.btn("PLAY").wait(2100).btn("PLAY").wait(1300)
    s.add(f"down {BTN['REC']}").wait(1600).add(f"up {BTN['REC']}").wait(1300)   # held: cleared
    d, out = s.run()
    if d is None:
        return
    want = [(0, 261.6), (500, 329.6), (1000, 392.0), (1500, 523.3)]
    got = [pitch_of(seg(d, t0 + 4000 + dt + 40, t0 + 4000 + dt + 190)[:, 0]) for dt, _ in want]
    quiet = rms(seg(d, t_undo + 100, t_undo + 2000))
    st = [pitch_of(seg(d, t_play + dt + 30, t_play + dt + 110)[:, 0]) for dt in (0, 125)]
    print(f"  played back {', '.join(f'{f:.1f}' for f in got)} Hz; undone: rms {quiet:.0f}; steps {st[0]:.1f} {st[1]:.1f} Hz")
    if any(abs(f - w) > w * 0.02 for f, (_, w) in zip(got, want)):
        fail("seq", "the pattern recorded does not play back in time and in tune")
    if quiet > 30:
        fail("seq", "the pass undone still plays")
    if abs(st[0] - 349.2) > 7 or abs(st[1] - 440.0) > 9:
        fail("seq", "the steps written from the keys are wrong")
    if "bank 2 saved" not in out:
        fail("seq", "the patterns were not written to the flash")


def t_fuzz(seed, rounds=60):
    """random sounds, random turns, buttons and notes: nothing may trip the sanitizers or reach full scale"""
    rng = np.random.default_rng(seed)
    s = Script(f"fuzz_{seed}")
    held, kheld = [], []
    for _ in range(rounds):
        s.enc(ENC_PRESETS, int(rng.integers(-5, 6))).wait(int(rng.integers(5, 300)))
        for _ in range(int(rng.integers(1, 30))):
            r = rng.random()
            if r < 0.10:
                s.enc(ENC_SELECT, int(rng.integers(-3, 4)))
            elif r < 0.30:                        # the FM-1's own keys, its octave, HOME
                q = rng.random()
                if q < 0.15:
                    s.add(f"btn {int(rng.choice([OCT_DOWN, OCT_UP, HOME]))}")
                elif q < 0.20:
                    s.add(f"down {OCT_DOWN}").add(f"down {OCT_UP}").add(f"up {OCT_DOWN}").add(f"up {OCT_UP}")
                elif q < 0.65:
                    k = int(rng.integers(0, 27))
                    s.add(f"key {k} {int(rng.integers(0, 128))}")
                    kheld.append(k)
                elif kheld:
                    k = kheld.pop(int(rng.integers(0, len(kheld))))
                    if rng.random() < 0.3:
                        s.add(f"press {k} {int(rng.integers(0, 128))}")
                    s.add(f"keyup {k}")
            elif r < 0.72:
                s.enc(int(rng.integers(K1, K4 + 1)), int(rng.choice([-200, -40, -7, -1, 1, 7, 40, 200])))
            elif r < 0.78:
                s.enc(ENC_ALGO, int(rng.integers(-24, 25)))
            elif r < 0.84:
                s.btn(str(rng.choice([b for b in BTN if b != "SAVE"])))
            elif r < 0.86:
                s.btn("SAVE").btn("SAVE")
            elif r < 0.90:
                s.midi(0xE0, int(rng.integers(0, 128)), int(rng.integers(0, 128)))
            elif r < 0.93:
                s.midi(0xB0, int(rng.choice([1, 64, 74, 71, 16, 17, 123])), int(rng.integers(0, 128)))
            elif r < 0.95:
                s.midi(0xD0, int(rng.integers(0, 128)), 0)
            elif r < 0.97:
                s.midi(0xC0, int(rng.integers(0, 128)), 0)
            else:
                s.midi(0xF8)
            s.wait(int(rng.integers(0, 12)))
            if rng.random() < 0.5:
                k = int(rng.integers(0, 128))
                s.on(k, int(rng.integers(1, 128)))
                held.append(k)
                if rng.random() < 0.3:
                    s.midi(0xA0, k, int(rng.integers(0, 128)))
            if held and rng.random() < 0.4:
                s.off(held.pop(int(rng.integers(0, len(held)))))
        s.wait(int(rng.integers(20, 500)))
    for k in held:
        s.off(k)
    for k in kheld:
        s.add(f"keyup {k}")
    s.midi(0xB0, 123, 0).wait(500)
    d, out = s.run()
    if d is None:
        return
    over = int(re.search(r"over full scale (\d+)", out).group(1))
    print(f"  seed {seed}: {len(s.lines)} events, {len(d) / FS:.0f} s, peak {np.abs(d).max():.0f}")
    if over:
        fail(f"fuzz {seed}", f"{over} samples over full scale")
    if not np.isfinite(d).all():
        fail(f"fuzz {seed}", "not a number")


TESTS = {"tuning": t_tuning, "alias": t_alias, "factory": t_factory, "filters": t_filters, "effects": t_effects,
         "link": t_link, "budget": t_unison_budget, "guard": t_guard,
         "legato": t_legato,
         "arp": t_arp, "pages": t_pages, "save": t_save_compare, "random": t_random, "keys": t_keys, "seq": t_seq, "knobs": t_knobs,
         "fuzz": lambda: [t_fuzz(seed) for seed in (1, 2, 3, 4)]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-k", help="only the tests whose name contains this")
    a = ap.parse_args()
    if not (DOCK / "firmware" / "dock" / "hb_module.h").exists():
        raise SystemExit(f"no fm1-dock in {DOCK} (set DOCK)")
    build()
    for name, fn in TESTS.items():
        if a.k and a.k not in name:
            continue
        print(name)
        fn()
    # the screens as PNG
    from PIL import Image
    for p in sorted(OUT.glob("*.ppm")):
        Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
        p.unlink()
    if failures:
        print(f"\n{len(failures)} FAILED")
        for t, w in failures:
            print(f"  {t}: {w}")
        sys.exit(1)
    print("\nall passed")


if __name__ == "__main__":
    main()
