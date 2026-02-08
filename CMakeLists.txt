# ============================================================================
# UnifiedFloppyTool - CMake Build System
# Version: 4.1.0
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
file(READ "${CMAKE_SOURCE_DIR}/UFT_VERSION.txt" UFT_VERSION_RAW)
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

# ============================================================================
# Dependencies
# ============================================================================

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt6 OPTIONAL_COMPONENTS SerialPort)

if(Qt6SerialPort_FOUND)
    add_compile_definitions(UFT_HAS_SERIALPORT)
    message(STATUS "Qt SerialPort: FOUND")
else()
    message(STATUS "Qt SerialPort: NOT FOUND (serial features disabled)")
endif()

# Platform libraries
if(UNIX AND NOT APPLE)
    find_package(PkgConfig)
    pkg_check_modules(LIBUSB libusb-1.0)
endif()

# ============================================================================
# Sources - collected via glob (mirrors .pro file)
# ============================================================================

file(GLOB_RECURSE UFT_SOURCES
    "src/*.cpp"
    "src/*.c"
)
file(GLOB_RECURSE UFT_HEADERS
    "include/*.h"
    "src/*.h"
)
file(GLOB UFT_FORMS "forms/*.ui")
set(UFT_RESOURCES resources/resources.qrc)

# Remove test files from main build
list(FILTER UFT_SOURCES EXCLUDE REGEX ".*/tests?/.*")
list(FILTER UFT_SOURCES EXCLUDE REGEX ".*/test_.*")

# Remove duplicate main.cpp (use src/gui/main.cpp)
list(FILTER UFT_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

# ============================================================================
# Target
# ============================================================================

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

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/fatfs
    ${CMAKE_SOURCE_DIR}/include/fhis
    ${CMAKE_SOURCE_DIR}/include/tracks
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
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
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi shell32 advapi32 ws2_32)
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
