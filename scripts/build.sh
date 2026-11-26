#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/build.sh
#
# Builds renderHAM (the command-line tool, the GUI and the library) from this source tree with CMake,
# on any Linux distribution (and other Unix systems with CMake, a C++17 compiler and, for the GUI,
# wxWidgets). It checks the prerequisites first and says what is missing, finds the wx-config of
# distributions that name it differently, and tells you at the end what was built.
#
# Usage:
#   ./scripts/build.sh [options] [-- extra arguments for the cmake configure step]
#
#   --no-gui          do not build renderHAMgui (the command-line tool and the library need no wxWidgets)
#   --require-gui     fail if renderHAMgui can not be built (default: build it when wxWidgets is found,
#                     and say so when it is not)
#   --debug           a Debug build instead of Release
#   --build-dir DIR   where to build (default: build)
#   -j N, --jobs N    number of parallel jobs (default: the number of processors)
#   --test            run the tests (ctest) after the build
#
# Environment variable:
#   WX_CONFIG         the wx-config of the wxWidgets to build the GUI with (default: the one in the PATH)
#
# Missing tools: ./scripts/install_dependencies.sh installs them on Debian, Ubuntu, Linux Mint, Fedora,
# Arch Linux and openSUSE, and on macOS with Homebrew. Afterwards: ./scripts/install.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

gui=yes
require_gui=no
build_type=Release
build_dir=build
run_tests=no
jobs=""
extra=()

usage() {
    sed -n '/^# Usage:/,/^# Environment variable:/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-gui) gui=no ;;
        --require-gui) require_gui=yes ;;
        --debug) build_type=Debug ;;
        --build-dir) build_dir="${2:?--build-dir needs a directory}"; shift ;;
        -j | --jobs) jobs="${2:?--jobs needs a number}"; shift ;;
        --test) run_tests=yes ;;
        -h | --help) usage; exit 0 ;;
        --) shift; extra=("$@"); break ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [ -z "$jobs" ]; then
    jobs="$(nproc 2> /dev/null || getconf _NPROCESSORS_ONLN 2> /dev/null || echo 2)"
fi

# --- Prerequisites ----------------------------------------------------------------------------
missing=""
command -v cmake > /dev/null 2>&1 || missing="$missing cmake"
if ! command -v c++ > /dev/null 2>&1 && ! command -v g++ > /dev/null 2>&1 && ! command -v clang++ > /dev/null 2>&1; then
    missing="$missing a-C++-compiler"
fi
command -v make > /dev/null 2>&1 || command -v ninja > /dev/null 2>&1 || missing="$missing make"
if [ -n "$missing" ]; then
    echo "error: missing:$missing" >&2
    echo "       ./scripts/install_dependencies.sh installs everything that is needed." >&2
    exit 1
fi

cmake_version="$(cmake --version | sed -n '1s/[^0-9]*\([0-9][0-9.]*\).*/\1/p')"
if [ "$(printf '%s\n%s\n' 3.16 "$cmake_version" | sort -V | head -n 1)" != 3.16 ]; then
    echo "error: CMake $cmake_version is too old; renderHAM needs CMake 3.16 or newer." >&2
    exit 1
fi

if [ "$gui" = yes ] && ! command -v msgfmt > /dev/null 2>&1; then
    echo "note: msgfmt (gettext) not found; the GUI will be English only."
fi

# --- wxWidgets --------------------------------------------------------------------------------
wx_args=()
if [ "$gui" = no ]; then
    wx_args+=(-DAIC_BUILD_GUI=OFF)
else
    wx_config="${WX_CONFIG:-}"
    if [ -z "$wx_config" ] && ! command -v wx-config > /dev/null 2>&1; then
        # Some distributions install wx-config under another name only.
        for name in wx-config-gtk3 wx-config-3.2 wx-config-3.0; do
            if command -v "$name" > /dev/null 2>&1; then wx_config="$(command -v "$name")"; break; fi
        done
    fi
    if [ -n "$wx_config" ]; then
        wx_args+=("-DwxWidgets_CONFIG_EXECUTABLE=$wx_config")
        echo "wxWidgets: $("$wx_config" --version) (via $wx_config)"
    elif command -v wx-config > /dev/null 2>&1; then
        echo "wxWidgets: $(wx-config --version)"
    else
        echo "note: wx-config not found, so the GUI will be skipped (./scripts/install_dependencies.sh installs wxWidgets)."
    fi
fi

# --- Build ------------------------------------------------------------------------------------
echo "=== configure ($build_type, in $build_dir) ==="
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" ${wx_args[@]+"${wx_args[@]}"} ${extra[@]+"${extra[@]}"}

echo ""
echo "=== build ($jobs jobs) ==="
cmake --build "$build_dir" -j "$jobs"

cli="$build_dir/tools/renderham/renderHAM"
gui_exe="$build_dir/tools/renderhamgui/renderHAMgui"
# On macOS the GUI is an application bundle (renderHAM.app), not a plain program.
gui_app="$build_dir/tools/renderhamgui/renderHAM.app"
if [ -x "$gui_app/Contents/MacOS/renderHAM" ]; then gui_exe="$gui_app"; fi
[ -x "$cli" ] || { echo "error: the build finished but $cli is missing." >&2; exit 1; }
if [ "$gui" = yes ] && [ ! -e "$gui_exe" ]; then
    echo "" >&2
    echo "warning: renderHAMgui was not built because wxWidgets was not found." >&2
    echo "         ./scripts/install_dependencies.sh installs it; --no-gui silences this warning." >&2
    if [ "$require_gui" = yes ]; then exit 1; fi
fi

if [ "$run_tests" = yes ]; then
    echo ""
    echo "=== tests ==="
    (cd "$build_dir" && ctest --output-on-failure)
fi

echo ""
echo "Built in $build_dir:"
echo "  $cli"
if [ -e "$gui_exe" ]; then echo "  $gui_exe"; fi
echo "Next: ./scripts/install.sh   (system-wide, with sudo)   or   ./scripts/install.sh --user"
