# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
add_executable(test_lookahead test_lookahead.cpp)
# Ditherers/quantizers self-register from their own translation units, see
# cmake/AicWholeArchive.cmake.
aic_link_whole_archive(test_lookahead renderhamlib)
target_compile_features(test_lookahead PRIVATE cxx_std_17)
add_test(NAME lookahead COMMAND test_lookahead)

# Input validation (library + CLI). Runs the real renderHAM binary.
add_executable(test_validation test_validation.cpp)
aic_link_whole_archive(test_validation renderhamlib)
target_compile_features(test_validation PRIVATE cxx_std_17)
add_dependencies(test_validation renderHAM)
target_compile_definitions(test_validation PRIVATE
    RENDERHAM_CLI="$<TARGET_FILE:renderHAM>"
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPFILE="${CMAKE_CURRENT_BINARY_DIR}/validation_out.iff")
add_test(NAME validation COMMAND test_validation)

# EXIF orientation, ImageTransform, alpha blending.
add_executable(test_orientation_alpha test_orientation_alpha.cpp)
aic_link_whole_archive(test_orientation_alpha renderhamlib)
target_compile_features(test_orientation_alpha PRIVATE cxx_std_17)
target_compile_definitions(test_orientation_alpha PRIVATE
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME orientation_alpha COMMAND test_orientation_alpha)

# Progress/cancel and the threaded animation renderer (render_sequence).
add_executable(test_sequence test_sequence.cpp)
aic_link_whole_archive(test_sequence renderhamlib)
target_compile_features(test_sequence PRIVATE cxx_std_17)
target_compile_definitions(test_sequence PRIVATE
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME sequence COMMAND test_sequence)

# ANIM playback speed (fps -> jiffies, ANHD reltime).
add_executable(test_anim_speed test_anim_speed.cpp)
aic_link_whole_archive(test_anim_speed renderhamlib)
target_compile_features(test_anim_speed PRIVATE cxx_std_17)
target_compile_definitions(test_anim_speed PRIVATE RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME anim_speed COMMAND test_anim_speed)

# Dither pattern anchored to the source picture (set_pattern_origin, anchor_dither_to_source).
add_executable(test_anchor_dither test_anchor_dither.cpp)
aic_link_whole_archive(test_anchor_dither renderhamlib)
target_compile_features(test_anchor_dither PRIVATE cxx_std_17)
target_compile_definitions(test_anchor_dither PRIVATE RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata")
add_test(NAME anchor_dither COMMAND test_anchor_dither)

# Perceptual quality figures (blurred error, SSIM), checked against an independent NumPy implementation.
add_executable(test_quality_metrics test_quality_metrics.cpp)
aic_link_whole_archive(test_quality_metrics renderhamlib)
target_compile_features(test_quality_metrics PRIVATE cxx_std_17)
target_compile_definitions(test_quality_metrics PRIVATE
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME quality_metrics COMMAND test_quality_metrics)

# The image formats Image::load() reads; also checks that the desktop entry of the Linux packages
# (MimeType=) names the same formats.
add_executable(test_formats test_formats.cpp)
aic_link_whole_archive(test_formats renderhamlib)
target_compile_features(test_formats PRIVATE cxx_std_17)
target_compile_definitions(test_formats PRIVATE
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}"
    RENDERHAM_DESKTOP_FILE="${CMAKE_SOURCE_DIR}/packaging/linux/io.github.ravvbit.renderham.desktop")
add_test(NAME formats COMMAND test_formats)

# Palette files: ILBM, JASC PAL and GIMP GPL (round trips, files from other programs, damaged files).
add_executable(test_palette_formats test_palette_formats.cpp)
aic_link_whole_archive(test_palette_formats renderhamlib)
target_compile_features(test_palette_formats PRIVATE cxx_std_17)
target_compile_definitions(test_palette_formats PRIVATE RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME palette_formats COMMAND test_palette_formats)

# Quality profiles (Fast / Standard / Best): standard equals the defaults, every profile is valid,
# and they order as their names say.
add_executable(test_profiles test_profiles.cpp)
aic_link_whole_archive(test_profiles renderhamlib)
target_compile_features(test_profiles PRIVATE cxx_std_17)
target_compile_definitions(test_profiles PRIVATE RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata")
add_test(NAME profiles COMMAND test_profiles)

# Where a point of the source lands on the canvas (the GUI couples the zoom and pan of its two views
# with it), checked against what the pipeline really does.
add_executable(test_canvas_mapping test_canvas_mapping.cpp)
aic_link_whole_archive(test_canvas_mapping renderhamlib)
target_compile_features(test_canvas_mapping PRIVATE cxx_std_17)
add_test(NAME canvas_mapping COMMAND test_canvas_mapping)

# File names with non-ASCII characters (umlauts, accents, CJK) in every place that opens a file.
add_executable(test_unicode_paths test_unicode_paths.cpp)
aic_link_whole_archive(test_unicode_paths renderhamlib)
target_compile_features(test_unicode_paths PRIVATE cxx_std_17)
target_compile_definitions(test_unicode_paths PRIVATE
    RENDERHAM_TESTDATA="${CMAKE_SOURCE_DIR}/testdata"
    RENDERHAM_TMPDIR="${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME unicode_paths COMMAND test_unicode_paths)
