cmake_minimum_required(VERSION 3.20)

project(UnifiedFloppyTool
    VERSION 3.1.2
    DESCRIPTION "The Ultimate Floppy Disk Preservation & Analysis Suite"
    LANGUAGES C CXX
)

# =============================================================================
# BUILD OPTIONS
# =============================================================================

option(UFT_ENABLE_SIMD "Enable SIMD optimizations (SSE2/AVX2)" ON)
option(UFT_ENABLE_OPENMP "Enable OpenMP parallelization" ON)
option(UFT_ENABLE_LTO "Enable Link-Time Optimization" OFF)

# SANITIZERS
option(UFT_ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(UFT_ENABLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(UFT_ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(UFT_ENABLE_MSAN "Enable Memory Sanitizer" OFF)

# TESTING
option(UFT_BUILD_TESTS "Build test suite" ON)
option(UFT_ENABLE_FUZZING "Enable fuzzing targets (libFuzzer)" OFF)

# ANALYSIS
option(UFT_ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(UFT_ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

# =============================================================================
# Qt Configuration
# =============================================================================

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS forms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Find Qt (try Qt6, fall back to Qt5)
find_package(Qt6 COMPONENTS Core Widgets QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Core Widgets)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

message(STATUS "Using Qt${QT_VERSION_MAJOR}")

# =============================================================================
# COMPILER FLAGS
# =============================================================================

if(MSVC)
    # MSVC Warnings
    add_compile_options(/W4 /WX-)
    
    # UTF-8 source files
    add_compile_options(/utf-8)
    
    # Sanitizers on MSVC
    if(UFT_ENABLE_ASAN)
        add_compile_options(/fsanitize=address)
        add_link_options(/fsanitize=address)
    endif()
else()
    # GCC/Clang Warnings
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
    )
    
    # Sanitizers on GCC/Clang
    set(SANITIZER_FLAGS "")
    
    if(UFT_ENABLE_ASAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=address")
        message(STATUS "Address Sanitizer enabled")
    endif()
    
    if(UFT_ENABLE_UBSAN)
        list(APPEND SANITIZER_FLAGS "-fsanitize=undefined")
        message(STATUS "Undefined Behavior Sanitizer enabled")
    endif()
    
    if(UFT_ENABLE_TSAN)
        if(UFT_ENABLE_ASAN)
            message(FATAL_ERROR "Cannot enable both ASan and TSan")
        endif()
        list(APPEND SANITIZER_FLAGS "-fsanitize=thread")
        message(STATUS "Thread Sanitizer enabled")
    endif()
    
    if(UFT_ENABLE_MSAN)
        if(UFT_ENABLE_ASAN OR UFT_ENABLE_TSAN)
            message(FATAL_ERROR "MSan cannot be combined with other sanitizers")
        endif()
        list(APPEND SANITIZER_FLAGS "-fsanitize=memory")
        message(STATUS "Memory Sanitizer enabled")
    endif()
    
    if(SANITIZER_FLAGS)
        add_compile_options(${SANITIZER_FLAGS})
        add_link_options(${SANITIZER_FLAGS})
    endif()
endif()

# =============================================================================
# STATIC ANALYSIS
# =============================================================================

if(UFT_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY
            ${CLANG_TIDY_EXE};
            -checks=-*,bugprone-*,performance-*,modernize-*,readability-*;
            -warnings-as-errors=*;
        )
        message(STATUS "clang-tidy enabled")
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

if(UFT_ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES cppcheck)
    if(CPPCHECK_EXE)
        set(CMAKE_C_CPPCHECK
            ${CPPCHECK_EXE};
            --enable=warning,style,performance,portability;
            --suppress=missingIncludeSystem;
        )
        message(STATUS "cppcheck enabled")
    else()
        message(WARNING "cppcheck not found")
    endif()
endif()

# =============================================================================
# SIMD
# =============================================================================

if(UFT_ENABLE_SIMD)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        if(MSVC)
            add_compile_options(/arch:AVX2)
        else()
            add_compile_options(-mavx2 -msse2)
        endif()
        message(STATUS "SIMD optimizations enabled (AVX2/SSE2)")
    endif()
endif()

# =============================================================================
# OpenMP
# =============================================================================

if(UFT_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_FOUND)
        message(STATUS "OpenMP enabled")
    else()
        message(WARNING "OpenMP not found")
    endif()
endif()

# =============================================================================
# LTO
# =============================================================================

if(UFT_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT IPO_SUPPORTED)
    if(IPO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link-Time Optimization enabled")
    else()
        message(WARNING "LTO not supported on this platform")
    endif()
endif()

# =============================================================================
# SOURCE FILES
# =============================================================================

set(UFT_SOURCES
    # Main
    src/main.cpp
    src/mainwindow.cpp
    src/decodejob.cpp
    
    # Tabs
    src/workflowtab.cpp
    src/hardwaretab.cpp
    src/formattab.cpp
    src/protectiontab.cpp
    src/catalogtab.cpp
    src/toolstab.cpp
    
    # Widgets
    src/widgets/trackgridwidget.cpp
    src/widgets/presetmanager.cpp
    src/widgets/diskvisualizationwindow.cpp
    src/visualdisk.cpp
    
    # Core (C)
    src/core/flux_core.c
    src/core/uft_mfm_scalar.c
    src/core/uft_gcr_scalar.c
    src/core/uft_simd.c
    src/core/uft_memory.c
    src/core/uft_sector_status.c
)

set(UFT_HEADERS
    # Main
    src/mainwindow.h
    src/decodejob.h
    
    # Tabs
    src/workflowtab.h
    src/hardwaretab.h
    src/formattab.h
    src/protectiontab.h
    src/catalogtab.h
    src/toolstab.h
    
    # Widgets
    src/widgets/trackgridwidget.h
    src/widgets/presetmanager.h
    src/widgets/diskvisualizationwindow.h
    src/visualdisk.h
    
    # Utilities (header-only)
    src/pathutils.h
    src/inputvalidation.h
    src/settingsmanager.h
    
    # Core
    include/uft/flux_core.h
    include/uft/uft_memory.h
    include/uft/uft_simd.h
    include/uft/uft_sector_status.h
    include/uft/uft_endian.h
)

set(UFT_UI_FILES
    forms/mainwindow.ui
    forms/tab_workflow.ui
    forms/tab_hardware.ui
    forms/tab_format.ui
    forms/tab_protection.ui
    forms/tab_catalog.ui
    forms/tab_tools.ui
    forms/visualdisk.ui
    forms/dialog_validation.ui
)

# =============================================================================
# EXECUTABLE
# =============================================================================

add_executable(UnifiedFloppyTool
    ${UFT_SOURCES}
    ${UFT_HEADERS}
    ${UFT_UI_FILES}
)

target_include_directories(UnifiedFloppyTool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(UnifiedFloppyTool PRIVATE Qt6::Core Qt6::Widgets)
else()
    target_link_libraries(UnifiedFloppyTool PRIVATE Qt5::Core Qt5::Widgets)
endif()

if(OpenMP_FOUND AND UFT_ENABLE_OPENMP)
    target_link_libraries(UnifiedFloppyTool PRIVATE OpenMP::OpenMP_CXX)
endif()

# =============================================================================
# TESTING
# =============================================================================

if(UFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# INSTALL
# =============================================================================

install(TARGETS UnifiedFloppyTool
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# =============================================================================
# SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "==================================================================")
message(STATUS "UnifiedFloppyTool v${PROJECT_VERSION} - Build Configuration")
message(STATUS "==================================================================")
message(STATUS "Qt Version:          Qt${QT_VERSION_MAJOR}")
message(STATUS "Compiler:            ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type:          ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  SIMD:              ${UFT_ENABLE_SIMD}")
message(STATUS "  OpenMP:            ${UFT_ENABLE_OPENMP}")
message(STATUS "  LTO:               ${UFT_ENABLE_LTO}")
message(STATUS "  Tests:             ${UFT_BUILD_TESTS}")
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  ASan:              ${UFT_ENABLE_ASAN}")
message(STATUS "  UBSan:             ${UFT_ENABLE_UBSAN}")
message(STATUS "  TSan:              ${UFT_ENABLE_TSAN}")
message(STATUS "  MSan:              ${UFT_ENABLE_MSAN}")
message(STATUS "")
message(STATUS "Static Analysis:")
message(STATUS "  clang-tidy:        ${UFT_ENABLE_CLANG_TIDY}")
message(STATUS "  cppcheck:          ${UFT_ENABLE_CPPCHECK}")
message(STATUS "==================================================================")
message(STATUS "")
