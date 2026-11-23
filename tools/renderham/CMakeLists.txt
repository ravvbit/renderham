# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
add_executable(renderHAM main.cpp)

# See cmake/AicWholeArchive.cmake for why this is needed (self-registering
# dithering/quantizer/importance implementations).
aic_link_whole_archive(renderHAM renderhamlib)

target_compile_features(renderHAM PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(renderHAM PRIVATE /W4)
else()
    target_compile_options(renderHAM PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS renderHAM RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
