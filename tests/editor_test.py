#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The editor (docs/index.html) in a headless Chrome: it starts, builds its controls from the synth inside it,
plays a note (the samples, not the loudspeaker), follows a parameter set by hand, by a file and by NRPN as the FM-1
would send it, undoes, and turns a WAV into a table that plays. A picture of the page is left in build/editor/.

  python3 tests/editor_test.py      (after tools/build_editor.py; needs google-chrome or chromium)"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "editor"
TEST = r"""
<pre id="testout"></pre>
<script>
(async () => {
  const R = {}, wait = ms => new Promise(r => setTimeout(r, ms));
  try {
    for (let i = 0; i < 100 && !window.aplat; i++) await wait(50);
    const A = window.aplat, W = A.W;
    R.started = !!A;
    R.controls = document.querySelectorAll(".p").length;
    R.cards = document.querySelectorAll(".card").length;
    R.presets = document.getElementById("preset").length;
    const peakOf = n => { let p = 0; for (let b = 0; b < n; b++) { const o = new Int32Array(W.memory.buffer, W.w_render(b * 6), 512); for (const v of o) p = Math.max(p, Math.abs(v)); } return p; };
    for (let i = 0; i < 60; i++) W.w_draw(i);
    W.w_midi(0x90, 60, 100); R.notePeak = peakOf(40); W.w_midi(0x80, 60, 0); peakOf(400);
    A.setParam(A.ix.F_CUT, 40); R.cutSet = A.get(A.ix.F_CUT);
    R.cutText = [...document.querySelectorAll(".card")].find(c => c.querySelector("h2").textContent === "FILTER").querySelectorAll(".v")[0].textContent;
    A.setParam(A.ix.FX3_TYPE, 7); R.fxKnob = document.querySelectorAll(".row.fxr")[2].querySelectorAll(".l")[0].textContent;
    R.fxDefault = A.get(A.ix.FX3_TYPE + 1);
    const saved = A.toJSON();
    A.setParam(A.ix.F_CUT, 99);
    A.fromJSON(saved); R.fileRoundTrip = A.get(A.ix.F_CUT) === 40 && A.get(A.ix.FX3_TYPE) === 7;
    await wait(700);
    A.setParam(A.ix.A_POS, 100); await wait(700); A.setParam(A.ix.A_POS, 20);
    document.getElementById("bundo").click(); R.undo = A.get(A.ix.A_POS);
    document.getElementById("bredo").click(); R.redo = A.get(A.ix.A_POS);
    // the FM-1 says: parameter F_RES = 77 (an NRPN: controllers 99, 98, 6, 38)
    const cc = (a, b) => A.midiIn({ data: [0xB0, a, b] });
    cc(99, A.ix.F_RES >> 7); cc(98, A.ix.F_RES & 127); cc(6, 0); cc(38, 77); await wait(120);
    R.nrpnIn = A.get(A.ix.F_RES);
    // a WAV made here: 4 frames of 2048, a sine that becomes a square
    const N = 2048, x = new Float32Array(4 * N);
    for (let f = 0; f < 4; f++) for (let i = 0; i < N; i++) { const s = Math.sin(2 * Math.PI * i / N); x[f * N + i] = s * (1 - f / 3) + Math.sign(s) * (f / 3); }
    const spec = A.tableFrom(x);
    R.tableFund = [spec[0], spec[(7 * 64) * 2], spec[(7 * 64 + 2) * 2]];
    A.setUserTable(0, spec); A.setParam(A.ix.A_WAVE, W.w_ntab());
    for (let i = 0; i < 200; i++) W.w_draw(1000 + i);
    W.w_midi(0x90, 57, 100); R.userTablePeak = peakOf(40); W.w_midi(0x80, 57, 0);
    document.querySelector("#panel button").click(); await wait(600);
    const cv = document.querySelector("canvas.wave"), px = cv.getContext("2d").getImageData(0, 0, 480, 128).data;
    let lit = 0; for (let y = 0; y < 128; y += 4) for (let x = 0; x < 480; x += 8) if (px[(y * 480 + x) * 4] > 200) lit++;
    R.wavePixels = lit;
    R.status = document.getElementById("status").textContent;
    R.ok = true;
  } catch (e) { R.error = String(e && e.stack || e); }
  document.getElementById("testout").textContent = "RESULT" + JSON.stringify(R) + "END";
})();
</script>
"""


def main():
    chrome = shutil.which("google-chrome") or shutil.which("chromium") or shutil.which("chromium-browser")
    if not chrome:
        raise SystemExit("no Chrome here")
    page = (ROOT / "docs" / "index.html").read_text()
    OUT.mkdir(parents=True, exist_ok=True)
    t = OUT / "test.html"
    t.write_text(page.replace("</body>", TEST + "</body>"))
    base = [chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--hide-scrollbars", "--window-size=1500,1500",
            "--autoplay-policy=no-user-gesture-required"]
    dom = subprocess.run([*base, "--virtual-time-budget=20000", "--dump-dom", t.as_uri()], capture_output=True, text=True,
                         timeout=180).stdout
    m = re.search(r"RESULT(\{.*?\})END", dom, re.S)
    if not m:
        print(dom[-800:])
        raise SystemExit("the page did not finish its test")
    r = json.loads(m.group(1))
    for k, v in r.items():
        print(f"  {k}: {v}")
    subprocess.run([*base, "--virtual-time-budget=6000", f"--screenshot={OUT / 'editor.png'}", (ROOT / "docs" / "index.html").as_uri()],
                   capture_output=True, timeout=180)
    want = {"started": True, "cutSet": 40, "cutText": "40", "fxKnob": "RATE", "fxDefault": 30, "fileRoundTrip": True,
            "undo": 100, "redo": 20, "nrpnIn": 77, "ok": True}
    bad = [k for k, v in want.items() if r.get(k) != v]
    if r.get("notePeak", 0) < 2000 or r.get("userTablePeak", 0) < 2000 or r.get("controls", 0) < 120 or r.get("wavePixels", 0) < 50:
        bad.append("sound or controls")
    if bad:
        raise SystemExit(f"FAILED: {bad}")
    print("the editor passed")


if __name__ == "__main__":
    main()
