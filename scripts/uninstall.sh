#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/uninstall.sh
#
# Removes what scripts/install.sh installed, using the list of files it recorded in
# <prefix>/share/renderham/install_manifest.txt. Only files below the prefix are removed, and
# only directories that belong to renderHAM (and only when they are empty). On macOS the
# application renderHAM.app that install.sh put into /Applications, ~/Applications or the prefix is
# removed as well. Your saved GUI settings are not touched.
#
# Usage:
#   ./scripts/uninstall.sh [options]
#
#   (no option)       remove the system-wide installation in /usr/local (sudo is used when needed)
#   --user            remove the installation in ~/.local
#   --prefix DIR      remove the installation below DIR
#   --destdir DIR     remove from below DIR (the counterpart of install.sh --destdir)
#   --dry-run         only list what would be removed
#
# Something installed with a package (the .deb, the Flatpak) is removed with its package manager
# instead: sudo apt remove renderham, or flatpak uninstall io.github.ravvbit.renderham.

set -euo pipefail

prefix=/usr/local
prefix_given=no
user_install=no
destdir=""
dry_run=no

usage() {
    sed -n '/^# Usage:/,/^# Something installed/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --user) user_install=yes ;;
        --prefix) prefix="${2:?--prefix needs a directory}"; prefix_given=yes; shift ;;
        --destdir) destdir="${2:?--destdir needs a directory}"; shift ;;
        --dry-run | -n) dry_run=yes ;;
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
if [ -z "$prefix" ]; then
    echo "error: refusing to work on the prefix /." >&2
    exit 2
fi
destdir="${destdir%/}"
if [ -n "$destdir" ]; then
    case "$destdir" in
        /*) ;;
        *) echo "error: --destdir must be an absolute path." >&2; exit 2 ;;
    esac
fi

root="$destdir$prefix"
manifest="$root/share/renderham/install_manifest.txt"
if [ ! -f "$manifest" ]; then
    echo "error: found no installation of renderHAM in $root (no $manifest)." >&2
    echo "       Installed with ./scripts/install.sh --user, --prefix or --destdir? Pass the same option here." >&2
    echo "       Installed from a .deb or a Flatpak? Use: sudo apt remove renderham  /  flatpak uninstall io.github.ravvbit.renderham" >&2
    exit 1
fi

# --- What to remove ----------------------------------------------------------------------------
# Only absolute paths below the prefix, without "..": the list is a plain text file.
files=()
while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        "") continue ;;
        *"/../"* | */..) echo "warning: skipping suspicious entry: $line" >&2; continue ;;
        "$prefix"/*) ;;
        *) echo "warning: skipping entry outside $prefix: $line" >&2; continue ;;
    esac
    path="$destdir$line"
    if [ -e "$path" ] || [ -L "$path" ]; then files+=("$path"); fi
done < "$manifest"

# macOS: the application bundle, recorded as a whole. It is removed only from where install.sh puts
# it, and only when it really is renderHAM.
app_record="$root/share/renderham/install_app.txt"
app=""
if [ -f "$app_record" ]; then
    IFS= read -r app_line < "$app_record" || true
    case "$app_line" in
        *"/../"*) app_line="" ;;
        "$prefix/renderHAM.app") ;;
        /Applications/renderHAM.app | "$HOME/Applications/renderHAM.app") [ -z "$destdir" ] || app_line="" ;;
        *) app_line="" ;;
    esac
    if [ -z "$app_line" ]; then
        echo "warning: ignoring the unexpected entry in $app_record" >&2
    elif [ -d "$destdir$app_line" ]; then
        if grep -q "<string>io.github.ravvbit.renderham</string>" "$destdir$app_line/Contents/Info.plist" 2> /dev/null; then
            app="$destdir$app_line"
        else
            echo "warning: $destdir$app_line is not renderHAM; leaving it alone." >&2
        fi
    fi
fi

# Directories that may be removed (when empty): the file's own directory and the ones above it,
# as long as renderHAM owns them: share/renderham, share/doc/renderham, bin/translations/... and
# include/aic/... (the library's headers).
dirs=()
add_dirs() {
    local d="$1" rel comp owned
    while [ "$d" != "$root" ] && [ "$d" != / ] && [ -n "$d" ]; do
        rel="${d#"$root"/}"
        owned=no
        case "$rel" in include/aic | include/aic/*) owned=yes ;; esac
        for comp in ${rel//\// }; do
            if [ "$comp" = renderham ] || [ "$comp" = translations ]; then owned=yes; fi
        done
        [ "$owned" = yes ] || break
        dirs+=("$d")
        d="$(dirname "$d")"
    done
}
for f in "${files[@]+"${files[@]}"}"; do add_dirs "$(dirname "$f")"; done
add_dirs "$root/share/renderham"
# deepest first, each directory once
# (a loop instead of mapfile: the bash of macOS, 3.2, does not have it)
sorted_dirs=()
while IFS= read -r d; do sorted_dirs+=("$d"); done \
    < <(printf '%s\n' "${dirs[@]+"${dirs[@]}"}" | awk 'NF' | sort -u | awk '{ print length($0) "\t" $0 }' | sort -rn | cut -f2-)
dirs=(${sorted_dirs[@]+"${sorted_dirs[@]}"})

# --- Root rights? ------------------------------------------------------------------------------
sudo_cmd=()
need_root=no
if [ "$(id -u)" -ne 0 ] && [ "$dry_run" = no ]; then
    for f in "${files[@]+"${files[@]}"}" "$manifest" ${app:+"$app"}; do
        if [ ! -w "$(dirname "$f")" ]; then need_root=yes; break; fi
    done
    if [ "$need_root" = yes ]; then
        if command -v sudo > /dev/null 2>&1; then
            sudo_cmd=(sudo)
            echo "Removing from $root needs root rights; using sudo."
        else
            echo "error: $root is not writable and 'sudo' is not installed; run this as root." >&2
            exit 1
        fi
    fi
fi
as_root() { ${sudo_cmd[@]+"${sudo_cmd[@]}"} "$@"; }

# --- Remove ------------------------------------------------------------------------------------
if [ "$dry_run" = yes ]; then
    echo "Would remove from $root:"
    for f in "${files[@]+"${files[@]}"}"; do echo "  ${f#"$destdir"}"; done
    echo "  ${manifest#"$destdir"}"
    if [ -f "$app_record" ]; then echo "  ${app_record#"$destdir"}"; fi
    if [ -n "$app" ]; then echo "  ${app#"$destdir"}/   (the application)"; fi
    for d in "${dirs[@]+"${dirs[@]}"}"; do echo "  ${d#"$destdir"}/   (if empty)"; done
    exit 0
fi

echo "=== removing renderHAM from $root ==="
if [ "${#files[@]}" -gt 0 ]; then
    as_root rm -f -- "${files[@]}"
fi
as_root rm -f -- "$manifest" "$app_record"
if [ -n "$app" ]; then
    as_root rm -rf -- "$app"
    echo "Removed the application $app."
fi
for d in "${dirs[@]+"${dirs[@]}"}"; do
    as_root rmdir "$d" 2> /dev/null || true
done

if [ -z "$destdir" ]; then
    if command -v update-desktop-database > /dev/null 2>&1 && [ -d "$prefix/share/applications" ]; then
        as_root update-desktop-database -q "$prefix/share/applications" 2> /dev/null || true
    fi
    if [ -f "$prefix/share/icons/hicolor/index.theme" ] && command -v gtk-update-icon-cache > /dev/null 2>&1; then
        as_root gtk-update-icon-cache -q -t -f "$prefix/share/icons/hicolor" 2> /dev/null || true
    fi
fi

settings="~/.renderHAMgui"
[ "$(uname -s)" != Darwin ] || settings="~/Library/Preferences/renderHAMgui Preferences"
echo "Removed ${#files[@]} files. Your saved settings ($settings) were left in place; delete that file by hand if you want them gone, too."
