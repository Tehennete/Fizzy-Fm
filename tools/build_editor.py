#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""APLAT's editor: the module compiled to WebAssembly (editor/wasm.c: aplat/aplat.c on fm1-dock's screen code and
font) and put inside editor/editor.html -> docs/index.html, one file that needs no server (open it in Chrome or
Edge, or serve docs/ as the repository's GitHub Pages).

  DOCK=/path/to/fm1-dock python3 tools/build_editor.py      (needs clang with the wasm32 target and wasm-ld)"""
import base64
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DOCK = Path(os.environ.get("DOCK", ROOT.parent / "fm1-dock")).resolve()
OUT = ROOT / "build" / "editor"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    wasm = OUT / "aplat.wasm"
    subprocess.run(["clang", "--target=wasm32", "-O2", "-nostdlib", "-ffreestanding", "-fno-builtin", "-Wall",
                    "-Wno-unused-function", "-Wno-missing-field-initializers", f"-I{DOCK}/firmware/dock",
                    f"-I{DOCK}/firmware/src", f"-I{DOCK}/firmware/gen", f"-I{DOCK}/modules/lib", "-Wl,--no-entry",
                    "-Wl,-z,stack-size=131072", "-Wl,--strip-all", "-o", str(wasm), str(ROOT / "editor" / "wasm.c")],
                   check=True)
    html = (ROOT / "editor" / "editor.html").read_text()
    assert html.count("/*WASM_BASE64*/") == 1
    out = ROOT / "docs" / "index.html"
    out.write_text(html.replace("/*WASM_BASE64*/", base64.b64encode(wasm.read_bytes()).decode()))
    print(f"{out}: {out.stat().st_size} B (the synth: {wasm.stat().st_size} B of WebAssembly)")


if __name__ == "__main__":
    main()
