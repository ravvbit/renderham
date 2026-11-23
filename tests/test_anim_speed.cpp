// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Playback speed of an ANIM file: fps -> jiffies, and what ends up in the ANHD chunks.
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "aic/aic.hpp"

using namespace aic;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

// The reltime field of every ANHD chunk in an ANIM file, in file order.
// ANHD data: operation(1) mask(1) w(2) h(2) x(2) y(2) abstime(4) reltime(4) ...
std::vector<uint32_t> anhd_reltimes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<uint32_t> out;
    auto be32 = [&](size_t at) {
        return (uint32_t(d[at]) << 24) | (uint32_t(d[at + 1]) << 16) | (uint32_t(d[at + 2]) << 8) | d[at + 3];
    };
    for (size_t i = 0; i + 12 <= d.size(); ++i) {
        if (d[i] == 'A' && d[i + 1] == 'N' && d[i + 2] == 'H' && d[i + 3] == 'D' && be32(i + 4) == 40) {
            out.push_back(be32(i + 8 + 14));
            i += 8 + 40 - 1;
        }
    }
    return out;
}

HAM6Frame make_frame(int shift) {
    HAM6Frame f;
    f.width = 32;
    f.height = 16;
    f.palette.colors.resize(16);
    int n = 0;
    for (AmigaColor& c : f.palette.colors) {
        c = AmigaColor{uint8_t(n), uint8_t(15 - n), uint8_t((n * 5) & 15)};
        ++n;
    }
    f.pixel_codes.resize(32 * 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 32; ++x) f.pixel_codes[y * 32 + x] = uint8_t(((x + shift) / 2 + y) & 15);
    return f;
}

}  // namespace

int main() {
    std::printf("fps -> jiffies\n");
    struct Row { int fps, jiffies; };
    const Row table[] = {{60, 1}, {30, 2}, {20, 3}, {15, 4}, {12, 5}, {10, 6}, {6, 10}, {5, 12}, {4, 15},
                         {3, 20}, {2, 30}, {1, 60}};
    for (const Row& r : table)
        check(anim_jiffies_for_fps(r.fps) == r.jiffies,
              std::to_string(r.fps) + " fps = " + std::to_string(r.jiffies) + " jiffies");
    check(anim_jiffies_for_fps(25) == 2, "25 fps: nearest representable is 30 fps (2 jiffies)");
    check(anim_jiffies_for_fps(100) == 1 && anim_jiffies_for_fps(0) == 60 && anim_jiffies_for_fps(-5) == 60,
          "out-of-range speeds are clamped to 1..60 fps");
    for (int fps = 1; fps <= 60; ++fps) {
        int j = anim_jiffies_for_fps(fps);
        if (j < 1) { check(false, "jiffies >= 1 for every fps"); break; }
    }

    std::printf("ANHD reltime in the written file\n");
    const std::vector<HAM6Frame> frames = {make_frame(0), make_frame(2), make_frame(4), make_frame(6)};
    const std::string path = std::string(RENDERHAM_TMPDIR) + "/anim_speed.anim";
    for (int fps : {60, 30, 10, 2, 1}) {
        AnimWriteOptions o;
        o.reltime_jiffies = anim_jiffies_for_fps(fps);
        std::string err;
        bool ok = write_ham6_anim(frames, path, o, &err);
        std::vector<uint32_t> rel = anhd_reltimes(path);
        bool all = rel.size() == frames.size() - 1;
        for (uint32_t r : rel) all = all && r == uint32_t(anim_jiffies_for_fps(fps));
        if (!ok) std::printf("    write error: %s\n", err.c_str());
        check(ok && all, std::to_string(fps) + " fps: " + std::to_string(rel.size()) + " ANHD chunks, all reltime=" +
                             std::to_string(anim_jiffies_for_fps(fps)));
    }
    {
        AnimWriteOptions o;  // untouched default: the traditional 30 fps, as before
        std::string err;
        write_ham6_anim(frames, path, o, &err);
        std::vector<uint32_t> rel = anhd_reltimes(path);
        check(!rel.empty() && rel[0] == 2, "default options still write reltime 2 (30 fps)");
    }
    std::filesystem::remove(path);

    if (g_failures) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall anim speed checks passed\n");
    return 0;
}
