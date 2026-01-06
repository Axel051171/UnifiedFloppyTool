# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.3.0 - GitHub Edition
# ═══════════════════════════════════════════════════════════════════════════════
# Cross-platform floppy disk preservation and analysis tool
# Build: cmake -B build -G Ninja && cmake --build build
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

project(UnifiedFloppyTool
    VERSION 3.5.0
    DESCRIPTION "Cross-platform floppy disk preservation and analysis tool"
    LANGUAGES CXX C
)

# ──────────────────────────────────────────────────────────────────────────────
# Zentrale Plattform-Funktionen (Math, Threads, OpenMP)
# DIREKT HIER DEFINIERT - kein externes include nötig!
# ──────────────────────────────────────────────────────────────────────────────

# uft_link_math() - Linkt Math-Library nur auf Unix (nicht auf Windows)
# Hintergrund: Windows hat Math in MSVCRT, es gibt kein m.lib → LNK1181
function(uft_link_math target)
    if(UNIX)
        target_link_libraries(${target} PRIVATE m)
    endif()
endfunction()

# uft_link_threads() - Plattformunabhängiges Thread-Linking
function(uft_link_threads target)
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(${target} PRIVATE Threads::Threads)
    endif()
endfunction()


# ──────────────────────────────────────────────────────────────────────────────
# Compiler warnings / flags (portable)
# - GCC/Clang: -Wall -Wextra ...
# - MSVC:      /W4 (approx.)
# Important: MSVC treats -Wextra as /Wextra (D8021). Never pass -Wextra to MSVC.
# ──────────────────────────────────────────────────────────────────────────────
add_compile_options(
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<C_COMPILER_ID:MSVC>:/W4>
  $<$<CXX_COMPILER_ID:MSVC>:/wd4100>  # unreferenced formal parameter
  $<$<C_COMPILER_ID:MSVC>:/wd4100>
)


# ═══════════════════════════════════════════════════════════════════════════════
# Build Options
# ═══════════════════════════════════════════════════════════════════════════════

option(UFT_ENABLE_LTO      "Enable Link-Time Optimization"     OFF)
option(UFT_ENABLE_SIMD     "Enable SIMD optimizations"         ON)
option(UFT_ENABLE_OPENMP   "Enable OpenMP parallelization"     ON)
option(UFT_ENABLE_SECURITY "Enable security hardening flags"   ON)

# ═══════════════════════════════════════════════════════════════════════════════
# Security Hardening (2026-01-06 Security Audit)
# ═══════════════════════════════════════════════════════════════════════════════
if(UFT_ENABLE_SECURITY AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SecurityFlags.cmake")
    include(cmake/SecurityFlags.cmake)
    message(STATUS "Security hardening flags enabled")
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# C++ Standard & Qt Settings
# FIXED R20: Expliziter C++17 Standard, keine GNU-Extensions, Required
# ═══════════════════════════════════════════════════════════════════════════════

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # Use -std=c++17, not -std=gnu++17

# Also set C standard explicitly
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ═══════════════════════════════════════════════════════════════════════════════
# POSIX Feature-Test-Macros (Fix für strdup, strcasecmp, gethostname, etc.)
# Diese Funktionen sind POSIX, aber nicht ISO C. Ohne diese Macros gibt es
# "implicit declaration" Warnings auf Linux/macOS mit -std=c11.
# Windows hat diese Funktionen nativ in der CRT.
# ═══════════════════════════════════════════════════════════════════════════════
if(NOT WIN32)
    add_compile_definitions(
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
endif()

# Suppress MSVC deprecation warnings for standard C functions
if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/forms)

# ═══════════════════════════════════════════════════════════════════════════════
# Find Qt6
# ═══════════════════════════════════════════════════════════════════════════════

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 COMPONENTS SerialPort QUIET)
find_package(Qt6 COMPONENTS Core5Compat QUIET)

if(Qt6SerialPort_FOUND)
    message(STATUS "  Qt6 SerialPort: Found - Hardware support enabled")
    add_compile_definitions(UFT_HAS_SERIALPORT=1)
else()
    message(STATUS "  Qt6 SerialPort: Not found - Hardware support disabled")
endif()

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "  UnifiedFloppyTool v${PROJECT_VERSION} - VISUAL Edition")
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
message(STATUS "════════════════════════════════════════════════════")

# ═══════════════════════════════════════════════════════════════════════════════
# UFT Core Modules (v3.3.4 TODO Audit)
# ═══════════════════════════════════════════════════════════════════════════════

# Core Module (cache, error, types, SIMD)
add_subdirectory(src/core)

# Hardware Abstraction Layer (Greaseweazle, FluxEngine, KryoFlux)
add_subdirectory(src/hal)

# Decoder Module (PLL, GCR, sync, fusion)
add_subdirectory(src/decoder)

# Sector Parser (IBM FM/MFM)
add_subdirectory(src/sector)

# Decoders Module (FM, MFM, GCR plugins + unified v2)
add_subdirectory(src/decoders)

# Encoding: FM/MFM/GCR
add_subdirectory(src/encoding)

# CRC Tools
add_subdirectory(src/crc)

# FDC Bitstream Emulator (C++)
add_subdirectory(src/flux/fdc_bitstream)

# PLL Module
add_subdirectory(src/flux/pll)

# Recovery Module
add_subdirectory(src/recovery)

# Protection Module
add_subdirectory(src/protection)

# Params Module
# add_subdirectory(src/params)  # DISABLED: many struct mismatches

# Format Modules
# add_subdirectory(src/formats/apple)  # DISABLED: missing headers
# add_subdirectory(src/formats/atari)  # DISABLED: missing Atari-specific headers
# add_subdirectory(src/formats/amiga_ext)  # DISABLED: missing Amiga-specific headers
# add_subdirectory(src/formats/ibm)  # DISABLED: needs HxC types.h
# add_subdirectory(src/formats/misc_hxc)  # DISABLED: needs HxC types.h
add_subdirectory(src/formats/scp)
add_subdirectory(src/formats/amstrad)
add_subdirectory(src/formats/msa)
# Floppy I/O Library (GCR, MFM, FAT12)
add_subdirectory(lib/floppy)
add_subdirectory(src/formats/flashfloppy)
add_subdirectory(src/forensic)

# Disk Image Format Parsers (v3.2.0)
add_subdirectory(src/loaders/disk_formats)

message(STATUS "  Modules: core, decoder, decoders_v2, encoding, crc, fdc_bitstream, formats/*, scp, amstrad")
message(STATUS "  Disk Formats: IMD, TD0, DMK, FDI, FDC, Altair (v3.2.0)")

# ═══════════════════════════════════════════════════════════════════════════════
# Source Files
# ═══════════════════════════════════════════════════════════════════════════════

set(UFT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/visualdisk.cpp
    src/workflowtab.cpp
    src/statustab.cpp
    src/hardwaretab.cpp
    src/formattab.cpp
    src/protectiontab.cpp
    src/catalogtab.cpp
    src/toolstab.cpp
    src/diskanalyzerwindow.cpp
    src/decodejob.cpp
)

# Optional sources - only add if they exist
foreach(OPTIONAL_SRC 
    src/settingsmanager.cpp
    src/forensictab.cpp
    src/nibbletab.cpp
    src/xcopytab.cpp
    src/inputvalidation.cpp
    src/fluxdecoder.cpp
    src/tracksectorwidget.cpp
    src/disk_image_validator.cpp
    src/gw_device_detector.cpp
    src/gw_output_parser.cpp
)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${OPTIONAL_SRC}")
        list(APPEND UFT_SOURCES ${OPTIONAL_SRC})
    endif()
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# Header Files
# ═══════════════════════════════════════════════════════════════════════════════

set(UFT_HEADERS
    src/mainwindow.h
    src/visualdisk.h
    src/workflowtab.h
    src/statustab.h
    src/hardwaretab.h
    src/formattab.h
    src/protectiontab.h
    src/catalogtab.h
    src/toolstab.h
    src/diskanalyzerwindow.h
    src/decodejob.h
)

# Optional headers
foreach(OPTIONAL_HDR
    src/settingsmanager.h
    src/forensictab.h
    src/nibbletab.h
    src/xcopytab.h
    src/inputvalidation.h
    src/fluxdecoder.h
    src/tracksectorwidget.h
    src/pathutils.h
)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${OPTIONAL_HDR}")
        list(APPEND UFT_HEADERS ${OPTIONAL_HDR})
    endif()
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# UI Forms
# ═══════════════════════════════════════════════════════════════════════════════

set(UFT_FORMS
    forms/mainwindow.ui
    forms/tab_workflow.ui
    forms/tab_status.ui
    forms/tab_hardware.ui
    forms/tab_format.ui
    forms/tab_protection.ui
    forms/tab_catalog.ui
    forms/tab_tools.ui
    forms/visualdisk.ui
    forms/diskanalyzer_window.ui
    forms/dialog_validation.ui
)

# Optional forms
foreach(OPTIONAL_FORM
    forms/tab_forensic.ui
    forms/tab_nibble.ui
    forms/tab_xcopy.ui
    forms/tab_diagnostics.ui
)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${OPTIONAL_FORM}")
        list(APPEND UFT_FORMS ${OPTIONAL_FORM})
    endif()
endforeach()

# ═══════════════════════════════════════════════════════════════════════════════
# Resources
# ═══════════════════════════════════════════════════════════════════════════════

set(UFT_RESOURCES "")
if(EXISTS "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
    list(APPEND UFT_RESOURCES resources/resources.qrc)
endif()

# Windows Resource File (icon, version info)
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/resources/uft.rc")
    list(APPEND UFT_SOURCES resources/uft.rc)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Executable
# ═══════════════════════════════════════════════════════════════════════════════

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        ${UFT_SOURCES}
        ${UFT_HEADERS}
        ${UFT_FORMS}
        ${UFT_RESOURCES}
    )
elseif(APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${UFT_SOURCES}
        ${UFT_HEADERS}
        ${UFT_FORMS}
        ${UFT_RESOURCES}
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "UnifiedFloppyTool"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.uft.UnifiedFloppyTool"
    )
else()
    add_executable(${PROJECT_NAME}
        ${UFT_SOURCES}
        ${UFT_HEADERS}
        ${UFT_FORMS}
        ${UFT_RESOURCES}
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Include Directories
# ═══════════════════════════════════════════════════════════════════════════════

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_CURRENT_BINARY_DIR}
)

# ═══════════════════════════════════════════════════════════════════════════════
# Link Libraries
# ═══════════════════════════════════════════════════════════════════════════════

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# SerialPort (optional - for hardware support)
if(Qt6SerialPort_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::SerialPort)
endif()

# UFT Core Modules (v3.3.5 GOD MODE)
target_link_libraries(${PROJECT_NAME} PRIVATE
    uft_core
    uft_decoder
    uft_decoders
    uft_decoders_v2
    uft_encoding
    uft_crc
    uft_fdc_bitstream
    uft_pll
    uft_recovery
    uft_protection
    # uft_params  # DISABLED
    # uft_format_apple  # DISABLED
    # uft_format_atari  # DISABLED
    # uft_format_amiga_ext  # DISABLED
    # uft_format_ibm  # DISABLED
    # uft_format_misc  # DISABLED
    uft_format_scp
    uft_format_amstrad
)

if(TARGET Qt6::Core5Compat)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core5Compat)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Compile Definitions
# ═══════════════════════════════════════════════════════════════════════════════

target_compile_definitions(${PROJECT_NAME} PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
    APP_NAME="UnifiedFloppyTool"
    APP_EDITION="VISUAL"
    UFT_VERSION="${PROJECT_VERSION}"
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
    $<$<CONFIG:Debug>:DEBUG_BUILD>
    $<$<CONFIG:Debug>:UFT_DEBUG_MEMORY>
    $<$<CONFIG:Release>:RELEASE_BUILD>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:RelWithDebInfo>:RELEASE_BUILD>
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        # Suppress MSVC CRT security warnings (we use safe functions where appropriate)
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Compiler Flags
# ═══════════════════════════════════════════════════════════════════════════════

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3
        /utf-8
        /permissive-
        /Zc:__cplusplus
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-parameter>
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Debug>:-g -O0>
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# OpenMP Support (FIXED R22: Platform-specific handling)
# ═══════════════════════════════════════════════════════════════════════════════

if(UFT_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND OR OpenMP_C_FOUND)
        if(MSVC)
            # MSVC: Only use /openmp flag, do NOT link OpenMP library
            # This avoids LNK1181 errors when OpenMP runtime is not installed
            target_compile_options(${PROJECT_NAME} PRIVATE /openmp)
            message(STATUS "  OpenMP: Enabled (MSVC /openmp)")
        else()
            # GCC/Clang: Link OpenMP library normally
            target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
            message(STATUS "  OpenMP: Enabled")
        endif()
    else()
        message(STATUS "  OpenMP: Not found")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# SIMD Support
# ═══════════════════════════════════════════════════════════════════════════════

if(UFT_ENABLE_SIMD)
    include(CheckCXXCompilerFlag)
    
    if(MSVC)
        check_cxx_compiler_flag("/arch:AVX2" HAVE_AVX2)
        if(HAVE_AVX2)
            target_compile_options(${PROJECT_NAME} PRIVATE /arch:AVX2)
            message(STATUS "  SIMD: AVX2")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
        # Only enable AVX2/SSE on x86 architectures (not ARM/Apple Silicon)
        check_cxx_compiler_flag("-mavx2" HAVE_AVX2)
        check_cxx_compiler_flag("-msse4.2" HAVE_SSE42)
        
        if(HAVE_AVX2)
            target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
            message(STATUS "  SIMD: AVX2")
        elseif(HAVE_SSE42)
            target_compile_options(${PROJECT_NAME} PRIVATE -msse4.2)
            message(STATUS "  SIMD: SSE4.2")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
        # ARM64/Apple Silicon - use NEON (auto-enabled on ARM64)
        message(STATUS "  SIMD: ARM NEON (auto)")
    endif()
    
    target_compile_definitions(${PROJECT_NAME} PRIVATE UFT_ENABLE_SIMD)
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# LTO Support
# ═══════════════════════════════════════════════════════════════════════════════

if(UFT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "  LTO: Enabled")
    else()
        message(STATUS "  LTO: Not supported")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════════

enable_testing()

# Simple test that always passes (placeholder)
add_test(NAME BuildTest COMMAND ${PROJECT_NAME} --version || true)

# ═══════════════════════════════════════════════════════════════════════════════
# Install
# ═══════════════════════════════════════════════════════════════════════════════

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Install desktop file and icons (Linux)
if(UNIX AND NOT APPLE)
    # Desktop file
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/UnifiedFloppyTool.desktop
            DESTINATION share/applications)
    
    # Icons (multiple sizes)
    install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/UnifiedFloppyTool.svg
            DESTINATION share/icons/hicolor/scalable/apps)
    
    # PNG icons for various sizes (if they exist)
    foreach(SIZE 16 32 48 64 128 256)
        if(EXISTS "${CMAKE_SOURCE_DIR}/resources/icons/UnifiedFloppyTool_${SIZE}.png")
            install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/UnifiedFloppyTool_${SIZE}.png
                    DESTINATION share/icons/hicolor/${SIZE}x${SIZE}/apps
                    RENAME UnifiedFloppyTool.png)
        endif()
    endforeach()
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# CPack Configuration - Cross-Platform Packaging
# ═══════════════════════════════════════════════════════════════════════════════

set(CPACK_PACKAGE_NAME "uft")
set(CPACK_PACKAGE_VENDOR "UFT Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Universal Floppy Tool - Cross-platform disk imaging")
set(CPACK_PACKAGE_DESCRIPTION "UFT is a comprehensive floppy disk preservation and analysis tool supporting 200+ disk formats, flux-level imaging, and copy protection analysis.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/your-username/uft")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# Resource/License files
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# Package file naming
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME)

# ───────────────────────────────────────────────────────────────────────────────
# TGZ (tar.gz) - All platforms
# ───────────────────────────────────────────────────────────────────────────────
set(CPACK_GENERATOR "TGZ")

# ───────────────────────────────────────────────────────────────────────────────
# DEB Package (Linux)
# ───────────────────────────────────────────────────────────────────────────────
if(UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB")
    
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "UFT Project <your-email@example.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.17), libusb-1.0-0 (>= 1.0.0), libqt6core6 (>= 6.2.0), libqt6gui6 (>= 6.2.0), libqt6widgets6 (>= 6.2.0)")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "greaseweazle")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    
    # Architecture
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
    else()
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
    endif()
endif()

# ───────────────────────────────────────────────────────────────────────────────
# ZIP Package (Windows)
# ───────────────────────────────────────────────────────────────────────────────
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    
    # Include Visual C++ Runtime
    include(InstallRequiredSystemLibraries)
endif()

# ───────────────────────────────────────────────────────────────────────────────
# macOS Bundle (optional, falls gewünscht)
# ───────────────────────────────────────────────────────────────────────────────
if(APPLE)
    set(CPACK_GENERATOR "TGZ")
    
    # Optional: DMG generator
    # list(APPEND CPACK_GENERATOR "DragNDrop")
    # set(CPACK_DMG_VOLUME_NAME "UFT")
    # set(CPACK_DMG_FORMAT "UDBZ")
endif()

# ───────────────────────────────────────────────────────────────────────────────
# Source Package
# ───────────────────────────────────────────────────────────────────────────────
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/\\\\.git/"
    "/\\\\.github/"
    "/build/"
    "/\\\\.vscode/"
    "/\\\\.idea/"
    "\\\\.orig$"
    "\\\\.swp$"
    "~$"
)

include(CPack)

message(STATUS "════════════════════════════════════════════════════")
