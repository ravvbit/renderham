// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// INTERNAL helper (not installed): opening files whose names contain non-ASCII characters.
//
// The library's API takes paths as UTF-8 std::strings (what the GUI, the command line and most
// libraries hand around). On Linux and macOS the file system takes those bytes as they are. On
// Windows the narrow-character file functions read a name in the ANSI code page instead, so a
// file called "Bücher.png" could not be opened; there, the name is converted to UTF-16 first.

#include <filesystem>
#include <string>

namespace aic {
namespace detail {

inline std::filesystem::path utf8_path(const std::string& utf8) {
#ifdef _WIN32
    return std::filesystem::u8path(utf8);
#else
    return std::filesystem::path(utf8);
#endif
}

}  // namespace detail
}  // namespace aic
