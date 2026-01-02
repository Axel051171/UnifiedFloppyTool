###############################################################################
# UnifiedFloppyTool v3.2.0 - VISUAL Edition
# 
# NEW IN v3.2.0:
# - Dark Mode Toggle (Ctrl+D)
# - Status Tab with Hex Dump & Sector Info
# - Status LED Bar (Hardware connection indicator)
# - Disk Analyzer Window (HxC-style visualization)
# - Recent Files Menu
# - Drag & Drop support
# - Full Keyboard Shortcuts
# - Complete Menu Structure (File/Drive/Tools/Settings/Help)
# - Multi-Language support (Load external language files)
# - Tool Buttons: Label Editor, BAM/FAT Viewer, Bootblock, Protection
#
# EXISTING FEATURES:
# - 7 Tabs (Workflow, Status, Hardware, Settings, Protection, Catalog, Tools)
# - 11 Protection Profiles
# - DiskDupe (dd*) Detection + Expert Mode
# - Batch Operations, Disk Catalog, Comparison Tool, Health Analyzer
# - 175+ Parameters
###############################################################################

QT       += core gui widgets

greaterThan(QT_MAJOR_VERSION, 5): QT += widgets

CONFIG += c++17

TARGET = UnifiedFloppyTool
TEMPLATE = app

###############################################################################
# Source Files
###############################################################################

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
    src/decodejob.cpp

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
    src/inputvalidation.h \
    src/pathutils.h \
    src/settingsmanager.h

###############################################################################
# UI Forms (Qt Designer files)
###############################################################################

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

###############################################################################
# Include Paths
###############################################################################

INCLUDEPATH += src
INCLUDEPATH += include

###############################################################################
# Compiler-Specific Settings
###############################################################################

# ─── MSVC (Windows with Visual Studio) ──────────────────────────────────────
win32-msvc* {
    message("Compiler: MSVC (Visual Studio)")
    
    QMAKE_CXXFLAGS += /W3
    QMAKE_CFLAGS += /W3
    
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += /Od /Zi
        QMAKE_CFLAGS += /Od /Zi
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += /O2
        QMAKE_CFLAGS += /O2
    }
}

# ─── MinGW (Windows with GCC) ───────────────────────────────────────────────
win32-g++ {
    message("Compiler: MinGW (GCC on Windows)")
    
    QMAKE_CFLAGS += -std=c11
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3
        QMAKE_CFLAGS += -O3
    }
    
    LIBS += -lpthread
}

# ─── GCC/Clang (Linux) ──────────────────────────────────────────────────────
unix:!macx {
    message("Compiler: GCC/Clang (Linux)")
    
    QMAKE_CFLAGS += -std=c11
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3
        QMAKE_CFLAGS += -O3
    }
    
    LIBS += -lpthread
}

# ─── macOS ───────────────────────────────────────────────────────────────────
macx {
    message("Compiler: Clang (macOS)")
    
    QMAKE_CFLAGS += -std=c11
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3
        QMAKE_CFLAGS += -O3
    }
    
    ICON = resources/icon.icns
}

###############################################################################
# Version Info
###############################################################################

VERSION = 3.2.0
DEFINES += UFT_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_NAME=\\\"UnifiedFloppyTool\\\"
DEFINES += APP_EDITION=\\\"VISUAL\\\"

###############################################################################
# Resources
###############################################################################

RESOURCES += \
    resources/resources.qrc

###############################################################################
# Installation
###############################################################################

unix {
    target.path = /usr/local/bin
    INSTALLS += target
}

###############################################################################
# Build Messages
###############################################################################

message("════════════════════════════════════════════════════")
message("  UnifiedFloppyTool v3.2.0 - VISUAL Edition")
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
