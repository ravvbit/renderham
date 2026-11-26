#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/build_deb.sh
#
# Builds an installable Debian package for renderHAM (renderhamlib + the
# renderHAM command-line tool + renderHAMgui), on top of the existing
# `cmake --install` rules in the CMakeLists.txt files. The script does not change
# the installation layout; it only packages it cleanly instead of copying it raw into
# the system with "sudo cmake --install".
#
# NOTE: renderHAMgui looks for its translation at run time relative to its own
# program directory (<exe path>/translations/<lang>/LC_MESSAGES/renderhamgui.mo,
# see tools/renderhamgui/Localization.cpp), not in the FHS-usual /usr/share/locale.
# So do not "improve" the locale placement here - it would silently (but
# harmlessly) break the German translation after installation.
#
# Usage:
#   ./scripts/build_deb.sh [version]
#
# Without an argument the version is read from the project() call in the top-level
# CMakeLists.txt, the single place the version lives. Environment variables:
#   DEB_MAINTAINER   "Name <address>" for the control file (default: a placeholder)
#   DEB_HOMEPAGE     project URL for the control file (default: the GitHub repository)
#
# The result is dist/renderham_<version>_<arch>.deb
#
# The package can only run where the C library is at least as new as the one it was built against,
# and where the libraries it depends on exist: build on the oldest system you want to support.
# The published package is built on Ubuntu 22.04 (Linux Mint 21 and newer) with a static
# wxWidgets 3.2, which that release does not have; WX_CONFIG points the build at it:
#   WX_CONFIG   path of the wx-config of the wxWidgets to build against (default: the system's)

set -euo pipefail

# --- Configuration -----------------------------------------------------
PKG_NAME="renderham"
MAINTAINER="${DEB_MAINTAINER:-renderHAM maintainers <noreply@example.invalid>}"
SECTION="graphics"
HOMEPAGE="${DEB_HOMEPAGE:-https://github.com/ravvbit/renderham}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

if [ -n "${1:-}" ]; then
    PKG_VERSION="$1"
else
    PKG_VERSION="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"
    if [ -z "$PKG_VERSION" ]; then
        echo "error: could not read the version from CMakeLists.txt; pass it as an argument" >&2
        exit 1
    fi
fi
if [ -z "${DEB_MAINTAINER:-}" ]; then
    echo "note: DEB_MAINTAINER is not set, using the placeholder '$MAINTAINER'" >&2
fi
PKG_ARCH="$(dpkg --print-architecture)"

BUILD_DIR="$PROJECT_ROOT/build"
STAGE_DIR="$(mktemp -d)"
OUTPUT_DIR="$PROJECT_ROOT/dist"
DEB_FILE="$OUTPUT_DIR/${PKG_NAME}_${PKG_VERSION}_${PKG_ARCH}.deb"

cleanup() { rm -rf "$STAGE_DIR"; }
trap cleanup EXIT

echo "=== 1/6: configure + build (Release) ==="
CMAKE_EXTRA=()
if [ -n "${WX_CONFIG:-}" ]; then
    CMAKE_EXTRA+=("-DwxWidgets_CONFIG_EXECUTABLE=$WX_CONFIG")
fi
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release ${CMAKE_EXTRA[@]+"${CMAKE_EXTRA[@]}"}
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo ""
echo "=== 2/6: install into $STAGE_DIR (prefix /usr) ==="
DESTDIR="$STAGE_DIR" cmake --install "$BUILD_DIR" --prefix /usr

mkdir -p "$STAGE_DIR/DEBIAN"

# A wxWidgets built by scripts/build_wx_linux.sh is part of renderHAMgui; its licence texts go with it.
WX_LICENCE_DIR=""
if [ -n "${WX_CONFIG:-}" ] && [ -d "$(dirname "$WX_CONFIG")/../share/doc/wxWidgets" ]; then
    WX_LICENCE_DIR="$(cd "$(dirname "$WX_CONFIG")/../share/doc/wxWidgets" && pwd)"
    mkdir -p "$STAGE_DIR/usr/share/doc/$PKG_NAME/wxWidgets"
    cp -p "$WX_LICENCE_DIR"/* "$STAGE_DIR/usr/share/doc/$PKG_NAME/wxWidgets/"
fi

echo ""
echo "=== 3/6: determine run-time dependencies automatically ==="
# dpkg-shlibdeps insists on running inside a Debian source tree with a
# debian/control (even though we do not build a full source package here) - a
# minimal skeleton is enough, removed again before the actual .deb is built.
mkdir -p "$STAGE_DIR/debian"
cat > "$STAGE_DIR/debian/control" <<EOF
Source: $PKG_NAME
Priority: optional
Maintainer: $MAINTAINER

Package: $PKG_NAME
Architecture: $PKG_ARCH
Description: placeholder
EOF
SUBSTVARS="$STAGE_DIR/debian/substvars"
: > "$SUBSTVARS"
BINARIES=()
for f in "$STAGE_DIR"/usr/bin/*; do
    [ -f "$f" ] && [ -x "$f" ] && BINARIES+=("$f")
done
if [ "${#BINARIES[@]}" -eq 0 ]; then
    echo "error: no executables found under $STAGE_DIR/usr/bin" >&2
    exit 1
fi
(
    cd "$STAGE_DIR"
    RELATIVE_BINARIES=()
    for f in "${BINARIES[@]}"; do
        RELATIVE_BINARIES+=("${f#"$STAGE_DIR/"}")
    done
    dpkg-shlibdeps --ignore-missing-info -Tdebian/substvars "${RELATIVE_BINARIES[@]}"
)
DEPENDS="$(sed -n 's/^shlibs:Depends=//p' "$SUBSTVARS")"
rm -rf "$STAGE_DIR/debian"
echo "Determined Depends: $DEPENDS"

echo ""
echo "=== 4/6: write DEBIAN/control, copyright, changelog and man pages ==="

echo "  - strip binaries (removes debug symbols, reduces size)"
strip --strip-unneeded "${BINARIES[@]}"

MAN_DIR="$STAGE_DIR/usr/share/man/man1"
mkdir -p "$MAN_DIR"
if command -v help2man >/dev/null 2>&1; then
    # renderHAM (CLI) prints its --help output without a display - ideal
    # for help2man, which derives a complete man page from it.
    help2man --no-info --name="Amiga HAM6 image converter (command line)" \
        --version-string="$PKG_VERSION" \
        "$STAGE_DIR/usr/bin/renderHAM" > "$MAN_DIR/renderHAM.1" 2>/dev/null || true
fi
if [ ! -s "$MAN_DIR/renderHAM.1" ]; then
    echo "  - help2man failed/missing, writing a minimal renderHAM.1 by hand"
    "$STAGE_DIR/usr/bin/renderHAM" --help > "$STAGE_DIR/.help_output" 2>&1 || true
    {
        echo ".TH RENDERHAM 1"
        echo ".SH NAME"
        echo "renderHAM \\- Amiga HAM6 image converter (command line)"
        echo ".SH SYNOPSIS"
        echo ".B renderHAM"
        echo "-i <input> -o <output.iff> [options]"
        echo ".SH DESCRIPTION"
        sed 's/^/.br\n/' "$STAGE_DIR/.help_output"
    } > "$MAN_DIR/renderHAM.1"
    rm -f "$STAGE_DIR/.help_output"
fi
# renderHAMgui (GUI) cannot answer --help without a running display
# (wxWidgets initializes GTK+ before it evaluates the command line) -
# help2man would depend on a running Xvfb. The actual command-line surface is
# small anyway, so by hand:
cat > "$MAN_DIR/renderHAMgui.1" <<EOF
.TH RENDERHAMGUI 1
.SH NAME
renderHAMgui \\- Amiga HAM6 image converter (wxWidgets GUI)
.SH SYNOPSIS
.B renderHAMgui
[--lang <code>] [image file]
.SH DESCRIPTION
Graphical front end for renderHAM. Started without arguments the application
opens empty; if an image file is given it is loaded right away.
.SH OPTIONS
.TP
.B \\-\\-lang <code>
Overrides the language selection (e.g. "de"), regardless of the language last
chosen in the settings dialog.
EOF
gzip -9 -n -f "$MAN_DIR/renderHAM.1" "$MAN_DIR/renderHAMgui.1"

INSTALLED_SIZE_KB="$(du -sk "$STAGE_DIR" --exclude=DEBIAN --exclude=substvars | cut -f1)"

HOMEPAGE_LINE=""
if [ -n "$HOMEPAGE" ]; then
    HOMEPAGE_LINE="Homepage: $HOMEPAGE"
fi

cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: $PKG_NAME
Version: $PKG_VERSION
Section: $SECTION
Priority: optional
Architecture: $PKG_ARCH
Installed-Size: $INSTALLED_SIZE_KB
Maintainer: $MAINTAINER
Depends: $DEPENDS
$HOMEPAGE_LINE
Description: Amiga HAM6 image and animation converter
 renderHAM converts photos and other images into Amiga HAM6 IFF/ILBM files,
 with palette control, dithering, cropping, and a Ken Burns-style animation
 preview and export (IFF ANIM).
 .
 Contains the command-line tool renderHAM and the wxWidgets GUI renderHAMgui
 (with a German translation).
 .
 This is AI-generated software: the code was written by Claude (Anthropic)
 under human direction. See the README for details.
EOF
# Remove the empty line left by a possibly empty Homepage field
sed -i '/^$/d' "$STAGE_DIR/DEBIAN/control"

DOC_DIR="$STAGE_DIR/usr/share/doc/$PKG_NAME"
mkdir -p "$DOC_DIR"
{
    echo "Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/"
    echo "Upstream-Name: renderHAM"
    [ -n "$HOMEPAGE" ] && echo "Source: $HOMEPAGE"
    echo ""
    echo "Files: *"
    echo "Copyright: 2026 rawbit (@ravvbit)"
    echo "License: Expat"
    echo "Comment: AI-generated software, written by Claude (Anthropic) under human direction."
    if [ -n "$WX_LICENCE_DIR" ]; then
        echo ""
        echo "Files: usr/bin/renderHAMgui"
        echo "Copyright: wxWidgets contributors (see the licence texts)"
        echo "License: wxWindows-3.1"
        echo " The wxWindows Library Licence, Version 3.1: the GNU Library General Public Licence with an"
        echo " exception that permits distributing binaries under the terms of one's own choice. The"
        echo " full texts are in /usr/share/doc/$PKG_NAME/wxWidgets/ (licence.txt and lgpl.txt)."
        echo "Comment: renderHAMgui contains a static copy of wxWidgets and of the libraries bundled with"
        echo " it (libpng, zlib, libjpeg, libtiff, expat, PCRE2). Their licence texts are in"
        echo " /usr/share/doc/$PKG_NAME/wxWidgets/ (licence.txt is the wxWindows Library Licence 3.1)."
    fi
    echo ""
    echo "Files: third_party/stb/*"
    echo "Copyright: 2017 Sean Barrett and contributors"
    echo "License: Expat"
    echo "Comment: stb is offered under the MIT License or the Unlicense, at the user's choice."
    echo ""
    echo "License: Expat"
    # The license text, in the indented form the copyright format requires
    sed -e '/^MIT License$/,/^$/d' -e '/^Copyright (c)/,/^$/d' \
        -e 's/^/ /' -e 's/^ $/ ./' "$PROJECT_ROOT/LICENSE"
} > "$DOC_DIR/copyright"
# LICENSE and THIRD_PARTY_NOTICES.md are already in $DOC_DIR: cmake --install puts them there.
gzip -9 -n -c "$PROJECT_ROOT/README.md" > "$DOC_DIR/README.md.gz"

# A "native" Debian package (no separate upstream/Debian revision) -
# policy still demands a changelog for it.
{
    echo "$PKG_NAME ($PKG_VERSION) unstable; urgency=low"
    echo ""
    echo "  * See CHANGELOG.md in the source repository for details."
    echo ""
    echo " -- $MAINTAINER  $(date -R)"
} | gzip -9 -n -c > "$DOC_DIR/changelog.gz"

echo ""
echo "=== 5/6: build the .deb ==="
mkdir -p "$OUTPUT_DIR"
rm -f "$DEB_FILE"
# Normalize permissions before building - DESTDIR installs partly inherit the
# current umask, but dpkg-deb strictly expects root:root and correct
# directory/file permissions.
find "$STAGE_DIR" -mindepth 1 -not -path "$STAGE_DIR/DEBIAN*" -exec chmod go+rX {} +
dpkg-deb --build --root-owner-group "$STAGE_DIR" "$DEB_FILE"

echo ""
echo "=== 6/6: check the package ==="
dpkg-deb --info "$DEB_FILE"
echo ""
echo "--- contents ---"
dpkg-deb --contents "$DEB_FILE"
if command -v lintian >/dev/null 2>&1; then
    echo ""
    echo "--- lintian (informational only) ---"
    lintian "$DEB_FILE" || true
fi

echo ""
echo "Done: $DEB_FILE"
echo "Install with:   sudo apt install $DEB_FILE"
echo "Remove with:    sudo apt remove $PKG_NAME"
