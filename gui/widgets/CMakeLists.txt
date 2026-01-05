# =============================================================================
# UFT GUI Widgets CMakeLists.txt
# =============================================================================
# Version: 3.3.2
# Description: Qt6 widgets for UnifiedFloppyTool GUI
# =============================================================================

# Require Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Define the widgets library
add_library(uft_gui_widgets STATIC
    trackgridwidget.cpp
    trackgridwidget.h
    uft_pll_panel.cpp
    uft_pll_panel.h
    uft_recovery_panel.cpp
    uft_recovery_panel.h
    uft_flux_view_widget.cpp
    uft_flux_view_widget.h
)

# Enable automoc for Qt
set_target_properties(uft_gui_widgets PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
)

# Include directories
target_include_directories(uft_gui_widgets
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Link Qt libraries
target_link_libraries(uft_gui_widgets
    PUBLIC
        Qt6::Widgets
)

# Compile options
target_compile_features(uft_gui_widgets PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(uft_gui_widgets PRIVATE /W4)
else()
    target_compile_options(uft_gui_widgets PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wpedantic>)
endif()

# Install headers
install(FILES
    trackgridwidget.h
    uft_pll_panel.h
    uft_recovery_panel.h
    uft_flux_view_widget.h
    DESTINATION include/uft/gui/widgets
)

# Optional: Build test executable
option(UFT_BUILD_WIDGET_TESTS "Build GUI widget tests" OFF)

if(UFT_BUILD_WIDGET_TESTS)
    add_executable(test_widgets
        tests/test_widgets.cpp
    )
    target_link_libraries(test_widgets
        uft_gui_widgets
        Qt6::Widgets
    )
endif()
