#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/package_macos.sh
#
# Packs the macOS build into a disk image (.dmg): renderHAM.app (the GUI), the command-line tool
# in a folder next to it, a link to /Applications for drag-and-drop installation, the license
# texts and a short README. The application and the tool are signed ad hoc (no developer
# certificate, so macOS asks for confirmation the first time; README.txt in the image says how).
#
# It also checks that nothing in the programs depends on libraries outside macOS itself: the
# build is expected to link wxWidgets statically (see the macOS job of .github/workflows/build.yml).
#
# Run it on macOS after a build (see README.md, "Windows and macOS").
#
# Usage:
#   ./scripts/package_macos.sh [build-dir] [output-dir]       (defaults: build, dist)
#
# The result is <output-dir>/renderHAM-<version>-macos-universal.dmg (the name says "universal"
# whatever the build contains; the workflow checks that it really is).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"

VERSION="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
[ -n "$VERSION" ] || { echo "error: cannot read the version from CMakeLists.txt" >&2; exit 1; }
DMG="$OUT_DIR/renderHAM-${VERSION}-macos-universal.dmg"
STAGE="$OUT_DIR/dmg-root"

APP="$BUILD_DIR/tools/renderhamgui/renderHAM.app"
CLI="$BUILD_DIR/tools/renderham/renderHAM"
[ -d "$APP" ] || { echo "error: $APP not found; build the project first" >&2; exit 1; }
[ -f "$CLI" ] || { echo "error: $CLI not found; build the project first" >&2; exit 1; }

rm -rf "$STAGE" "$DMG"
mkdir -p "$STAGE/Command-line tool"

ditto "$APP" "$STAGE/renderHAM.app"
cp "$CLI" "$STAGE/Command-line tool/renderHAM"
cp LICENSE THIRD_PARTY_NOTICES.md "$STAGE/"
cp packaging/macos/README.txt "$STAGE/README.txt"
ln -s /Applications "$STAGE/Applications"

# Nothing but macOS' own libraries and frameworks may be linked.
status=0
for f in "$STAGE/renderHAM.app/Contents/MacOS/renderHAM" "$STAGE/Command-line tool/renderHAM"; do
    echo "== $f: $(lipo -archs "$f")"
    others="$(otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -v '^/usr/lib/' | grep -v '^/System/' || true)"
    if [ -n "$others" ]; then
        echo "error: $f links libraries that are not part of macOS:" >&2
        echo "$others" >&2
        status=1
    fi
done
[ "$status" -eq 0 ] || exit 1

# Ad-hoc signature ("-"): Apple Silicon refuses programs without any signature. The signature
# comes last, after everything inside the bundle is final.
codesign --force --deep --sign - "$STAGE/renderHAM.app"
codesign --force --sign - "$STAGE/Command-line tool/renderHAM"
codesign --verify --deep --strict "$STAGE/renderHAM.app"

hdiutil create -volname "renderHAM $VERSION" -srcfolder "$STAGE" -ov -format UDZO "$DMG"
rm -rf "$STAGE"
ls -l "$DMG"
echo "Wrote $DMG"
