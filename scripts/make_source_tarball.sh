#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/make_source_tarball.sh
#
# Writes the source code of a version of renderHAM as renderham-<version>.tar.gz (everything below
# one directory renderham-<version>/), the usual form for people who build from source and for
# distributions that package it. It takes the committed files of a git revision, so it needs
# a git checkout; files named in .gitattributes with "export-ignore" (the CI configuration) are left out.
# The same revision always gives the same file.
#
# Usage:
#   ./scripts/make_source_tarball.sh [--ref REVISION] [--output-dir DIR]
#
#   --ref REVISION    which commit or tag (default: HEAD)
#   --output-dir DIR  where to write the file (default: dist)
#
# Unpacking and building it:
#   tar xzf renderham-<version>.tar.gz && cd renderham-<version> && ./scripts/build.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

ref=HEAD
out_dir=dist

usage() {
    sed -n '/^# Usage:/,/^# Unpacking/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --ref) ref="${2:?--ref needs a revision}"; shift ;;
        --output-dir) out_dir="${2:?--output-dir needs a directory}"; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if ! command -v git > /dev/null 2>&1 || ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "error: this needs a git checkout of renderHAM (and git)." >&2
    exit 1
fi
if ! git rev-parse --verify --quiet "$ref^{commit}" > /dev/null; then
    echo "error: '$ref' is not a revision of this repository." >&2
    exit 1
fi

# The version is defined in CMakeLists.txt, in the revision that is packed.
version="$(git show "$ref:CMakeLists.txt" | sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p')"
if [ -z "$version" ]; then
    echo "error: could not read the version from CMakeLists.txt." >&2
    exit 1
fi

if [ "$ref" = HEAD ] && [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "note: there are uncommitted changes; the tarball holds the last commit, without them." >&2
fi

name="renderham-$version"
mkdir -p "$out_dir"
out="$(cd "$out_dir" && pwd)/$name.tar.gz"

git archive --format=tar --prefix="$name/" "$ref" | gzip -9n > "$out"

# Check it: unpacked, it must hold what scripts/build.sh needs.
for f in CMakeLists.txt LICENSE THIRD_PARTY_NOTICES.md README.md scripts/build.sh scripts/install.sh; do
    if ! tar -tzf "$out" "$name/$f" > /dev/null 2>&1; then
        echo "error: $f is missing in $out" >&2
        rm -f "$out"
        exit 1
    fi
done

echo "$out"
(cd "$(dirname "$out")" && sha256sum "$(basename "$out")")
