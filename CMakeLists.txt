cmake_minimum_required(VERSION 3.15)

project(ClipTransfer VERSION 2.0.0 LANGUAGES CXX)

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

# Set visibility for symbols (hidden by default)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

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
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
        set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
        set(CMAKE_LINK_SEARCH_START_STATIC TRUE)
        set(CMAKE_LINK_SEARCH_END_STATIC TRUE)
    endif()
endif()

# Compiler options
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        -fcolor-diagnostics   
        -fansi-escape-codes   
        -Wno-c++98-compat     
        -Wno-c++98-compat-pedantic
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-unknown-warning-option
    )
endif()
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(
        -Wall 
        -Wextra 
        -pedantic 
        -Wshadow 
        -Wnon-virtual-dtor 
        -Wold-style-cast 
        -Wcast-align 
        -Wunused 
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wno-unknown-pragmas
        -Wextra-semi
        -Wmisleading-indentation
        -fdiagnostics-show-option
        -pipe
        -fstack-protector-strong
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
        -Wno-volatile   
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
        # Sanitizers are great on desktop, but they are often problematic/unavailable on mobile toolchains.
        if(NOT ANDROID AND NOT IOS)
            if(NOT (WIN32 AND NOT MSVC)) # Address and undefined behavior sanitizers are not supported on MSVC
                add_compile_options(-fsanitize=address,undefined)
                add_link_options(-fsanitize=address,undefined)
            endif()
        endif()
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---- Core library (shared between desktop and mobile) ----
add_library(ClipTransferCore
    src/chat.cpp
)
set_target_properties(ClipTransferCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(ClipTransferCore PUBLIC ${CMAKE_SOURCE_DIR}/include)

# Find and link Qt6 (Core always; UI depends on platform)
find_package(Qt6 REQUIRED COMPONENTS Core Gui LinguistTools)
target_link_libraries(ClipTransferCore PRIVATE Qt6::Core)

# ---- App target (desktop Widgets OR mobile Qt Quick) ----
set(RESOURCES "")
if (WIN32)
    set(RESOURCES ${CMAKE_SOURCE_DIR}/res/resource.rc)
endif()

if(ANDROID OR IOS)
    # Mobile
    find_package(Qt6 REQUIRED COMPONENTS Qml Quick)

    qt_add_executable(ClipTransfer
        MANUAL_FINALIZATION
        src/mobile/main.cpp
        src/mobile/chat_controller.cpp
        ${RESOURCES}
        res/mobile.qrc
    )
    target_link_libraries(ClipTransfer
        PRIVATE
            ClipTransferCore
            Qt6::Core
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
    )

    # Android packaging hooks (manifest/Java sources)
    if(ANDROID)
        set_property(TARGET ClipTransfer PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
            ${CMAKE_SOURCE_DIR}/platform/android)
    endif()

    # iOS: allow providing a custom Info.plist
    if(IOS)
        set_target_properties(ClipTransfer PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/platform/ios/Info.plist
        )
    endif()

    qt_finalize_executable(ClipTransfer)
else()
    # Desktop (Widgets)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(ClipTransfer
        src/main.cpp
        src/gui/window.cpp
        ${RESOURCES}
    )
    target_include_directories(ClipTransfer PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(ClipTransfer
        PRIVATE
            ClipTransferCore
            Qt6::Widgets
    )
endif()
set_target_properties(ClipTransfer PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Position-independent code for shared libraries
if(WIN32)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>") # force source encoding to UTF-8 (for Qt, etc.)
    set_target_properties(ClipTransfer PROPERTIES WIN32_EXECUTABLE ON) # Create a GUI application (no console window)
    target_link_libraries(ClipTransfer # Link against Windows libraries
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
    set_target_properties(ClipTransfer PROPERTIES WIN32_EXECUTABLE OFF)
endif()


# Include directories
target_include_directories(ClipTransfer PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Translations (desktop-style deployment). On Android/iOS, translations should be packaged as assets/resources,
# not copied next to the binary, so we skip this for mobile builds.
if(NOT ANDROID AND NOT IOS)
    set(TS_FILES
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_en.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_es.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_pt.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_de.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_nl.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_zh.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_ar.ts
        ${CMAKE_SOURCE_DIR}/res/i18n/ClipTransfer_pl.ts
    )

    qt_add_translations(ClipTransfer
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
    )

    add_custom_command(TARGET ClipTransfer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ClipTransfer>/i18n
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES} $<TARGET_FILE_DIR:ClipTransfer>/i18n
        VERBATIM
    )
endif()

# Installation (desktop only)
if(NOT ANDROID AND NOT IOS)
    install(TARGETS ClipTransfer RUNTIME DESTINATION bin)
    install(FILES ${QM_FILES} DESTINATION bin/i18n)
endif()
