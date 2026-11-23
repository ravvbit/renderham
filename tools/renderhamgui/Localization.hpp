// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// Localization.hpp
//
// Small shared helper: where translation catalogs live, and which
// languages are actually available at runtime. Both main.cpp (choosing
// the startup language) and SettingsDialog (populating the picker) use
// this, so adding a new language later is just "drop a .mo file in the
// right place" - nothing here needs to change.

#include <wx/wx.h>

#include <vector>

struct LanguageOption {
    wxString code;          // "en", "de", ... (wxLocale canonical name)
    wxString display_name;  // "English", "Deutsch", ...
};

// <directory containing the running executable>/translations
wxString GetTranslationsDir();

// Always includes English first (the source language, needs no catalog
// file to exist). Any other entry is only included if a real
// <dir>/<code>/LC_MESSAGES/renderhamgui.mo catalog is present on disk.
std::vector<LanguageOption> DiscoverAvailableLanguages();
