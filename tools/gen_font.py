#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert (Fizzy-Fm)
"""The screen's lettering -> fizzyfm/ap_font.h: Inter Tight (assets/fonts, SIL OFL 1.1, LICENSES/OFL-InterTight.txt)
rasterised at four sizes into 16 levels of coverage, so the firmware can draw it smoothly over the flat colours
under it (ap_ui.h, t_draw).

Characters 32..95 (the space to the underscore: capitals, figures, signs): what the synth writes, and what a sound's
name may hold. Each glyph is trimmed to its ink, 4 bits a pixel (high nibble first), rows back to back; where it
sits is said from the pen (x) and from the top of the capitals (y), and how far the pen goes after it in quarters
of a pixel. The font's own kerning goes with it (the pairs it moves by a quarter of a pixel or more), and a word
space a tenth of an em wider than the font's: set on whole pixels, capitals need the air."""
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
FONT = ROOT / "assets" / "fonts" / "InterTight-VF.ttf"
# name, size (px to the em), weight, tracking (px), what it is for
FACES = [("S", 10, 640, 0.55, "the knobs' names, the tabs"),
         ("M", 12, 520, 0.25, "the sound's line, lists, a value too long for L"),
         ("L", 16, 540, 0.15, "the knobs' values, a page's title"),
         ("XL", 25, 620, 0.0, "the sound's name, the effect in a slot")]
FIRST, LAST = 32, 95
GAMMA = 0.82                                    # (light on dark reads thin: the coverage lifted a little)
WORD = 0.10                                     # (the space, wider by this much of the em)


def face(size, weight):
    f = ImageFont.truetype(str(FONT), size)
    f.set_variation_by_axes([weight])
    return f


def raster(f, ch, base):
    im = Image.new("L", (size_box := 4 * f.size, size_box), 0)
    ImageDraw.Draw(im).text((f.size, base), ch, font=f, fill=255, anchor="ls")
    a = np.asarray(im, float) / 255.0
    ys, xs = np.nonzero(a > 1 / 40)
    if not len(xs):
        return None
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    q = np.clip(np.round(15 * a[y0:y1, x0:x1] ** GAMMA), 0, 15).astype(int)
    return int(x0) - f.size, int(y0), q


def main():
    out = ["/* SPDX-License-Identifier: GPL-3.0-only AND OFL-1.1 */",
           "/* Fizzy-Fm's lettering: Inter Tight (The Inter Project Authors, SIL Open Font License 1.1,",
           " * LICENSES/OFL-InterTight.txt), rasterised by tools/gen_font.py: do not edit. */",
           "#ifndef AP_FONT_H", "#define AP_FONT_H", "#include <stdint.h>", "",
           "typedef struct { uint16_t off; uint8_t adv4; int8_t bx, by; uint8_t bw, bh; } fglyph_t;",
           "/* kk: the pairs kerned, (first - 32) << 6 | (second - 32), in order; kd: by how much, in quarters of a pixel */",
           "typedef struct { const fglyph_t *g; const uint8_t *d; const uint16_t *kk; const int8_t *kd; uint16_t nk; uint8_t cap; } fface_t;",
           ""]
    total = 0
    names = []
    for name, size, weight, track, what in FACES:
        f = face(size, weight)
        base = 3 * size
        cap_top = f.getbbox("H", anchor="ls")[1] + base      # (the top of the capitals, in the raster)
        cap = base - cap_top
        glyphs, data = [], []
        for c in range(FIRST, LAST + 1):
            ch = chr(c)
            adv = int(round(4 * (f.getlength(ch) + track + (WORD * size if ch == " " else 0))))
            r = raster(f, ch, base)
            if r is None:
                glyphs.append((len(data), adv, 0, 0, 0, 0))
                continue
            bx, y0, q = r
            flat = q.flatten().tolist()
            if len(flat) & 1:
                flat.append(0)
            off = len(data)
            data += [flat[i] << 4 | flat[i + 1] for i in range(0, len(flat), 2)]
            glyphs.append((off, adv, bx, y0 - cap_top, q.shape[1], q.shape[0]))
        # a byte 0xC0..0xCF and then 0xFF reads, wherever it falls, as part of an address in the chip's ROM
        # (0xFFC00000..0xFFCFFFFF), which fm1-dock's build refuses: that pixel's coverage goes 12 -> 13 of 15
        for i in range(len(data) - 1):
            if data[i] >> 4 == 0xC and data[i + 1] == 0xFF:
                data[i] = 0xD0 | (data[i] & 15)
        assert len(data) < 65536 and all(0 < g[1] < 256 for g in glyphs)
        kern = []
        for a in range(FIRST + 1, LAST + 1):
            for b in range(FIRST + 1, LAST + 1):
                k = f.getlength(chr(a) + chr(b)) - f.getlength(chr(a)) - f.getlength(chr(b))
                q = int(round(4 * k))
                if q:
                    kern.append(((a - FIRST) << 6 | (b - FIRST), q))
        out.append(f"/* {name}: {size} px, weight {weight}: {what} */")
        out.append(f"static const fglyph_t AF_{name}_G[{len(glyphs)}] = {{")
        for i in range(0, len(glyphs), 6):
            out.append("    " + " ".join("{%d, %d, %d, %d, %d, %d}," % g for g in glyphs[i:i + 6]))
        out.append("};")
        out.append(f"static const uint8_t AF_{name}_D[{len(data)}] = {{")
        for i in range(0, len(data), 24):
            out.append("    " + ", ".join(str(v) for v in data[i:i + 24]) + ",")
        out.append("};")
        out.append(f"static const uint16_t AF_{name}_KK[{len(kern)}] = {{")
        for i in range(0, len(kern), 16):
            out.append("    " + ", ".join(str(k) for k, _ in kern[i:i + 16]) + ",")
        out.append("};")
        out.append(f"static const int8_t AF_{name}_KD[{len(kern)}] = {{")
        for i in range(0, len(kern), 24):
            out.append("    " + ", ".join(str(q) for _, q in kern[i:i + 24]) + ",")
        out.append("};")
        out.append("")
        names.append((name, cap, len(kern)))
        cost = len(data) + 8 * len(glyphs) + 3 * len(kern)
        total += cost
        print(f"{name:2s} {size:2d} px: capitals {cap} px tall, {len(kern)} kerning pairs, {cost} B")
    out.append("enum { " + ", ".join(f"F_{n}" for n, _, _ in names) + ", NFACE };")
    out.append("static const fface_t FACES[NFACE] = {")
    out += [f"    {{AF_{n}_G, AF_{n}_D, AF_{n}_KK, AF_{n}_KD, {k}, {c}}}," for n, c, k in names]
    out.append("};")
    out += ["", "#endif", ""]
    (ROOT / "fizzyfm" / "ap_font.h").write_text("\n".join(out))
    print(f"fizzyfm/ap_font.h: {total} B in all")


if __name__ == "__main__":
    main()
