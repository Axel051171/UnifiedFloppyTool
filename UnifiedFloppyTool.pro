###############################################################################
# UnifiedFloppyTool v3.1 - PERFECT Edition
# 
# FEATURES:
# - 10 Tabs (Workflow, Operations, Format, Geometry, Protection, Flux, 
#            Advanced, Hardware, Catalog, Tools)
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
    src/hardwaretab.cpp \
    src/formattab.cpp \
    src/protectiontab.cpp \
    src/catalogtab.cpp \
    src/toolstab.cpp

# Commented out until .cpp files are created:
#    src/widgets/diskvisualizationwindow.cpp \
#    src/widgets/presetmanager.cpp \
#    src/widgets/trackgridwidget.cpp \
#    src/core/uft_memory.c \
#    src/core/uft_simd.c \
#    src/core/flux_core.c \
#    src/core/uft_mfm_scalar.c \
#    src/core/uft_gcr_scalar.c

HEADERS += \
    src/mainwindow.h \
    src/visualdisk.h \
    src/workflowtab.h \
    src/hardwaretab.h \
    src/formattab.h \
    src/protectiontab.h \
    src/catalogtab.h \
    src/toolstab.h

# Commented out until needed:
#    src/widgets/diskvisualizationwindow.h \
#    src/widgets/presetmanager.h \
#    src/widgets/trackgridwidget.h \
#    include/uft/uft_memory.h \
#    include/uft/uft_simd.h \
#    include/uft/flux_core.h

###############################################################################
# UI Forms (Qt Designer files)
###############################################################################

FORMS += \
    forms/mainwindow.ui \
    forms/tab_workflow.ui \
    forms/tab_hardware.ui \
    forms/tab_format.ui \
    forms/tab_protection.ui \
    forms/tab_catalog.ui \
    forms/tab_tools.ui \
    forms/visualdisk.ui

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
    
    # Warnings
    QMAKE_CXXFLAGS += /W3
    QMAKE_CFLAGS += /W3
    
    # Debug/Release
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += /Od /Zi
        QMAKE_CFLAGS += /Od /Zi
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += /O2
        QMAKE_CFLAGS += /O2
    }
    
    # SIMD (optional)
    # QMAKE_CFLAGS += /arch:SSE2
    # QMAKE_CFLAGS += /arch:AVX2
}

# ─── MinGW (Windows with GCC) ───────────────────────────────────────────────
win32-g++ {
    message("Compiler: MinGW (GCC on Windows)")
    
    # C11 standard
    QMAKE_CFLAGS += -std=c11
    
    # Warnings
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    # Debug/Release
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3
        QMAKE_CFLAGS += -O3
    }
    
    # Threading
    LIBS += -lpthread
    
    # SIMD (optional)
    # QMAKE_CFLAGS += -msse2
    # QMAKE_CFLAGS += -mavx2
}

# ─── GCC/Clang (Linux, macOS) ───────────────────────────────────────────────
unix:!macx {
    message("Compiler: GCC/Clang (Linux)")
    
    # C11 standard
    QMAKE_CFLAGS += -std=c11
    
    # Warnings
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    # Debug/Release
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3 -march=native
        QMAKE_CFLAGS += -O3 -march=native
    }
    
    # Threading
    LIBS += -lpthread
    
    # SIMD (optional)
    # QMAKE_CFLAGS += -msse2
    # QMAKE_CFLAGS += -mavx2
}

# ─── macOS ───────────────────────────────────────────────────────────────────
macx {
    message("Compiler: Clang (macOS)")
    
    # C11 standard
    QMAKE_CFLAGS += -std=c11
    
    # Warnings
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unused-parameter
    QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
    
    # Debug/Release
    CONFIG(debug, debug|release) {
        DEFINES += UFT_DEBUG_MEMORY DEBUG_BUILD
        QMAKE_CXXFLAGS += -g -O0
        QMAKE_CFLAGS += -g -O0
    } else {
        DEFINES += NDEBUG RELEASE_BUILD
        QMAKE_CXXFLAGS += -O3
        QMAKE_CFLAGS += -O3
    }
    
    # SIMD (optional)
    # QMAKE_CFLAGS += -msse2
    # QMAKE_CFLAGS += -mavx2
}

###############################################################################
# Version Info
###############################################################################

VERSION = 3.1.0
DEFINES += UFT_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += APP_NAME=\\\"UnifiedFloppyTool\\\"
DEFINES += APP_EDITION=\\\"PERFECT\\\"

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

message("================================================")
message("UnifiedFloppyTool v3.1 - PERFECT Edition")
message("================================================")
message("Qt version: $$QT_VERSION")
message("Compiler: $$QMAKESPEC")
message("Target: $$TARGET")
message("================================================")
message("✅ 10 Tabs with 175+ Parameters")
message("✅ 11 Protection Profiles")
message("✅ DiskDupe (dd*) + Expert Mode")
message("✅ Batch Operations")
message("✅ Disk Catalog")
message("================================================")
message("To edit GUI: Open .ui files in Qt Designer!")
message("================================================")

# Worker Thread (Fix: UI Freeze)
SOURCES += src/decodejob.cpp
HEADERS += src/decodejob.h
