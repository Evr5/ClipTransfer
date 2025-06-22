cmake_minimum_required(VERSION 3.15)

project(ClipTransfer VERSION 1.0.0 LANGUAGES CXX)

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

# Static linking (cross-platform)
if(BUILD_STATIC)
    message(STATUS "Static linking enabled")
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
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

# Source and include directories
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Main executable
add_executable(clip-transfer ${SOURCES})

# External libraries target
include(${CMAKE_SOURCE_DIR}/cmake/asio.cmake)

# Link external libraries
target_link_libraries(clip-transfer PUBLIC asio)

# Include directories
target_include_directories(clip-transfer PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Installation
install(TARGETS clip-transfer RUNTIME DESTINATION bin)
