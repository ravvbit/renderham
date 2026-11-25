#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/package_windows.sh
#
# Packs the Windows build into a ZIP file that runs on its own: renderHAMgui.exe, renderHAM.exe,
# the German translation, the license texts and every DLL of the MSYS2 environment the two
# programs need (wxWidgets, the C++ runtime, image libraries, ...). Windows' own DLLs are not
# copied; they are part of Windows.
#
# Run it in an MSYS2 UCRT64 shell after a build (see README.md, "Windows and macOS"); it needs
# `zip` (pacman -S zip) and objdump (comes with the compiler).
#
# Usage:
#   ./scripts/package_windows.sh [build-dir] [output-dir]       (defaults: build, dist)
#
# The result is <output-dir>/renderHAM-<version>-windows-x64.zip, which unpacks into a folder of
# the same name.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"

VERSION="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
[ -n "$VERSION" ] || { echo "error: cannot read the version from CMakeLists.txt" >&2; exit 1; }
NAME="renderHAM-${VERSION}-windows-x64"
STAGE="$OUT_DIR/$NAME"

CLI="$BUILD_DIR/tools/renderham/renderHAM.exe"
GUI="$BUILD_DIR/tools/renderhamgui/renderHAMgui.exe"
for f in "$CLI" "$GUI"; do
    [ -f "$f" ] || { echo "error: $f not found; build the project first" >&2; exit 1; }
done
[ -d "$BUILD_DIR/tools/renderhamgui/translations" ] || echo "warning: no translations found, the GUI will be English only" >&2
[ -n "${MSYSTEM_PREFIX:-}" ] || { echo "error: run this in an MSYS2 shell (MSYSTEM_PREFIX is not set)" >&2; exit 1; }

rm -rf "$STAGE" "$OUT_DIR/$NAME.zip"
mkdir -p "$STAGE"

cp "$CLI" "$GUI" "$STAGE/"
strip --strip-unneeded "$STAGE/renderHAM.exe" "$STAGE/renderHAMgui.exe"
if [ -d "$BUILD_DIR/tools/renderhamgui/translations" ]; then
    cp -r "$BUILD_DIR/tools/renderhamgui/translations" "$STAGE/translations"
fi
cp LICENSE THIRD_PARTY_NOTICES.md "$STAGE/"
cp packaging/windows/README.txt "$STAGE/README.txt"

# Copy the DLLs the programs import, and the DLLs those import in turn, as far as they live in the
# MSYS2 environment (anything else is a Windows system DLL).
copied=" "
queue=("$STAGE/renderHAM.exe" "$STAGE/renderHAMgui.exe")
while [ "${#queue[@]}" -gt 0 ]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    for dll in $(objdump -p "$file" | sed -n 's/^[[:space:]]*DLL Name: //p'); do
        case "$copied" in *" ${dll,,} "*) continue ;; esac
        if [ -f "$MSYSTEM_PREFIX/bin/$dll" ]; then
            cp "$MSYSTEM_PREFIX/bin/$dll" "$STAGE/$dll"
            copied="$copied${dll,,} "
            queue+=("$STAGE/$dll")
        fi
    done
done
echo "DLLs copied from $MSYSTEM_PREFIX/bin:$copied"

(cd "$OUT_DIR" && zip -q -r -9 "$NAME.zip" "$NAME")
ls -l "$OUT_DIR/$NAME.zip"
echo "Wrote $OUT_DIR/$NAME.zip"
