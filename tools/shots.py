#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Hugo Gilbert (Fizzy-Fm)
"""The README's pictures of the screen (docs/*.png), from the pictures the tests' "pages" leaves in build/tests:

    DOCK=../fm1-dock python3 tests/run_tests.py -k pages && python3 tools/shots.py

(docs/editor.png is tests/editor_test.py's build/editor/editor.png.)"""
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SRC, DOCS = ROOT / "build" / "tests", ROOT / "docs"
ONE = {"play": 0, "osc": 2, "filter": 8, "env": 12, "lfo": 15, "matrix": 18, "fx": 21}      # a page each
COLS, GAP = 7, 4

pages = sorted(SRC.glob("pages-page*.png"))
if not pages:
    sys.exit("tools/shots.py: no build/tests/pages-page*.png: run tests/run_tests.py -k pages first")
for name, i in ONE.items():
    Image.open(pages[i]).convert("RGB").resize((480, 480), Image.NEAREST).save(DOCS / f"{name}.png", optimize=True)
rows = -(-len(pages) // COLS)
sheet = Image.new("RGB", (COLS * (240 + GAP), rows * (240 + GAP)), (90, 90, 90))
for i, p in enumerate(pages):
    sheet.paste(Image.open(p).convert("RGB").resize((240, 240), Image.NEAREST),
                (GAP // 2 + (i % COLS) * (240 + GAP), GAP // 2 + (i // COLS) * (240 + GAP)))
sheet.save(DOCS / "pages.png", optimize=True)
print(f"{len(ONE) + 1} pictures in {DOCS}")
