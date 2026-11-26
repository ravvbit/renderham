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
# Every one of those DLLs belongs to an MSYS2 package, and the package's license files (MSYS2
# installs them under share/licenses) go into the ZIP too: licenses/<package>/..., with
# licenses/PACKAGES.txt saying which package, version, license and DLLs each is. The script stops
# if a DLL has no package or a package has no license file, so that nothing is shipped without its
# license text. (A package that has none of its own, like wxWidgets' -msw-libs, uses those of a
# dependency with the same license, like -common-libs; PACKAGES.txt says so.)
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

# --- License texts of the DLLs --------------------------------------------------------------------
LICENSES="$STAGE/licenses"
mkdir -p "$LICENSES"
declare -A package_dlls=()
for dll in $copied; do
    # The import table spells a name in any case; pacman knows the one on disk.
    real="$(find "$MSYSTEM_PREFIX/bin" -maxdepth 1 -iname "$dll" | head -n 1)"
    package="$(pacman -Qqo "$real" 2>/dev/null || true)"
    [ -n "$package" ] || { echo "error: no MSYS2 package owns $dll" >&2; exit 1; }
    package_dlls["$package"]="${package_dlls[$package]:-} $(basename "$real")"
done
{
    echo "Packages of the DLLs in this folder"
    echo "==================================="
    echo
    echo "The DLLs next to the programs come from these MSYS2 (MinGW-w64, UCRT64) packages. The license"
    echo "texts the packages come with are in the folders next to this file. The build recipes and the"
    echo "sources of the packages are at https://github.com/msys2/MINGW-packages (one folder per package,"
    echo "named like the package without the mingw-w64-ucrt-x86_64- prefix); the program's own license"
    echo "is in LICENSE, and THIRD_PARTY_NOTICES.md in this ZIP file says more."
} > "$LICENSES/PACKAGES.txt"
# Copies the license files an MSYS2 package installs (under share/licenses) into the ZIP file's
# licenses/ folder and prints how many there were.
copy_license_files() {
    local count=0 file rel
    while IFS= read -r file; do
        [ -f "$file" ] || continue
        rel="${file#"$MSYSTEM_PREFIX"/share/licenses/}"
        mkdir -p "$LICENSES/$(dirname "$rel")"
        cp "$file" "$LICENSES/$rel"
        count=$((count + 1))
    done < <(pacman -Ql "$1" | awk '{print $2}' | grep "^$MSYSTEM_PREFIX/share/licenses/" || true)
    echo "$count"
}
package_license() {
    pacman -Qi "$1" | sed -n 's/^Licenses[[:space:]]*:[[:space:]]*//p'
}
# The installed packages another package depends on (without version constraints).
package_dependencies() {
    local dep
    for dep in $(pacman -Qi "$1" | sed -n 's/^Depends On[[:space:]]*:[[:space:]]*//p'); do
        dep="${dep%%[<>=]*}"
        if [ "$dep" != "None" ]; then pacman -Qq "$dep" 2>/dev/null || true; fi
    done
}
missing=""
for package in $(printf '%s\n' "${!package_dlls[@]}" | sort); do
    version="$(pacman -Q "$package" | awk '{print $2}')"
    info="$(pacman -Qi "$package")"
    license="$(printf '%s\n' "$info" | sed -n 's/^Licenses[[:space:]]*:[[:space:]]*//p')"
    homepage="$(printf '%s\n' "$info" | sed -n 's/^URL[[:space:]]*:[[:space:]]*//p')"
    texts="$(copy_license_files "$package")"
    from=""
    if [ "$texts" -eq 0 ]; then
        # A package made from the same source as another one (wxWidgets: the -msw-libs package and
        # the -common-libs package) may leave the license files to that one. Take them from a
        # dependency that has the same license.
        for dep in $(package_dependencies "$package"); do
            [ "$(package_license "$dep")" = "$license" ] || continue
            texts="$(copy_license_files "$dep")"
            if [ "$texts" -gt 0 ]; then from="$dep"; break; fi
        done
    fi
    [ "$texts" -gt 0 ] || missing="$missing $package"
    {
        echo
        echo "$package $version"
        echo "    DLLs:      ${package_dlls[$package]# }"
        echo "    license:   $license"
        [ -z "$from" ] || echo "    license texts: those of $from"
        echo "    home page: $homepage"
    } >> "$LICENSES/PACKAGES.txt"
    echo "$package $version ($license): $texts license file(s)${from:+ (from $from)}, DLLs:${package_dlls[$package]}"
done
if [ -n "$missing" ]; then
    echo "error: no license file found for the MSYS2 package(s):$missing" >&2
    exit 1
fi

(cd "$OUT_DIR" && zip -q -r -9 "$NAME.zip" "$NAME")
ls -l "$OUT_DIR/$NAME.zip"
echo "Wrote $OUT_DIR/$NAME.zip"
