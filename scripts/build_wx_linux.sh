#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/build_wx_linux.sh
#
# Builds wxWidgets 3.2 (GTK 3) as a static library from the official source archive and installs
# it into a directory of its own. The published Debian package and AppImage are built with it on
# Ubuntu 22.04, which only has wxWidgets 3.0 in its repositories, so that they run on that
# release (Linux Mint 21 and newer) and need no wxWidgets package. The version and the checksum are
# the ones of the Flatpak manifest; change them together.
#
# Build prerequisites (Debian/Ubuntu names): build-essential, pkg-config, libgtk-3-dev, libsm-dev,
# libxtst-dev, curl, ca-certificates, bzip2.
#
# Usage:
#   ./scripts/build_wx_linux.sh [prefix]
#
# The prefix defaults to build-wx/prefix in the project. The programs are then built against it with
#   WX_CONFIG=<prefix>/bin/wx-config ./scripts/build_deb.sh        (or build_appimage.sh)

set -euo pipefail

WX_VERSION="3.2.12"
WX_URL="https://github.com/wxWidgets/wxWidgets/releases/download/v${WX_VERSION}/wxWidgets-${WX_VERSION}.tar.bz2"
WX_SHA256="a62719bb5e1dcc41c1a6fc1eecd499c7ee8521f70402c7b1bb3342a1e16344e0"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

PREFIX="${1:-$PROJECT_ROOT/build-wx/prefix}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "=== fetch and verify wxWidgets ${WX_VERSION} ==="
curl --fail --silent --show-error --location --retry 3 --output "$WORK_DIR/wx.tar.bz2" "$WX_URL"
echo "${WX_SHA256}  $WORK_DIR/wx.tar.bz2" | sha256sum --check --status || {
    echo "error: SHA-256 of the wxWidgets archive does not match the pinned value" >&2
    exit 1
}
tar -C "$WORK_DIR" -xjf "$WORK_DIR/wx.tar.bz2"

echo ""
echo "=== configure, build and install into $PREFIX ==="
cd "$WORK_DIR/wxWidgets-${WX_VERSION}"
# Static, with the bundled copies of the image and compression libraries (nothing but GTK and the
# system libraries is needed at run time), and without the libraries renderHAMgui does not use.
./configure --prefix="$PREFIX" --with-gtk=3 --enable-unicode \
    --disable-shared --disable-sys-libs \
    --disable-tests --disable-stc --disable-richtext --disable-ribbon --disable-propgrid \
    --disable-aui --disable-xrc --disable-html --disable-htmlhelp --disable-webview \
    --disable-mediactrl --disable-secretstore \
    --without-opengl --without-libnotify --without-sdl --without-libmspack --without-gnomevfs
make -j"$(nproc)"
make install

# The programs contain the library, so the licence texts of wxWidgets and of the libraries bundled
# with it go with them: build_deb.sh and build_appimage.sh copy this folder into their packages.
DOC_DIR="$PREFIX/share/doc/wxWidgets"
mkdir -p "$DOC_DIR"
install -m644 docs/licence.txt docs/lgpl.txt docs/gpl.txt docs/preamble.txt "$DOC_DIR/"
install -m644 src/png/LICENSE      "$DOC_DIR/libpng-LICENSE"
install -m644 src/zlib/LICENSE     "$DOC_DIR/zlib-LICENSE"
install -m644 src/jpeg/README      "$DOC_DIR/libjpeg-README"
install -m644 src/tiff/COPYRIGHT   "$DOC_DIR/libtiff-COPYRIGHT"
install -m644 src/expat/expat/COPYING "$DOC_DIR/expat-COPYING"
install -m644 3rdparty/pcre/LICENCE   "$DOC_DIR/pcre2-LICENCE"

echo ""
echo "Done: $PREFIX/bin/wx-config"
