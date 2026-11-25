// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "AboutDialog.hpp"

#include "aic/version.hpp"

AboutDialog::AboutDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("About"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, _("renderHAMgui"));
    wxFont title_font = title->GetFont();
    title_font.SetPointSize(title_font.GetPointSize() + 4);
    title_font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(title_font);
    root->Add(title, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxLEFT | wxRIGHT, 12);
    root->Add(new wxStaticText(this, wxID_ANY, wxString::Format(_("Version %s"), RENDERHAM_VERSION)), 0,
              wxALIGN_CENTER_HORIZONTAL | wxALL, 6);

    root->Add(new wxStaticText(this, wxID_ANY,
                                _("Converts photos and other images into Amiga HAM6\n"
                                  "IFF/ILBM files - with palette control, dithering,\n"
                                  "cropping, and a Ken Burns-style animation preview\n"
                                  "and export.")),
              0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    // The origin of the code is stated plainly, not tucked away in a credits line.
    auto* ai_note = new wxStaticText(this, wxID_ANY,
                                      _("AI-generated software: the code was written by Claude (Anthropic)\n"
                                        "under the direction of rawbit (@ravvbit)."));
    wxFont ai_font = ai_note->GetFont();
    ai_font.SetWeight(wxFONTWEIGHT_BOLD);
    ai_note->SetFont(ai_font);
    root->Add(ai_note, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    root->Add(new wxStaticText(this, wxID_ANY,
                                wxString(RENDERHAM_COPYRIGHT) + "\n" + _("Released under the MIT License.")),
              0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* shortcuts_box = new wxStaticBoxSizer(wxVERTICAL, this, _("Keyboard shortcuts"));
    auto* grid = new wxFlexGridSizer(2, 4, 12);
    struct Shortcut {
        const char* key;
        const wxString description;
    };
    const Shortcut shortcuts[] = {
        {"Ctrl+O", _("Open an image")},
        {"Ctrl+S", _("Save to the target directory")},
        {"Ctrl+Shift+S", _("Save with a file picker")},
        {"F5", _("Convert / play preview")},
        {"F6", _("Cycle preview view (preview / HAM map / error map / original)")},
        {"F7", _("Switch between the original (before HAM) and the previous view")},
        {"Esc", _("Cancel the running conversion or export")},
        {"Ctrl+L / Ctrl+R", _("Rotate the source image left / right")},
        {"Ctrl+Shift+H / Ctrl+Shift+V", _("Mirror the source image horizontally / vertically")},
        {"Ctrl+K", _("Add keyframe")},
        {"Ctrl+Q", _("Quit")},
        {"Ctrl+= / Ctrl+-", _("Zoom preview in/out")},
        {"Ctrl+9 / Ctrl+0", _("Zoom preview to fit / 100%")},
    };
    for (const auto& s : shortcuts) {
        auto* key_label = new wxStaticText(shortcuts_box->GetStaticBox(), wxID_ANY, s.key);
        wxFont bold_font = key_label->GetFont();
        bold_font.SetWeight(wxFONTWEIGHT_BOLD);
        key_label->SetFont(bold_font);
        grid->Add(key_label, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(shortcuts_box->GetStaticBox(), wxID_ANY, s.description), 0,
                  wxALIGN_CENTER_VERTICAL);
    }
    shortcuts_box->Add(grid, 0, wxALL, 8);
    root->Add(shortcuts_box, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    root->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxALL, 8);

    SetSizerAndFit(root);
    CentreOnParent();
}
