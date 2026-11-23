#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#
# scripts/update_translations.sh
#
# Re-extracts the translatable strings of renderHAMgui (everything wrapped in _("...")) into
# tools/renderhamgui/po/renderhamgui.pot and merges them into every <lang>.po next to it.
# New strings show up in the .po files with an empty msgstr (and changed ones as "fuzzy");
# translate those, then run:  msgfmt --check -o /dev/null tools/renderhamgui/po/<lang>.po
#
# Needs GNU gettext (xgettext, msgmerge, msgattrib).
set -euo pipefail

GUI_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../tools/renderhamgui" && pwd)"
cd "$GUI_DIR"

VERSION="$(sed -n 's/^project(renderham VERSION \([0-9][0-9.]*\).*/\1/p' ../../CMakeLists.txt)"

xgettext --keyword=_ --from-code=UTF-8 \
    --package-name=renderHAM --package-version="$VERSION" --copyright-holder="rawbit (@ravvbit)" \
    -o po/renderhamgui.pot *.cpp

# Replace the generic xgettext boilerplate in the template's header
sed -i -e 's/^# SOME DESCRIPTIVE TITLE\.$/# renderHAMgui translation template./' \
       -e 's/^# Copyright (C) YEAR rawbit (@ravvbit)$/# Copyright (c) 2026 rawbit (@ravvbit)/' \
       -e 's/^# This file is distributed under the same license as the renderHAM package\.$/# This file is distributed under the same license as the renderHAM package (MIT)./' \
       -e '/^# FIRST AUTHOR <EMAIL@ADDRESS>, YEAR\.$/d' \
       -e '/^#, fuzzy$/d' po/renderhamgui.pot

for po in po/*.po; do
    msgmerge --update --backup=none "$po" po/renderhamgui.pot
    msgattrib --no-obsolete -o "$po" "$po"
    msgfmt --statistics -o /dev/null "$po"
done
