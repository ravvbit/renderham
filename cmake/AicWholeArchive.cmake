# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
# aic_link_whole_archive(<target> <library>)
#
# Links <library> into <target> ensuring every object file is included,
# even ones only referenced via static self-registration (see
# aic/registry.hpp) that would otherwise look "unused" to the linker and
# get silently dropped from a plain static-archive link. Needed by every
# executable that links against renderhamlib (renderHAM, renderHAMgui, ...).
function(aic_link_whole_archive target library)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        target_link_libraries(${target} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${library}>)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_link_libraries(${target} PRIVATE -Wl,--whole-archive ${library} -Wl,--no-whole-archive)
    else()
        message(FATAL_ERROR
            "${target} needs either CMake >= 3.24, or GCC/Clang for the "
            "whole-archive linker workaround. Detected: CMake ${CMAKE_VERSION}, "
            "compiler ${CMAKE_CXX_COMPILER_ID}.")
    endif()
endfunction()
