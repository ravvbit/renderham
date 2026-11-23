// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include <wx/cmdline.h>
#include <wx/config.h>
#include <wx/filename.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>

#include <memory>

#include "Localization.hpp"
#include "MainFrame.hpp"

class AicGuiApp : public wxApp {
public:
    void OnInitCmdLine(wxCmdLineParser& parser) override {
        wxApp::OnInitCmdLine(parser);
        static const wxCmdLineEntryDesc extra[] = {
            {wxCMD_LINE_OPTION, "", "lang", "UI language (e.g. en, de). Overrides the saved setting.",
             wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL},
            {wxCMD_LINE_NONE, nullptr, nullptr, nullptr, wxCMD_LINE_VAL_NONE, 0},
        };
        parser.SetDesc(extra);
        parser.AddParam("Image file to load on startup", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override {
        if (!wxApp::OnCmdLineParsed(parser)) return false;
        parser.Found("lang", &lang_code_override_);
        if (parser.GetParamCount() > 0) initial_path_ = parser.GetParam(0);
        return true;
    }

    bool OnInit() override {
        SetAppName("renderHAMgui");
        SetVendorName("renderHAM");
        if (!wxApp::OnInit()) return false;
        InitLocale();
        auto* frame = initial_path_.IsEmpty() ? new MainFrame() : new MainFrame(initial_path_);
        frame->Show(true);
        return true;
    }

private:
    // Precedence: --lang on the command line (explicit override, e.g.
    // for testing) > previously saved Settings-dialog choice > English.
    // English is the source language every _() string is written in, so
    // it needs no catalog at all - only non-English UI languages load a
    // translations/<lang>/LC_MESSAGES/renderhamgui.mo catalog. Missing/broken
    // catalogs fail silently and _() just returns the English source
    // text, so an incomplete or absent translation never breaks the UI.
    void InitLocale() {
        wxString requested = lang_code_override_;
        if (requested.IsEmpty()) requested = wxConfig::Get()->Read("Language", "en");

        wxLanguage language = wxLANGUAGE_ENGLISH;
        if (!requested.IsEmpty() && requested != "en") {
            const wxLanguageInfo* info = wxLocale::FindLanguageInfo(requested);
            if (info) {
                language = static_cast<wxLanguage>(info->Language);
            } else {
                fprintf(stderr, "Unknown language '%s', falling back to English\n", requested.mb_str().data());
            }
        }

        locale_ = std::make_unique<wxLocale>();
        locale_->Init(language);
        locale_->AddCatalogLookupPathPrefix(GetTranslationsDir());
        locale_->AddCatalog("renderhamgui");
    }

    wxString initial_path_;
    wxString lang_code_override_;
    std::unique_ptr<wxLocale> locale_;  // must outlive the app - destroying it reverts translations
};

wxIMPLEMENT_APP(AicGuiApp);
