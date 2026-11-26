#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/install_dependencies.sh
#
# Installs what is needed to build renderHAM from source, with the package manager of this
# Linux distribution: a C++17 compiler, CMake, gettext (for the German translation of the GUI)
# and wxWidgets (for the GUI). It knows
#
#   Debian, Ubuntu, Linux Mint, Pop!_OS and relatives   apt
#   Fedora, RHEL, Rocky, AlmaLinux and relatives        dnf     (RHEL and relatives need EPEL for wxWidgets)
#   Arch Linux, Manjaro, EndeavourOS and relatives      pacman
#   openSUSE (Tumbleweed, Leap) and relatives           zypper
#
# Usage:
#   ./scripts/install_dependencies.sh [--no-gui] [--yes] [--print]
#
#   --no-gui   only what the command-line tool and the library need (no wxWidgets)
#   --yes      do not ask before installing
#   --print    only print the commands, do not run them
#
# The packages are installed with root rights (sudo is used when you are not root). Other
# distributions: install a C++17 compiler, make, CMake 3.16 or newer, gettext and wxWidgets 3.0 or
# newer (GTK 3 version) with its development files, then continue with scripts/build.sh.
#
# Afterwards:  ./scripts/build.sh && ./scripts/install.sh

set -euo pipefail

gui=yes
assume_yes=no
print_only=no

usage() {
    sed -n '/^# Usage:/,/^# The packages/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --no-gui) gui=no ;;
        --yes | -y) assume_yes=yes ;;
        --print) print_only=yes ;;
        -h | --help) usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# --- Which distribution? -----------------------------------------------------------------------
os_id=""
os_like=""
os_name="this system"
if [ -r /etc/os-release ]; then
    # shellcheck source=/dev/null
    os_id="$(. /etc/os-release && printf '%s' "${ID:-}")"
    # shellcheck source=/dev/null
    os_like="$(. /etc/os-release && printf '%s' "${ID_LIKE:-}")"
    # shellcheck source=/dev/null
    os_name="$(. /etc/os-release && printf '%s' "${PRETTY_NAME:-${NAME:-this system}}")"
fi

family=""
for word in $os_id $os_like; do
    case "$word" in
        debian | ubuntu) family=apt ;;
        fedora | rhel | centos) family=dnf ;;
        arch) family=pacman ;;
        suse | opensuse | opensuse-leap | opensuse-tumbleweed | sles) family=zypper ;;
    esac
    [ -z "$family" ] || break
done

if [ -z "$family" ]; then
    cat >&2 <<EOF
error: do not know the package manager of $os_name.
Install by hand: a C++17 compiler, make, CMake 3.16 or newer, gettext (msgfmt) and, for the GUI,
wxWidgets 3.0 or newer (GTK 3) with its development files. Then run ./scripts/build.sh.
EOF
    exit 1
fi

# --- Root rights -------------------------------------------------------------------------------
sudo_cmd=()
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo > /dev/null 2>&1; then
        sudo_cmd=(sudo)
    elif [ "$print_only" = no ]; then
        echo "error: this needs root rights and 'sudo' is not installed; run it as root." >&2
        exit 1
    fi
fi

# Prints a command (with sudo in front where needed) and, unless --print was given, runs it.
run() {
    echo "+ ${sudo_cmd[*]-}${sudo_cmd[*]:+ }$*"
    if [ "$print_only" = no ]; then
        ${sudo_cmd[@]+"${sudo_cmd[@]}"} "$@"
    fi
}

confirm() {
    if [ "$assume_yes" = yes ] || [ "$print_only" = yes ]; then return 0; fi
    if [ ! -t 0 ]; then
        echo "error: not running in a terminal; pass --yes to install without asking." >&2
        exit 1
    fi
    read -r -p "Install these packages on $os_name? [Y/n] " answer
    case "$answer" in
        "" | y | Y | yes | YES) ;;
        *) echo "Cancelled."; exit 1 ;;
    esac
}

echo "System: $os_name (package manager: $family)"

case "$family" in
    apt)
        packages=(build-essential cmake gettext)
        confirm
        run env DEBIAN_FRONTEND=noninteractive apt-get update
        if [ "$gui" = yes ]; then
            # wxWidgets 3.2 where the release has it (Ubuntu 24.04, Debian 12 and newer), else 3.0
            # (Ubuntu 22.04 and Linux Mint 21, Ubuntu 20.04). renderHAMgui builds with either.
            wx_package=""
            for candidate in libwxgtk3.2-dev libwxgtk3.0-gtk3-dev; do
                if apt-cache show "$candidate" > /dev/null 2>&1; then wx_package="$candidate"; break; fi
            done
            if [ -z "$wx_package" ]; then
                if [ "$print_only" = yes ]; then
                    wx_package="libwxgtk3.2-dev"  # or libwxgtk3.0-gtk3-dev on releases without 3.2
                else
                    echo "error: this system has neither libwxgtk3.2-dev nor libwxgtk3.0-gtk3-dev;" >&2
                    echo "       use --no-gui, or build wxWidgets yourself (scripts/build_wx_linux.sh)." >&2
                    exit 1
                fi
            fi
            packages+=("$wx_package")
        fi
        run env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends "${packages[@]}"
        ;;
    dnf)
        packages=(gcc-c++ make cmake gettext)
        [ "$gui" = no ] || packages+=(wxGTK-devel)
        confirm
        if [ "$gui" = yes ] && { [ "$os_id" = rhel ] || [ "$os_id" = centos ] || [ "$os_id" = rocky ] || [ "$os_id" = almalinux ]; }; then
            echo "note: wxGTK is in EPEL on this system; installing epel-release first."
            run dnf install -y epel-release
        fi
        run dnf install -y "${packages[@]}"
        ;;
    pacman)
        packages=(base-devel cmake gettext)
        [ "$gui" = no ] || packages+=(wxwidgets-gtk3)
        confirm
        # No -y here on purpose: refreshing the package lists without upgrading the system is not
        # supported on Arch (a "partial upgrade"), so the system is left to the user to update.
        if ! run pacman -S --needed --noconfirm "${packages[@]}"; then
            echo "If pacman said 'target not found', its package lists are out of date: update the system" >&2
            echo "with 'sudo pacman -Syu', then run this script again." >&2
            exit 1
        fi
        ;;
    zypper)
        packages=(gcc-c++ make cmake gettext-tools)
        confirm
        run zypper --non-interactive refresh
        if [ "$gui" = yes ]; then
            wx_package=""
            # openSUSE names the GTK 3 build wxGTK3-...; wxWidgets-3_2-devel there is the GTK 2 one.
            for candidate in wxGTK3-3_2-devel wxGTK3-devel; do
                if zypper --non-interactive search --match-exact --type package "$candidate" 2> /dev/null | grep -q "^[a-z ]*| $candidate "; then
                    wx_package="$candidate"; break
                fi
            done
            if [ -z "$wx_package" ]; then
                if [ "$print_only" = yes ]; then
                    wx_package="wxGTK3-3_2-devel"
                else
                    echo "error: found no wxWidgets (GTK 3) development package (wxGTK3-3_2-devel); use --no-gui." >&2
                    exit 1
                fi
            fi
            packages+=("$wx_package")
        fi
        run zypper --non-interactive install --no-recommends "${packages[@]}"
        ;;
esac

echo ""
echo "Done. Next: ./scripts/build.sh && ./scripts/install.sh"
