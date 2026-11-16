cmake_minimum_required(VERSION 3.20)

project(fried_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CoreIsUiFree.cmake)
fried_core_check_is_ui_free(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
)

add_library(fried_core STATIC
    src/asset_check.cpp
    src/build.cpp
    src/engine_location.cpp
    src/icon.cpp
    src/image.cpp
    src/line_kind.cpp
    src/project.cpp
    src/stb.cpp
    src/toolchains.cpp
)

if(UNIX)
    target_sources(fried_core PRIVATE src/posix/subprocess.cpp src/step_runner.cpp)
endif()
target_include_directories(fried_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)
target_compile_features(fried_core PUBLIC cxx_std_17)

# An application that takes the library as a subdirectory does not build or run its tests.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()

    enable_testing()
    add_subdirectory(tests)
endif()
