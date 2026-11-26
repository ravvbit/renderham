#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/install.sh
#
# Installs what scripts/build.sh built: the command-line tool renderHAM and the GUI renderHAMgui
# (with its German translation, menu entry and icon), the library with its headers, and the licence
# texts. Works on any Linux distribution. It records what it installed, so that scripts/uninstall.sh
# can remove it again.
#
# Usage:
#   ./scripts/install.sh [options]
#
#   (no option)       install system-wide into /usr/local (sudo is used when you are not root)
#   --user            install for yourself only, into ~/.local (no root rights needed)
#   --prefix DIR      install below DIR instead (e.g. /usr or /opt/renderham)
#   --destdir DIR     put everything below DIR instead of the real root, for people who build
#                     packages; nothing is registered with the system then
#   --build-dir DIR   where scripts/build.sh built (default: build)
#
# The programs end up in <prefix>/bin, the menu entry, icon and documentation in <prefix>/share,
# the library in <prefix>/lib and the headers in <prefix>/include/aic. Do not use the prefix /usr
# on a system where the Debian package or another package of renderHAM is installed; the default
# /usr/local never clashes with a package.
#
# Remove it again with the same options: ./scripts/uninstall.sh [--user | --prefix DIR]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

prefix=/usr/local
prefix_given=no
user_install=no
destdir=""
build_dir=build

usage() {
    sed -n '/^# Usage:/,/^# Remove it again/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --user) user_install=yes ;;
        --prefix) prefix="${2:?--prefix needs a directory}"; prefix_given=yes; shift ;;
        --destdir) destdir="${2:?--destdir needs a directory}"; shift ;;
        --build-dir) build_dir="${2:?--build-dir needs a directory}"; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [ "$user_install" = yes ]; then
    if [ "$prefix_given" = yes ]; then
        echo "error: --user and --prefix exclude each other." >&2
        exit 2
    fi
    prefix="$HOME/.local"
fi
case "$prefix" in
    /*) ;;
    *) echo "error: the prefix must be an absolute path, not '$prefix'." >&2; exit 2 ;;
esac
prefix="${prefix%/}"
[ -n "$prefix" ] || prefix=/
destdir="${destdir%/}"
if [ -n "$destdir" ]; then
    case "$destdir" in
        /*) ;;
        *) echo "error: --destdir must be an absolute path." >&2; exit 2 ;;
    esac
fi

# --- Is there something to install? ------------------------------------------------------------
if [ ! -f "$build_dir/CMakeCache.txt" ] || [ ! -x "$build_dir/tools/renderham/renderHAM" ]; then
    echo "error: nothing built in '$build_dir' yet. Run ./scripts/build.sh first." >&2
    exit 1
fi
if ! command -v cmake > /dev/null 2>&1; then
    echo "error: cmake not found (./scripts/install_dependencies.sh installs it)." >&2
    exit 1
fi
cmake_bin="$(command -v cmake)"
build_dir="$(cd "$build_dir" && pwd)"
version="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)"

# --- Root rights? ------------------------------------------------------------------------------
# Needed when the nearest directory that exists on the way to the target can not be written.
target_root="$destdir$prefix"
probe="$target_root"
while [ ! -e "$probe" ] && [ "$probe" != / ]; do probe="$(dirname "$probe")"; done
sudo_cmd=()
if [ ! -w "$probe" ] && [ "$(id -u)" -ne 0 ]; then
    if command -v sudo > /dev/null 2>&1; then
        sudo_cmd=(sudo)
        echo "Installing into $target_root needs root rights; using sudo."
    else
        echo "error: $target_root is not writable and 'sudo' is not installed." >&2
        echo "       Run this as root, or install for yourself only: ./scripts/install.sh --user" >&2
        exit 1
    fi
fi

# A command that runs with those rights.
as_root() { ${sudo_cmd[@]+"${sudo_cmd[@]}"} "$@"; }

# --- Install -----------------------------------------------------------------------------------
echo "=== installing renderHAM ${version:-} into $target_root ==="
if [ -n "$destdir" ]; then
    as_root env "DESTDIR=$destdir" "$cmake_bin" --install "$build_dir" --prefix "$prefix"
else
    as_root "$cmake_bin" --install "$build_dir" --prefix "$prefix"
fi

# CMake wrote the list of installed files into the build directory (as root, when sudo was used):
# hand it back to whoever owns the build directory, and keep a copy next to the installed files.
manifest="$build_dir/install_manifest.txt"
if [ "$(id -u)" -eq 0 ] || [ "${#sudo_cmd[@]}" -gt 0 ]; then
    as_root chown "$(stat -c '%u:%g' "$build_dir")" "$manifest" 2> /dev/null || true
fi

stored="$target_root/share/renderham/install_manifest.txt"
tmp_manifest="$(mktemp)"
trap 'rm -f "$tmp_manifest"' EXIT
# The paths in the list never contain the --destdir directory (some CMake versions add it; drop it).
awk -v d="$destdir" -v p="$prefix" 'BEGIN { pre = d p "/" } { if (d != "" && index($0, pre) == 1) print substr($0, length(d) + 1); else print $0 }' \
    "$manifest" > "$tmp_manifest"
as_root install -D -m 644 "$tmp_manifest" "$stored"

# --- Register with the desktop (best effort; a failure here is harmless) -----------------------------
if [ -z "$destdir" ]; then
    if command -v update-desktop-database > /dev/null 2>&1; then
        as_root update-desktop-database -q "$prefix/share/applications" 2> /dev/null || true
    fi
    if [ -f "$prefix/share/icons/hicolor/index.theme" ] && command -v gtk-update-icon-cache > /dev/null 2>&1; then
        as_root gtk-update-icon-cache -q -t -f "$prefix/share/icons/hicolor" 2> /dev/null || true
    fi
fi

echo ""
echo "Installed:"
echo "  $target_root/bin/renderHAM"
if [ -x "$target_root/bin/renderHAMgui" ]; then
    echo "  $target_root/bin/renderHAMgui   (and the entry \"renderHAM\" in the application menu)"
else
    echo "  (renderHAMgui was not built, so only the command-line tool is installed)"
fi
if [ -z "$destdir" ]; then
    case ":$PATH:" in
        *":$prefix/bin:"*) ;;
        *)
            echo ""
            echo "Note: $prefix/bin is not in your PATH. Add it, e.g. with this line in ~/.profile:"
            echo "      export PATH=\"$prefix/bin:\$PATH\""
            ;;
    esac
fi
uninstall_hint="./scripts/uninstall.sh"
if [ "$user_install" = yes ]; then
    uninstall_hint="$uninstall_hint --user"
elif [ "$prefix_given" = yes ]; then
    uninstall_hint="$uninstall_hint --prefix $prefix"
fi
if [ -n "$destdir" ]; then uninstall_hint="$uninstall_hint --destdir $destdir"; fi
echo ""
echo "To remove it again: $uninstall_hint"
