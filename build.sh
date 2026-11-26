#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build APLAT with fm1-dock's own build, at the commit APLAT was written against:
#   build/aplat.dock          the module, to add to an fm1-dock already on the FM-1 (its page's console, or
#                             fm1-dock's tools/dock.py deploy)
#   build/aplat[-X.Y].fwsc    a firmware of its own: fm1-dock with APLAT as its only synth, Bluetooth MIDI included
#
#   ./build.sh [--release X.Y]
#
# Needs what fm1-dock's build needs (its README, "Build it"): Linux x86-64 (or Docker), Python 3 with Pillow,
# JieLi's toolchain (fm1-dock's tools/get_toolchain.sh) and JieLi's AC79 SDK. On GitHub the release workflow
# (.github/workflows/release.yml) does all of it: nothing to install.
#   DOCK              a checkout of fm1-dock to use (default: .dock/fm1-dock, cloned here at DOCK_COMMIT)
#   APLAT_IDENTITY    the package identity (see README, "Before you release")
set -e
cd "$(dirname "$0")"
HERE="$(pwd)"
DOCK_REPO="https://github.com/joakimeriksson/fm1-dock"
DOCK_COMMIT="3daf189e9f386eb247b29f5f99d2e64e57b6f054"       # fm1-dock 1.0, 2026-11-26
IDENTITY="${APLAT_IDENTITY:-FM-1_961}"

if [ -z "$DOCK" ]; then
    DOCK="$HERE/.dock/fm1-dock"
    if [ ! -d "$DOCK/.git" ]; then
        mkdir -p "$HERE/.dock"
        git clone "$DOCK_REPO" "$DOCK"
    fi
    git -C "$DOCK" fetch -q origin
    git -C "$DOCK" checkout -q "$DOCK_COMMIT"
fi

# A firmware of one synth ("solo") is built by fm1-dock without its Bluetooth MIDI part (HX), to leave its 40 KiB
# to the synth. APLAT fits beside it, and its editor's link wants Bluetooth: the one line that drops HX for a solo
# build is changed here, in the checkout made above only. If fm1-dock's build no longer has that line, stop: look.
if [ -z "$APLAT_NO_BLE" ]; then
    if grep -q 'if a.no_ble or a.pack or a.name:' "$DOCK/tools/build.py"; then
        sed -i.orig 's/if a.no_ble or a.pack or a.name:/if a.no_ble or a.pack:/' "$DOCK/tools/build.py"
    elif ! grep -q 'if a.no_ble or a.pack:' "$DOCK/tools/build.py"; then
        echo "build.sh: fm1-dock's tools/build.py has changed: cannot keep Bluetooth MIDI (APLAT_NO_BLE=1 builds without)"
        exit 1
    fi
fi

mkdir -p build
(cd "$DOCK" && HB_MOUNT="$HERE" ./build.sh --modules "$HERE/aplat" --name aplat --app-name APLAT --identity "$IDENTITY" "$@")
cp "$DOCK"/build/aplat*.fwsc build/
cp "$DOCK"/build/modules/aplat.dock build/
ls -l build/aplat*.fwsc build/aplat.dock
