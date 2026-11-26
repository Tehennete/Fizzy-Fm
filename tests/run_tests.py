#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""APLAT's tests, on a computer: the module unchanged on host/host.c (fm1-dock's screen code under it), built with
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
BIN = OUT / "aplat_host_san"
FS = 44118
ENC_SELECT, ENC_ALGO, K1, K2, K3, K4, ENC_PRESETS = 0, 1, 2, 3, 4, 5, 6
BTN = dict(FX=2, SEL=3, ENV=4, LFO=5, EDIT=6, GLO=7, SAVE=9, ARP=10, SEQ=11, PLAY=12, REC=13)
failures = []


def names_from_source():
    src = (ROOT / "aplat" / "ap_def.h").read_text()
    fac = re.search(r"static const factory_t FACTORY\[\] = \{(.*?)\n\};", src, re.S).group(1)
    return re.findall(r'\{"([^"]+)",', fac)


FACTORY = names_from_source()
_src = (ROOT / "aplat" / "ap_def.h").read_text()
MONO = [n for n, arr in re.findall(r'\{"([^"]+)", (FS_\w+)\}', _src)
        if re.search(r"P_MODE, MODE_(MONO|LEGATO)", re.search(arr + r"\[\] = \{(.*?)NP\};", _src, re.S).group(1))]
NPAGES = (ROOT / "aplat" / "ap_ui.h").read_text().split("static const page_t PAGES[] = {")[1].split("};")[0].count("{\"")


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


def t_guard():
    """a pretend chip on which every note takes 15 % of a block: of 8 keys, notes must go until the blocks are on
    time again (5 notes are 75 %), and on a fast chip nothing may change"""
    for pct, want in ((15, 5), (5, 8)):
        s = Script(f"guard_{pct}").preset(0)
        for k in (48, 52, 55, 59, 62, 65, 69, 72):
            s.on(k)
        s.wait(1500)
        d, out = s.run(env={"APLAT_LOAD": str(pct)})
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
        s.btn("FX").enc(ENC_SELECT, 1).enc(K4, -127)      # the echo off, to count the notes
        s.btn("FX").btn("FX").btn("FX").enc(ENC_SELECT, 0)
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
    for i in range(NPAGES):
        s.wait(120).shot(f"page{i:02d}")
        s.enc(ENC_SELECT, 1)
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
    s.btn("SEL").enc(K2, 40).wait(100).shot("edited")       # CUTOFF up: edited
    s.btn("REC").wait(100).shot("compare")                  # the stored one
    s.btn("REC").wait(50)
    s.btn("SAVE").enc(ENC_PRESETS, 4).wait(100).shot("saveq")
    s.btn("SAVE").wait(200).shot("saved")
    s.enc(ENC_PRESETS, -3).wait(400).enc(ENC_PRESETS, 3).wait(500).shot("back")
    d, out = s.run()
    if d is None:
        return
    if "bank saved (4096 B)" not in out:
        fail("save", "the user sounds were not written")


def t_fuzz(seed, rounds=60):
    """random sounds, random turns, buttons and notes: nothing may trip the sanitizers or reach full scale"""
    rng = np.random.default_rng(seed)
    s = Script(f"fuzz_{seed}")
    held = []
    for _ in range(rounds):
        s.enc(ENC_PRESETS, int(rng.integers(-5, 6))).wait(int(rng.integers(5, 300)))
        for _ in range(int(rng.integers(1, 30))):
            r = rng.random()
            if r < 0.30:
                s.enc(ENC_SELECT, int(rng.integers(-3, 4)))
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


TESTS = {"tuning": t_tuning, "alias": t_alias, "factory": t_factory, "budget": t_unison_budget, "guard": t_guard,
         "legato": t_legato,
         "arp": t_arp, "pages": t_pages, "save": t_save_compare,
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
