// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/palette_io.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "aic/ham6_encoder.hpp"
#include "aic/ilbm_writer.hpp"

namespace aic {
namespace {

uint32_t read_u32_be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void fail(std::string* error_out, const std::string& msg) {
    if (error_out) *error_out = msg;
}

// 8-bit -> 4-bit: the inverse of the v*17 this project writes. Rounds, so values authored
// elsewhere (which are not multiples of 17) land on the nearest Amiga value.
uint8_t to4bit(int v8) {
    return static_cast<uint8_t>(std::clamp(static_cast<int>(std::lround(v8 / 17.0)), 0, 15));
}

constexpr size_t kPaletteColors = 16;  // HAM6's base palette

std::string wrong_count_message(size_t n) {
    return "Expected a 16-color palette (HAM6's base palette), file has " + std::to_string(n);
}

// --- text formats -----------------------------------------------------------------------------

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// The lines of a text file, without line ends (\n, \r\n and a lone \r all end a line) and
// without a UTF-8 byte order mark.
std::vector<std::string> split_lines(const std::vector<uint8_t>& data) {
    std::string text(data.begin(), data.end());
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    std::vector<std::string> lines;
    std::string line;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n' || c == '\r') {
            lines.push_back(line);
            line.clear();
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        } else {
            line += c;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

// Parses an integer that must be the whole of `token`.
bool parse_whole_int(const std::string& token, int* out) {
    if (token.empty() || token.size() > 9) return false;
    size_t i = 0;
    if (token[0] == '+' || token[0] == '-') i = 1;
    if (i == token.size()) return false;
    for (size_t k = i; k < token.size(); ++k)
        if (!std::isdigit(static_cast<unsigned char>(token[k]))) return false;
    *out = std::stoi(token);
    return true;
}

// "R G B" (JASC; nothing may follow) or "R G B name" (GIMP; the rest of the line is the name).
bool parse_color_line(const std::string& line, bool allow_name, AmigaColor* out, std::string* why) {
    std::istringstream in(line);
    int v[3];
    for (int k = 0; k < 3; ++k) {
        std::string token;
        if (!(in >> token) || !parse_whole_int(token, &v[k])) {
            *why = "expected three whole numbers (red green blue), found '" + trim(line) + "'";
            return false;
        }
        if (v[k] < 0 || v[k] > 255) {
            *why = "the color value " + token + " is outside 0-255";
            return false;
        }
    }
    std::string rest;
    if (!allow_name && (in >> rest)) {
        *why = "unexpected text after the three color values: '" + rest + "'";
        return false;
    }
    *out = AmigaColor{to4bit(v[0]), to4bit(v[1]), to4bit(v[2])};
    return true;
}

std::string line_error(size_t line_index, const std::string& why) {
    return "Line " + std::to_string(line_index + 1) + ": " + why;
}

bool parse_jasc(const std::vector<std::string>& lines, Palette* out, std::string* error) {
    // JASC-PAL / 0100 / <count> / <count> lines "R G B"
    if (lines.size() < 3) {
        fail(error, "Truncated JASC palette (expected the header lines JASC-PAL, 0100 and the color count)");
        return false;
    }
    if (trim(lines[1]) != "0100") {
        fail(error, line_error(1, "unsupported JASC palette version '" + trim(lines[1]) + "' (expected 0100)"));
        return false;
    }
    int count = 0;
    if (!parse_whole_int(trim(lines[2]), &count) || count < 0) {
        fail(error, line_error(2, "expected the number of colors, found '" + trim(lines[2]) + "'"));
        return false;
    }
    if (static_cast<size_t>(count) != kPaletteColors) {
        fail(error, wrong_count_message(static_cast<size_t>(count)));
        return false;
    }
    Palette result;
    size_t i = 3;
    for (; i < lines.size() && result.colors.size() < kPaletteColors; ++i) {
        if (trim(lines[i]).empty()) continue;
        AmigaColor c;
        std::string why;
        if (!parse_color_line(lines[i], false, &c, &why)) {
            fail(error, line_error(i, why));
            return false;
        }
        result.colors.push_back(c);
    }
    if (result.colors.size() != kPaletteColors) {
        fail(error, "Truncated JASC palette: the header announces 16 colors, the file has " +
                        std::to_string(result.colors.size()));
        return false;
    }
    for (; i < lines.size(); ++i) {
        if (!trim(lines[i]).empty()) {
            fail(error, line_error(i, "more colors than the 16 the header announces"));
            return false;
        }
    }
    *out = result;
    return true;
}

bool parse_gimp(const std::vector<std::string>& lines, Palette* out, std::string* error) {
    // "GIMP Palette", then any of: "Name: ...", "Columns: n", "# comment", blank lines,
    // and one "R G B name" line per color.
    Palette result;
    int total = 0;
    for (size_t i = 1; i < lines.size(); ++i) {
        const std::string t = trim(lines[i]);
        if (t.empty() || t[0] == '#') continue;
        if (t.rfind("Name:", 0) == 0 || t.rfind("Columns:", 0) == 0) continue;
        AmigaColor c;
        std::string why;
        if (!parse_color_line(t, true, &c, &why)) {
            fail(error, line_error(i, why));
            return false;
        }
        ++total;
        if (result.colors.size() < kPaletteColors) result.colors.push_back(c);
    }
    if (total != static_cast<int>(kPaletteColors)) {
        fail(error, wrong_count_message(static_cast<size_t>(total)));
        return false;
    }
    *out = result;
    return true;
}

// --- writing the text formats ---------------------------------------------------------------

int to8bit(uint8_t v4) { return v4 * 17; }

std::string file_stem(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) name.erase(dot);
    // The name goes into a one-line header: no line breaks.
    for (char& c : name)
        if (c == '\n' || c == '\r') c = ' ';
    return name.empty() ? "renderHAM" : name;
}

bool write_text_file(const std::string& path, const std::string& text, std::string* error_out) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        fail(error_out, "Could not create file: " + path);
        return false;
    }
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    f.flush();
    if (!f) {
        fail(error_out, "Could not write file: " + path);
        return false;
    }
    return true;
}

std::string amiga_hex(const AmigaColor& c) {
    static const char* digits = "0123456789ABCDEF";
    return std::string("$") + digits[c.r4 & 15] + digits[c.g4 & 15] + digits[c.b4 & 15];
}

}  // namespace

const std::vector<PaletteFormatInfo>& supported_palette_formats() {
    static const std::vector<PaletteFormatInfo> formats = {
        {PaletteFileFormat::Ilbm, "IFF ILBM", {"iff", "ilbm", "lbm"}},
        {PaletteFileFormat::JascPal, "JASC palette (Paint Shop Pro)", {"pal"}},
        {PaletteFileFormat::GimpGpl, "GIMP palette", {"gpl"}},
    };
    return formats;
}

PaletteFileFormat palette_format_for_path(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return PaletteFileFormat::Ilbm;
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const PaletteFormatInfo& f : supported_palette_formats())
        for (const std::string& e : f.extensions)
            if (e == ext) return f.id;
    return PaletteFileFormat::Ilbm;
}

bool save_palette(const Palette& palette, const std::string& path, std::string* error_out) {
    return save_palette(palette, path, palette_format_for_path(path), error_out);
}

bool save_palette(const Palette& palette, const std::string& path, PaletteFileFormat format, std::string* error_out) {
    if (palette.colors.size() != kPaletteColors) {
        fail(error_out, "A HAM6 palette has 16 colors, this one has " + std::to_string(palette.colors.size()));
        return false;
    }
    if (format == PaletteFileFormat::JascPal) {
        std::string text = "JASC-PAL\r\n0100\r\n16\r\n";
        for (const AmigaColor& c : palette.colors)
            text += std::to_string(to8bit(c.r4)) + " " + std::to_string(to8bit(c.g4)) + " " +
                    std::to_string(to8bit(c.b4)) + "\r\n";
        return write_text_file(path, text, error_out);
    }
    if (format == PaletteFileFormat::GimpGpl) {
        std::string text = "GIMP Palette\nName: " + file_stem(path) + "\nColumns: 4\n# Amiga HAM6 base palette (16 colors), written by renderHAM\n";
        for (const AmigaColor& c : palette.colors) {
            char line[64];
            std::snprintf(line, sizeof line, "%3d %3d %3d\t", to8bit(c.r4), to8bit(c.g4), to8bit(c.b4));
            text += std::string(line) + amiga_hex(c) + "\n";
        }
        return write_text_file(path, text, error_out);
    }
    // A trivial 1x1 HAM6Frame carrying only this palette, written
    // through the exact same writer real output uses - deliberately
    // not a bespoke "palette file" format, so the result is a genuine,
    // openable ILBM (a real Amiga viewer just sees a 1x1 picture).
    HAM6Frame frame;
    frame.width = 1;
    frame.height = 1;
    frame.palette = palette;
    frame.pixel_codes = {0};  // single pixel, select-mode -> palette index 0
    return write_ham6_ilbm(frame, path, IlbmWriteOptions{}, error_out);
}

bool load_palette(const std::string& path, Palette* out, std::string* error_out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fail(error_out, "Could not open file: " + path);
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    // The content decides the format. The two text formats are tiny (16 colors); a file bigger
    // than a few KB is not one of them, and is not read line by line.
    if (data.size() >= 4 && std::memcmp(data.data(), "FORM", 4) != 0) {
        const bool bom = data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF;
        const char* text = reinterpret_cast<const char*>(data.data()) + (bom ? 3 : 0);
        const size_t text_len = data.size() - (bom ? 3 : 0);
        const bool jasc = text_len >= 8 && std::memcmp(text, "JASC-PAL", 8) == 0;
        const bool gimp = text_len >= 12 && std::memcmp(text, "GIMP Palette", 12) == 0;
        if (jasc || gimp) {
            if (data.size() > 64 * 1024) {
                fail(error_out, "File too large for a palette file (" + std::to_string(data.size()) + " bytes)");
                return false;
            }
            const std::vector<std::string> lines = split_lines(data);
            Palette parsed;
            if (!(jasc ? parse_jasc(lines, &parsed, error_out) : parse_gimp(lines, &parsed, error_out))) return false;
            *out = parsed;
            return true;
        }
    }

    if (data.size() < 12 || std::memcmp(data.data(), "FORM", 4) != 0) {
        fail(error_out, "Not a palette file (an IFF ILBM, a JASC palette or a GIMP palette is expected)");
        return false;
    }
    if (std::memcmp(data.data() + 8, "ILBM", 4) != 0) {
        fail(error_out, "Not an ILBM file");
        return false;
    }

    size_t pos = 12;
    while (pos + 8 <= data.size()) {
        const uint8_t* chunk_id = data.data() + pos;
        uint32_t chunk_size = read_u32_be(data.data() + pos + 4);
        size_t data_start = pos + 8;
        if (data_start + chunk_size > data.size()) {
            fail(error_out, "Truncated chunk in file");
            return false;
        }
        if (std::memcmp(chunk_id, "CMAP", 4) == 0) {
            if (chunk_size % 3 != 0) {
                fail(error_out, "Malformed CMAP chunk (size not a multiple of 3)");
                return false;
            }
            size_t num_colors = chunk_size / 3;
            if (num_colors != kPaletteColors) {
                fail(error_out, wrong_count_message(num_colors));
                return false;
            }
            Palette result;
            result.colors.reserve(16);
            for (size_t i = 0; i < 16; ++i) {
                const uint8_t* rgb = data.data() + data_start + i * 3;
                // Inverse of the nibble-replication (v*17) this project's own writer uses; see
                // to4bit() for the rounding of genuine real-Amiga-authored CMAP values.
                result.colors.push_back(AmigaColor{to4bit(rgb[0]), to4bit(rgb[1]), to4bit(rgb[2])});
            }
            *out = result;
            return true;
        }
        pos = data_start + chunk_size + (chunk_size % 2);  // chunks are word-aligned
    }

    fail(error_out, "No CMAP chunk found in file");
    return false;
}

}  // namespace aic
