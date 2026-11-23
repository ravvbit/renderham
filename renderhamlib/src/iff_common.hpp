// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/iff_common.hpp - INTERNAL shared helpers for the IFF writers
// (ilbm_writer.cpp, anim_writer.cpp). Not part of the public API - not
// installed, not included by aic.hpp. Kept separate purely so the ANIM
// writer's frame-two-onward delta encoding operates on the exact same
// planar bitplane byte layout, and the exact same ByteRun1 compressor,
// that the existing single-image ILBM writer already uses and this
// project has already verified - a second, drifted copy of either
// would be a correctness risk for no benefit.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace aic {
namespace iff {

// Amiga bitplane rows are always word-aligned (padded to a multiple of
// 16 pixels).
int row_bytes_for(int width);

void put_u8(std::vector<uint8_t>& b, uint8_t v);
void put_u16(std::vector<uint8_t>& b, uint16_t v);
void put_i16(std::vector<uint8_t>& b, int16_t v);
void put_u32(std::vector<uint8_t>& b, uint32_t v);
void put_bytes(std::vector<uint8_t>& b, const std::vector<uint8_t>& d);
void put_id(std::vector<uint8_t>& b, const char* id4);

// Wraps `data` as a complete IFF chunk: 4-byte ID, 4-byte big-endian
// size, the data itself, and a zero pad byte if size is odd (IFF
// chunks are word-aligned).
std::vector<uint8_t> make_chunk(const char* id4, const std::vector<uint8_t>& data);

// One byte per pixel (HAM6 code, 0..63) -> interleaved Amiga bitplanes.
// Row order matches "ILBM" (InterLeaved BitMap): for each scanline,
// each of the planes in turn, each row word-aligned. Output size is
// exactly row_bytes_for(width) * num_planes * height.
std::vector<uint8_t> chunky_to_planar(const std::vector<uint8_t>& codes, int width, int height, int num_planes);

// IFF "ByteRun1" (PackBits-style) RLE compression of a SINGLE row. Must
// never be called across a row boundary - see byterun1_compress_planar.
std::vector<uint8_t> byterun1_compress(const std::vector<uint8_t>& src);

// Compresses a full interleaved bitplane buffer (as produced by
// chunky_to_planar) one row at a time, since real ILBM readers
// decompress row-by-row and expect each row's compressed stream to
// decode to exactly row_bytes output bytes before resetting.
std::vector<uint8_t> byterun1_compress_planar(const std::vector<uint8_t>& planar, int width, int height,
                                               int num_planes);

}  // namespace iff
}  // namespace aic
