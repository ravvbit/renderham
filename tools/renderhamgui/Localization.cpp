// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "Localization.hpp"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>

wxString GetTranslationsDir() {
    wxFileName exe_dir(wxStandardPaths::Get().GetExecutablePath());
    return exe_dir.GetPathWithSep() + "translations";
}

std::vector<LanguageOption> DiscoverAvailableLanguages() {
    std::vector<LanguageOption> result;
    result.push_back({"en", "English"});

    wxString root = GetTranslationsDir();
    if (!wxDir::Exists(root)) return result;

    wxDir dir(root);
    if (!dir.IsOpened()) return result;

    wxString sub;
    bool cont = dir.GetFirst(&sub, wxEmptyString, wxDIR_DIRS);
    while (cont) {
        wxString catalog_path = root + wxFileName::GetPathSeparator() + sub + wxFileName::GetPathSeparator() +
                                 "LC_MESSAGES" + wxFileName::GetPathSeparator() + "renderhamgui.mo";
        if (wxFileName::FileExists(catalog_path)) {
            const wxLanguageInfo* info = wxLocale::FindLanguageInfo(sub);
            wxString display = info ? info->Description : sub;
            result.push_back({sub, display});
        }
        cont = dir.GetNext(&sub);
    }
    return result;
}
