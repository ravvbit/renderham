// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// AboutDialog.hpp
//
// Simple credits/info dialog, opened via the "About" button (see
// MainFrame::OnAbout).

#include <wx/wx.h>

class AboutDialog : public wxDialog {
public:
    explicit AboutDialog(wxWindow* parent);
};
