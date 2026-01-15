#-------------------------------------------------
#
# UnifiedFloppyTool - Qt Project File
# Version: 3.7.8
# "Bei uns geht kein Bit verloren"
#
# Compatible with Qt 6.5+ (including 6.10.x)
# ALL hardware providers included
#
#-------------------------------------------------

QT += core gui widgets

# Optional modules - only add if installed
qtHaveModule(serialport): QT += serialport

TARGET = UnifiedFloppyTool
TEMPLATE = app

# Qt 6.10+ requires C++20
greaterThan(QT_MAJOR_VERSION, 5) {
    greaterThan(QT_MINOR_VERSION, 9) {
        CONFIG += c++20
    } else {
        CONFIG += c++17
    }
} else {
    CONFIG += c++17
}

CONFIG += sdk_no_version_check

# Compiler flags - suppress warnings for legacy code
# Compiler flags moved to platform-specific sections below
QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter

# Windows specific
win32 {
    LIBS += -lshlwapi -lshell32 -ladvapi32 -lws2_32
    DEFINES += _CRT_SECURE_NO_WARNINGS
}

# macOS specific
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
}

# Include paths
INCLUDEPATH += \
    $$PWD/src/core/unified \
    include \
    include/uft \
    include/uft/flux \
    src \
    src/samdisk \
    src/hardware_providers \
    src/widgets \
    src/flux/fdc_bitstream

# Forms
FORMS += \
    forms/mainwindow.ui \
    forms/diskanalyzer_window.ui \
    forms/dialog_validation.ui \
    forms/visualdisk.ui \
    forms/tab_explorer.ui \
    forms/tab_diagnostics.ui \
    forms/tab_forensic.ui \
    forms/tab_format.ui \
    forms/tab_hardware.ui \
    forms/tab_nibble.ui \
    forms/tab_protection.ui \
    forms/tab_status.ui \
    forms/tab_tools.ui \
    forms/visualdiskdialog.ui \
    forms/rawformatdialog.ui \
    forms/tab_workflow.ui \
    forms/tab_xcopy.ui

# Main GUI Sources
SOURCES += \
    src/advanceddialogs.cpp \
    src/main.cpp \
    src/mainwindow.cpp \
    src/diskanalyzerwindow.cpp \
    src/visualdisk.cpp \
    src/explorertab.cpp \
    src/forensictab.cpp \
    src/formattab.cpp \
    src/hardwaretab.cpp \
    src/nibbletab.cpp \
    src/protectiontab.cpp \
    src/statustab.cpp \
    src/toolstab.cpp \
    src/visualdiskdialog.cpp \
    src/rawformatdialog.cpp \
    src/workflowtab.cpp \
    src/xcopytab.cpp \
    src/decodejob.cpp \
    src/disk_image_validator.cpp \
    src/settingsmanager.cpp \
    src/gw_device_detector.cpp \
    src/gw_output_parser.cpp \
    src/qmake_stubs/uft_protection_stubs.cpp

# Main GUI Headers (CRITICAL for MOC!)
HEADERS += \
    src/advanceddialogs.h \
    src/mainwindow.h \
    src/diskanalyzerwindow.h \
    src/visualdisk.h \
    src/explorertab.h \
    src/forensictab.h \
    src/formattab.h \
    src/hardwaretab.h \
    src/nibbletab.h \
    src/protectiontab.h \
    src/statustab.h \
    src/toolstab.h \
    src/visualdiskdialog.h \
    src/rawformatdialog.h \
    src/workflowtab.h \
    src/xcopytab.h \
    src/decodejob.h \
    src/disk_image_validator.h \
    src/settingsmanager.h \
    src/gw_device_detector.h \
    src/gw_output_parser.h \
    src/inputvalidation.h \
    src/pathutils.h

# FDC Bitstream Sources (VFO/PLL implementations)
SOURCES += \
    src/flux/fdc_bitstream/bit_array.cpp \
    src/flux/fdc_bitstream/fdc_bitstream.cpp \
    src/flux/fdc_bitstream/fdc_crc.cpp \
    src/flux/fdc_bitstream/fdc_misc.cpp \
    src/flux/fdc_bitstream/fdc_vfo_base.cpp \
    src/flux/fdc_bitstream/mfm_codec.cpp \
    src/flux/fdc_bitstream/vfo_experimental.cpp \
    src/flux/fdc_bitstream/vfo_fixed.cpp \
    src/flux/fdc_bitstream/vfo_pid.cpp \
    src/flux/fdc_bitstream/vfo_pid2.cpp \
    src/flux/fdc_bitstream/vfo_pid3.cpp \
    src/flux/fdc_bitstream/vfo_simple.cpp \
    src/flux/fdc_bitstream/vfo_simple2.cpp

# ALL Hardware Provider Sources
SOURCES += \
    src/hardware_providers/hardwaremanager.cpp \
    src/hardware_providers/mockhardwareprovider.cpp \
    src/hardware_providers/greaseweazlehardwareprovider.cpp \
    src/hardware_providers/fluxenginehardwareprovider.cpp \
    src/hardware_providers/kryofluxhardwareprovider.cpp \
    src/hardware_providers/scphardwareprovider.cpp \
    src/hardware_providers/applesaucehardwareprovider.cpp \
    src/hardware_providers/fc5025hardwareprovider.cpp \
    src/hardware_providers/xum1541hardwareprovider.cpp \
    src/hardware_providers/catweaselhardwareprovider.cpp \
    src/hardware_providers/adfcopyhardwareprovider.cpp

# Hardware Provider Headers (CRITICAL for MOC!)
HEADERS += \
    src/hardware_providers/hardwareprovider.h \
    src/hardware_providers/hardwaremanager.h \
    src/hardware_providers/mockhardwareprovider.h \
    src/hardware_providers/greaseweazlehardwareprovider.h \
    src/hardware_providers/fluxenginehardwareprovider.h \
    src/hardware_providers/kryofluxhardwareprovider.h \
    src/hardware_providers/scphardwareprovider.h \
    src/hardware_providers/applesaucehardwareprovider.h \
    src/hardware_providers/fc5025hardwareprovider.h \
    src/hardware_providers/xum1541hardwareprovider.h \
    src/hardware_providers/catweaselhardwareprovider.h \
    src/hardware_providers/adfcopyhardwareprovider.h \
    src/hardware_providers/fc5025_usb.h \
    src/hardware_providers/xum1541_usb.h

# Widget Sources
SOURCES += \
    src/widgets/diskvisualizationwindow.cpp \
    src/widgets/fluxvisualizerwidget.cpp \
    src/widgets/parameterpanelwidget.cpp \
    src/widgets/presetmanager.cpp \
    src/widgets/recoveryworkflowwidget.cpp \
    src/widgets/trackgridwidget.cpp

# Widget Headers (CRITICAL for MOC - Q_OBJECT classes!)
HEADERS += \
    src/widgets/diskvisualizationwindow.h \
    src/widgets/fluxvisualizerwidget.h \
    src/widgets/parameterpanelwidget.h \
    src/widgets/presetmanager.h \
    src/widgets/recoveryworkflowwidget.h \
    src/widgets/trackgridwidget.h

# UFT Core Headers
HEADERS += \
    include/uft/uft_common.h \
    include/uft/uft_version.h \
    include/uft/uft_types.h \
    include/uft/uft_error.h \
    include/uft/uft_protection.h

# Default deployment
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# ═══════════════════════════════════════════════════════════════════════════════
# MSVC-specific settings
# ═══════════════════════════════════════════════════════════════════════════════
win32-msvc* {
    QMAKE_CXXFLAGS += /W3 /WX-
    QMAKE_CXXFLAGS_RELEASE += /O2
    LIBS += shlwapi.lib
}

# ═══════════════════════════════════════════════════════════════════════════════
# GCC/Clang-specific settings (Linux, macOS)
# ═══════════════════════════════════════════════════════════════════════════════
unix|macx {
    QMAKE_CXXFLAGS += -Wall -Wextra -Wno-unknown-pragmas -Wno-sign-compare -Wno-unused-parameter
}

# ═══════════════════════════════════════════════════════════════════════════════
# FORMAT PARSERS (P0-003)
# ═══════════════════════════════════════════════════════════════════════════════

INCLUDEPATH += $$PWD/src/formats
    $$PWD/src/core/unified \

# D64 - Commodore 64 (most important for preservation)
SOURCES += \
    src/formats/d64/uft_d64_parser_v3.c \

# G64 - Commodore 64 with timing data
SOURCES += \
    src/formats/g64/uft_g64_parser_v3.c \

# ADF - Amiga
SOURCES += \
    src/formats/adf/uft_adf_parser_v3.c \

# SCP - SuperCard Pro raw flux
SOURCES += \
    src/formats/scp/uft_scp_parser_v3.c \

# IMD - ImageDisk
SOURCES += \
    src/formats/imd/uft_imd_parser_v3.c \

# DSK - Standard disk image
SOURCES += \
    src/formats/dsk/uft_dsk_parser_v3.c

# STX - Atari ST with protection
SOURCES += \
    src/formats/stx/uft_stx_parser_v3.c

# ═══════════════════════════════════════════════════════════════════════════════
# God-Mode Algorithms
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/algorithms/god_mode/uft_god_mode_api.c \
    src/algorithms/god_mode/uft_kalman_pll_v2.c \
    src/algorithms/god_mode/uft_gcr_viterbi.c \
    src/algorithms/god_mode/uft_gcr_viterbi_v2.c \
    src/algorithms/god_mode/uft_bayesian_detect.c \
    src/algorithms/god_mode/uft_bayesian_detect_v2.c \
    src/algorithms/god_mode/uft_multi_rev_fusion.c \
    src/algorithms/god_mode/uft_crc_correction_v2.c \
    src/algorithms/god_mode/uft_fuzzy_sync_v2.c \
    src/algorithms/god_mode/uft_decoder_metrics.c

HEADERS += \
    include/uft/uft_god_mode.h \
    include/uft/uft_format_probes.h

# ═══════════════════════════════════════════════════════════════════════════════
# UFT Smart Pipeline
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/core/uft_smart_open.c

HEADERS += \
    include/uft/uft_smart_open.h

# ═══════════════════════════════════════════════════════════════════════════════
# UFT Advanced Mode
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += src/core/uft_advanced_mode.c

HEADERS += include/uft/uft_advanced_mode.h
HEADERS += include/uft/uft_v3_bridge.h

# ═══════════════════════════════════════════════════════════════════════════════
# Additional Format Parsers
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/formats/d71/uft_d71.c \
    src/formats/d80/uft_d80.c \
    src/formats/d81/uft_d81.c \
    src/formats/d82/uft_d82.c \
    src/formats/g71/uft_g71.c \
    src/formats/atr/uft_atr.c \
    src/formats/dmk/uft_dmk.c \
    src/formats/trd/uft_trd.c

# ═══════════════════════════════════════════════════════════════════════════════
# Core Format Registry
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/formats/uft_format_registry.c \
    src/formats/uft_v3_bridge.c \
    src/core/uft_format_plugin.c

# ═══════════════════════════════════════════════════════════════════════════════
# Track Analysis
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/uft_track_analysis.c
