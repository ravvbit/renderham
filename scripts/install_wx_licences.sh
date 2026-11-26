#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/install_wx_licences.sh
#
# Copies the licence texts of wxWidgets, and of the libraries that wxWidgets bundles (libpng,
# zlib, libjpeg, libtiff, expat, PCRE2), out of an unpacked wxWidgets source tree. A program that
# contains a wxWidgets built from that source has to carry them: the Debian package, the AppImage
# and the macOS application do (the Windows ZIP gets the texts of its DLLs from MSYS2, see
# package_windows.sh).
#
# Usage:
#   ./scripts/install_wx_licences.sh <wxWidgets-source-dir> <destination-dir>
#
# build_wx_linux.sh calls it for the wxWidgets it builds; the macOS job of the workflow calls it for the
# one it builds and caches. `licence.txt` is the wxWindows Library Licence 3.1.

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <wxWidgets-source-dir> <destination-dir>" >&2
    exit 1
fi
SRC="$1"
DEST="$2"

# source file -> name in the destination
FILES=(
    "docs/licence.txt licence.txt"
    "docs/lgpl.txt lgpl.txt"
    "docs/gpl.txt gpl.txt"
    "docs/preamble.txt preamble.txt"
    "src/png/LICENSE libpng-LICENSE"
    "src/zlib/LICENSE zlib-LICENSE"
    "src/jpeg/README libjpeg-README"
    "src/tiff/COPYRIGHT libtiff-COPYRIGHT"
    "src/expat/expat/COPYING expat-COPYING"
    "3rdparty/pcre/LICENCE pcre2-LICENCE"
)

mkdir -p "$DEST"
for entry in "${FILES[@]}"; do
    read -r from to <<< "$entry"
    if [ ! -f "$SRC/$from" ]; then
        echo "error: $SRC/$from not found; is $SRC an unpacked wxWidgets 3.2 source tree?" >&2
        exit 1
    fi
    install -m644 "$SRC/$from" "$DEST/$to"
done
echo "wxWidgets licence texts: $DEST"
