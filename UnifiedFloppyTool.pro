# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.2.0 - VISUAL Edition
# ═══════════════════════════════════════════════════════════════════════════════
# Cross-platform floppy disk preservation and analysis tool
# Supports: Windows, macOS, Linux
# ═══════════════════════════════════════════════════════════════════════════════

QT += core gui widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += core5compat

CONFIG += c++17
CONFIG += sdk_no_version_check

TARGET = UnifiedFloppyTool
TEMPLATE = app

VERSION = 3.2.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_NAME=\\\"UnifiedFloppyTool\\\"
DEFINES += APP_EDITION=\\\"VISUAL\\\"
DEFINES += UFT_VERSION=\\\"$$VERSION\\\"

# ═══════════════════════════════════════════════════════════════════════════════
# Build Information Messages
# ═══════════════════════════════════════════════════════════════════════════════
win32: message("Compiler: MSVC (Visual Studio)")
macx: message("Compiler: Clang (macOS)")
unix:!macx: message("Compiler: GCC/Clang (Linux)")

message("════════════════════════════════════════════════════")
message("  UnifiedFloppyTool v$$VERSION - VISUAL Edition")
message("════════════════════════════════════════════════════")
message("Qt version: $$QT_VERSION")
message("Compiler: $$QMAKESPEC")
message("Target: $$TARGET")
message("════════════════════════════════════════════════════")
message("NEW IN v3.2.0:")
message("  ✅ Dark Mode Toggle (Ctrl+D)")
message("  ✅ Status Tab with Hex Dump")
message("  ✅ Status LED Bar")
message("  ✅ Disk Analyzer Window")
message("  ✅ Recent Files Menu")
message("  ✅ Drag & Drop Support")
message("  ✅ Full Keyboard Shortcuts")
message("════════════════════════════════════════════════════")

# ═══════════════════════════════════════════════════════════════════════════════
# Platform-Specific Settings
# ═══════════════════════════════════════════════════════════════════════════════

# Windows
win32 {
    # Optional icon - only include if file exists
    exists(resources/icon.ico): RC_ICONS = resources/icon.ico
    
    DEFINES += WIN32_LEAN_AND_MEAN
    DEFINES += NOMINMAX
    
    # MSVC specific
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_CXXFLAGS_WARN_ON += /W3
}

# macOS
macx {
    # Optional icon - only include if file exists
    exists(resources/icon.icns): ICON = resources/icon.icns
    
    # Optional Info.plist
    exists(resources/Info.plist): QMAKE_INFO_PLIST = resources/Info.plist
    
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
}

# Linux
unix:!macx {
    QMAKE_CXXFLAGS += -Wno-unused-parameter
}

# ═══════════════════════════════════════════════════════════════════════════════
# Source Files
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/visualdisk.cpp \
    src/workflowtab.cpp \
    src/statustab.cpp \
    src/hardwaretab.cpp \
    src/formattab.cpp \
    src/protectiontab.cpp \
    src/catalogtab.cpp \
    src/toolstab.cpp \
    src/diskanalyzerwindow.cpp \
    src/decodejob.cpp \
    src/settingsmanager.cpp

# ═══════════════════════════════════════════════════════════════════════════════
# Header Files
# ═══════════════════════════════════════════════════════════════════════════════

HEADERS += \
    src/mainwindow.h \
    src/visualdisk.h \
    src/workflowtab.h \
    src/statustab.h \
    src/hardwaretab.h \
    src/formattab.h \
    src/protectiontab.h \
    src/catalogtab.h \
    src/toolstab.h \
    src/diskanalyzerwindow.h \
    src/decodejob.h \
    src/settingsmanager.h

# ═══════════════════════════════════════════════════════════════════════════════
# UI Forms (Qt Designer)
# ═══════════════════════════════════════════════════════════════════════════════

FORMS += \
    forms/mainwindow.ui \
    forms/tab_workflow.ui \
    forms/tab_status.ui \
    forms/tab_hardware.ui \
    forms/tab_format.ui \
    forms/tab_protection.ui \
    forms/tab_catalog.ui \
    forms/tab_tools.ui \
    forms/visualdisk.ui \
    forms/diskanalyzer_window.ui \
    forms/dialog_validation.ui

# ═══════════════════════════════════════════════════════════════════════════════
# Resources (Optional)
# ═══════════════════════════════════════════════════════════════════════════════

exists(resources/resources.qrc): RESOURCES += resources/resources.qrc

# ═══════════════════════════════════════════════════════════════════════════════
# Include Paths
# ═══════════════════════════════════════════════════════════════════════════════

INCLUDEPATH += \
    src \
    include \
    include/uft

# ═══════════════════════════════════════════════════════════════════════════════
# Compiler Flags
# ═══════════════════════════════════════════════════════════════════════════════

# GCC/Clang
gcc|clang {
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CXXFLAGS_RELEASE += -O2
    QMAKE_CXXFLAGS_DEBUG += -g -O0
}

# MSVC
msvc {
    QMAKE_CXXFLAGS += /W3
    QMAKE_CXXFLAGS_RELEASE += /O2
}

# ═══════════════════════════════════════════════════════════════════════════════
# Build Configuration
# ═══════════════════════════════════════════════════════════════════════════════

CONFIG(debug, debug|release) {
    DEFINES += DEBUG_BUILD
    DEFINES += UFT_DEBUG_MEMORY
    DESTDIR = debug
} else {
    DEFINES += RELEASE_BUILD
    DEFINES += NDEBUG
    DESTDIR = release
}

# Output directories
OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui

# ═══════════════════════════════════════════════════════════════════════════════
# End of UnifiedFloppyTool.pro
# ═══════════════════════════════════════════════════════════════════════════════
