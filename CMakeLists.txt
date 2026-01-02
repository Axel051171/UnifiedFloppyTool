# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.2.0 - VISUAL Edition
# ═══════════════════════════════════════════════════════════════════════════════
# Cross-platform floppy disk preservation and analysis tool
# Build: cmake -B build -G Ninja && cmake --build build
# ═══════════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.16)

project(UnifiedFloppyTool
    VERSION 3.2.0
    DESCRIPTION "Cross-platform floppy disk preservation and analysis tool"
    LANGUAGES CXX C
)

# ═══════════════════════════════════════════════════════════════════════════════
# Build Options
# ═══════════════════════════════════════════════════════════════════════════════

option(UFT_ENABLE_LTO      "Enable Link-Time Optimization"     OFF)
option(UFT_ENABLE_SIMD     "Enable SIMD optimizations"         ON)
option(UFT_ENABLE_OPENMP   "Enable OpenMP parallelization"     ON)

# ═══════════════════════════════════════════════════════════════════════════════
# C++ Standard & Qt Settings
# ═══════════════════════════════════════════════════════════════════════════════

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/forms)

# ═══════════════════════════════════════════════════════════════════════════════
# Find Qt6
# ═══════════════════════════════════════════════════════════════════════════════

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 COMPONENTS Core5Compat QUIET)

message(STATUS "════════════════════════════════════════════════════")
message(STATUS "  UnifiedFloppyTool v${PROJECT_VERSION} - VISUAL Edition")
message(STATUS "════════════════════════════════════════════════════")
message(STATUS "  Qt Version: ${Qt6_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Platform:   ${CMAKE_SYSTEM_NAME}")
message(STATUS "════════════════════════════════════════════════════")

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
        -Wall
        -Wextra
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:RelWithDebInfo>:-O2 -g>
        $<$<CONFIG:Debug>:-g -O0>
    )
endif()

# ═══════════════════════════════════════════════════════════════════════════════
# OpenMP Support
# ═══════════════════════════════════════════════════════════════════════════════

if(UFT_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
        message(STATUS "  OpenMP: Enabled")
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
    else()
        check_cxx_compiler_flag("-mavx2" HAVE_AVX2)
        check_cxx_compiler_flag("-msse4.2" HAVE_SSE42)
        
        if(HAVE_AVX2)
            target_compile_options(${PROJECT_NAME} PRIVATE -mavx2)
            message(STATUS "  SIMD: AVX2")
        elseif(HAVE_SSE42)
            target_compile_options(${PROJECT_NAME} PRIVATE -msse4.2)
            message(STATUS "  SIMD: SSE4.2")
        endif()
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

message(STATUS "════════════════════════════════════════════════════")
