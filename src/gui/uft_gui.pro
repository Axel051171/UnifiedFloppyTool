# UFT GUI - qmake Project File
# For use with Qt Creator

QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

CONFIG += c++17
CONFIG += sdk_no_version_check

TARGET = uft_gui
TEMPLATE = app

# Application info
VERSION = 5.32.0
QMAKE_TARGET_COMPANY = UFT
QMAKE_TARGET_PRODUCT = UnifiedFloppyTool
QMAKE_TARGET_DESCRIPTION = Floppy Disk Preservation Tool
QMAKE_TARGET_COPYRIGHT = 2025

# Source files
SOURCES += \
    main.cpp \
    uft_main_window.cpp \
    uft_flux_panel.cpp \
    uft_format_panel.cpp \
    uft_xcopy_panel.cpp \
    uft_nibble_recovery_panel.cpp \
    uft_forensic_protection_panel.cpp \
    uft_hex_file_panel.cpp \
    uft_hardware_panel.cpp \
    uft_track_grid_widget.cpp

# Header files
HEADERS += \
    uft_main_window.h \
    uft_flux_panel.h \
    uft_format_panel.h \
    uft_xcopy_panel.h \
    uft_nibble_panel.h \
    uft_recovery_panel.h \
    uft_forensic_panel.h \
    uft_protection_panel.h \
    uft_hex_viewer_panel.h \
    uft_file_browser_panel.h \
    uft_hardware_panel.h \
    uft_track_grid_widget.h

# UI files
FORMS += \
    uft_main_window.ui

# Resources
# RESOURCES += resources.qrc

# Include paths
INCLUDEPATH += \
    $$PWD \
    $$PWD/../include

# Platform-specific settings
win32 {
    # Windows icon
    RC_ICONS = uft.ico
    
    # Hide console
    CONFIG += windows
}

macx {
    # macOS icon
    ICON = uft.icns
    
    # Bundle info
    QMAKE_INFO_PLIST = Info.plist
}

unix:!macx {
    # Linux desktop file
    desktop.path = /usr/share/applications
    desktop.files = uft.desktop
    INSTALLS += desktop
}

# Installation
target.path = /usr/local/bin
INSTALLS += target

# Compiler flags
QMAKE_CXXFLAGS += -Wall -Wextra
QMAKE_CXXFLAGS_RELEASE += -O2

# Default rules for deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
