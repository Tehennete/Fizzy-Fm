#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert (Fizzy-Fm)
"""Fizzy-Fm's tables -> fizzyfm/ap_gen.h.

Every wavetable is original: 24 tables of 8 frames, each frame designed here as a formula (a wave in time or a
spectrum) and kept as its first 64 harmonics (a sine and a cosine part each). The FM-1 builds the band-limited
tables it plays from these numbers when a table is picked, so 24 tables cost 49 KB of flash and no RAM.

  python3 tools/gen_tables.py        (needs numpy)
"""
from pathlib import Path
import numpy as np

FS = 44117.647            # the FM-1's codec: 192 MHz / 17 / 256
SUB = 32                  # frames per control tick
CR = FS / SUB
NH = 64                   # harmonics kept per frame
NF = 8                    # frames per table
N = 2048
x = np.arange(N) / N
TAU = 2 * np.pi
# (length, harmonics) of the levels a frame is played at (fizzyfm/ap_eng.h WT_LEN, WT_NH)
LEVELS = list(zip((512, 512, 256, 256, 128, 128, 64, 64, 32, 32, 32, 32), (64, 45, 32, 22, 16, 11, 8, 5, 4, 3, 2, 1)))
rng = np.random.default_rng(1977)


def from_spec(amp, phase=None):
    """a wave from harmonic amplitudes (index 0 = the fundamental); sine phases unless phase is given"""
    y = np.zeros(N)
    for h, a in enumerate(amp[:NH], 1):
        if a:
            y += a * np.sin(TAU * h * x + (phase[h - 1] if phase is not None else 0))
    return y


def saw(t=None):
    return 2 * ((x if t is None else t) % 1.0) - 1


def pulse(w):
    return np.where(x < w, 1.0, -1.0)


def tri():
    return 2 * np.abs(saw(x + 0.25)) - 1


def mix(a, b, k):
    return a * (1 - k) + b * k


def formant(freqs, bw, tilt, f0=130.0):
    h = np.arange(1, NH + 1)
    amp = h ** -tilt
    env = np.zeros(NH)
    for i, f in enumerate(freqs):
        env += (0.9 ** i) * np.exp(-0.5 * ((h * f0 - f) / bw) ** 2)
    return amp * (0.06 + env)


VOW = {"A": (730, 1090, 2440), "E": (530, 1840, 2480), "I": (270, 2290, 3010), "O": (570, 840, 2410),
       "U": (300, 870, 2240)}


def vow(a, b, k):
    return tuple(mix(np.array(VOW[a], float), np.array(VOW[b], float), k))


def tables():
    t = []
    t.append(("BASIC", [np.sin(TAU * x), tri(), mix(tri(), saw(), 0.6), saw(), mix(saw(), pulse(0.5), 0.5),
                        pulse(0.5), pulse(0.3), pulse(0.12)]))
    h = np.arange(1, NH + 1)
    t.append(("SAW", [from_spec(h ** -k) for k in (2.6, 2.1, 1.7, 1.35, 1.0, 0.85, 0.7, 0.55)]))
    odd = (h % 2).astype(float)
    t.append(("SQUARE", [from_spec(odd * h ** -k) for k in (2.6, 2.1, 1.7, 1.35, 1.0, 0.85, 0.7, 0.55)]))
    t.append(("PWM", [pulse(w) for w in (0.5, 0.42, 0.34, 0.26, 0.19, 0.13, 0.08, 0.04)]))
    t.append(("SYNC", [saw(r * x) * (1 - 0.35 * x) for r in (1, 1.5, 2, 2.5, 3, 3.5, 4.25, 5)]))
    t.append(("FORMANT", [from_spec(formant(f, 150, 1.0)) for f in
                          (VOW["A"], vow("A", "E", .5), VOW["E"], vow("E", "I", .5), VOW["I"], VOW["O"],
                           vow("O", "U", .5), VOW["U"])]))
    t.append(("VOCAL", [from_spec(formant(vow(a, b, k), 260, 1.6)) for a, b, k in
                        (("U", "U", 0), ("U", "O", .35), ("U", "O", .7), ("O", "O", 0), ("O", "A", .35),
                         ("O", "A", .7), ("A", "A", 0), ("A", "E", .4))]))
    idx = (0, 0.5, 1, 1.6, 2.4, 3.4, 4.6, 6)
    t.append(("FM 1:1", [np.sin(TAU * x + i * np.sin(TAU * x)) for i in idx]))
    t.append(("FM 1:2", [np.sin(TAU * x + i * np.sin(2 * TAU * x)) for i in idx]))
    t.append(("BELL", [np.sin(TAU * x + i * 0.8 * np.sin(3 * TAU * x) + i * 0.3 * np.sin(7 * TAU * x)) for i in idx]))
    fr = []
    for k in range(NF):
        a = np.zeros(NH)
        for j, hh in enumerate((1, 3 + k, 6 + 2 * k, 10 + 3 * k, 15 + 4 * k)):
            if hh <= NH:
                a[hh - 1] = 1.0 / (1 + j * 0.9)
        fr.append(from_spec(a))
    t.append(("GLASS", fr))
    bars = (1, 2, 3, 4, 6, 8, 10, 12, 16)                    # the harmonics of nine drawbars
    regs = ("800000000", "880000000", "888000000", "808808000", "888800008", "688600000", "888888888", "806060606")
    fr = []
    for r in regs:
        a = np.zeros(NH)
        for hh, d in zip(bars, r):
            a[hh - 1] += int(d) / 8
        fr.append(from_spec(a))
    t.append(("ORGAN", fr))
    t.append(("HARMO", [from_spec(0.5 * (h == 1) + np.exp(-0.5 * ((h - c) / (1 + c * 0.12)) ** 2))
                        for c in (2, 3, 5, 8, 12, 17, 24, 33)]))
    t.append(("FOLD", [np.sin(g * np.sin(TAU * x) * np.pi / 2) for g in (1, 1.6, 2.3, 3.1, 4, 5, 6.3, 8)]))
    t.append(("DRIVE", [np.tanh(g * (np.sin(TAU * x) + a * np.sin(2 * TAU * x)))
                        for g, a in ((1, 0), (2, 0), (4, 0), (8, 0.1), (12, 0.25), (16, 0.4), (20, 0.6), (30, 0.8))]))
    t.append(("BITS", [np.round(np.sin(TAU * np.floor(x * s * 4) / (s * 4)) * s / 2) / (s / 2)
                       for s in (64, 32, 24, 16, 12, 8, 6, 4)]))
    primes = (1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61)
    fr = []
    for k in range(NF):
        a = np.zeros(NH)
        for j, p in enumerate(primes):
            a[p - 1] = np.exp(-0.5 * ((j - 1.6 * k) / 3.2) ** 2) + 0.12
        a[0] = 0.8
        fr.append(from_spec(a, phase=rng.uniform(0, TAU, NH)))
    t.append(("METAL", fr))
    t.append(("COMB", [from_spec(h ** -1.0 * (1.02 + np.cos(np.pi * h / m))) for m in (32, 22, 15, 10.5, 7.5, 5.5, 4, 3)]))
    t.append(("ODDEVEN", [from_spec(h ** -1.1 * mix(odd, 1 - odd + (h == 1), k)) for k in np.linspace(0, 1, NF)]))
    base = rng.uniform(0, 1, (4, NH)) ** 2
    fr = []
    for k in range(NF):
        p = k * 3 / (NF - 1)
        i0 = min(int(p), 2)
        a = mix(base[i0], base[i0 + 1], p - i0) * h ** -0.7
        a[0] = 1.0
        fr.append(from_spec(a, phase=np.cumsum(np.full(NH, 0.9 + 0.2 * k))))
    t.append(("DIGI", fr))
    t.append(("STRING", [from_spec(h ** -1.05 * (0.55 + np.exp(-0.5 * ((h - c) / 4.5) ** 2)))
                         for c in (4, 5, 6.5, 8, 10, 12, 15, 19)]))
    t.append(("BRASS", [from_spec(h ** -1.0 / (1 + (h / c) ** 6)) for c in (2.5, 4, 6, 9, 13, 19, 28, 44)]))
    t.append(("REED", [from_spec(h ** -1.25 * mix(odd, np.ones(NH), k) * (0.5 + np.exp(-0.5 * ((h - 7 - 6 * k) / 3.5) ** 2)))
                       for k in np.linspace(0, 1, NF)]))
    fr = []
    for k in range(NF):
        a = np.zeros(NH)
        a[0], a[1], a[2] = 1.0, 0.22, 0.08
        for hh, w in ((7, 0.1), (11, 0.07), (14, 0.05), (21, 0.03)):
            a[hh - 1] += w * (0.2 + 1.6 * k)
        fr.append(from_spec(a))
    t.append(("TINE", fr))
    t += more_tables(h, odd)
    return t


def spec_frames(fn, ks):
    return [from_spec(np.asarray(fn(k), float)) for k in ks]


def more_tables(h, odd):
    """the second set: 40 more tables, as formulas too"""
    t = []
    lin = np.linspace(0, 1, NF)
    even = 1 - odd + (h == 1)

    def only(hs, amps=None):
        a = np.zeros(NH)
        for i, hh in enumerate(hs):
            if hh <= NH:
                a[hh - 1] = 1.0 if amps is None else amps[i]
        return a

    # additive families: more partials come in from frame to frame
    t.append(("SINES", [from_spec(only((1, 2, 3, 4, 5, 6, 7, 8)[:k + 1], [1 / (1 + 0.6 * j) for j in range(k + 1)])) for k in range(NF)]))
    t.append(("ODDS", [from_spec(only((1, 3, 5, 7, 9, 11, 13, 15)[:k + 1], [1 / (1 + 0.5 * j) for j in range(k + 1)])) for k in range(NF)]))
    t.append(("EVENS", [from_spec(only((1, 2, 4, 6, 8, 10, 12, 14)[:k + 1], [1 / (1 + 0.5 * j) for j in range(k + 1)])) for k in range(NF)]))
    t.append(("OCTAVES", [from_spec(only((1, 2, 4, 8, 16, 32, 64), [max(0.0, 1 - abs(j - k * 6 / 7) / 2.2) + (0.3 if j == 0 else 0) for j in range(7)])) for k in range(NF)]))
    t.append(("FIFTHS", [from_spec(only((1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48), [max(0.0, 1 - abs(j - k * 10 / 7) / 3.0) + (0.4 if j == 0 else 0) for j in range(11)])) for k in range(NF)]))
    # shapes in time
    t.append(("PULSE2", [np.where((x < w) | ((x > 0.5) & (x < 0.5 + w * 0.6)), 1.0, -1.0) for w in (0.25, 0.21, 0.17, 0.14, 0.11, 0.08, 0.06, 0.04)]))
    t.append(("TRISAW", [np.where(x < p, x / p, (1 - x) / (1 - p)) * 2 - 1 for p in (0.5, 0.42, 0.34, 0.26, 0.18, 0.11, 0.05, 0.015)]))
    t.append(("SINSYNC", [np.sin(TAU * r * x) * np.sin(np.pi * x) ** 0.5 for r in (1, 1.5, 2.25, 3, 4, 5.25, 6.5, 8)]))
    t.append(("RESO", [np.sin(TAU * r * x) * (1 - x) ** 2 + 0.25 * np.sin(TAU * x) for r in (2, 3, 4, 6, 8, 11, 15, 20)]))
    t.append(("PD", [np.cos(TAU * np.where(x < d, x * 0.5 / d, 0.5 + (x - d) * 0.5 / (1 - d))) for d in (0.5, 0.4, 0.31, 0.23, 0.16, 0.1, 0.055, 0.02)]))
    idx = (0, 0.5, 1, 1.6, 2.4, 3.4, 4.6, 6)
    t.append(("FM 1:3", [np.sin(TAU * x + i * np.sin(3 * TAU * x)) for i in idx]))
    t.append(("FM 2:1", [np.sin(2 * TAU * x + i * np.sin(TAU * x)) for i in idx]))
    fr = []
    for b in (0, 0.35, 0.7, 1.0, 1.25, 1.5, 1.75, 2.0):      # feedback FM, settled by going round a few times
        y = np.sin(TAU * x)
        for _ in range(40):
            y = 0.5 * y + 0.5 * np.sin(TAU * x + b * y)
        fr.append(y)
    t.append(("FM FB", fr))
    t.append(("FM 3OP", [np.sin(TAU * x + i * np.sin(2 * TAU * x + 0.8 * i * np.sin(5 * TAU * x))) for i in (0, 0.4, 0.8, 1.2, 1.7, 2.2, 2.8, 3.5)]))
    # voices
    t.append(("CHOIR", [from_spec(formant(vow(a, b, k), 320, 2.0) * (0.6 + 0.4 * odd)) for a, b, k in
                        (("A", "A", 0), ("A", "O", .5), ("O", "O", 0), ("O", "U", .5), ("U", "U", 0), ("U", "I", .5),
                         ("I", "I", 0), ("I", "E", .5))]))
    t.append(("NASAL", spec_frames(lambda c: h ** -0.8 * (0.08 + np.exp(-0.5 * ((h - c) / 1.6) ** 2) + 0.5 * np.exp(-0.5 * ((h - 2.6 * c) / 2.2) ** 2)), (3, 4, 5, 6.5, 8, 10, 12.5, 15))))
    t.append(("THROAT", spec_frames(lambda c: h ** -1.4 * (0.1 + np.exp(-0.5 * ((h - c) / 1.2) ** 2) + 0.7 * np.exp(-0.5 * ((h - 9 + c) / 2.0) ** 2)), (1.5, 2, 2.6, 3.3, 4, 5, 6, 7.2))))
    t.append(("GROWL", spec_frames(lambda k: h ** -0.9 * (0.05 + np.exp(-0.5 * ((h - 2 - 10 * k) / 2.0) ** 2) + np.exp(-0.5 * ((h - 26 + 16 * k) / 3.0) ** 2)) * (1 + 0.6 * np.cos(h * (1 + 3 * k))), lin)))
    t.append(("TALK", [from_spec(formant(f, 110, 0.9)) for f in
                       (VOW["A"], vow("A", "I", .3), vow("A", "I", .6), VOW["I"], vow("I", "U", .5), VOW["U"],
                        vow("U", "A", .4), vow("U", "A", .8))]))
    # instruments
    t.append(("FLUTE", spec_frames(lambda k: only((1, 2, 3, 4, 5, 6, 7), [1, 0.10 + 0.5 * k, 0.04 + 0.35 * k, 0.02 + 0.2 * k, 0.12 * k, 0.08 * k, 0.05 * k]), lin)))
    t.append(("PIPES", spec_frames(lambda k: only((1, 2, 3, 4, 5, 6, 8, 10, 12, 16, 20, 24), [1, 0.5, 0.25 + 0.3 * k, 0.3, 0.3 * k, 0.2 + 0.3 * k, 0.5 * k, 0.3 * k, 0.4 * k, 0.35 * k, 0.2 * k, 0.25 * k]), lin)))
    t.append(("CELLO", spec_frames(lambda c: h ** -1.15 * (0.35 + np.exp(-0.5 * ((h - c) / 1.8) ** 2) + 0.6 * np.exp(-0.5 * ((h - 3.2 * c) / 3.5) ** 2)) * (1 - 0.5 * (h % 7 == 0)), (2, 2.5, 3, 3.6, 4.3, 5.2, 6.2, 7.5))))
    t.append(("PIANO", spec_frames(lambda k: h ** -(2.4 - 1.5 * k) * (1 - 0.75 * (h % 8 == 0)) * np.exp(-h * (0.10 - 0.085 * k)), lin)))
    t.append(("CLAV", spec_frames(lambda k: (np.abs(np.sin(np.pi * h * (0.12 + 0.05 * k))) + 0.05) * h ** -(0.9 - 0.4 * k), lin)))
    t.append(("MALLET", spec_frames(lambda k: only((1, 4, 10, 17, 26), [1, 0.5 * k + 0.05, 0.4 * k * k, 0.25 * k * k, 0.15 * k ** 3]), lin)))
    t.append(("CHIME", spec_frames(lambda k: only((2, 5, 9, 14, 20, 27, 35), [1, 0.8 - 0.3 * k, 0.3 + 0.5 * k, 0.6 * k, 0.5 * k, 0.4 * k * k, 0.3 * k * k]), lin)))
    t.append(("GONG", [from_spec(only((1, 3, 4, 7, 11, 13, 18, 23, 29, 37, 47), [0.9, 0.5, 0.7, 0.6, 0.5 + 0.3 * k, 0.4, 0.5 * k + 0.1, 0.6 * k, 0.5 * k, 0.4 * k, 0.3 * k]), phase=rng.uniform(0, TAU, NH)) for k in lin]))
    # spectra carved and turned
    t.append(("HOLLOW", spec_frames(lambda c: h ** -1.0 * (1 - 0.97 * np.exp(-0.5 * ((h - c) / (1 + c * 0.3)) ** 2)), (2, 3, 4.5, 6.5, 9, 13, 18, 26))))
    t.append(("PHASES", [from_spec(h ** -1.0, phase=k * 0.42 * (h - 1) ** 1.3) for k in range(NF)]))
    t.append(("STEPS", [np.floor(x * n) / (n - 1) * 2 - 1 for n in (24, 16, 12, 9, 7, 5, 4, 3)]))
    t.append(("FIN", [np.where(x < 0.5, 1 - np.exp(-x * c), np.exp(-(x - 0.5) * c) - 1) for c in (3, 5, 8, 12, 18, 26, 40, 70)]))
    t.append(("IMPULSE", [np.exp(-0.5 * (((x + 0.5) % 1 - 0.5) / w) ** 2) - np.exp(-0.5 * ((x - 0.5) / w) ** 2) for w in (0.16, 0.11, 0.075, 0.05, 0.034, 0.023, 0.015, 0.009)]))
    t.append(("PARABOL", [np.sign(np.sin(TAU * x)) * np.abs(np.sin(TAU * x)) ** e for e in (2.5, 1.8, 1.3, 1.0, 0.7, 0.45, 0.25, 0.1)]))
    t.append(("PURE", [from_spec(only((1, n), [0.45, 1.0])) if n > 1 else np.sin(TAU * x) for n in (1, 2, 3, 4, 5, 7, 9, 12)]))
    t.append(("BUZZ", spec_frames(lambda c: (h >= c) * (h < c + 10 + c * 0.6) * 1.0 + 0.5 * (h == 1), (1, 2, 4, 7, 11, 17, 26, 38))))
    fr = []
    grit = rng.uniform(0, 1, NH)
    for k in range(NF):
        a = h ** -(1.2 - 0.14 * k) * (0.15 + grit ** (3.0 - 0.3 * k))
        a[0] = 1.0
        fr.append(from_spec(a, phase=rng.uniform(0, TAU, NH) * (k / (NF - 1))))
    t.append(("GRIT", fr))
    b4 = lambda y: np.round(y * 7.5) / 7.5                   # four bits
    t.append(("8 BIT", [b4(np.sin(TAU * x)), b4(tri()), b4(saw()), b4(pulse(0.5)), b4(pulse(0.25)), b4(pulse(0.125)),
                        b4(np.where(x < 0.5, saw(2 * x), pulse(0.5) * 0.6)), b4(saw(3 * x) * (1 - x))]))
    gl = rng.integers(-7, 8, (NF + 1, 16)) / 7.0
    t.append(("GLITCH", [mix(gl[k], gl[k + 1], 0.35)[np.minimum((x * (4 + 12 * k // 7)).astype(int), 15)] for k in range(NF)]))
    # strings by where they are plucked or bowed
    t.append(("PLUCK", spec_frames(lambda p: np.sin(np.pi * h * p) / h ** 2, (0.5, 0.38, 0.29, 0.22, 0.16, 0.12, 0.085, 0.05))))
    t.append(("BOWED", spec_frames(lambda p: np.abs(np.sin(np.pi * h * p)) / h * (1 + 0.8 * np.exp(-0.5 * ((h - 9) / 4.0) ** 2)), (0.5, 0.33, 0.25, 0.2, 0.143, 0.111, 0.077, 0.04))))
    assert len(t) == 40, len(t)
    return t


def harmonics(y):
    """a frame's first NH harmonics: (sine part, cosine part)"""
    X = np.fft.rfft(y - np.mean(y)) / (N / 2)
    return -X[1:NH + 1].imag, X[1:NH + 1].real


def render(s, c, length, nh):
    i = np.arange(length) / length
    y = np.zeros(length)
    for hh in range(1, nh + 1):
        y += s[hh - 1] * np.sin(TAU * hh * i) + c[hh - 1] * np.cos(TAU * hh * i)
    return y


def carr(name, typ, vals, per=12, fmt="{}"):
    out = [f"static const {typ} {name}[{len(vals)}] = {{"]
    for i in range(0, len(vals), per):
        out.append("    " + ", ".join(fmt.format(v) for v in vals[i:i + per]) + ",")
    out.append("};")
    return "\n".join(out)


def main():
    out = ["/* SPDX-License-Identifier: GPL-3.0-only */",
           "/* Fizzy-Fm's tables: generated by tools/gen_tables.py, do not edit. */",
           "#ifndef AP_GEN_H", "#define AP_GEN_H", "#include <stdint.h>", "",
           f"#define AP_SUB {SUB}u", f"#define AP_NH {NH}u", f"#define AP_NF {NF}u", ""]
    # a sine, Q15, 1024 + 1 points
    out.append(carr("AP_SINE", "int16_t", [int(round(32767 * np.sin(TAU * i / 1024))) for i in range(1025)], 16))
    # phase steps for the top octave, MIDI 120..132 in 768 steps (+1 to interpolate, +1 guard): f * 2^32 / FS
    out.append(carr("AP_PITCH", "uint32_t",
                    [int(round(8.1757989156 * 2 ** ((120 + i / 64) / 12) * 2 ** 32 / FS)) for i in range(770)], 8, "{}u"))
    # the filter's g = tan(pi f / FS), Q24, a value every half semitone from MIDI pitch 8 to 136 (+ guard)
    g = []
    for i in range(258):
        f = min(8.1757989156 * 2 ** ((8 + i / 2) / 12), FS * 0.46)
        g.append(int(round(np.tan(np.pi * f / FS) * 2 ** 24)))
    out.append(carr("AP_GTAB", "uint32_t", g, 8, "{}u"))
    # envelope times 0..127 -> a one-pole step per control tick, Q24: 1 ms .. 20 s to get most of the way
    env = []
    for v in range(128):
        t = 0.001 * 20000 ** (v / 127)
        env.append(max(1, int(round((1 - np.exp(-1 / (t / 4 * CR))) * 2 ** 24))))
    out.append(carr("AP_ENVC", "uint32_t", env, 8, "{}u"))
    # LFO rates 0..127 -> phase step per control tick: 0.02 .. 50 Hz
    out.append(carr("AP_LFOINC", "uint32_t",
                    [int(round(0.02 * 2500 ** (v / 127) / CR * 2 ** 32)) for v in range(128)], 8, "{}u"))
    tabs = tables()
    names, data = [], []
    report = []
    for name, frames in tabs:
        assert len(frames) == NF and len(name) <= 7, name
        sc = [harmonics(np.asarray(f, float)) for f in frames]
        rows = []
        for s, c in sc:
            lv = [render(s, c, ln, nh) for ln, nh in LEVELS]
            rms = np.sqrt(np.mean(lv[0] ** 2))
            k = 11000 / rms
            peak = max(np.max(np.abs(y)) for y in lv) * k
            if peak > 28000:
                k *= 28000 / peak
            rows.append((s * k, c * k))
            report.append((name, rms * k, max(np.max(np.abs(y)) for y in lv) * k))
        names.append(name)
        for s, c in rows:
            for hh in range(NH):
                data += [int(round(s[hh])), int(round(c[hh]))]
    # fm1-dock's build refuses anything that reads as an address in the chip's ROM (0xFFC00000..0xFFCFFFFF), and a
    # module's tables are disassembled with its code: no 16-bit value here is 0xFFC0..0xFFCF (-64..-49). The nearest
    # value outside is at most 8 away, of 28000
    data = [(-48 if v > -57 else -65) if -64 <= v <= -49 else v for v in data]
    assert max(abs(v) for v in data) < 32768
    out.append(f"#define AP_NTAB {len(names)}u")
    out.append("enum { " + ", ".join("T_" + "".join(c if c.isalnum() else "_" for c in n) for n in names) + " };")
    out.append("static const char *const AP_WT_NAME[AP_NTAB] = {" + ", ".join(f'"{n}"' for n in names) + "};")
    out.append("/* [table][frame][harmonic] = {sine part, cosine part}, in output units (a full wave is about +-28000) */")
    out.append(carr("AP_WT", "int16_t", data, 16))
    out += ["", "#endif", ""]
    p = Path(__file__).resolve().parents[1] / "fizzyfm" / "ap_gen.h"
    p.write_text("\n".join(out))
    print(f"{p}: {len(names)} tables, {len(data) * 2} B of spectra")
    for name in names:
        r = [x for x in report if x[0] == name]
        print(f"  {name:8s} rms {min(v[1] for v in r):6.0f}..{max(v[1] for v in r):6.0f}  peak {max(v[2] for v in r):6.0f}")


if __name__ == "__main__":
    main()
