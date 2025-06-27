cmake_minimum_required(VERSION 3.15)

project(ClipTransfer VERSION 1.1.0 LANGUAGES CXX)

# Options
option(BUILD_STATIC "Link standard libs statically" OFF)
option(BUILD_TESTS "Build unit tests" ON)

# Export compile_commands.json for LSPs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# User-defined test directory (can be overridden from CLI or Makefile)
set(TEST_DIR "${CMAKE_SOURCE_DIR}/tests" CACHE PATH "Path to test directory")

# C++ standard (easily modifiable)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Static linking (fully static - cross-platform)
if(BUILD_STATIC)
    message(STATUS "Static linking enabled")

    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        # Link everything statically, including libc
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    endif()
endif()

# Compilation flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Define the minimum Windows version for compatibility
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0600)
endif()

# Active l'automoc pour Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# For Windows: force source encoding to UTF-8 (for Qt, etc.)
if (WIN32)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif ()

# Source and include directories
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/client.cpp
    src/MainWindow.cpp
)

# Main executable
add_definitions(-DASIO_HEADER_ONLY)
add_executable(clip-transfer ${SOURCES})

# External libraries target
include(${CMAKE_SOURCE_DIR}/cmake/asio.cmake)

# Link external libraries
target_link_libraries(clip-transfer PRIVATE asio)
if (WIN32)
    target_link_libraries(clip-transfer PRIVATE ws2_32)
endif()

# Include directories
target_include_directories(clip-transfer PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Find and link Qt6
find_package(Qt5 COMPONENTS Widgets REQUIRED)
target_link_libraries(clip-transfer
    PRIVATE
        Qt5::Widgets
        Qt5::Gui
        Qt5::Core
        ws2_32
        kernel32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        advapi32
)

# Installation
install(TARGETS clip-transfer RUNTIME DESTINATION bin)
