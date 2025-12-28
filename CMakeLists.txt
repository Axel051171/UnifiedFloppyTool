cmake_minimum_required(VERSION 3.20)

project(UnifiedFloppyTool
    VERSION 3.1.0
    DESCRIPTION "The Ultimate Floppy Disk Preservation & Analysis Suite"
    LANGUAGES C CXX
)

# Qt Configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS forms)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Options
option(UFT_ENABLE_SIMD "Enable SIMD optimizations (AVX2/SSE2)" ON)
option(UFT_ENABLE_OPENMP "Enable OpenMP parallelization" ON)
option(UFT_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(UFT_ENABLE_ASAN "Enable Address Sanitizer" OFF)

# Find Qt (try Qt6 first, fallback to Qt5)
find_package(Qt6 COMPONENTS Core Gui Widgets QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

# Compiler Flags
if(MSVC)
    add_compile_options(/W4)
    if(UFT_ENABLE_SIMD)
        add_compile_options(/arch:AVX2)
    endif()
else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(UFT_ENABLE_SIMD)
        add_compile_options(-mavx2 -msse2)
    endif()
endif()

# OpenMP
if(UFT_ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_FOUND)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif()
endif()

# Address Sanitizer
if(UFT_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Sources
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/workflowtab.cpp
    src/hardwaretab.cpp
    src/formattab.cpp
    src/protectiontab.cpp
    src/catalogtab.cpp
    src/toolstab.cpp
    src/visualdisk.cpp
    src/widgets/trackgridwidget.cpp
    src/widgets/presetmanager.cpp
    src/widgets/diskvisualizationwindow.cpp
)

# Headers
set(HEADERS
    src/mainwindow.h
    src/workflowtab.h
    src/hardwaretab.h
    src/formattab.h
    src/protectiontab.h
    src/catalogtab.h
    src/toolstab.h
    src/visualdisk.h
    src/widgets/trackgridwidget.h
    src/widgets/presetmanager.h
    src/widgets/diskvisualizationwindow.h
)

# UI Files
set(UI_FILES
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

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Core Library (C)
add_library(uft_core STATIC
    src/core/flux_core.c
    src/core/uft_memory.c
    src/core/uft_mfm_scalar.c
    src/core/uft_gcr_scalar.c
    src/core/uft_simd.c
    include/uft/flux_core.h
    include/uft/uft_memory.h
    include/uft/uft_simd.h
)

target_include_directories(uft_core PUBLIC include)

# SIMD Support
if(UFT_ENABLE_SIMD)
    target_compile_definitions(uft_core PRIVATE UFT_ENABLE_AVX2 UFT_ENABLE_SSE2)
endif()

# Main Executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
    ${RESOURCES}
)

# Link Libraries
target_link_libraries(${PROJECT_NAME}
    uft_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

if(OpenMP_FOUND)
    target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
endif()

# LTO
if(UFT_ENABLE_LTO)
    set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_property(TARGET uft_core PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# Install
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# CPack Configuration
set(CPACK_PACKAGE_NAME "UnifiedFloppyTool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VENDOR "UnifiedFloppyTool Contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

include(CPack)

# Print Configuration
message(STATUS "")
message(STATUS "UnifiedFloppyTool v${PROJECT_VERSION} Configuration:")
message(STATUS "  Qt Version:        Qt${QT_VERSION_MAJOR}")
message(STATUS "  C++ Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "  C Standard:        C${CMAKE_C_STANDARD}")
message(STATUS "  SIMD:              ${UFT_ENABLE_SIMD}")
message(STATUS "  OpenMP:            ${UFT_ENABLE_OPENMP}")
message(STATUS "  LTO:               ${UFT_ENABLE_LTO}")
message(STATUS "  Address Sanitizer: ${UFT_ENABLE_ASAN}")
message(STATUS "")
