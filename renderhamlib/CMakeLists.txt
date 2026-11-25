# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
add_library(renderhamlib STATIC
    src/image.cpp
    src/exif.cpp
    src/transform.cpp
    src/crop.cpp
    src/color_correction.cpp
    src/palette_io.cpp
    src/resize.cpp
    src/ham6_encoder.cpp
    src/ham_maps.cpp
    src/quality_metrics.cpp
    src/quality_profile.cpp
    src/iff_common.cpp
    src/ilbm_writer.cpp
    src/anim_writer.cpp
    src/pipeline.cpp
    src/sequence.cpp
    src/quantizers/median_cut_quantizer.cpp
    src/quantizers/kmeans_quantizer.cpp
    src/quantizers/octree_quantizer.cpp
    src/dithering/none.cpp
    src/dithering/bayer.cpp
    src/dithering/floyd_steinberg.cpp
    src/dithering/kernel_diffuser.cpp
    src/dithering/atkinson.cpp
    src/dithering/sierra_lite.cpp
    src/dithering/jarvis_judice_ninke.cpp
    src/dithering/blue_noise.cpp
    src/dithering/checks.cpp
    src/importance/uniform.cpp
    src/importance/center.cpp
    src/importance/edge.cpp
    src/importance/center_edge.cpp
    src/importance/golden_ratio.cpp
)

# aic/version.hpp is generated from the version in the top-level project() call.
configure_file(include/aic/version.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/generated/aic/version.hpp @ONLY)

target_include_directories(renderhamlib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
        $<INSTALL_INTERFACE:include>
)

# stb is a vendored, header-only, single-TU-implementation dependency.
# Only image.cpp defines STB_IMAGE_IMPLEMENTATION / STB_IMAGE_WRITE_IMPLEMENTATION;
# resize.cpp uses stb_image_resize2.h as declarations-only there too, so
# giving the whole target access to third_party/stb as a private include
# dir is sufficient - no separate stb target needed for a project this size.
# SYSTEM tells the compiler to treat this path's headers as system
# headers (-isystem instead of -I), which suppresses warnings that
# originate FROM stb's own code (e.g. -Wmissing-field-initializers on
# its "= {0}" struct-zeroing idiom) without touching the vendored file
# or disabling that warning class for our own code.
target_include_directories(renderhamlib SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/stb)

target_compile_features(renderhamlib PUBLIC cxx_std_17)

# render_sequence() renders animation frames on std::thread workers. (A MinGW
# cross build needs the posix thread model, i.e. the x86_64-w64-mingw32-g++-posix
# compiler, for std::thread/std::mutex.)
find_package(Threads REQUIRED)
target_link_libraries(renderhamlib PUBLIC Threads::Threads)

set_target_properties(renderhamlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
    target_compile_options(renderhamlib PRIVATE /W4)
else()
    target_compile_options(renderhamlib PRIVATE -Wall -Wextra)
endif()

# The internal C++ namespace and include convention (aic::..., "aic/*.hpp")
# are unchanged from before this rename - only the product-facing library/
# executable/directory names moved to the renderHAM family. See the
# top-level README for the reasoning.
include(GNUInstallDirs)
install(TARGETS renderhamlib ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/aic DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} PATTERN "*.in" EXCLUDE)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/aic/version.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aic)
