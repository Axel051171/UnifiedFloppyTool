# ═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool v3.2.0 - VISUAL Edition
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

# Build info
win32: message("Compiler: MSVC")
macx: message("Compiler: Clang (macOS)")
unix:!macx: message("Compiler: GCC/Clang (Linux)")
message("════════════════════════════════════════════════════")
message("  UnifiedFloppyTool v$$VERSION - VISUAL Edition")
message("════════════════════════════════════════════════════")

# Platform specific
win32 {
    exists(resources/icon.ico): RC_ICONS = resources/icon.ico
    DEFINES += WIN32_LEAN_AND_MEAN NOMINMAX
}

macx {
    exists(resources/icon.icns): ICON = resources/icon.icns
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
}

# Sources
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

# Headers
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

# UI Forms
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

# Resources
exists(resources/resources.qrc): RESOURCES += resources/resources.qrc

# Include paths
INCLUDEPATH += src include include/uft

# Compiler flags
gcc|clang {
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CXXFLAGS_RELEASE += -O2
}

msvc {
    QMAKE_CXXFLAGS += /W3 /utf-8
    QMAKE_CXXFLAGS_RELEASE += /O2
}

# Output directories
CONFIG(debug, debug|release) {
    DEFINES += DEBUG_BUILD UFT_DEBUG_MEMORY
    DESTDIR = debug
} else {
    DEFINES += RELEASE_BUILD NDEBUG
    DESTDIR = release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui
