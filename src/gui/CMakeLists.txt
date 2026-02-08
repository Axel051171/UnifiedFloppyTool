# UFT GUI - CMake Build Configuration
# Qt 6.x Required

cmake_minimum_required(VERSION 3.20)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

# Enable Qt MOC/UIC/RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Source files
set(UFT_GUI_SOURCES
    main.cpp
    uft_main_window.cpp
    uft_flux_panel.cpp
    uft_format_panel.cpp
    uft_xcopy_panel.cpp
    uft_nibble_recovery_panel.cpp
    uft_forensic_protection_panel.cpp
    uft_hex_file_panel.cpp
    uft_hardware_panel.cpp
    uft_track_grid_widget.cpp
    uft_gw2dmk_panel.cpp
    uft_dmk_analyzer_panel.cpp
    uft_flux_histogram_widget.cpp
    # P2-12: AnalyzerToolbar
    AnalyzerToolbar.cpp
)

# Header files
set(UFT_GUI_HEADERS
    uft_main_window.h
    uft_flux_panel.h
    uft_format_panel.h
    uft_xcopy_panel.h
    uft_nibble_panel.h
    uft_recovery_panel.h
    uft_forensic_panel.h
    uft_protection_panel.h
    uft_hex_viewer_panel.h
    uft_file_browser_panel.h
    uft_hardware_panel.h
    uft_track_grid_widget.h
    uft_gw2dmk_panel.h
    uft_dmk_analyzer_panel.h
    uft_flux_histogram_widget.h
    # P2-12: AnalyzerToolbar
    AnalyzerToolbar.h
)

# UI files
set(UFT_GUI_UI
    uft_main_window.ui
)

# Resources (optional)
# set(UFT_GUI_RESOURCES
#     resources.qrc
# )

# Create executable
add_executable(uft_gui
    ${UFT_GUI_SOURCES}
    ${UFT_GUI_HEADERS}
    ${UFT_GUI_UI}
    # ${UFT_GUI_RESOURCES}
)

# Link Qt libraries
target_link_libraries(uft_gui
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    # Link UFT core library
    # uft_core
)

# Include directories
target_include_directories(uft_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Set C++ standard
set_target_properties(uft_gui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Windows-specific: Hide console window
if(WIN32)
    set_target_properties(uft_gui PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# macOS-specific: Create app bundle
if(APPLE)
    set_target_properties(uft_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_STRING "UnifiedFloppyTool"
        MACOSX_BUNDLE_ICON_FILE "uft.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.uft.app"
        MACOSX_BUNDLE_BUNDLE_NAME "UFT"
        MACOSX_BUNDLE_BUNDLE_VERSION "3.8.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "3.8"
    )
endif()

# Installation
install(TARGETS uft_gui
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
