// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// UTF-8 conversions between wxString and std::string. The library takes and gives UTF-8 text;
// wxString::utf8_string() and the std::string overload of wxString::FromUTF8() only exist since
// wxWidgets 3.1, and the GUI builds with 3.0 as well.

#include <wx/string.h>

#include <string>

inline std::string ToUtf8(const wxString& text) {
    const wxScopedCharBuffer buffer = text.utf8_str();
    return std::string(buffer.data(), buffer.length());
}

inline wxString FromUtf8(const std::string& text) { return wxString::FromUTF8(text.data(), text.size()); }
