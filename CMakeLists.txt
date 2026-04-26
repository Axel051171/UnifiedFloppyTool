# ============================================================================
# UnifiedFloppyTool - CMake Build System
# Version: 4.1.3  (single source of truth: VERSION.txt; see project(... VERSION ...))
# ============================================================================
#
# Build:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
#
# Install:
#   cmake --install build --prefix /usr/local
#
# Package:
#   cd build && cpack
#
# Note: The primary build system is qmake (UnifiedFloppyTool.pro).
#       This CMakeLists.txt is provided for CI/CD and packaging.
# ============================================================================

cmake_minimum_required(VERSION 3.20)

# Read version from file
file(READ "${CMAKE_SOURCE_DIR}/VERSION.txt" UFT_VERSION_RAW)
string(STRIP "${UFT_VERSION_RAW}" UFT_VERSION)

project(UnifiedFloppyTool
    VERSION ${UFT_VERSION}
    DESCRIPTION "Forensic Floppy Disk Preservation Tool"
    HOMEPAGE_URL "https://github.com/Axel051171/UnifiedFloppyTool"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/forms")

# ============================================================================
# Dependencies
# ============================================================================

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 OPTIONAL_COMPONENTS SerialPort Charts)

if(Qt6SerialPort_FOUND)
    add_compile_definitions(UFT_HAS_SERIALPORT)
    message(STATUS "Qt SerialPort: FOUND")
else()
    message(STATUS "Qt SerialPort: NOT FOUND (serial features disabled)")
endif()

if(Qt6Charts_FOUND)
    add_compile_definitions(UFT_HAS_QT_CHARTS)
    message(STATUS "Qt Charts: FOUND")
else()
    message(STATUS "Qt Charts: NOT FOUND (chart widgets disabled)")
endif()

# Platform libraries
if(UNIX AND NOT APPLE)
    find_package(PkgConfig)
    pkg_check_modules(LIBUSB libusb-1.0)
endif()

# ============================================================================
# Sources - collected via glob (mirrors .pro file)
# ============================================================================

# Source list comes from UnifiedFloppyTool.pro (qmake) — single source of
# truth. Avoids glob-recursive picking up orphan / experimental files that
# qmake silently ignores but CMake would compile (multiple-definition land,
# AUTOMOC redefinitions, missing-include cascades).
#
# Generated at configure time by scripts/verify_build_sources.py which is
# the same parser the build-system parity check uses (MF-006).
find_package(Python3 COMPONENTS Interpreter REQUIRED)

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/verify_build_sources.py
            --emit-cmake-sources
            --root ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE UFT_SOURCES
    RESULT_VARIABLE _src_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _src_rc EQUAL 0)
    message(FATAL_ERROR "Failed to extract SOURCES from .pro (rc=${_src_rc})")
endif()

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/scripts/verify_build_sources.py
            --emit-cmake-headers
            --root ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE UFT_HEADERS
    RESULT_VARIABLE _hdr_rc
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT _hdr_rc EQUAL 0)
    message(FATAL_ERROR "Failed to extract HEADERS from .pro (rc=${_hdr_rc})")
endif()

# Make absolute (verify_build_sources emits repo-relative paths).
set(_abs_sources)
foreach(_p ${UFT_SOURCES})
    list(APPEND _abs_sources "${CMAKE_SOURCE_DIR}/${_p}")
endforeach()
set(UFT_SOURCES ${_abs_sources})
unset(_abs_sources)

set(_abs_headers)
foreach(_p ${UFT_HEADERS})
    list(APPEND _abs_headers "${CMAKE_SOURCE_DIR}/${_p}")
endforeach()
set(UFT_HEADERS ${_abs_headers})
unset(_abs_headers)

file(GLOB UFT_FORMS "forms/*.ui")
set(UFT_RESOURCES resources/resources.qrc)

# ============================================================================
# Target
# ============================================================================

# qmake is the production build per CLAUDE.md. CMake is used for the test
# suite only. With UFT_BUILD_APP=OFF, CMake skips the Qt main-app target
# entirely — avoids AUTOMOC / Qt-Widgets quirks that don't surface under
# qmake's selective source list.
option(UFT_BUILD_APP "Build the Qt main app (default OFF; qmake is canonical)" OFF)

if(UFT_BUILD_APP)
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE app.icns)
    set(APP_ICON_MACOS "${CMAKE_SOURCE_DIR}/resources/icons/app.icns")
    set_source_files_properties(${APP_ICON_MACOS} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")

    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
        ${UFT_SOURCES} ${UFT_HEADERS} ${UFT_FORMS} ${UFT_RESOURCES}
        ${APP_ICON_MACOS}
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.uft.UnifiedFloppyTool"
        MACOSX_BUNDLE_BUNDLE_NAME "UnifiedFloppyTool"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in"
    )
elseif(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        ${UFT_SOURCES} ${UFT_HEADERS} ${UFT_FORMS} ${UFT_RESOURCES}
        resources/uft.rc
    )
else()
    add_executable(${PROJECT_NAME}
        ${UFT_SOURCES} ${UFT_HEADERS} ${UFT_FORMS} ${UFT_RESOURCES}
    )
endif()

# Skip AUTOUIC/AUTOMOC for C files that include ui_*.h (non-Qt headers)
# and for files with Q_OBJECT that shouldn't be MOC'd
foreach(_src ${UFT_SOURCES})
    if(_src MATCHES "\\.c$")
        set_source_files_properties(${_src} PROPERTIES SKIP_AUTOUIC ON SKIP_AUTOMOC ON)
    endif()
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/flux
    ${CMAKE_SOURCE_DIR}/include/uft/hal
    ${CMAKE_SOURCE_DIR}/include/uft/compat
    ${CMAKE_SOURCE_DIR}/include/uft/formats
    ${CMAKE_SOURCE_DIR}/include/uft/detect
    ${CMAKE_SOURCE_DIR}/include/uft/analysis
    ${CMAKE_SOURCE_DIR}/include/uft/forensic
    ${CMAKE_SOURCE_DIR}/include/uft/protection
    ${CMAKE_SOURCE_DIR}/include/fatfs
    ${CMAKE_SOURCE_DIR}/include/fhis
    ${CMAKE_SOURCE_DIR}/include/tracks
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/analysis
    ${CMAKE_SOURCE_DIR}/src/analysis/otdr
    ${CMAKE_SOURCE_DIR}/src/samdisk
    ${CMAKE_SOURCE_DIR}/src/algorithms
    ${CMAKE_SOURCE_DIR}/src/algorithms/disk
    ${CMAKE_SOURCE_DIR}/src/algorithms/bitstream
    ${CMAKE_SOURCE_DIR}/src/hardware_providers
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${CMAKE_SOURCE_DIR}/src/flux/fdc_bitstream
    ${CMAKE_SOURCE_DIR}/src/hal
    ${CMAKE_SOURCE_DIR}/src/protection
    ${CMAKE_SOURCE_DIR}/src/core/unified
)

# ============================================================================
# Linking
# ============================================================================

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(Qt6SerialPort_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::SerialPort)
endif()

if(UNIX AND NOT APPLE)
    if(LIBUSB_FOUND)
        target_include_directories(${PROJECT_NAME} PRIVATE ${LIBUSB_INCLUDE_DIRS})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBUSB_LIBRARIES})
    endif()
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi shell32 advapi32 ws2_32 setupapi)
endif()

# Compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UFT_HAS_HAL
    UFT_HAS_SWITCH
    UFT_HAS_CART7
    UFT_VERSION_STRING="${PROJECT_VERSION}"
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)

# ============================================================================
# Install
# ============================================================================

if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
    # Qt deploy handled by windeployqt in CI
else()
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES packaging/linux/UnifiedFloppyTool.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES resources/icons/app_256.png
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
            RENAME UnifiedFloppyTool.png)
    install(FILES tools/99-floppy-devices.rules
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/uft
            OPTIONAL)
endif()

endif()  # UFT_BUILD_APP

# ============================================================================
# CPack
# ============================================================================

set(CPACK_PACKAGE_NAME "UnifiedFloppyTool")
set(CPACK_PACKAGE_VENDOR "Axel Kramer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Forensic Floppy Disk Preservation Tool")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "axel@example.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "UnifiedFloppyTool")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.5), libqt6gui6 (>= 6.5), libqt6serialport6, libusb-1.0-0")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/Axel051171/UnifiedFloppyTool")
endif()

include(CPack)

# ============================================================================
# Tests (CMake-based unit test suite)
# ============================================================================
# Build library targets that the test suite links against, then add tests.
# The main app above uses a monolithic GLOB; tests use modular static libs.

option(UFT_BUILD_TESTS "Build the UFT test suite" ON)

if(UFT_BUILD_TESTS)
    enable_testing()

    # Library targets required by tests
    add_subdirectory(src/crc)
    add_subdirectory(src/core)
    add_subdirectory(src/protection/c64)
    add_subdirectory(src/hardware_providers)

    add_subdirectory(tests)
endif()
