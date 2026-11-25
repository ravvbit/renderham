# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
cmake_minimum_required(VERSION 3.16)
# The one place the version number is defined (Semantic Versioning, see README.md). It flows into
# aic/version.hpp (renderHAM --version, the GUI's About dialog) and into scripts/build_deb.sh,
# which reads this line. When releasing: bump it, update CHANGELOG.md, tag vX.Y.Z.
project(renderham VERSION 0.1.0 LANGUAGES CXX)

if (WIN32)
    enable_language(RC)  # the GUI's icon, manifest and version information (packaging/windows/)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(cmake/AicWholeArchive.cmake)

add_subdirectory(renderhamlib)
add_subdirectory(tools/renderham)

option(AIC_BUILD_GUI "Build the wxWidgets GUI (renderHAMgui)" ON)
if (AIC_BUILD_GUI)
    find_package(wxWidgets QUIET COMPONENTS core base)
    if (wxWidgets_FOUND)
        add_subdirectory(tools/renderhamgui)
    else()
        message(STATUS "wxWidgets not found - skipping renderHAMgui "
                        "(install e.g. libwxgtk3.2-dev, or pass -DAIC_BUILD_GUI=OFF to silence this)")
    endif()
endif()

# The license texts travel with every installed copy, which also puts them into the AppImage and
# the Flatpak (share/doc/renderham/).
include(GNUInstallDirs)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

enable_testing()
if (EXISTS ${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt)
    add_subdirectory(tests)
endif()
