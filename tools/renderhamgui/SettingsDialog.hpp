// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// SettingsDialog.hpp
//
// Language picker and the background color for transparent images; its own
// dialog class so further settings have an obvious home without touching MainFrame.

#include <wx/clrpicker.h>
#include <wx/wx.h>

#include <vector>

#include "Localization.hpp"

class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, const wxString& current_lang_code, const wxColour& transparent_background);

    // Valid after ShowModal() returns wxID_OK.
    wxString GetSelectedLanguageCode() const;
    wxColour GetTransparentBackground() const;

private:
    std::vector<LanguageOption> languages_;
    wxChoice* language_choice_ = nullptr;
    wxColourPickerCtrl* background_picker_ = nullptr;
};
