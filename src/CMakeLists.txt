# SPDX-License-Identifier: GPL-2.0-or-later

# ------------------------------------------------------------------ core library
add_library(cactuscore STATIC
    core/Types.cpp
    core/Model.cpp
    core/Presets.cpp
    core/color/Colors.cpp
    math/Expr.cpp
    math/SpecialFunctions.cpp
    plot/Scale.cpp
    plot/Ticks.cpp
    plot/Fonts.cpp
    plot/RichText.cpp
    render/PlotRenderer.cpp
    render/PsEngine.cpp
    render/Export.cpp
    io/DataReader.cpp
    math/Transforms.cpp
    math/Fit.cpp
    grace/Interpreter.cpp
    grace/ProjectIO.cpp
    app/Cli.cpp
    ${CMAKE_SOURCE_DIR}/resources/fonts.qrc
)
target_include_directories(cactuscore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(cactuscore PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::PrintSupport)

# ------------------------------------------------------------------ GUI library
if(CACTUS_BUILD_GUI)
    file(GLOB CACTUS_UI_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h)
    add_library(cactusui STATIC ${CACTUS_UI_SOURCES} ${CMAKE_SOURCE_DIR}/resources/app.qrc)
    target_link_libraries(cactusui PUBLIC cactuscore Qt6::Widgets)
    target_compile_definitions(cactusui PUBLIC CACTUS_HAVE_GUI)
endif()

# ------------------------------------------------------------------ application
set(CACTUS_APP_SOURCES app/main.cpp)
if(WIN32)
    configure_file(${CMAKE_SOURCE_DIR}/packaging/windows/cactusplot.rc.in ${CMAKE_BINARY_DIR}/generated/cactusplot.rc @ONLY)
    list(APPEND CACTUS_APP_SOURCES ${CMAKE_BINARY_DIR}/generated/cactusplot.rc)
endif()
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE CactusPlot.icns)
    set(CACTUS_ICNS ${CMAKE_SOURCE_DIR}/packaging/macos/CactusPlot.icns)
    set_source_files_properties(${CACTUS_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND CACTUS_APP_SOURCES ${CACTUS_ICNS})
endif()

add_executable(cactusplot ${CACTUS_APP_SOURCES})
if(CACTUS_BUILD_GUI)
    target_link_libraries(cactusplot PRIVATE cactusui)
    set_target_properties(cactusplot PROPERTIES WIN32_EXECUTABLE ON MACOSX_BUNDLE ON)
else()
    target_link_libraries(cactusplot PRIVATE cactuscore)
endif()
set_target_properties(cactusplot PROPERTIES
    OUTPUT_NAME cactusplot
    MACOSX_BUNDLE_BUNDLE_NAME "CactusPlot"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.cactusplot.CactusPlot"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in")
if(APPLE)
    # macdeployqt copies the Qt frameworks into Contents/Frameworks; the binary must search there
    set_target_properties(cactusplot PROPERTIES OUTPUT_NAME CactusPlot INSTALL_RPATH "@executable_path/../Frameworks")
endif()

include(GNUInstallDirs)
install(TARGETS cactusplot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)

# Bundle the Qt libraries next to the program on Windows and macOS (windeployqt/macdeployqt).
option(CACTUS_DEPLOY_QT "Deploy Qt runtime files when installing (Windows/macOS)" ${WIN32})
if(APPLE)
    set(CACTUS_DEPLOY_QT ON CACHE BOOL "" FORCE)
endif()
if(CACTUS_DEPLOY_QT AND CACTUS_BUILD_GUI AND COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(TARGET cactusplot OUTPUT_SCRIPT CACTUS_DEPLOY_SCRIPT NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${CACTUS_DEPLOY_SCRIPT})
endif()
