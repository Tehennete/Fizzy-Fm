#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build APLAT for this computer and run a script: host/run.sh SCRIPT [OUT]   (DOCK: a checkout of fm1-dock)
set -e
cd "$(dirname "$0")/.."
DOCK="${DOCK:-../fm1-dock}"
mkdir -p build
cc -O2 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers \
   -I"$DOCK/firmware/dock" -I"$DOCK/firmware/src" -I"$DOCK/firmware/gen" -o build/aplat_host host/host.c
out="${2:-build/$(basename "$1" .txt)}"
build/aplat_host "$out" "$1"
python3 - "$out" <<'PY'
import sys
from pathlib import Path
from PIL import Image
out = Path(sys.argv[1])
for p in sorted(out.parent.glob(out.name + "-*.ppm")):
    Image.open(p).resize((480, 480), Image.NEAREST).save(p.with_suffix(".png"))
    p.unlink()
PY
