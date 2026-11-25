// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "FileFilters.hpp"

#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/intl.h>

#include <algorithm>
#include <cstddef>

namespace gui {

namespace {

// "*.jpg" as shown to the person.
wxString ShownPattern(const std::string& ext) { return "*." + wxString::FromUTF8(ext.c_str()); }

// "*.jpg" for the dialog: on GTK "*.[jJ][pP][gG]" (see the header).
wxString DialogPattern(const std::string& ext) {
#ifdef __WXGTK__
    wxString pattern = "*.";
    for (const char c : ext) {
        const wxString lower(1, static_cast<wxChar>(c));
        pattern += "[" + lower + lower.Upper() + "]";
    }
    return pattern;
#else
    return ShownPattern(ext);
#endif
}

// "Title (*.a;*.b)|<patterns>" for several extensions, ";"-separated as wxFileDialog wants.
// `shown` and `patterns` are appended to, so several formats can share one filter.
void AppendExtensions(const std::vector<std::string>& extensions, wxString* shown, wxString* patterns) {
    for (const std::string& ext : extensions) {
        if (!shown->empty()) {
            *shown += ";";
            *patterns += ";";
        }
        *shown += ShownPattern(ext);
        *patterns += DialogPattern(ext);
    }
}

wxString AllFilesFilter() { return wxString::Format("%s|%s", _("All Files"), wxFileSelectorDefaultWildcardStr); }

wxString Filter(const wxString& title, const wxString& shown, const wxString& patterns) {
    return wxString::Format("%s (%s)|%s", title, shown, patterns);
}

}  // namespace

wxString ImageFileWildcard() {
    wxString shown, patterns;
    for (const aic::ImageFormat& format : aic::supported_image_formats())
        AppendExtensions(format.extensions, &shown, &patterns);
    return Filter(_("Images"), shown, patterns) + "|" + AllFilesFilter();
}

wxString PaletteFormatName(aic::PaletteFileFormat format) {
    switch (format) {
        case aic::PaletteFileFormat::Ilbm: return _("Amiga IFF ILBM");
        case aic::PaletteFileFormat::JascPal: return _("JASC palette (Paint Shop Pro)");
        case aic::PaletteFileFormat::GimpGpl: return _("GIMP palette");
    }
    return wxString();
}

wxString PaletteLoadWildcard() {
    wxString all_shown, all_patterns;
    wxString per_format;
    for (const aic::PaletteFormatInfo& format : aic::supported_palette_formats()) {
        AppendExtensions(format.extensions, &all_shown, &all_patterns);
        wxString shown, patterns;
        AppendExtensions(format.extensions, &shown, &patterns);
        per_format += "|" + Filter(PaletteFormatName(format.id), shown, patterns);
    }
    return Filter(_("All palette files"), all_shown, all_patterns) + per_format + "|" + AllFilesFilter();
}

wxString PaletteSaveWildcard() {
    wxString out;
    for (const aic::PaletteFormatInfo& format : aic::supported_palette_formats()) {
        wxString shown, patterns;
        AppendExtensions(format.extensions, &shown, &patterns);
        if (!out.empty()) out += "|";
        out += Filter(PaletteFormatName(format.id), shown, patterns);
    }
    return out;
}

wxString PaletteSavePath(const wxString& typed, int filter_index) {
    const wxString ext = wxFileName(typed).GetExt().Lower();
    const auto& formats = aic::supported_palette_formats();
    if (!ext.empty()) {
        for (const aic::PaletteFormatInfo& format : formats)
            for (const std::string& known : format.extensions)
                if (ext == wxString::FromUTF8(known.c_str())) return typed;
    }
    if (filter_index < 0 || static_cast<size_t>(filter_index) >= formats.size()) filter_index = 0;
    const aic::PaletteFormatInfo& chosen = formats[static_cast<size_t>(filter_index)];
    // A name that ends in a dot ("palette.") gets the extension without a second one.
    return typed + (typed.EndsWith(".") ? "" : ".") + wxString::FromUTF8(chosen.extensions.front().c_str());
}

}  // namespace gui
