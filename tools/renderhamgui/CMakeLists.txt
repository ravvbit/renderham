# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
include(${wxWidgets_USE_FILE})

add_executable(renderHAMgui
    main.cpp
    MainFrame.cpp
    BackgroundJob.cpp
    ParameterPanel.cpp
    FileFilters.cpp
    ImageView.cpp
    Localization.cpp
    SettingsDialog.cpp
    AboutDialog.cpp
)

target_include_directories(renderHAMgui PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(renderHAMgui PRIVATE ${wxWidgets_DEFINITIONS})
target_link_libraries(renderHAMgui PRIVATE ${wxWidgets_LIBRARIES})

# See cmake/AicWholeArchive.cmake for why this is needed (self-registering
# dithering/quantizer/importance implementations).
aic_link_whole_archive(renderHAMgui renderhamlib)

target_compile_features(renderHAMgui PRIVATE cxx_std_17)

if (MSVC)
    target_compile_options(renderHAMgui PRIVATE /W4)
else()
    target_compile_options(renderHAMgui PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS renderHAMgui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Desktop integration on Linux/BSD: menu entry, icon and AppStream metadata. The .deb, the
# AppImage and the Flatpak all pick these up through "cmake --install"; the file names are
# the application ID (reverse-DNS), which Flatpak requires. See packaging/linux/.
if (UNIX AND NOT APPLE)
    set(AIC_APP_ID io.github.ravvbit.renderham)
    set(AIC_PACKAGING_DIR ${PROJECT_SOURCE_DIR}/packaging/linux)
    install(FILES ${AIC_PACKAGING_DIR}/${AIC_APP_ID}.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${AIC_PACKAGING_DIR}/${AIC_APP_ID}.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES ${AIC_PACKAGING_DIR}/${AIC_APP_ID}.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# Compiles po/<lang>.po -> translations/<lang>/LC_MESSAGES/renderhamgui.mo next
# to the built executable (see Localization.cpp for the runtime lookup).
# Adding a new language later is just "drop another .po file in po/,
# add its code to AIC_GUI_LOCALES" - Localization.cpp discovers whatever
# .mo files actually exist at runtime, no further code changes needed.
find_program(MSGFMT_EXECUTABLE msgfmt)
if (MSGFMT_EXECUTABLE)
    set(AIC_GUI_LOCALES de)
    set(AIC_GUI_MO_FILES "")
    foreach(lang ${AIC_GUI_LOCALES})
        set(PO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/po/${lang}.po)
        set(MO_DIR ${CMAKE_CURRENT_BINARY_DIR}/translations/${lang}/LC_MESSAGES)
        set(MO_FILE ${MO_DIR}/renderhamgui.mo)
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MO_DIR}
            COMMAND ${MSGFMT_EXECUTABLE} -o ${MO_FILE} ${PO_FILE}
            DEPENDS ${PO_FILE}
            COMMENT "Compiling ${lang} translation catalog for renderHAMgui"
        )
        list(APPEND AIC_GUI_MO_FILES ${MO_FILE})
    endforeach()
    add_custom_target(renderhamgui_translations ALL DEPENDS ${AIC_GUI_MO_FILES})
    add_dependencies(renderHAMgui renderhamgui_translations)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/translations
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            OPTIONAL)
else()
    message(STATUS "msgfmt not found - renderHAMgui will build English-only (no translation catalogs)")
endif()
