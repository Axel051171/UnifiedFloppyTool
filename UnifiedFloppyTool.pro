#-------------------------------------------------
#
# UnifiedFloppyTool - Qt Project File
# Version: 4.1.3  (canonical source: VERSION.txt)
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
# C++ Standard: immer c++17 als Basis.
# C++20-Features im Code sind mit #if __cplusplus >= 202002L abgesichert.
# Für C++20-Build explizit: qmake CONFIG+=cxx20
cxx20 {
    CONFIG += c++20
    DEFINES += UFT_CXX20
    message("C++20 aktiv (explizit angefordert)")
} else {
    CONFIG += c++17
    message("C++17 aktiv (Standard)")
}

CONFIG += sdk_no_version_check

# Mirror source tree in object output dirs to avoid MSVC/NMAKE basename collisions
# (e.g. src/core/uft_format_registry.c vs src/formats/uft_format_registry.c)
CONFIG += object_parallel_to_source

# Enable console output for debugging (remove for release)
win32:CONFIG += console

# Compiler flags - suppress warnings for legacy code
# Compiler flags moved to platform-specific sections below
unix:QMAKE_CFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
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
    # POSIX shims for hactool (getopt.h, strings.h) — only with switch_support
    switch_support {
        INCLUDEPATH += $$PWD/src/switch/hactool/compat
        # getopt.c was removed in an earlier cleanup; the header-only
        # pieces under compat/ (getopt.h, strings.h) are sufficient for
        # MinGW via inline definitions.
    }
}

# MSVC specific - POSIX string functions not available
msvc {
    DEFINES += strcasecmp=_stricmp strncasecmp=_strnicmp
    # MSVC hardening: buffer security check, ASLR, DEP, Control Flow Guard
    QMAKE_CFLAGS_RELEASE += /GS /DYNAMICBASE /NXCOMPAT
    QMAKE_CXXFLAGS_RELEASE += /GS /DYNAMICBASE /NXCOMPAT
    QMAKE_LFLAGS += /DYNAMICBASE /NXCOMPAT
}

# GCC/Clang hardening (Linux, macOS, MinGW)
#
# Principle 4 (no dangerous defaults): we do NOT set -D_FORTIFY_SOURCE
# by default. Reasons, in order of importance:
#
#   1. Distros (Fedora, Debian, Ubuntu, Arch, openSUSE) and Flathub
#      runtimes inject _FORTIFY_SOURCE through their own CFLAGS, often
#      via -Wp,-D_FORTIFY_SOURCE=N which is passed DIRECTLY to the
#      preprocessor and cannot be overridden reliably by -U/-D on the
#      GCC command line — see issue #16, @hadess's analysis.
#   2. Fedora / Ubuntu 24.04+ ship with =3, which is stricter than =2.
#      Forcing =2 here would DOWNGRADE the distro's hardening level.
#   3. _FORTIFY_SOURCE is only meaningful at -O2+; distros handle the
#      interaction with their own optimisation defaults correctly.
#
# We keep -fstack-protector-strong because it is NOT uniformly set by
# every distro and adds defense-in-depth without conflict risk.
#
# ─── Escape hatch for bare-metal developer builds ────────────────────
#   qmake CONFIG+=uft_force_fortify
# adds -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 for people who build on
# a plain toolchain without a distro hardening setup and want the
# check anyway. Consciously opt-in, never a default.
!msvc {
    QMAKE_CFLAGS_RELEASE   += -fstack-protector-strong
    QMAKE_CXXFLAGS_RELEASE += -fstack-protector-strong

    uft_force_fortify {
        QMAKE_CFLAGS_RELEASE   += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
        QMAKE_CXXFLAGS_RELEASE += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
        message(UFT hardening: _FORTIFY_SOURCE=2 forced via uft_force_fortify)
    }
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
    include/uft/forensic \
    src \
    src/gui \
    src/analysis/otdr \
    src/samdisk \
    src/hardware_providers \
    src/widgets \
    src/flux/fdc_bitstream \
    src/hal \
    include/uft/protection \
    src/protection

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
# NOTE: analysis/events + analysis/denoise sources are listed here ONCE;
#       they appear duplicated in later SOURCES blocks but $$unique() deduplicates
#
# OTDR Analysis Pipeline — v12 is the current entry point.
# v2-v11 are NOT dead code — they are pipeline stages called by v12:
#   v2:  baseline event detection
#   v7:  multi-pass alignment + fusion
#   v8:  multi-scale feature extraction
#   v9:  signal integrity analysis
#   v10: confidence map fusion
#   v11: streaming pipeline
#   v12: export + golden vectors (entry point)
# Bridge files connect these stages to the UFT API.
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
    # src/qmake_stubs/uft_protection_stubs.cpp \ # DISABLED: conflicts with real impls
    src/gui/uft_otdr_panel.cpp \
    src/gui/ProtectionAnalysisWidget.cpp \
    src/gui/uft_sector_editor.cpp \
    src/flux/uft_scp_parser.c \
    src/flux/uft_flux_decoder.c \
    src/fileops/uft_file_ops_extended.c \
    src/analysis/uft_sector_compare.c

# Main GUI Headers (CRITICAL for MOC!)
HEADERS += \
    include/uft/analysis/uft_sector_compare.h \
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
    include/uft/analysis/uft_triage.h \
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
    src/gui/ProtectionAnalysisWidget.h \
    src/gui/uft_sector_editor.h \
    include/uft/flux/uft_scp_parser.h

# FDC Bitstream Sources (third-party library by Yasunori Shimura)
# VFO/PLL: The vfo_* files are EXPERIMENTAL VFO implementations from this library.
# Production PLL is in src/decoder/uft_pll_v2.c (with SIMD dispatch) and
# src/flux/pll/uft_pll_pi.c (PI controller variant) -- neither is in this block.
SOURCES += \
    src/flux/fdc_bitstream/bit_array.cpp \
    src/flux/fdc_bitstream/fdc_bitstream.cpp \
    src/flux/fdc_bitstream/fdc_crc.cpp \
    src/flux/fdc_bitstream/fdc_misc.cpp \
    src/flux/fdc_bitstream/fdc_vfo_base.cpp \
    src/flux/fdc_bitstream/mfm_codec.cpp \
    src/flux/fdc_bitstream/vfo_pid.cpp \
    src/flux/fdc_bitstream/vfo_pid2.cpp \
    src/flux/fdc_bitstream/vfo_pid3.cpp \
    src/flux/fdc_bitstream/vfo_simple.cpp \
    src/flux/fdc_bitstream/vfo_simple2.cpp

# Optional: experimental VFO (reads vfo_settings.txt at runtime)
experimental_vfo {
    SOURCES += src/flux/fdc_bitstream/vfo_experimental.cpp
    DEFINES += UFT_HAS_EXPERIMENTAL_VFO
    message("Experimental VFO enabled")
}

# vfo_fixed.cpp removed: 7 LOC trivial stub, never selected by VFO_TYPE_DEFAULT

# Optional: Kalman filter PLL (research, not battle-tested)
kalman_pll {
    SOURCES += \
        src/algorithms/advanced/uft_kalman_pll_v2.c \
        src/algorithms/uft_kalman_pll.c
    DEFINES += UFT_HAS_KALMAN_PLL
    message("Kalman PLL enabled (experimental)")
}

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
    src/hardware_providers/adfcopyhardwareprovider.cpp \
    src/hardware_providers/usbfloppyhardwareprovider.cpp \
    src/hardware_providers/unified_hal_bridge.cpp

# USB Floppy UFI Backend (C)
SOURCES += src/hal/ufi.c

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
    src/hardware_providers/usbfloppyhardwareprovider.h \
    src/hardware_providers/unified_hal_bridge.h \
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
    src/formats/imd/uft_imd_parser_v3.c \
    src/formats/imd/uft_imd_plugin.c \
    src/formats/fdi/uft_fdi_plugin.c

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
    src/formats/dsk/uft_dsk_parser_v3.c \
    src/formats/dsk_generic/uft_dsk_generic.c \
    src/formats/st/uft_st.c \
    src/formats/d77/uft_d77.c \
    src/formats/dim/uft_dim.c \
    src/formats/dc42/uft_dc42.c \
    src/formats/fds/uft_fds_plugin.c \
    src/formats/cas/uft_cas.c \
    src/formats/mfi/uft_mfi.c \
    src/formats/pri/uft_pri.c \
    src/formats/amstrad/uft_edsk.c \
    src/formats/kfx/uft_kfx.c \
    src/formats/jv1/uft_jv1.c \
    src/formats/jv3/uft_jv3.c \
    src/formats/xfd/uft_xfd.c \
    src/formats/tan/uft_tan.c \
    src/formats/t1k/uft_t1k.c \
    src/formats/sam/uft_sam.c \
    src/formats/2img/uft_2img.c \
    src/formats/adf_arc/uft_adf_arc.c \
    src/formats/xdm86/uft_xdm86.c \
    src/formats/pdp/uft_pdp.c \
    src/formats/syn/uft_syn.c \
    src/formats/msa/uft_msa_plugin.c \
    src/formats/ipf/uft_ipf_plugin.c \
    src/formats/apple/uft_woz_plugin.c \
    src/formats/edk/uft_edk.c \
    src/formats/d64/uft_d64_plugin.c \
    src/formats/dcm/uft_dcm.c \
    src/formats/atx/uft_atx.c \
    src/formats/adf/uft_adf_plugin.c \
    src/formats/scl/uft_scl_plugin.c \
    src/formats/format_registry/uft_format_registry.c \
    src/core/uft_format_verify.c \
    src/core/uft_disk_stream.c \
    src/core/uft_disk_verify.c \
    src/core/uft_disk_stats.c \
    src/core/uft_disk_compare.c \
    src/core/uft_disk_transaction.c \
    src/core/uft_disk_convert.c \
    src/core/uft_disk_batch.c \
    src/core/uft_metadata.c \
    src/core/uft_recovery_fusion.c \
    src/core/uft_log.c \
    src/core/uft_hw_batch.c \
    src/core/uft_fs_registry.c \
    src/core/uft_capture.c \
    src/core/uft_mfm_encoder.c \
    src/core/uft_detect_format_impl.c \
    src/core/uft_detect_buffer_impl.c \
    src/core/uft_probe_format_impl.c \
    src/fs/uft_amigados.c \
    src/fs/uft_amiga_virus_db.c \
    src/fs/uft_bootblock_scanner.c \
    src/fs/uft_fs_amigados_driver.c \
    src/fs/uft_fat12.c \
    src/formats/uft_fat12_legacy.c \
    src/formats/uft_format_converters.c \
    src/formats/uft_format_validators.c \
    src/hal/greaseweazle_backend.c \
    src/hal/sync_backends.c \
    src/formats/stx/uft_stx_plugin.c \
    src/formats/atari/uft_pro_plugin.c \
    src/formats/do/uft_do.c \
    src/formats/po/uft_po.c \
    src/formats/adl/uft_adl.c \
    src/formats/v9t9/uft_v9t9.c \
    src/formats/vdk/uft_vdk_plugin.c \
    src/formats/victor9k/uft_victor9k.c \
    src/formats/micropolis/uft_micropolis.c \
    src/formats/northstar/uft_northstar.c \
    src/formats/dim_atari/uft_dim_atari.c \
    src/formats/fdi_pc98/uft_fdi_pc98.c \
    src/formats/d13/uft_d13.c \
    src/formats/sap/uft_sap_plugin.c \
    src/formats/86box/uft_86f_plugin.c \
    src/core/uft_decode_pipeline.c \
    src/parsers/a2r/uft_a2r_parser.c \
    src/formats/jvc/uft_jvc_plugin.c \
    src/formats/dms/uft_dms_plugin.c \
    src/formats/d67/uft_d67.c \
    src/formats/dsk_msx/uft_dsk_msx.c \
    src/formats/udi/uft_udi_plugin.c \
    src/formats/nfd/uft_nfd_plugin.c

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
# Advanced Algorithms (formerly "god_mode")
# Production algorithms for difficult disk recovery.
# See src/algorithms/advanced/README for module descriptions.
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
    src/algorithms/advanced/uft_god_mode_api.c \
    # src/algorithms/advanced/uft_kalman_pll_v2.c \ # optional: CONFIG+=kalman_pll
    src/algorithms/advanced/uft_gcr_viterbi.c \
    src/algorithms/advanced/uft_gcr_viterbi_v2.c \
    src/algorithms/advanced/uft_bayesian_detect.c \
    src/algorithms/advanced/uft_bayesian_detect_v2.c \
    src/algorithms/advanced/uft_multi_rev_fusion.c \
    src/algorithms/advanced/uft_crc_correction_v2.c \
    src/algorithms/advanced/uft_fuzzy_sync_v2.c \
    src/algorithms/advanced/uft_decoder_metrics.c \
    src/algorithms/encoding/uft_otdr_encoding_boost.c \
    src/algorithms/recovery/uft_otdr_adaptive_decode.c \
    src/analysis/deepread/uft_deepread_splice.c \
    src/analysis/deepread/uft_deepread_aging.c \
    src/analysis/deepread/uft_deepread_crosstrack.c \
    src/analysis/deepread/uft_deepread_fingerprint.c \
    src/analysis/deepread/uft_deepread_soft_decode.c \
    src/analysis/uft_anomaly_detect.c \
    src/analysis/uft_ml_protection.c

HEADERS += \
    include/uft/encoding/uft_otdr_encoding_boost.h \
    include/uft/recovery/uft_otdr_adaptive_decode.h \
    include/uft/analysis/uft_deepread_splice.h \
    include/uft/analysis/uft_deepread_aging.h \
    include/uft/analysis/uft_deepread_crosstrack.h \
    include/uft/analysis/uft_deepread_fingerprint.h \
    include/uft/analysis/uft_deepread_soft_decode.h \
    include/uft/analysis/uft_anomaly_detect.h \
    include/uft/analysis/uft_ml_protection.h \
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

SOURCES += \
    src/core/uft_advanced_mode.c \
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
HEADERS += include/uft/core/uft_interleave.h \
           include/uft/core/uft_write_precomp.h \
           include/uft/fs/uft_bootblock_scanner.h \
           include/uft/uft_amiga_virus_db.h \
           include/uft/uft_v3_bridge.h \
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
    src/analysis/uft_triage.c \
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
    src/formats/ssd/uft_ssd_plugin.c \
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
    src/core/uft_decode_pipeline.c \
    src/parsers/a2r/uft_a2r_parser.c \
    src/formats/cas/uft_cas.c \
    src/formats/d77/uft_d77.c \
    src/formats/dc42/uft_dc42.c \
    # src/formats/dim/uft_dim.c  # already in main SOURCES block \
    src/formats/dms/uft_dms.c \
    src/detect/mfm/mfm_detect.c \
    src/detect/mfm/cpm_fs.c \
    src/detect/mfm/uft_mfm_detect_bridge.c \
    src/formats/amstrad/uft_edsk.c \
    src/formats/fds/uft_fds_plugin.c \
    src/formats/kfx/uft_kfx.c \
    src/formats/kfx/uft_kfstream_air.c \
    src/formats/kfx/uft_kf_histogram.c \
    src/formats/mfi/uft_mfi.c \
    src/formats/pri/uft_pri.c \
    src/formats/st/uft_st.c \
    # WOZ: real impl in src/formats/apple/uft_woz.c (already in SOURCES below)


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
    src/formats/d64/uft_d64_parser_v3.c \
    src/formats/d71/uft_d71_parser_v2.c \
    src/formats/d81/uft_d81_parser_v2.c \
    src/formats/d88/uft_d88.c \
    src/formats/d88/uft_d88_parser_v2.c \
    src/formats/dmk/uft_dmk_parser_v2.c \
    src/formats/dsk_cpc/uft_dsk_cpc_parser_v2.c \
    src/formats/fdi/uft_fdi_parser_v2.c \
    src/formats/g64/uft_g64.c \
    src/formats/g64/uft_g64_parser_v3.c \
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
# Format Conversion Engine (split from uft_format_convert.c)
# ═══════════════════════════════════════════════════════════════════════════════

SOURCES += \
    src/formats/uft_format_convert_tables.c \
    src/formats/uft_format_convert_archive.c \
    src/formats/uft_format_convert_flux.c \
    src/formats/uft_format_convert_bitstream.c \
    src/formats/uft_format_convert_sector.c \
    src/formats/uft_format_convert_dispatch.c

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

# SOURCES += src/formats/mega65/uft_mega65_d81.c  # deleted (35 compile errors)

# v4.0 GUI Panels (DMK Analyzer, GW-to-DMK, Flux Histogram)
# ===============================================================================

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
    src/core/uft_ir_format.c \
    src/core/uft_interleave.c \
    src/core/uft_write_precomp.c

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
    include/uft/hal/internal/uft_hal_unified.h \
    include/uft/hal/internal/uft_hal_profiles.h \
    include/uft/hal/uft_hal_v3.h \
    include/uft/hal/internal/uft_hal_v2.h \
    include/uft/hal/internal/uft_greaseweazle.h \
    include/uft/hal/uft_drive.h \
    include/uft/hal/uft_kryoflux.h \
    include/uft/uft_ir_format.h

# ═══════════════════════════════════════════════════════════════════════════════
# Switch/MIG Dumper Module (Nintendo Switch Cartridge Support)
# Optional — enable with: qmake CONFIG+=switch_support
# ═══════════════════════════════════════════════════════════════════════════════

switch_support {
    DEFINES += UFT_HAS_SWITCH
    message("Switch/MIG Dumper Module ENABLED")

    INCLUDEPATH += \
        $$PWD/src/switch \
        $$PWD/src/switch/hactool \
        $$PWD/src/switch/hactool/mbedtls/include

    SOURCES += \
        src/switch/uft_mig_dumper.c \
        src/switch/uft_xci_parser_stubs.c \
        src/gui/uft_switch_panel.cpp

    HEADERS += \
        src/switch/uft_switch_types.h \
        src/switch/uft_mig_dumper.h \
        src/switch/uft_xci_parser.h \
        src/gui/uft_switch_panel.h

    # hactool sources (ISC License - third party)
    SOURCES += \
        src/switch/hactool/xci.c \
        src/switch/hactool/nca.c \
        src/switch/hactool/pfs0.c \
        src/switch/hactool/hfs0.c \
        src/switch/hactool/romfs.c \
        src/switch/hactool/nca0_romfs.c \
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

    # mbedtls (Apache 2.0 License - vendor copy, see src/switch/hactool/mbedtls/VENDOR.md)
    # Note: hactool wrappers renamed to hactool_aes.c/hactool_rsa.c to avoid collision
    MBEDTLS_PATH = $$PWD/src/switch/hactool/mbedtls/library
    MBEDTLS_SOURCES = $$files($$MBEDTLS_PATH/*.c)
    SOURCES += $$MBEDTLS_SOURCES
} else {
    message("Switch/MIG Dumper Module DISABLED (use CONFIG+=switch_support to enable)")
}

# NOTE: Warning suppressions already set globally (lines 59-67, 413)
# -Wno-unused-parameter, -Wno-sign-compare are still required (860+ source files)

# ═══════════════════════════════════════════════════════════════════════════════
# Cart7/Cart8 Multi-System Cartridge Reader (NES, SNES, N64, MD, GBA, GB, 3DS)
# Optional — enable with: qmake CONFIG+=cart7_support
# ═══════════════════════════════════════════════════════════════════════════════

cart7_support {
    DEFINES += UFT_HAS_CART7
    message("Cart7/Cart8 Cartridge Reader Module ENABLED")

    INCLUDEPATH += \
        $$PWD/src/cart7 \
        $$PWD/include/uft/cart7

    SOURCES += \
        src/cart7/uft_cart7.c \
        src/cart7/uft_cart7_3ds.c \
        src/cart7/uft_cart7_hal.c \
        src/gui/uft_cart7_panel.cpp

    HEADERS += \
        include/uft/cart7/cart7_protocol.h \
        include/uft/cart7/cart7_3ds_protocol.h \
        include/uft/cart7/uft_cart7.h \
        include/uft/cart7/uft_cart7_3ds.h \
        src/cart7/uft_cart7_hal.h \
        src/gui/uft_cart7_panel.h
} else {
    message("Cart7/Cart8 Cartridge Reader Module DISABLED (use CONFIG+=cart7_support to enable)")
}

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

# Apple formats (15 files)
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
    src/formats/apple/uft_moof_parser.c \
    src/formats/apple/uft_woz.c \
    src/formats/apple/woz.c \
    src/formats/apple/uft_ndif.c \
    src/formats/apple/uft_edd.c \
    src/formats/apple/uft_dart.c

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
    src/formats/amiga/uft_amiga_protection.c \
    src/protection/ufm_c64_scheme_detect.c \
    src/protection/uft_protection_unified.c \
    src/protection/uft_c64_missing_schemes.c \
    src/protection/uft_amiga_missing_schemes.c \
    src/protection/uft_atarist_missing_schemes.c

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
    include/uft/formats/polyglot_boot.h \
    include/uft/formats/apple/uft_moof.h \
    include/uft/formats/apple/uft_ndif.h \
    include/uft/formats/apple/uft_edd.h \
    include/uft/formats/apple/uft_dart.h

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
    src/formats/mame/uft_mame_mfi.c \
    src/formats/mame/uft_chd.c

HEADERS += \
    include/uft/formats/mame/uft_chd.h

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
    src/formats/nintendo/uft_switch.c \
    src/formats/nintendo/uft_fds.c

HEADERS += \
    include/uft/formats/nintendo/uft_fds.h

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
# Forensic Provenance Chain
# ═══════════════════════════════════════════════════════════════════════════════
SOURCES += src/forensic/uft_provenance.c
HEADERS += include/uft/forensic/uft_provenance.h

# ═══════════════════════════════════════════════════════════════════════════════
# Recovery Wizard + Format Suggestion Engine
# ═══════════════════════════════════════════════════════════════════════════════
SOURCES += \
    src/recovery/uft_recovery_wizard.c \
    src/analysis/uft_format_suggest.c

HEADERS += \
    include/uft/recovery/uft_recovery_wizard.h \
    include/uft/analysis/uft_format_suggest.h

# ═══════════════════════════════════════════════════════════════════════════════
# Smart Export Dialog + ML Analysis GUI
# ═══════════════════════════════════════════════════════════════════════════════
SOURCES += src/gui/uft_smart_export_dialog.cpp
HEADERS += src/gui/uft_smart_export_dialog.h

# ═══════════════════════════════════════════════════════════════════════════════
# Sector Compare Dialog + Recovery Wizard Dialog
# ═══════════════════════════════════════════════════════════════════════════════
SOURCES += \
    src/gui/uft_compare_dialog.cpp \
    src/gui/uft_recovery_dialog.cpp

# ═══════════════════════════════════════════════════════════════════════════════
# New Format Parsers: Aaru, HxCStream, 86F (pc), SaveDskF
# ═══════════════════════════════════════════════════════════════════════════════
SOURCES += \
    src/formats/modern/uft_aaru.c \
    src/formats/flux/uft_hxcstream.c \
    src/formats/pc/uft_86f.c \
    src/formats/pc/uft_savedskf.c

HEADERS += \
    include/uft/formats/modern/uft_aaru.h \
    include/uft/formats/flux/uft_hxcstream.h \
    include/uft/formats/pc/uft_86f.h \
    include/uft/formats/pc/uft_savedskf.h

INCLUDEPATH += \
    $$PWD/src/formats/modern \
    $$PWD/src/formats/pc

HEADERS += \
    src/gui/uft_compare_dialog.h \
    src/gui/uft_recovery_dialog.h

# ═══════════════════════════════════════════════════════════════════════════════
# Copy Protection Detection (complete set — 38 source files)
# Previously only 5 files were compiled; audit found 33 missing.
# ═══════════════════════════════════════════════════════════════════════════════

# Include path for c64 internal headers
INCLUDEPATH += $$PWD/src/protection/c64

# C64 Protection Subdirectory (5 files)
SOURCES += \
    src/protection/c64/c64_protection_analysis.c \
    src/protection/c64/c64_protection_db.c \
    src/protection/c64/uft_c64_protection.c \
    src/protection/c64/uft_c64_protection_ext.c \
    src/protection/c64/uft_track_align.c

# Core Protection Framework (4 files)
SOURCES += \
    src/protection/uft_protection.c \
    src/protection/uft_protection_api.c \
    src/protection/uft_protection_classify.c \
    src/protection/uft_protection_detect.c

# Protection Extensions & Stubs (4 files)
SOURCES += \
    src/protection/uft_protection_ext.c \
    src/protection/uft_protection_extended.c \
    src/protection/uft_protection_params.c \
    src/protection/uft_protection_stubs.c

# Amiga Protection (3 files)
SOURCES += \
    src/protection/uft_amiga_caps.c \
    src/protection/uft_amiga_protection.c \
    src/protection/uft_amiga_protection_full.c

# Atari ST Protection (5 files)
SOURCES += \
    src/protection/uft_atarist_copylock.c \
    src/protection/uft_atarist_dec0de.c \
    src/protection/uft_atarist_macrodos.c \
    src/protection/uft_atarist_protection.c \
    src/protection/uft_atari8_protection.c

# C64 Protection — Top-level (2 files)
SOURCES += \
    src/protection/uft_c64_protection_enhanced.c \
    src/protection/uft_geos_protection.c

# Apple II Protection (1 file)
SOURCES += \
    src/protection/uft_apple2_protection.c

# PC Protection (2 files)
SOURCES += \
    src/protection/uft_pc_protection.c \
    src/protection/uft_pc_cdrom_protection.c

# Individual Scheme Detectors (5 files)
SOURCES += \
    src/protection/uft_copylock.c \
    src/protection/uft_fuzzy_bits.c \
    src/protection/uft_longtrack.c \
    src/protection/uft_rapidlok.c \
    src/protection/uft_speedlock.c

# Support Modules (2 files)
SOURCES += \
    src/protection/uft_magnetic_state.c \
    src/protection/uft_rtc_decompress.c

# Protection Headers (31 files)
HEADERS += \
    include/uft/protection/ufm_c64_protection_taxonomy.h \
    include/uft/protection/ufm_c64_scheme_detect.h \
    include/uft/protection/ufm_cbm_protection_methods.h \
    include/uft/protection/uft_amiga_caps.h \
    include/uft/protection/uft_amiga_protection_full.h \
    include/uft/protection/uft_amiga_protection_registry.h \
    include/uft/protection/uft_apple2_protection.h \
    include/uft/protection/uft_atari8_protection.h \
    include/uft/protection/uft_atarist_copylock.h \
    include/uft/protection/uft_atarist_dec0de.h \
    include/uft/protection/uft_atarist_macrodos.h \
    include/uft/protection/uft_atarist_protection.h \
    include/uft/protection/uft_c64_protection.h \
    include/uft/protection/uft_c64_protection_enhanced.h \
    include/uft/protection/uft_c64_protection_ext.h \
    include/uft/protection/uft_copylock.h \
    include/uft/protection/uft_fuzzy_bits.h \
    include/uft/protection/uft_geos_protection.h \
    include/uft/protection/uft_longtrack.h \
    include/uft/protection/uft_magnetic_state.h \
    include/uft/protection/uft_pc_cdrom_protection.h \
    include/uft/protection/uft_protection.h \
    include/uft/protection/uft_protection_classify.h \
    include/uft/protection/uft_protection_ext.h \
    include/uft/protection/uft_protection_extended.h \
    include/uft/protection/uft_protection_params.h \
    include/uft/protection/uft_protection_stubs.h \
    include/uft/protection/uft_protection_unified.h \
    include/uft/protection/uft_rtc_decompress.h \
    include/uft/protection/uft_speedlock.h \
    include/uft/protection/uft_track_align.h

# Internal header (not in include/uft/)
HEADERS += src/protection/c64/c64_protection_internal.h

# ═══════════════════════════════════════════════════════════════════════════════
# BUILD FIXES (v4.1.0)
# ═══════════════════════════════════════════════════════════════════════════════

# Fix 1: Deduplicate SOURCES/HEADERS (OTDR bridge files 100x duplicated)
# NOTE: qmake deduplicates, but clean listing preferred
# TODO: Remove duplicate analysis/events + analysis/denoise entries from each
#       SOURCES block above — they are listed 99x but only needed once (lines 144-160)
SOURCES = $$unique(SOURCES)
HEADERS = $$unique(HEADERS)

# Fix 2: MSVC C11 mode for .c files (MSVC defaults to C89)
win32-msvc* {
    QMAKE_CFLAGS += /std:c11
}
