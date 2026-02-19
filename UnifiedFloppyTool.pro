#-------------------------------------------------
#
# UnifiedFloppyTool - Qt Project File
# Version: 4.1.0
# "Bei uns geht kein Bit verloren"
#
# Compatible with Qt 6.5+ (including 6.10.x)
# ALL hardware providers included
#
#-------------------------------------------------

QT += core gui widgets

# Try to use SerialPort if available
packagesExist(Qt6SerialPort) | packagesExist(Qt5SerialPort) {
    QT += serialport
    DEFINES += UFT_HAS_SERIALPORT
    message("Qt SerialPort found")
}

# Also check with qtHaveModule (Qt 6 style)
qtHaveModule(serialport) {
    QT += serialport
    DEFINES += UFT_HAS_SERIALPORT  
    message("Qt SerialPort found via qtHaveModule")
}

# ═══════════════════════════════════════════════════════════════════════════
# HAL (Hardware Abstraction Layer) - ALWAYS ENABLED
# ═══════════════════════════════════════════════════════════════════════════
DEFINES += UFT_HAS_HAL
message("HAL (Hardware Abstraction Layer) ENABLED")

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

# Enable console output for debugging (remove for release)
win32:CONFIG += console

# Compiler flags - suppress warnings for legacy code
# Compiler flags moved to platform-specific sections below
unix:QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter
win32-g++:QMAKE_CFLAGS += -Wall -Wextra \
    -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
    -Wno-unused-variable -Wno-unused-const-variable \
    -Wno-stringop-truncation -Wno-type-limits -Wno-pragmas
win32-g++:QMAKE_CXXFLAGS += -Wall -Wextra \
    -Wno-unused-parameter -Wno-unused-function -Wno-sign-compare \
    -Wno-unused-variable -Wno-unused-const-variable \
    -Wno-stringop-truncation -Wno-type-limits -Wno-pragmas

# Windows specific
win32 {
    LIBS += -lshlwapi -lshell32 -ladvapi32 -lws2_32 -lsetupapi
    DEFINES += _CRT_SECURE_NO_WARNINGS
    # POSIX shims for hactool (getopt.h, strings.h)
    INCLUDEPATH += $$PWD/src/switch/hactool/compat
    SOURCES += src/switch/hactool/compat/getopt.c
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
    include/uft/hal \
    include/uft/compat \
    include/uft/formats \
    include/uft/detect \
    include/uft/analysis \
    src \
    src/gui \
    src/analysis/otdr \
    src/samdisk \
    src/hardware_providers \
    src/widgets \
    src/flux/fdc_bitstream \
    src/hal

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
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
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
    src/qmake_stubs/uft_protection_stubs.cpp \
    src/gui/uft_otdr_panel.cpp \
    src/flux/uft_scp_parser.c

# Main GUI Headers (CRITICAL for MOC!)
HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/formats/uft_dms.h \
    include/uft/analysis/mfm_detect.h \
    include/uft/analysis/cpm_fs.h \
    include/uft/analysis/uft_mfm_detect_bridge.h \
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
    src/pathutils.h \
    src/gui/uft_otdr_panel.h \
    include/uft/flux/uft_scp_parser.h

# FDC Bitstream Sources (VFO/PLL implementations)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
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
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
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
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
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
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/widgets/diskvisualizationwindow.cpp \
    src/widgets/fluxvisualizerwidget.cpp \
    src/widgets/parameterpanelwidget.cpp \
    src/widgets/presetmanager.cpp \
    src/widgets/recoveryworkflowwidget.cpp \
    src/widgets/trackgridwidget.cpp

# Widget Headers (CRITICAL for MOC - Q_OBJECT classes!)
HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    src/widgets/diskvisualizationwindow.h \
    src/widgets/fluxvisualizerwidget.h \
    src/widgets/parameterpanelwidget.h \
    src/widgets/presetmanager.h \
    src/widgets/recoveryworkflowwidget.h \
    src/widgets/trackgridwidget.h

# UFT Core Headers
HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
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

INCLUDEPATH += \
    $$PWD/src/formats \
    $$PWD/src/core/unified

# D64 - Commodore 64 (most important for preservation)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/d64/uft_d64_parser_v3.c

# G64 - Commodore 64 with timing data
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/g64/uft_g64_parser_v3.c

# ADF - Amiga
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/adf/uft_adf_parser_v3.c

# HDF - Amiga Hard Disk (P1 Feature)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/uft_hdf_parser.c

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    src/formats/uft_hdf_parser.h

# OTDR Signal Analysis
HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/analysis/floppy_otdr.h \
    include/uft/analysis/tdfc.h \
    include/uft/analysis/uft_otdr_bridge.h \
    src/analysis/otdr/FloppyOtdrWidget.h

# SCP - SuperCard Pro raw flux
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/scp/uft_scp_parser_v3.c

# IMD - ImageDisk
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/imd/uft_imd_parser_v3.c

# DSK - Standard disk image
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/dsk/uft_dsk_parser_v3.c

# STX - Atari ST with protection
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/stx/uft_stx_parser_v3.c \
    src/formats/stx/uft_stx_air.c

# ═══════════════════════════════════════════════════════════════════════════════
# God-Mode Algorithms
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
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
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/uft_god_mode.h \
    include/uft/uft_format_probes.h

# ═══════════════════════════════════════════════════════════════════════════════
# UFT Smart Pipeline
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/core/uft_smart_open.c \
    src/core/uft_unified_types.c

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/uft_smart_open.h \
    include/uft/core/uft_unified_types.h

# ═══════════════════════════════════════════════════════════════════════════════
# UFT Advanced Mode
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += src/core/uft_advanced_mode.c \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

HEADERS += include/uft/uft_advanced_mode.h \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h
HEADERS += include/uft/uft_v3_bridge.h \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h

# ═══════════════════════════════════════════════════════════════════════════════
# Additional Format Parsers
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
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
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/uft_format_registry.c \
    src/formats/uft_v3_bridge.c \
    src/core/uft_format_plugin.c

# ═══════════════════════════════════════════════════════════════════════════════
# Track Analysis
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/analysis/uft_track_analysis.c \
    src/analysis/otdr/floppy_otdr.c \
    src/analysis/otdr/tdfc.c \
    src/analysis/otdr/tdfc_plus.c \
    src/analysis/otdr/uft_otdr_bridge.c

# ═══════════════════════════════════════════════════════════════════════════════
# AmigaDOS Extended (P2 Feature - inspired by amitools)
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/fs/uft_amigados_extended.c

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/fs/uft_amigados_extended.h

# ═══════════════════════════════════════════════════════════════════════════════

# ═══════════════════════════════════════════════════════════════════════════════
# ═══════════════════════════════════════════════════════════════════════════════
# Core stubs (minimal functions from uft_core.c)
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/core/uft_core_stubs.c

# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2: Additional Disk Image Formats
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/hfe/uft_hfe.c \
    src/formats/sad/uft_sad.c \
    src/formats/scl/uft_scl.c \
    src/formats/ssd/uft_ssd.c \
    src/formats/td0/uft_td0.c \
    src/formats/udi/uft_udi.c \
    src/formats/dsk_cpc/uft_dsk_cpc.c \
    src/formats/tzx/uft_tzx_wav.c \
    src/formats/tzx/uft_zxtap.c \
    src/formats/img/uft_img.c \
    src/formats/imz/uft_imz.c \
    src/formats/cqm/uft_cqm.c \
    src/formats/uff/uft_uff.c \
    src/formats/ldbs/uft_ldbs.c \
    src/formats/cmd_fd/uft_cmd_fd.c \
    src/formats/motorola/uft_versados.c \
    src/formats/soviet/uft_bk0010.c


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2b: v3 GOD MODE Parsers (379 format parsers)
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/2img/uft_2img_parser_v3.c \
    src/formats/2sf/uft_2sf_parser_v3.c \
    src/formats/7z/uft_7z_parser_v3.c \
    src/formats/a26/uft_a26_parser_v3.c \
    src/formats/a2r/uft_a2r_parser_v3.c \
    src/formats/a52/uft_a52_parser_v3.c \
    src/formats/a78/uft_a78_parser_v3.c \
    src/formats/aac/uft_aac_parser_v3.c \
    src/formats/ace/uft_ace_parser_v3.c \
    src/formats/adf_arc/uft_adf_arc_parser_v3.c \
    src/formats/adl/uft_adl_parser_v3.c \
    src/formats/adx/uft_adx_parser_v3.c \
    src/formats/adz/uft_adz_parser_v3.c \
    src/formats/alt/uft_alt_parser_v3.c \
    src/formats/apf/uft_apf_parser_v3.c \
    src/formats/apx/uft_apx_parser_v3.c \
    src/formats/aqu/uft_aqu_parser_v3.c \
    src/formats/arc/uft_arc_parser_v3.c \
    src/formats/asf/uft_asf_parser_v3.c \
    src/formats/ast/uft_ast_parser_v3.c \
    src/formats/atr/uft_atr_parser_v3.c \
    src/formats/atx/uft_atx_parser_v3.c \
    src/formats/avi/uft_avi_parser_v3.c \
    src/formats/avif/uft_avif_parser_v3.c \
    src/formats/axex/uft_axex_parser_v3.c \
    src/formats/ay/uft_ay_parser_v3.c \
    src/formats/bal/uft_bal_parser_v3.c \
    src/formats/bin/uft_bin_parser_v3.c \
    src/formats/bkm/uft_bkm_parser_v3.c \
    src/formats/bmp/uft_bmp_parser_v3.c \
    src/formats/bps/uft_bps_parser_v3.c \
    src/formats/bst/uft_bst_parser_v3.c \
    src/formats/bsx/uft_bsx_parser_v3.c \
    src/formats/bz2/uft_bz2_parser_v3.c \
    src/formats/c128/uft_c128_parser_v3.c \
    src/formats/c16/uft_c16_parser_v3.c \
    src/formats/car/uft_car_parser_v3.c \
    src/formats/cas/uft_cas_parser_v3.c \
    src/formats/cd32/uft_cd32_parser_v3.c \
    src/formats/cdi/uft_cdi_parser_v3.c \
    src/formats/cdtv/uft_cdtv_parser_v3.c \
    src/formats/cgb/uft_cgb_parser_v3.c \
    src/formats/ch8/uft_ch8_parser_v3.c \
    src/formats/chd/uft_chd_parser_v3.c \
    src/formats/chf/uft_chf_parser_v3.c \
    src/formats/cht/uft_cht_parser_v3.c \
    src/formats/cia/uft_cia_parser_v3.c \
    src/formats/col/uft_col_parser_v3.c \
    src/formats/com/uft_com_parser_v3.c \
    src/formats/cps/uft_cps_parser_v3.c \
    src/formats/cqm/uft_cqm_parser_v3.c \
    src/formats/crt/uft_crt_parser_v3.c \
    src/formats/cso/uft_cso_parser_v3.c \
    src/formats/css/uft_css_parser_v3.c \
    src/formats/csvx/uft_csvx_parser_v3.c \
    src/formats/ctr/uft_ctr_parser_v3.c \
    src/formats/cue/uft_cue_parser_v3.c \
    src/formats/cv/uft_cv_parser_v3.c \
    src/formats/d1m/uft_d1m_parser_v3.c \
    src/formats/d2m/uft_d2m_parser_v3.c \
    src/formats/d4m/uft_d4m_parser_v3.c \
    src/formats/d67/uft_d67_parser_v3.c \
    src/formats/d71/uft_d71_parser_v3.c \
    src/formats/d77/uft_d77_parser_v3.c \
    src/formats/d80/uft_d80_parser_v3.c \
    src/formats/d81/uft_d81_parser_v3.c \
    src/formats/d82/uft_d82_parser_v3.c \
    src/formats/d88/uft_d88_parser_v3.c \
    src/formats/dae/uft_dae_parser_v3.c \
    src/formats/dart/uft_dart_parser_v3.c \
    src/formats/dbf/uft_dbf_parser_v3.c \
    src/formats/dc42/uft_dc42_parser_v3.c \
    src/formats/dcm/uft_dcm_parser_v3.c \
    src/formats/dds/uft_dds_parser_v3.c \
    src/formats/dfi/uft_dfi_parser_v3.c \
    src/formats/dhd/uft_dhd_parser_v3.c \
    src/formats/dim/uft_dim_parser_v3.c \
    src/formats/dmg/uft_dmg_parser_v3.c \
    src/formats/dmk/uft_dmk_parser_v3.c \
    src/formats/dms/uft_dms_parser_v3.c \
    src/formats/dms/uft_dms.c \
    src/detect/mfm/mfm_detect.c \
    src/detect/mfm/cpm_fs.c \
    src/detect/mfm/uft_mfm_detect_bridge.c \
    src/formats/dnp/uft_dnp_parser_v3.c \
    src/formats/do/uft_do_parser_v3.c \
    src/formats/dol/uft_dol_parser_v3.c \
    src/formats/dsk_ace/uft_dsk_ace_parser_v3.c \
    src/formats/dsk_agt/uft_dsk_agt_parser_v3.c \
    src/formats/dsk_ak/uft_dsk_ak_parser_v3.c \
    src/formats/dsk_alp/uft_dsk_alp_parser_v3.c \
    src/formats/dsk_aq/uft_dsk_aq_parser_v3.c \
    src/formats/dsk_bk/uft_dsk_bk_parser_v3.c \
    src/formats/dsk_bw/uft_dsk_bw_parser_v3.c \
    src/formats/dsk_cg/uft_dsk_cg_parser_v3.c \
    src/formats/dsk_cro/uft_dsk_cro_parser_v3.c \
    src/formats/dsk_dc42/uft_dsk_dc42_parser_v3.c \
    src/formats/dsk_ein/uft_dsk_ein_parser_v3.c \
    src/formats/dsk_emu/uft_dsk_emu_parser_v3.c \
    src/formats/dsk_eqx/uft_dsk_eqx_parser_v3.c \
    src/formats/dsk_flex/uft_dsk_flex_parser_v3.c \
    src/formats/dsk_fm7/uft_dsk_fm7_parser_v3.c \
    src/formats/dsk_fp/uft_dsk_fp_parser_v3.c \
    src/formats/dsk_hk/uft_dsk_hk_parser_v3.c \
    src/formats/dsk_hp/uft_dsk_hp_parser_v3.c \
    src/formats/dsk_kc/uft_dsk_kc_parser_v3.c \
    src/formats/dsk_krg/uft_dsk_krg_parser_v3.c \
    src/formats/dsk_lyn/uft_dsk_lyn_parser_v3.c \
    src/formats/dsk_m5/uft_dsk_m5_parser_v3.c \
    src/formats/dsk_msx/uft_dsk_msx_parser_v3.c \
    src/formats/dsk_mtx/uft_dsk_mtx_parser_v3.c \
    src/formats/dsk_mz/uft_dsk_mz_parser_v3.c \
    src/formats/dsk_nas/uft_dsk_nas_parser_v3.c \
    src/formats/dsk_nb/uft_dsk_nb_parser_v3.c \
    src/formats/dsk_nec/uft_dsk_nec_parser_v3.c \
    src/formats/dsk_ns/uft_dsk_ns_parser_v3.c \
    src/formats/dsk_oli/uft_dsk_oli_parser_v3.c \
    src/formats/dsk_orc/uft_dsk_orc_parser_v3.c \
    src/formats/dsk_os9/uft_dsk_os9_parser_v3.c \
    src/formats/dsk_p3/uft_dsk_p3_parser_v3.c \
    src/formats/dsk_pcw/uft_dsk_pcw_parser_v3.c \
    src/formats/dsk_px/uft_dsk_px_parser_v3.c \
    src/formats/dsk_rc/uft_dsk_rc_parser_v3.c \
    src/formats/dsk_rld/uft_dsk_rld_parser_v3.c \
    src/formats/dsk_san/uft_dsk_san_parser_v3.c \
    src/formats/dsk_sc3/uft_dsk_sc3_parser_v3.c \
    src/formats/dsk_smc/uft_dsk_smc_parser_v3.c \
    src/formats/dsk_sv/uft_dsk_sv_parser_v3.c \
    src/formats/dsk_tok/uft_dsk_tok_parser_v3.c \
    src/formats/dsk_uni/uft_dsk_uni_parser_v3.c \
    src/formats/dsk_vec/uft_dsk_vec_parser_v3.c \
    src/formats/dsk_vic/uft_dsk_vic_parser_v3.c \
    src/formats/dsk_vt/uft_dsk_vt_parser_v3.c \
    src/formats/dsk_wng/uft_dsk_wng_parser_v3.c \
    src/formats/dsk_x820/uft_dsk_x820_parser_v3.c \
    src/formats/dsk_xm/uft_dsk_xm_parser_v3.c \
    src/formats/dsv/uft_dsv_parser_v3.c \
    src/formats/dtm/uft_dtm_parser_v3.c \
    src/formats/ecm/uft_ecm_parser_v3.c \
    src/formats/ecv/uft_ecv_parser_v3.c \
    src/formats/ede/uft_ede_parser_v3.c \
    src/formats/edk/uft_edk_parser_v3.c \
    src/formats/edsk/uft_edsk_parser_v3.c \
    src/formats/elf/uft_elf_parser_v3.c \
    src/formats/epub/uft_epub_parser_v3.c \
    src/formats/eve/uft_eve_parser_v3.c \
    src/formats/exr/uft_exr_parser_v3.c \
    src/formats/fcm/uft_fcm_parser_v3.c \
    src/formats/fdd/uft_fdd_parser_v3.c \
    src/formats/fdi/uft_fdi_parser_v3.c \
    src/formats/fds/uft_fds_parser_v3.c \
    src/formats/fdx/uft_fdx_parser_v3.c \
    src/formats/flac/uft_flac_parser_v3.c \
    src/formats/fld/uft_fld_parser_v3.c \
    src/formats/flp/uft_flp_parser_v3.c \
    src/formats/flv/uft_flv_parser_v3.c \
    src/formats/fm2/uft_fm2_parser_v3.c \
    src/formats/fmenc/uft_fm_parser_v3.c \
    src/formats/g71/uft_g71_parser_v3.c \
    src/formats/gam/uft_gam_parser_v3.c \
    src/formats/gb/uft_gb_parser_v3.c \
    src/formats/gba/uft_gba_parser_v3.c \
    src/formats/gbs/uft_gbs_parser_v3.c \
    src/formats/gcm/uft_gcm_parser_v3.c \
    src/formats/gcr/uft_gcr_parser_v3.c \
    src/formats/gdi/uft_gdi_parser_v3.c \
    src/formats/gen/uft_gen_parser_v3.c \
    src/formats/gif/uft_gif_parser_v3.c \
    src/formats/giz/uft_giz_parser_v3.c \
    src/formats/gmv/uft_gmv_parser_v3.c \
    src/formats/god/uft_god_parser_v3.c \
    src/formats/gp32/uft_gp32_parser_v3.c \
    src/formats/gpx/uft_gpx_parser_v3.c \
    src/formats/gsf/uft_gsf_parser_v3.c \
    src/formats/gym/uft_gym_parser_v3.c \
    src/formats/gz/uft_gz_parser_v3.c \
    src/formats/hdf/uft_hdf_parser_v3.c \
    src/formats/hdm/uft_hdm_parser_v3.c \
    src/formats/heif/uft_heif_parser_v3.c \
    src/formats/hes/uft_hes_parser_v3.c \
    src/formats/hfe/uft_hfe_parser_v3.c \
    src/formats/htmlx/uft_htmlx_parser_v3.c \
    src/formats/ico/uft_ico_parser_v3.c \
    src/formats/imz/uft_imz_parser_v3.c \
    src/formats/ini/uft_ini_parser_v3.c \
    src/formats/int/uft_int_parser_v3.c \
    src/formats/ipf/uft_ipf_parser_v3.c \
    src/formats/ips/uft_ips_parser_v3.c \
    src/formats/irm/uft_irm_parser_v3.c \
    src/formats/iso/uft_iso_parser_v3.c \
    src/formats/it/uft_it_parser_v3.c \
    src/formats/jag/uft_jag_parser_v3.c \
    src/formats/jpeg/uft_jpeg_parser_v3.c \
    src/formats/json/uft_json_parser_v3.c \
    src/formats/jv1/uft_jv1_parser_v3.c \
    src/formats/jv3/uft_jv3_parser_v3.c \
    src/formats/jvc/uft_jvc_parser_v3.c \
    src/formats/jxl/uft_jxl_parser_v3.c \
    src/formats/kcs/uft_kcs_parser_v3.c \
    src/formats/kfx/uft_kfx_parser_v3.c \
    src/formats/kfx/uft_kfstream_air.c \
    src/formats/kfx/uft_kf_histogram.c \
    src/formats/kon/uft_kon_parser_v3.c \
    src/formats/kss/uft_kss_parser_v3.c \
    src/formats/lda/uft_lda_parser_v3.c \
    src/formats/lnx/uft_lnx_parser_v3.c \
    src/formats/ltm/uft_ltm_parser_v3.c \
    src/formats/lua/uft_lua_parser_v3.c \
    src/formats/lz4/uft_lz4_parser_v3.c \
    src/formats/lzh/uft_lzh_parser_v3.c \
    src/formats/m3u/uft_m3u_parser_v3.c \
    src/formats/m64/uft_m64_parser_v3.c \
    src/formats/mbc/uft_mbc_parser_v3.c \
    src/formats/mbd/uft_mbd_parser_v3.c \
    src/formats/mcr/uft_mcr_parser_v3.c \
    src/formats/mdk/uft_mdk_parser_v3.c \
    src/formats/mds/uft_mds_parser_v3.c \
    src/formats/mdv/uft_mdv_parser_v3.c \
    src/formats/mdx/uft_mdx_parser_v3.c \
    src/formats/mes/uft_mes_parser_v3.c \
    src/formats/mfi/uft_mfi_parser_v3.c \
    src/formats/mfm/uft_mfm_parser_v3.c \
    src/formats/mgt/uft_mgt_parser_v3.c \
    src/formats/mid/uft_mid_parser_v3.c \
    src/formats/midi/uft_midi_parser_v3.c \
    src/formats/mkv/uft_mkv_parser_v3.c \
    src/formats/mod/uft_mod_parser_v3.c \
    src/formats/mp3/uft_mp3_parser_v3.c \
    src/formats/mp4/uft_mp4_parser_v3.c \
    src/formats/msa/uft_msa_parser_v3.c \
    src/formats/n3ds/uft_n3ds_parser_v3.c \
    src/formats/n64/uft_n64_parser_v3.c \
    src/formats/nam/uft_nam_parser_v3.c \
    src/formats/nds/uft_nds_parser_v3.c \
    src/formats/neo/uft_neo_parser_v3.c \
    src/formats/nes/uft_nes_parser_v3.c \
    src/formats/nfd/uft_nfd_parser_v3.c \
    src/formats/nge/uft_nge_parser_v3.c \
    src/formats/ngp/uft_ngp_parser_v3.c \
    src/formats/nib/uft_nib_parser_v3.c \
    src/formats/nrg/uft_nrg_parser_v3.c \
    src/formats/nsf/uft_nsf_parser_v3.c \
    src/formats/nsfe/uft_nsfe_parser_v3.c \
    src/formats/nsp/uft_nsp_parser_v3.c \
    src/formats/nuo/uft_nuo_parser_v3.c \
    src/formats/o2/uft_o2_parser_v3.c \
    src/formats/ode/uft_ode_parser_v3.c \
    src/formats/ogg/uft_ogg_parser_v3.c \
    src/formats/opd/uft_opd_parser_v3.c \
    src/formats/otf/uft_otf_parser_v3.c \
    src/formats/p00/uft_p00_parser_v3.c \
    src/formats/p4/uft_p4_parser_v3.c \
    src/formats/p64/uft_p64_parser_v3.c \
    src/formats/pbp/uft_pbp_parser_v3.c \
    src/formats/pc99/uft_pc99_parser_v3.c \
    src/formats/pce/uft_pce_parser_v3.c \
    src/formats/pcx/uft_pcx_parser_v3.c \
    src/formats/pdf/uft_pdf_parser_v3.c \
    src/formats/pdfx/uft_pdfx_parser_v3.c \
    src/formats/pdi/uft_pdi_parser_v3.c \
    src/formats/pdp/uft_pdp_parser_v3.c \
    src/formats/pet/uft_pet_parser_v3.c \
    src/formats/pip/uft_pip_parser_v3.c \
    src/formats/pkm/uft_pkm_parser_v3.c \
    src/formats/pls/uft_pls_parser_v3.c \
    src/formats/png/uft_png_parser_v3.c \
    src/formats/po/uft_po_parser_v3.c \
    src/formats/ppg/uft_ppg_parser_v3.c \
    src/formats/prg/uft_prg_parser_v3.c \
    src/formats/pri/uft_pri_parser_v3.c \
    src/formats/pro/uft_pro_parser_v3.c \
    src/formats/ps2/uft_ps2_parser_v3.c \
    src/formats/ps3/uft_ps3_parser_v3.c \
    src/formats/ps4/uft_ps4_parser_v3.c \
    src/formats/psd/uft_psd_parser_v3.c \
    src/formats/psf/uft_psf_parser_v3.c \
    src/formats/psio/uft_psio_parser_v3.c \
    src/formats/psp/uft_psp_parser_v3.c \
    src/formats/psu/uft_psu_parser_v3.c \
    src/formats/psx/uft_psx_parser_v3.c \
    src/formats/pv1/uft_pv1_parser_v3.c \
    src/formats/qcow2/uft_qcow2_parser_v3.c \
    src/formats/rar/uft_rar_parser_v3.c \
    src/formats/rawflux/uft_raw_parser_v3.c \
    src/formats/rca/uft_rca_parser_v3.c \
    src/formats/rvz/uft_rvz_parser_v3.c \
    src/formats/rx/uft_rx_parser_v3.c \
    src/formats/s3m/uft_s3m_parser_v3.c \
    src/formats/sad/uft_sad_parser_v3.c \
    src/formats/sam/uft_sam_parser_v3.c \
    src/formats/sap/uft_sap_parser_v3.c \
    src/formats/sat/uft_sat_parser_v3.c \
    src/formats/sav/uft_sav_parser_v3.c \
    src/formats/scd/uft_scd_parser_v3.c \
    src/formats/scl/uft_scl_parser_v3.c \
    src/formats/sfm/uft_sfm_parser_v3.c \
    src/formats/sg/uft_sg_parser_v3.c \
    src/formats/sid/uft_sid_parser_v3.c \
    src/formats/sms/uft_sms_parser_v3.c \
    src/formats/smv/uft_smv_parser_v3.c \
    src/formats/snes/uft_snes_parser_v3.c \
    src/formats/sns/uft_sns_parser_v3.c \
    src/formats/soc/uft_soc_parser_v3.c \
    src/formats/sol/uft_sol_parser_v3.c \
    src/formats/spc/uft_spc_parser_v3.c \
    src/formats/sqlite/uft_sqlite_parser_v3.c \
    src/formats/sram/uft_sram_parser_v3.c \
    src/formats/srm/uft_srm_parser_v3.c \
    src/formats/ssd/uft_ssd_parser_v3.c \
    src/formats/ssf/uft_ssf_parser_v3.c \
    src/formats/ssy/uft_ssy_parser_v3.c \
    src/formats/st/uft_st_parser_v3.c \
    src/formats/sva/uft_sva_parser_v3.c \
    src/formats/svd/uft_svd_parser_v3.c \
    src/formats/svg/uft_svg_parser_v3.c \
    src/formats/swf/uft_swf_parser_v3.c \
    src/formats/syn/uft_syn_parser_v3.c \
    src/formats/t1k/uft_t1k_parser_v3.c \
    src/formats/t64/uft_t64_parser_v3.c \
    src/formats/tai/uft_tai_parser_v3.c \
    src/formats/tan/uft_tan_parser_v3.c \
    src/formats/tap/uft_tap_parser_v3.c \
    src/formats/tar/uft_tar_parser_v3.c \
    src/formats/td0/uft_td0_parser_v3.c \
    src/formats/tdo/uft_tdo_parser_v3.c \
    src/formats/tga/uft_tga_parser_v3.c \
    src/formats/tgc/uft_tgc_parser_v3.c \
    src/formats/tif/uft_tif_parser_v3.c \
    src/formats/tiff/uft_tiff_parser_v3.c \
    src/formats/toml/uft_toml_parser_v3.c \
    src/formats/trd/uft_trd_parser_v3.c \
    src/formats/ttf/uft_ttf_parser_v3.c \
    src/formats/tzx/uft_tzx_parser_v3.c \
    src/formats/udi/uft_udi_parser_v3.c \
    src/formats/uef/uft_uef_parser_v3.c \
    src/formats/ups/uft_ups_parser_v3.c \
    src/formats/usf/uft_usf_parser_v3.c \
    src/formats/v20/uft_v20_parser_v3.c \
    src/formats/v9t9/uft_v9t9_parser_v3.c \
    src/formats/vb/uft_vb_parser_v3.c \
    src/formats/vbm/uft_vbm_parser_v3.c \
    src/formats/vcs/uft_vcs_parser_v3.c \
    src/formats/vdk/uft_vdk_parser_v3.c \
    src/formats/vdk_drg/uft_vdk_drg_parser_v3.c \
    src/formats/vgm/uft_vgm_parser_v3.c \
    src/formats/vhd/uft_vhd_parser_v3.c \
    src/formats/vmdk/uft_vmdk_parser_v3.c \
    src/formats/wav/uft_wav_parser_v3.c \
    src/formats/webp/uft_webp_parser_v3.c \
    src/formats/wia/uft_wia_parser_v3.c \
    src/formats/wii/uft_wii_parser_v3.c \
    src/formats/woff/uft_woff_parser_v3.c \
    src/formats/woff2/uft_woff2_parser_v3.c \
    src/formats/woz/uft_woz_parser_v3.c \
    src/formats/wsc/uft_wsc_parser_v3.c \
    src/formats/wsv/uft_wsv_parser_v3.c \
    src/formats/x32/uft_x32_parser_v3.c \
    src/formats/x64/uft_x64_parser_v3.c \
    src/formats/xbe/uft_xbe_parser_v3.c \
    src/formats/xci/uft_xci_parser_v3.c \
    src/formats/xdf/uft_xdf_parser_v3.c \
    src/formats/xdm86/uft_xdm86_parser_v3.c \
    src/formats/xegs/uft_xegs_parser_v3.c \
    src/formats/xex/uft_xex_parser_v3.c \
    src/formats/xfd/uft_xfd_parser_v3.c \
    src/formats/xiso/uft_xiso_parser_v3.c \
    src/formats/xm/uft_xm_parser_v3.c \
    src/formats/xml/uft_xml_parser_v3.c \
    src/formats/xz/uft_xz_parser_v3.c \
    src/formats/ym/uft_ym_parser_v3.c \
    src/formats/yml/uft_yml_parser_v3.c \
    src/formats/zip/uft_zip_parser_v3.c \
    src/formats/zod/uft_zod_parser_v3.c \
    src/formats/zstd/uft_zstd_parser_v3.c \
    src/formats/zx81/uft_zx81_parser_v3.c \
    src/formats/zxs/uft_zxs_parser_v3.c


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2c: v2 Parsers, Utilities & Extended Format Support
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/d64/uft_d64_parser_v2.c \
    src/formats/d71/uft_d71_parser_v2.c \
    src/formats/d81/uft_d81_parser_v2.c \
    src/formats/d88/uft_d88.c \
    src/formats/d88/uft_d88_parser_v2.c \
    src/formats/dmk/uft_dmk_parser_v2.c \
    src/formats/dsk_cpc/uft_dsk_cpc_parser_v2.c \
    src/formats/fdi/uft_fdi_parser_v2.c \
    src/formats/g64/uft_g64.c \
    src/formats/imd/uft_imd_parser_v2.c \
    src/formats/ipf/uft_caps_ipf.c \
    src/formats/ipf/uft_ipf_caps.c \
    src/formats/ipf/uft_ipf_ctraw_v2.c \
    src/formats/ipf/uft_ipf_air.c \
    src/formats/jv/uft_jv_parser_v2.c \
    src/formats/msa/uft_msa.c \
    src/formats/msa/uft_msa_parser_v2.c \
    src/formats/nib/uft_nib.c \
    src/formats/nib/uft_nib_parser_v2.c \
    src/formats/sap/uft_sap_parser_v2.c \
    src/formats/scl/uft_scl_parser_v2.c \
    src/formats/scp/uft_scp_multirev.c \
    src/formats/scp/uft_scp_reader_v2.c \
    src/formats/scp/uft_scp_writer.c \
    src/formats/ssd/uft_ssd_parser_v2.c \
    src/formats/tap/uft_tap_parser_v2.c \
    src/formats/td0/uft_td0_lzss.c \
    src/formats/td0/uft_td0_parser_v2.c \
    src/formats/trd/uft_trd_parser_v2.c \
    src/formats/uft_d64_writer.c \
    src/formats/uft_format_extensions.c \
    src/formats/uft_format_names_extended.c \
    src/formats/uft_format_versions.c


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2d: Additional Utilities & Format Support
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/adf/uft_adf_parser_v2.c \
    src/formats/uft_adf.c \
    src/formats/uft_axdf.c \
    src/formats/uft_cw_raw.c \
    src/formats/uft_fdc_gaps.c \
    src/formats/uft_format_registry_v2.c

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/2img/uft_2img_parser_v2.c \
    src/formats/g64/uft_g64_parser_v2.c \
    src/formats/hfe/uft_hfe_parser_v2.c \
    src/formats/stx/uft_stx_parser_v2.c


# ═══════════════════════════════════════════════════════════════════════════════
# Phase 2e: Cat A/B Plugin API + Typedef-Conflict Formats
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/apridisk/uft_apridisk.c \
    src/formats/cfi/uft_cfi.c \
    src/formats/nanowasp/uft_nanowasp.c \
    src/formats/qrst/uft_qrst.c \
    src/formats/hardsector/uft_hardsector.c \
    src/formats/mgt/uft_mgt.c \
    src/formats/myz80/uft_myz80.c \
    src/formats/opus/uft_opus.c \
    src/formats/logical/uft_logical.c \
    src/formats/posix/uft_posix.c

SOURCES += src/formats/mega65/uft_mega65_d81.c \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# v4.0 GUI Panels (DMK Analyzer, GW→DMK, Flux Histogram)
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/uft_gw2dmk_panel.cpp \
    src/uft_flux_histogram_widget.cpp

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    src/uft_gw2dmk_panel.h \
    src/uft_flux_histogram_widget.h

# ═══════════════════════════════════════════════════════════════════════════════
# HAL (Hardware Abstraction Layer) - Greaseweazle, KryoFlux, SuperCard Pro
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/hal/uft_greaseweazle_full.c \
    src/hal/uft_hal_unified.c \
    src/hal/uft_hal_profiles.c \
    src/hal/uft_kryoflux_dtc.c \
    src/hal/uft_drive.c \
    src/core/uft_ir_format.c

# Note: uft_hal.c and uft_hal_v3.c removed to avoid multiple definition errors
# uft_hal_unified.c provides the complete unified implementation

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/hal/uft_greaseweazle_full.h \
    include/uft/hal/uft_hal.h \
    include/uft/hal/uft_hal_unified.h \
    include/uft/hal/uft_hal_profiles.h \
    include/uft/hal/uft_hal_v3.h \
    include/uft/hal/uft_hal_v2.h \
    include/uft/hal/uft_greaseweazle.h \
    include/uft/hal/uft_drive.h \
    include/uft/hal/uft_kryoflux.h \
    include/uft/uft_ir_format.h

# ═══════════════════════════════════════════════════════════════════════════════
# Switch/MIG Dumper Module (Nintendo Switch Cartridge Support)
# ═══════════════════════════════════════════════════════════════════════════════

DEFINES += UFT_HAS_SWITCH
message("Switch/MIG Dumper Module ENABLED")

INCLUDEPATH += \
    $$PWD/src/switch \
    $$PWD/src/switch/hactool \
    $$PWD/src/switch/hactool/mbedtls/include

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/switch/uft_mig_dumper.c \
    src/switch/uft_xci_parser_stubs.c \
    src/gui/uft_switch_panel.cpp

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    src/switch/uft_switch_types.h \
    src/switch/uft_mig_dumper.h \
    src/switch/uft_xci_parser.h \
    src/gui/uft_switch_panel.h

# hactool sources (ISC License - third party)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/switch/hactool/xci.c \
    src/switch/hactool/nca.c \
    src/switch/hactool/pfs0.c \
    src/switch/hactool/hfs0.c \
    src/switch/hactool/romfs.c \
    src/switch/hactool/save.c \
    src/switch/hactool/npdm.c \
    src/switch/hactool/kip.c \
    src/switch/hactool/nso.c \
    src/switch/hactool/nax0.c \
    src/switch/hactool/packages.c \
    src/switch/hactool/pki.c \
    src/switch/hactool/extkeys.c \
    src/switch/hactool/hactool_aes.c \
    src/switch/hactool/sha.c \
    src/switch/hactool/hactool_rsa.c \
    src/switch/hactool/utils.c \
    src/switch/hactool/filepath.c \
    src/switch/hactool/lz4.c \
    src/switch/hactool/bktr.c \
    src/switch/hactool/ConvertUTF.c \
    src/switch/hactool/cJSON.c

# mbedtls (Apache 2.0 License - third party)
# Note: hactool wrappers renamed to hactool_aes.c/hactool_rsa.c to avoid collision
MBEDTLS_PATH = $$PWD/src/switch/hactool/mbedtls/library
MBEDTLS_SOURCES = $$files($$MBEDTLS_PATH/*.c)
SOURCES += $$MBEDTLS_SOURCES \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Suppress warnings in third-party code
unix:QMAKE_CFLAGS += -Wno-unused-parameter -Wno-sign-compare

# ═══════════════════════════════════════════════════════════════════════════════
# Cart7/Cart8 Multi-System Cartridge Reader (NES, SNES, N64, MD, GBA, GB, 3DS)
# ═══════════════════════════════════════════════════════════════════════════════

DEFINES += UFT_HAS_CART7
message("Cart7/Cart8 Cartridge Reader Module ENABLED")

INCLUDEPATH += \
    $$PWD/src/cart7 \
    $$PWD/include/uft/cart7

SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/cart7/uft_cart7.c \
    src/cart7/uft_cart7_3ds.c \
    src/cart7/uft_cart7_hal.c \
    src/gui/uft_cart7_panel.cpp

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/cart7/cart7_protocol.h \
    include/uft/cart7/cart7_3ds_protocol.h \
    include/uft/cart7/uft_cart7.h \
    include/uft/cart7/uft_cart7_3ds.h \
    src/cart7/uft_cart7_hal.h \
    src/gui/uft_cart7_panel.h

# ============================================================================
# Legacy FloppyDevice Format Modules
# ============================================================================

# Commodore formats (21 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/commodore/crt.c \
    src/formats/commodore/d64.c \
    src/formats/commodore/d67.c \
    src/formats/commodore/d71.c \
    src/formats/commodore/d80.c \
    src/formats/commodore/d81.c \
    src/formats/commodore/d82.c \
    src/formats/commodore/d90.c \
    src/formats/commodore/d91.c \
    src/formats/commodore/dnp.c \
    src/formats/commodore/dnp2.c \
    src/formats/commodore/g64.c \
    src/formats/commodore/p00.c \
    src/formats/commodore/prg.c \
    src/formats/commodore/t64.c \
    src/formats/commodore/uft_d64_view.c \
    src/formats/commodore/uft_m2i.c \
    src/formats/commodore/x64.c \
    src/formats/commodore/x71.c \
    src/formats/commodore/x81.c

# Amstrad formats (6 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/amstrad/dsk.c \
    src/formats/amstrad/dsk_mfm.c \
    src/formats/amstrad/edsk_extdsk.c \
    src/formats/amstrad/mgt_sad_sdf.c \
    src/formats/amstrad/trd_scl.c \
    src/formats/amstrad/uft_edsk_parser.c

# Apple formats (12 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/apple/2mg.c \
    src/formats/apple/mac_dsk.c \
    src/formats/apple/nib.c \
    src/formats/apple/nib_nbz.c \
    src/formats/apple/prodos_po_do.c \
    src/formats/apple/uft_2mg_parser.c \
    src/formats/apple/uft_diskcopy.c \
    src/formats/apple/uft_woz.c \
    src/formats/apple/woz.c

# Atari formats (19 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/atari/atr.c \
    src/formats/atari/atx.c \
    src/formats/atari/st.c \
    src/formats/atari/st_msa.c \
    src/formats/atari/stt.c \
    src/formats/atari/stx.c \
    src/formats/atari/stz.c \
    src/formats/atari/uft_atari.c \
    src/formats/atari/uft_atari8_disk.c \
    src/formats/atari/uft_atari_dos.c \
    src/formats/atari/uft_atari_st.c \
    src/formats/atari/uft_atx_parser_v2.c \
    src/formats/atari/uft_dcm_parser_v2.c \
    src/formats/atari/uft_pro_parser_v2.c \
    src/formats/atari/uft_stx_parser.c \
    src/formats/atari/uft_xfd_parser_v2.c \
    src/formats/atari/xdf.c

# BBC formats (4 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/bbc/adf_adl.c \
    src/formats/bbc/ssd_dsd.c \
    src/formats/bbc/uft_bbc_dfs.c \
    src/formats/bbc/uft_bbc_tape.c

# TRS80 formats (5 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/trs80/dmk.c \
    src/formats/trs80/jv3_jvc.c \
    src/formats/trs80/jvc.c \
    src/formats/trs80/uft_trs80.c \
    src/formats/trs80/vdk.c

# PC98 formats (7 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/pc98/d88.c \
    src/formats/pc98/dim.c \
    src/formats/pc98/fdd.c \
    src/formats/pc98/fdx.c \
    src/formats/pc98/hdm.c \
    src/formats/pc98/nfd.c \
    src/formats/pc98/uft_pc98.c

# Misc formats (23 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/misc/adf.c \
    src/formats/misc/adz.c \
    src/formats/misc/cqm.c \
    src/formats/misc/d1m.c \
    src/formats/misc/d2m.c \
    src/formats/misc/d4m.c \
    src/formats/misc/dcp_dcu.c \
    src/formats/misc/dhd.c \
    src/formats/misc/dmf_msx.c \
    src/formats/misc/edd.c \
    src/formats/misc/fdi.c \
    src/formats/misc/fds.c \
    src/formats/misc/imd.c \
    src/formats/misc/imz.c \
    src/formats/misc/lnx.c \
    src/formats/misc/ms_dmf.c \
    src/formats/misc/oric_dsk.c \
    src/formats/misc/osd.c \
    src/formats/misc/pc_img.c \
    src/formats/misc/sf7.c \
    src/formats/misc/tap.c \
    src/formats/misc/td0.c \
    src/formats/misc/udi.c

# Flux formats (12 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/flux/dfi.c \
    src/formats/flux/f86.c \
    src/formats/flux/gwraw.c \
    src/formats/flux/ipf.c \
    src/formats/flux/kfraw.c \
    src/formats/flux/mfi.c \
    src/formats/flux/pfi.c \
    src/formats/flux/pri.c \
    src/formats/flux/psi.c \
    src/formats/flux/scp.c

# 86Box (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/86box/uft_86box.c

# Amiga (2 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/amiga/uft_amiga_protection.c

# Amiga Extended (9 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/amiga_ext/crc.c \
    src/formats/amiga_ext/snprintf.c

# Apridisk (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Brother (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/brother/brother.c

# C64 Extended (19 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/c64/uft_bam_editor.c \
    src/formats/c64/uft_c64rom.c \
    src/formats/c64/uft_cmd.c \
    src/formats/c64/uft_crt.c \
    src/formats/c64/uft_d64_file.c \
    src/formats/c64/uft_d64_g64.c \
    src/formats/c64/uft_d71_d81.c \
    src/formats/c64/uft_freezer.c \
    src/formats/c64/uft_frz.c \
    src/formats/c64/uft_gcr_ops.c \
    src/formats/c64/uft_geos.c \
    src/formats/c64/uft_nib_format.c \
    src/formats/c64/uft_p00.c \
    src/formats/c64/uft_reu.c \
    src/formats/c64/uft_sid.c \
    src/formats/c64/uft_t64.c \
    src/formats/c64/uft_tap.c \
    src/formats/c64/uft_vsf.c

# CBM (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/cbm/uft_cbm_formats.c

# CFI (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# CMD FD (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# CPM (5 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/cpm/uft_supercopy_detect.c \
    src/formats/cpm/uft_cpm_diskdef.c \
    src/formats/cpm/uft_cpm_diskdefs.c \
    src/formats/rcpmfs/uft_rcpmfs.c \
    src/formats/cpm/uft_cpm_parser_v3.c \
    src/formats/retro_image/uft_retro_image_detect.c

# Dec (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/dec/uft_rx50.c

# Eastern Block (3 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/eastblock/uft_meritum.c \
    src/formats/eastblock/uft_pravetz.c \
    src/formats/eastblock/uft_robotron.c \
    src/formats/misc/polyglot_boot.c

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/formats/polyglot_boot.h

# Atari DOS Filesystem Module
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/atari/atari_atr.c \
    src/formats/atari/atari_dos2.c \
    src/formats/atari/atari_sparta.c \
    src/formats/atari/atari_check.c \
    src/formats/atari/atari_util.c

HEADERS += \
    include/uft/analysis/uft_export_bridge.h \
    include/uft/analysis/otdr_event_core_v12.h \
    include/uft/analysis/uft_pipeline_bridge.h \
    include/uft/analysis/otdr_event_core_v11.h \
    include/uft/analysis/uft_confidence_bridge.h \
    include/uft/analysis/otdr_event_core_v10.h \
    include/uft/analysis/uft_integrity_bridge.h \
    include/uft/analysis/otdr_event_core_v9.h \
    include/uft/analysis/uft_event_v8_bridge.h \
    include/uft/analysis/otdr_event_core_v8.h \
    include/uft/analysis/uft_align_fuse_bridge.h \
    include/uft/analysis/otdr_event_core_v7.h \
    include/uft/analysis/uft_event_bridge.h \
    include/uft/analysis/otdr_event_core_v2.h \
    include/uft/analysis/uft_denoise_bridge.h \
    include/uft/analysis/phi_otdr_denoise_1d.h \
    include/uft/formats/atari_dos.h

# FAT (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/fat/uft_fat_bootsector.c

# FAT32 (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/fat32/uft_fat32_mbr.c

# FlashFloppy (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/flashfloppy/uft_ff_formats.c

# Flex (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/flex/uft_flex.c

# Format Core (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Geometry (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# HP (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/hp/lif.c

# Hardsector (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Industrial (2 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/industrial/uft_cromemco.c \
    src/formats/industrial/uft_heathkit.c

# Japanese Ext (3 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/japanese_ext/uft_hitachi_s1.c \
    src/formats/japanese_ext/uft_sanyo_mbc.c \
    src/formats/japanese_ext/uft_sharp_x1.c

# KryoFlux (2 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/kryoflux/uft_kryoflux_checker.c

# Legacy (4 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/legacy/uft_altair_hd.c \
    src/formats/legacy/uft_fdi.c \
    src/formats/legacy/uft_imd.c

# Logical (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# MAME (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/mame/uft_mame_mfi.c

# MFM Native (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/mfm_native/uft_mfm_image.c

# MSX (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/msx/uft_msx.c

# Mega65 (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Micropolis (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/micropolis/micropolis.c

# Minicomputer (2 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/minicomputer/uft_dg_nova.c \
    src/formats/minicomputer/uft_prime.c

# MyZ80 (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# NEC (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/nec/uft_pce.c

# Nanowasp (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Nintendo (7 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/nintendo/uft_3ds.c \
    src/formats/nintendo/uft_gameboy.c \
    src/formats/nintendo/uft_n64.c \
    src/formats/nintendo/uft_nds.c \
    src/formats/nintendo/uft_nes.c \
    src/formats/nintendo/uft_snes.c \
    src/formats/nintendo/uft_switch.c

# Nordic (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/nordic/uft_abc800.c

# Northstar (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/northstar/northstar.c

# Obscure (5 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/obscure/uft_applix.c \
    src/formats/obscure/uft_calcomp.c \
    src/formats/obscure/uft_pmc_micromate.c \
    src/formats/obscure/uft_pyldin.c \
    src/formats/obscure/uft_rc759.c

# Opus (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Posix (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# QL (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/ql/qdos.c

# QRST (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# RCPMFS (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Roland (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Russian (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# SIMH (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# SNK (2 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/snk/uft_neogeo.c \
    src/formats/snk/uft_snk_parser_v3.c

# Sega (3 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/sega/uft_genesis.c \
    src/formats/sega/uft_sega_cd.c \
    src/formats/sega/uft_sms.c

# Sinclair (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/sinclair/uft_spectrum.c

# Sony (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/sony/uft_ps1.c

# TC (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# TI99 (4 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/ti99/uft_fiad.c \
    src/formats/ti99/uft_tifiles.c \
    src/formats/ti99/v9t9_pc99.c

# Thomson (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/thomson/sap.c

# Victor (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/victor/victor9k.c

# X68K (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# YDSK (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c

# Zilog (1 files)
SOURCES += \
    src/analysis/events/uft_export_bridge.c \
    src/analysis/events/otdr_event_core_v12.c \
    src/analysis/events/uft_pipeline_bridge.c \
    src/analysis/events/otdr_event_core_v11.c \
    src/analysis/events/uft_confidence_bridge.c \
    src/analysis/events/otdr_event_core_v10.c \
    src/analysis/events/uft_integrity_bridge.c \
    src/analysis/events/otdr_event_core_v9.c \
    src/analysis/events/uft_event_v8_bridge.c \
    src/analysis/events/otdr_event_core_v8.c \
    src/analysis/events/uft_align_fuse_bridge.c \
    src/analysis/events/otdr_align_fuse_v7.c \
    src/analysis/events/uft_event_bridge.c \
    src/analysis/events/otdr_event_core_v2.c \
    src/analysis/denoise/uft_denoise_bridge.c \
    src/analysis/denoise/phi_otdr_denoise_1d.c \
    src/formats/zilog/zilogmcz.c


# Additional include paths for legacy format modules
INCLUDEPATH += \
    $$PWD/src/formats/commodore \
    $$PWD/src/formats/amstrad \
    $$PWD/src/formats/apple \
    $$PWD/src/formats/atari \
    $$PWD/src/formats/bbc \
    $$PWD/src/formats/trs80 \
    $$PWD/src/formats/pc98 \
    $$PWD/src/formats/misc \
    $$PWD/src/formats/flux \
    $$PWD/src/formats/c64 \
    $$PWD/src/formats/cbm \
    $$PWD/src/formats/amiga \
    $$PWD/src/formats/amiga_ext \
    $$PWD/src/formats/nintendo \
    $$PWD/src/formats/sega \
    $$PWD/include/uft/floppy

# ═══════════════════════════════════════════════════════════════════════════════
# BUILD FIXES (v4.1.0)
# ═══════════════════════════════════════════════════════════════════════════════

# Fix 1: Deduplicate SOURCES/HEADERS (OTDR bridge files 100× duplicated)
SOURCES = $$unique(SOURCES)
HEADERS = $$unique(HEADERS)

# Fix 2: MSVC C11 mode for .c files (MSVC defaults to C89)
win32-msvc* {
    QMAKE_CFLAGS += /std:c11
}
