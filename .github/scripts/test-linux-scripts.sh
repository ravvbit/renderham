#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# Run by the "Scripts on <distribution>" jobs of .github/workflows/build.yml inside a container of
# a Linux distribution, as root, with the source tarball in /pkg. It does what the README tells a
# user to do, and checks the result: unpack the tarball, install the dependencies with
# scripts/install_dependencies.sh, build and test with scripts/build.sh, install with
# scripts/install.sh (system-wide, for the user alone, and into a staging directory), start the
# programs, and remove everything again with scripts/uninstall.sh.

set -euo pipefail

group() { echo "::group::$*"; }
endgroup() { echo "::endgroup::"; }

# --- Tools the test itself needs (not the scripts): tar, and a virtual X server for the GUI ------------------
group "Tools for the test"
# shellcheck source=/dev/null
. /etc/os-release
family=""
for word in ${ID:-} ${ID_LIKE:-}; do
    case "$word" in
        debian | ubuntu) family=apt ;;
        fedora | rhel | centos) family=dnf ;;
        arch) family=pacman ;;
        suse | opensuse | opensuse-leap | opensuse-tumbleweed | sles) family=zypper ;;
    esac
    [ -z "$family" ] || break
done
echo "${PRETTY_NAME:-$ID} -> $family"
case "$family" in
    apt)
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends tar gzip xvfb xauth ca-certificates
        ;;
    dnf) dnf install -y -q tar gzip diffutils xorg-x11-server-Xvfb xauth ;;
    pacman) pacman -Syu --noconfirm --needed tar gzip xorg-server-xvfb xorg-xauth ;;
    zypper) zypper --non-interactive install tar gzip xvfb-run xauth ;;
    *) echo "unknown distribution family"; exit 1 ;;
esac
endgroup

# --- Unpack ---------------------------------------------------------------------------------------------
group "Unpack the source tarball"
tarball="$(ls /pkg/renderham-*.tar.gz)"
mkdir -p /work
tar -xzf "$tarball" -C /work
cd /work/renderham-*/
pwd
test ! -e .github   # the CI configuration is not part of the tarball
endgroup

# --- Dependencies, build, tests ---------------------------------------------------------------------------
group "scripts/install_dependencies.sh"
./scripts/install_dependencies.sh --yes
endgroup

group "scripts/build.sh --test --require-gui"
./scripts/build.sh --test --require-gui
test -x build/tools/renderhamgui/renderHAMgui
# The GUI is built with the GTK 3 version of wxWidgets (openSUSE, for one, has a GTK 2 one as well).
command -v wx-config wx-config-gtk3 || true
readelf -d build/tools/renderhamgui/renderHAMgui | grep NEEDED | grep wx_
readelf -d build/tools/renderhamgui/renderHAMgui | grep -q 'libwx_gtk3u'
endgroup

group "scripts/build.sh --no-gui"
./scripts/build.sh --no-gui --build-dir build-cli
test -x build-cli/tools/renderham/renderHAM
test ! -e build-cli/tools/renderhamgui/renderHAMgui
endgroup

# --- System-wide installation (the default prefix /usr/local) ---------------------------------------------------
group "scripts/install.sh"
find /usr/local -type f ! -path '/usr/local/share/applications/*.cache' | sort > /tmp/usr-local-before
./scripts/install.sh
test -x /usr/local/bin/renderHAM
test -x /usr/local/bin/renderHAMgui
test -f /usr/local/bin/translations/de/LC_MESSAGES/renderhamgui.mo
test -f /usr/local/share/applications/io.github.ravvbit.renderham.desktop
test -f /usr/local/share/metainfo/io.github.ravvbit.renderham.metainfo.xml
test -f /usr/local/share/icons/hicolor/scalable/apps/io.github.ravvbit.renderham.svg
test -f /usr/local/share/doc/renderham/LICENSE
test -f /usr/local/share/doc/renderham/THIRD_PARTY_NOTICES.md
test -f /usr/local/share/renderham/install_manifest.txt
# lib or lib64, as the distribution has it (Fedora, Rocky Linux and openSUSE: lib64)
test -f /usr/local/lib/librenderhamlib.a -o -f /usr/local/lib64/librenderhamlib.a
test -f /usr/local/include/aic/aic.hpp
endgroup

group "Run the installed programs"
/usr/local/bin/renderHAM --version
/usr/local/bin/renderHAM -i testdata/test_input.png -o /tmp/test.iff
test -s /tmp/test.iff
status=0
timeout 10 xvfb-run -a /usr/local/bin/renderHAMgui || status=$?
[ "$status" -eq 124 ]   # still running when the timeout stopped it
echo "renderHAMgui ran until the timeout"
endgroup

group "scripts/uninstall.sh"
./scripts/uninstall.sh --dry-run > /tmp/dry-run.txt   # (no pipe to head: that would end the script with SIGPIPE)
head -5 /tmp/dry-run.txt
./scripts/uninstall.sh
find /usr/local -type f ! -path '/usr/local/share/applications/*.cache' | sort > /tmp/usr-local-after
diff /tmp/usr-local-before /tmp/usr-local-after
[ -z "$(find /usr/local \( -name renderham -o -name translations -o -name aic \))" ]
if ./scripts/uninstall.sh 2> /dev/null; then echo "uninstalling nothing should fail"; exit 1; fi
echo "/usr/local is as it was"
endgroup

# --- For the user alone (~/.local) -------------------------------------------------------------------------------
group "scripts/install.sh --user"
(
    export HOME=/tmp/home-user
    mkdir -p "$HOME"
    ./scripts/install.sh --user
    test -x "$HOME/.local/bin/renderHAM"
    test -f "$HOME/.local/share/renderham/install_manifest.txt"
    "$HOME/.local/bin/renderHAM" --version
    ./scripts/uninstall.sh --user
    [ -z "$(find "$HOME/.local" -type f ! -path '*/share/applications/*.cache')" ]
)
endgroup

# --- Into a staging directory, command-line tool only (what a package builder does) ----------------------------------
group "scripts/install.sh --destdir --prefix, without the GUI"
./scripts/install.sh --build-dir build-cli --destdir /tmp/staging --prefix /opt/renderham
test -x /tmp/staging/opt/renderham/bin/renderHAM
test ! -e /tmp/staging/opt/renderham/bin/renderHAMgui
test ! -e /opt/renderham
./scripts/uninstall.sh --destdir /tmp/staging --prefix /opt/renderham
[ -z "$(find /tmp/staging -type f)" ]
endgroup

echo "All checks passed."
