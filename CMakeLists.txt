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
        add_compile_options(-g)
        if(NOT (WIN32 AND NOT MSVC)) # Address and undefined behavior sanitizers are not supported on MSVC
            add_compile_options(-fsanitize=address,undefined)
            add_link_options(-fsanitize=address,undefined)
        endif()
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

# Source and include directories
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/client.cpp
    src/MainWindow.cpp
)

# Active l'automoc pour Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Main executable
add_definitions(-DASIO_HEADER_ONLY)
add_executable(clip-transfer ${SOURCES})

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0600) # Define the minimum Windows version for compatibility
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>") # force source encoding to UTF-8 (for Qt, etc.)
    set_target_properties(clip-transfer PROPERTIES WIN32_EXECUTABLE ON) # Create a GUI application (no console window)
    target_link_libraries(clip-transfer # Link against Windows libraries
        PRIVATE
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
            iphlpapi
    )
endif()

if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set_target_properties(clip-transfer PROPERTIES WIN32_EXECUTABLE OFF)
endif()

# External libraries target
include(${CMAKE_SOURCE_DIR}/cmake/asio.cmake)

# Link external libraries
target_link_libraries(clip-transfer PRIVATE asio)

# Include directories
target_include_directories(clip-transfer PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Find and link Qt5
find_package(Qt5 COMPONENTS Widgets REQUIRED)
target_link_libraries(clip-transfer
    PRIVATE
        Qt5::Widgets
        Qt5::Gui
        Qt5::Core
)

# Installation
install(TARGETS clip-transfer RUNTIME DESTINATION bin)
