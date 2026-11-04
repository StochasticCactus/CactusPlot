# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.19)

project(CactusPlot
    VERSION 0.9.0
    DESCRIPTION "Modern, cross-platform 2D scientific plotting (XMGrace successor)"
    HOMEPAGE_URL "https://github.com/StochasticCactus/CactusGui"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CACTUS_BUILD_GUI "Build the graphical application" ON)
option(CACTUS_BUILD_TESTS "Build unit tests" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Svg PrintSupport)
if(CACTUS_BUILD_GUI)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
endif()

if(MSVC)
    add_compile_options(/W3 /utf-8 /permissive- /Zc:__cplusplus)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Build identification: the git commit, and the CI run number for packages (each build gets its own
# package version, so installing a newer build over an older one always replaces it)
set(CACTUS_BUILD_NUMBER "" CACHE STRING "Package release number (CI run number)")
set(CACTUS_GIT_REVISION "")
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                    OUTPUT_VARIABLE CACTUS_GIT_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
set(CACTUS_BUILD_INFO "")
if(CACTUS_GIT_REVISION)
    string(APPEND CACTUS_BUILD_INFO "commit ${CACTUS_GIT_REVISION}")
endif()
if(CACTUS_BUILD_NUMBER)
    if(CACTUS_BUILD_INFO)
        string(APPEND CACTUS_BUILD_INFO ", ")
    endif()
    string(APPEND CACTUS_BUILD_INFO "build ${CACTUS_BUILD_NUMBER}")
endif()
configure_file(cmake/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

add_subdirectory(src)

if(CACTUS_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

include(cmake/Packaging.cmake OPTIONAL)
