// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// The image formats Image::load() reads (aic::supported_image_formats()):
//  - every listed format has a sample file here and loads with the right pixels,
//  - the table is well-formed (lower-case extensions without dots, no duplicates),
//  - the desktop entry of the Linux packages names exactly the MIME types of the table.
// The sample files are written by hand, byte by byte, so the test does not depend on any
// encoder: what is checked is the decoder's reading of the format as specified.

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "aic/aic.hpp"

namespace {

int g_failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

using Bytes = std::vector<uint8_t>;

void put16le(Bytes& b, unsigned v) { b.push_back(v & 0xff); b.push_back((v >> 8) & 0xff); }
void put32le(Bytes& b, unsigned v) { put16le(b, v & 0xffff); put16le(b, v >> 16); }
void put16be(Bytes& b, unsigned v) { b.push_back((v >> 8) & 0xff); b.push_back(v & 0xff); }
void put32be(Bytes& b, unsigned v) { put16be(b, v >> 16); put16be(b, v & 0xffff); }
void put_text(Bytes& b, const std::string& s) { b.insert(b.end(), s.begin(), s.end()); }

// A 2x2 test picture: red, green over blue, white.
struct Px { uint8_t r, g, b; };
const Px kRgb2x2[4] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};

Bytes make_bmp() {  // 24 bit, bottom-up, rows padded to 4 bytes
    Bytes b;
    put_text(b, "BM");
    put32le(b, 54 + 2 * 8);
    put32le(b, 0);
    put32le(b, 54);
    put32le(b, 40); put32le(b, 2); put32le(b, 2);
    put16le(b, 1); put16le(b, 24);
    put32le(b, 0); put32le(b, 2 * 8); put32le(b, 2835); put32le(b, 2835); put32le(b, 0); put32le(b, 0);
    for (int row : {1, 0}) {  // bottom row first
        for (int x = 0; x < 2; ++x) {
            const Px& p = kRgb2x2[row * 2 + x];
            b.push_back(p.b); b.push_back(p.g); b.push_back(p.r);
        }
        b.push_back(0); b.push_back(0);  // padding
    }
    return b;
}

Bytes make_tga() {  // uncompressed true color, 24 bit, origin top left
    Bytes b = {0, 0, 2, 0, 0, 0, 0, 0};
    put16le(b, 0); put16le(b, 0);  // x/y origin
    put16le(b, 2); put16le(b, 2);
    b.push_back(24);
    b.push_back(0x20);  // top-left origin
    for (const Px& p : kRgb2x2) { b.push_back(p.b); b.push_back(p.g); b.push_back(p.r); }
    return b;
}

// 2x1 picture, palette of four colors, pixels = index 1 (red) and index 2 (green), LZW with
// minimum code size 2: the codes are clear(4) 1 2 end(5), three bits each, least significant bit first.
Bytes make_gif() {
    Bytes b;
    put_text(b, "GIF89a");
    put16le(b, 2); put16le(b, 1);
    b.push_back(0x80 | 0x01);  // global color table, 2 bits per color -> 4 entries
    b.push_back(0); b.push_back(0);
    const Px pal[4] = {{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
    for (const Px& p : pal) { b.push_back(p.r); b.push_back(p.g); b.push_back(p.b); }
    b.push_back(0x2C);  // image descriptor
    put16le(b, 0); put16le(b, 0); put16le(b, 2); put16le(b, 1);
    b.push_back(0);
    b.push_back(2);     // minimum LZW code size
    b.push_back(2);     // data sub-block of two bytes
    b.push_back(0x8C); b.push_back(0x0A);
    b.push_back(0);     // end of the data
    b.push_back(0x3B);  // trailer
    return b;
}

Bytes make_psd() {  // 8 bit RGB, uncompressed, planar
    Bytes b;
    put_text(b, "8BPS");
    put16be(b, 1);
    for (int i = 0; i < 6; ++i) b.push_back(0);
    put16be(b, 3);   // channels
    put32be(b, 2);   // height
    put32be(b, 2);   // width
    put16be(b, 8);   // bits per channel
    put16be(b, 3);   // color mode: RGB
    put32be(b, 0);   // color mode data
    put32be(b, 0);   // image resources
    put32be(b, 0);   // layer and mask information
    put16be(b, 0);   // compression: raw
    for (int plane = 0; plane < 3; ++plane)
        for (const Px& p : kRgb2x2) b.push_back(plane == 0 ? p.r : plane == 1 ? p.g : p.b);
    return b;
}

Bytes make_ppm() {
    Bytes b;
    put_text(b, "P6\n# a comment\n2 2\n255\n");
    for (const Px& p : kRgb2x2) { b.push_back(p.r); b.push_back(p.g); b.push_back(p.b); }
    return b;
}

const uint8_t kGray2x2[4] = {0, 85, 170, 255};

Bytes make_pgm() {
    Bytes b;
    put_text(b, "P5\n2 2\n255\n");
    for (uint8_t v : kGray2x2) b.push_back(v);
    return b;
}

std::string write_file(const std::string& name, const Bytes& data) {
    const std::string path = std::string(RENDERHAM_TMPDIR) + "/" + name;
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return path;
}

bool near_byte(float v, int expected) { return std::abs(v * 255.f - expected) <= 1.5f; }
bool pixel_is(const aic::Image& img, int x, int y, int r, int g, int b) {
    const aic::RGBf& c = img.at(x, y);
    return near_byte(c.r, r) && near_byte(c.g, g) && near_byte(c.b, b);
}

// Loads `path` and checks the 2x2 (or 2x1) picture; `what` names the format in messages.
void check_loads(const std::string& path, const std::string& what, int h, const Px* expected, const uint8_t* gray = nullptr) {
    std::string err;
    auto img = aic::Image::load(path, &err);
    check(img.has_value(), what + ": loads (" + err + ")");
    if (!img) return;
    check(img->width() == 2 && img->height() == h, what + ": size 2x" + std::to_string(h));
    if (img->width() != 2 || img->height() != h) return;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < 2; ++x) {
            const Px e = gray ? Px{gray[y * 2 + x], gray[y * 2 + x], gray[y * 2 + x]} : expected[y * 2 + x];
            check(pixel_is(*img, x, y, e.r, e.g, e.b),
                  what + ": pixel (" + std::to_string(x) + "," + std::to_string(y) + ") has the right color");
        }
    }
}

// MIME types named by "MimeType=" in the desktop entry.
std::set<std::string> desktop_mime_types(const std::string& path, bool* found_file) {
    std::set<std::string> out;
    std::ifstream f(path);
    *found_file = f.good();
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("MimeType=", 0) != 0) continue;
        std::stringstream ss(line.substr(9));
        std::string item;
        while (std::getline(ss, item, ';'))
            if (!item.empty()) out.insert(item);
    }
    return out;
}

}  // namespace

int main() {
    const auto& formats = aic::supported_image_formats();
    check(!formats.empty(), "the format table is not empty");

    // --- the table itself ------------------------------------------------------------------
    std::set<std::string> extensions, mimes;
    for (const aic::ImageFormat& f : formats) {
        const std::string name = f.name;
        check(!name.empty() && f.mime_type && std::string(f.mime_type).rfind("image/", 0) == 0,
              name + ": has a name and an image/... MIME type");
        check(!f.extensions.empty(), name + ": has at least one extension");
        check(mimes.insert(f.mime_type).second, name + ": MIME type is listed only once");
        for (const std::string& e : f.extensions) {
            check(!e.empty() && e.find('.') == std::string::npos && e.find('*') == std::string::npos,
                  name + ": extension '" + e + "' has no dot and no wildcard");
            bool lower = true;
            for (char c : e) lower = lower && !(c >= 'A' && c <= 'Z');
            check(lower, name + ": extension '" + e + "' is lower case");
            check(extensions.insert(e).second, name + ": extension '" + e + "' is listed only once");
        }
    }

    // --- every listed format loads -----------------------------------------------------------
    // One sample per extension; the table of samples is checked against the format table below,
    // so a format added there without a sample here fails the test.
    std::set<std::string> covered;
    {
        // PNG: round trip through our own writer, exact; JPEG: the sample from the EXIF test.
        aic::Image src(2, 2);
        for (int i = 0; i < 4; ++i)
            src.at(i % 2, i / 2) = aic::RGBf{kRgb2x2[i].r / 255.f, kRgb2x2[i].g / 255.f, kRgb2x2[i].b / 255.f};
        const std::string png = std::string(RENDERHAM_TMPDIR) + "/formats_sample.png";
        check(src.save_png(png), "png: sample written");
        check_loads(png, "png", 2, kRgb2x2);
        covered.insert("png");

        std::string err;
        auto jpg = aic::Image::load(std::string(RENDERHAM_TESTDATA) + "/exif_orient3.jpg", &err);
        check(jpg.has_value() && jpg->width() > 0 && jpg->height() > 0, "jpg/jpeg: sample loads (" + err + ")");
        covered.insert("jpg");
        covered.insert("jpeg");
    }
    check_loads(write_file("formats_sample.bmp", make_bmp()), "bmp", 2, kRgb2x2);
    covered.insert("bmp");
    check_loads(write_file("formats_sample.tga", make_tga()), "tga", 2, kRgb2x2);
    covered.insert("tga");
    check_loads(write_file("formats_sample.gif", make_gif()), "gif", 1, kRgb2x2);  // red, green
    covered.insert("gif");
    check_loads(write_file("formats_sample.psd", make_psd()), "psd", 2, kRgb2x2);
    covered.insert("psd");
    check_loads(write_file("formats_sample.ppm", make_ppm()), "ppm", 2, kRgb2x2);
    covered.insert("ppm");
    check_loads(write_file("formats_sample.pgm", make_pgm()), "pgm", 2, nullptr, kGray2x2);
    covered.insert("pgm");

    check(covered == extensions, "the test has a sample for every listed extension, and no extra ones");

    // The format is taken from the content: a PNG under a misleading name still loads.
    {
        const std::string png = std::string(RENDERHAM_TMPDIR) + "/formats_sample.png";
        std::ifstream in(png, std::ios::binary);
        Bytes data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string odd = write_file("formats_really_a_png.jpg", data);
        check(aic::Image::load(odd).has_value(), "the content decides, not the file name");
    }

    // A file that is no image at all fails with a message, it does not crash.
    {
        std::string err;
        const std::string path = write_file("formats_not_an_image.png", Bytes{'h', 'e', 'l', 'l', 'o'});
        check(!aic::Image::load(path, &err).has_value() && !err.empty(), "a non-image file is refused with a message");
    }

    // --- the desktop entry matches the table ---------------------------------------------------
    {
        bool found = false;
        const std::set<std::string> in_desktop = desktop_mime_types(RENDERHAM_DESKTOP_FILE, &found);
        check(found, "desktop entry found at " RENDERHAM_DESKTOP_FILE);
        check(in_desktop == mimes,
              "MimeType= in the desktop entry lists exactly the MIME types of aic::supported_image_formats()");
        if (in_desktop != mimes) {
            std::cerr << "  desktop entry:";
            for (const auto& m : in_desktop) std::cerr << " " << m;
            std::cerr << "\n  format table: ";
            for (const auto& m : mimes) std::cerr << " " << m;
            std::cerr << "\n";
        }
    }

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "formats: all checks passed\n";
    return 0;
}
