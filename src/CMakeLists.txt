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

# ------------------------------------------------------------------ application
if(CACTUS_BUILD_GUI AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/ui/App.h)
    file(GLOB CACTUS_UI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h)
    set(CACTUS_HAVE_GUI ON)
endif()

add_executable(cactusplot app/main.cpp ${CACTUS_UI_SOURCES})
target_link_libraries(cactusplot PRIVATE cactuscore)
if(CACTUS_HAVE_GUI)
    target_sources(cactusplot PRIVATE ${CMAKE_SOURCE_DIR}/resources/app.qrc)
    target_compile_definitions(cactusplot PRIVATE CACTUS_HAVE_GUI)
    target_link_libraries(cactusplot PRIVATE Qt6::Widgets)
endif()
set_target_properties(cactusplot PROPERTIES OUTPUT_NAME cactusplot)
include(GNUInstallDirs)
install(TARGETS cactusplot RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
