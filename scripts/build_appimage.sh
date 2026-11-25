#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/build_appimage.sh
#
# Builds renderHAMgui as a single-file AppImage for x86_64 Linux, on top of the normal
# `cmake --install` rules (which also install the desktop entry, icon and AppStream metadata
# from packaging/linux/). The GUI is the AppImage's application; the command-line tool is not
# reachable from inside an AppImage, use the .deb or a source build for it.
#
# What it does: installs into an AppDir, lets linuxdeploy (with its GTK plugin) copy the
# libraries renderHAMgui needs (wxWidgets and the GTK stack) into it, and packs the AppDir
# with appimagetool. The tools (and the AppImage runtime) are downloaded once into
# build-appimage/tools and checked against the SHA-256 sums pinned below; update version and sum
# together.
#
# Build prerequisites (Debian/Ubuntu names): a C++17 compiler, cmake, gettext, libwxgtk3.2-dev,
# libgtk-3-dev, librsvg2-dev, pkg-config, curl, file, desktop-file-utils.
#
# The AppImage can only run where the C library is at least as new as the one it was built
# against (glibc of the build machine); build on the oldest system you want to support.
#
# Usage:
#   ./scripts/build_appimage.sh
#
# The result is dist/renderHAM-<version>-x86_64.AppImage

set -euo pipefail

# --- Pinned tools --------------------------------------------------------------------------
LINUXDEPLOY_VERSION="1-alpha-20251107-1"
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_VERSION}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"

# The GTK plugin is a plain shell script without releases; pinned to a commit.
GTK_PLUGIN_COMMIT="7a3fbc31a9e5075073ff8790f26effbac5f84453"
GTK_PLUGIN_URL="https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/${GTK_PLUGIN_COMMIT}/linuxdeploy-plugin-gtk.sh"
GTK_PLUGIN_SHA256="b0f4cbc684a0103a9651f0955b635eaea0096b3a66c0f5a2c2aa337960375171"

APPIMAGETOOL_VERSION="1.9.1"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/appimagetool-x86_64.AppImage"
APPIMAGETOOL_SHA256="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"

# appimagetool would otherwise download the moving "continuous" runtime on every run.
RUNTIME_VERSION="20251108"
RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/${RUNTIME_VERSION}/runtime-x86_64"
RUNTIME_SHA256="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"

APP_ID="io.github.ravvbit.renderham"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

if [ "$(uname -m)" != "x86_64" ]; then
    echo "error: this script builds x86_64 AppImages only (running on $(uname -m))" >&2
    exit 1
fi

VERSION="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
if [ -z "$VERSION" ]; then
    echo "error: could not read the version from CMakeLists.txt" >&2
    exit 1
fi

BUILD_DIR="$PROJECT_ROOT/build-appimage"
TOOLS_DIR="$BUILD_DIR/tools"
APPDIR="$BUILD_DIR/AppDir"
OUTPUT_DIR="$PROJECT_ROOT/dist"
OUTPUT="$OUTPUT_DIR/renderHAM-${VERSION}-x86_64.AppImage"

# Download <url> to <file> unless it is already there with the expected SHA-256.
fetch() {
    local url="$1" file="$2" sha="$3"
    if [ -f "$file" ] && echo "$sha  $file" | sha256sum --check --status; then
        return 0
    fi
    echo "  - downloading $(basename "$file")"
    curl --fail --silent --show-error --location --retry 3 --output "$file" "$url"
    if ! echo "$sha  $file" | sha256sum --check --status; then
        echo "error: SHA-256 of $(basename "$file") does not match the pinned value" >&2
        rm -f "$file"
        exit 1
    fi
    chmod +x "$file"
}

echo "=== 1/5: fetch and verify the AppImage tools ==="
mkdir -p "$TOOLS_DIR"
fetch "$LINUXDEPLOY_URL"  "$TOOLS_DIR/linuxdeploy-x86_64.AppImage"  "$LINUXDEPLOY_SHA256"
fetch "$GTK_PLUGIN_URL"   "$TOOLS_DIR/linuxdeploy-plugin-gtk.sh"    "$GTK_PLUGIN_SHA256"
fetch "$APPIMAGETOOL_URL" "$TOOLS_DIR/appimagetool-x86_64.AppImage" "$APPIMAGETOOL_SHA256"
fetch "$RUNTIME_URL"      "$TOOLS_DIR/runtime-x86_64"               "$RUNTIME_SHA256"

echo ""
echo "=== 2/5: configure + build (Release) ==="
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR/cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR/cmake" -j"$(nproc)"

echo ""
echo "=== 3/5: install into the AppDir ==="
rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR/cmake"
# Development files of the library are not part of the application.
rm -rf "$APPDIR/usr/include" "$APPDIR/usr/lib"
# The command-line tool cannot be started from an AppImage; do not ship it dead weight.
rm -f "$APPDIR/usr/bin/renderHAM"

echo ""
echo "=== 4/5: collect libraries with linuxdeploy ==="
# The tools are AppImages themselves; run them without FUSE so this also works in containers
# and CI runners.
export APPIMAGE_EXTRACT_AND_RUN=1
export ARCH=x86_64
export DEPLOY_GTK_VERSION=3
# linuxdeploy finds its plugin by the name linuxdeploy-plugin-gtk.sh in PATH.
PATH="$TOOLS_DIR:$PATH" "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    --plugin gtk \
    --executable "$APPDIR/usr/bin/renderHAMgui" \
    --desktop-file "$APPDIR/usr/share/applications/${APP_ID}.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/${APP_ID}.svg"

echo ""
echo "=== 5/5: pack the AppImage ==="
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT"
"$TOOLS_DIR/appimagetool-x86_64.AppImage" --runtime-file "$TOOLS_DIR/runtime-x86_64" "$APPDIR" "$OUTPUT"
chmod +x "$OUTPUT"

echo ""
echo "Done: $OUTPUT"
ls -lh "$OUTPUT"
