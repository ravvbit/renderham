// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "SettingsDialog.hpp"

#include <wx/intl.h>

#include <cstddef>

SettingsDialog::SettingsDialog(wxWindow* parent, const wxString& current_lang_code,
                               const wxColour& transparent_background)
    : wxDialog(parent, wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE) {
    languages_ = DiscoverAvailableLanguages();

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(this, wxID_ANY, _("Language")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

    wxArrayString labels;
    int select_index = 0;
    for (size_t i = 0; i < languages_.size(); ++i) {
        labels.Add(languages_[i].display_name);
        if (languages_[i].code == current_lang_code) select_index = static_cast<int>(i);
    }
    language_choice_ = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, labels);
    language_choice_->SetSelection(select_index);
    row->Add(language_choice_, 1);

    root->Add(row, 0, wxEXPAND | wxALL, 12);
    root->Add(new wxStaticText(this, wxID_ANY,
                                _("Changing the language restarts the application.")),
              0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* bg_row = new wxBoxSizer(wxHORIZONTAL);
    bg_row->Add(new wxStaticText(this, wxID_ANY, _("Background for transparent images")), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    background_picker_ = new wxColourPickerCtrl(this, wxID_ANY, transparent_background);
    background_picker_->SetToolTip(_("Transparent areas of the source image (e.g. PNG) are blended over this color."));
    bg_row->Add(background_picker_, 0);
    root->Add(bg_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    root->Add(new wxStaticText(this, wxID_ANY, _("Changing it reloads the current image.")), 0,
              wxLEFT | wxRIGHT | wxBOTTOM, 12);
    root->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);

    SetSizerAndFit(root);
    CentreOnParent();
}

wxColour SettingsDialog::GetTransparentBackground() const { return background_picker_->GetColour(); }

wxString SettingsDialog::GetSelectedLanguageCode() const {
    int sel = language_choice_->GetSelection();
    if (sel == wxNOT_FOUND || static_cast<size_t>(sel) >= languages_.size()) return "en";
    return languages_[static_cast<size_t>(sel)].code;
}
