#═══════════════════════════════════════════════════════════════════════════════
# UnifiedFloppyTool.pro - Qt 5/6 Compatible Build
#═══════════════════════════════════════════════════════════════════════════════

QT += core gui widgets

CONFIG += c++17
CONFIG += sdk_no_version_check

VERSION = 3.7.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

message("════════════════════════════════════════════════════")
message("UnifiedFloppyTool v$$VERSION - VISUAL Edition")
message("════════════════════════════════════════════════════")

win32 {
    message("Compiler: MSVC (Windows)")
    QMAKE_CXXFLAGS += /W3 /utf-8
    RC_ICONS = resources/icons/app.ico
}

unix:!macx {
    message("Compiler: GCC/Clang (Linux)")
    QMAKE_CXXFLAGS += -Wall -Wextra
}

macx {
    message("Compiler: Clang (macOS)")
    QMAKE_CXXFLAGS += -Wall -Wextra
    ICON = resources/icons/app.icns
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
}

CONFIG(debug, debug|release) {
    DESTDIR = debug
} else {
    DESTDIR = release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui

INCLUDEPATH += src include include/uft

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

exists(src/forensictab.cpp): SOURCES += src/forensictab.cpp
exists(src/nibbletab.cpp): SOURCES += src/nibbletab.cpp
exists(src/xcopytab.cpp): SOURCES += src/xcopytab.cpp

# Protection API Implementation (C)
SOURCES += src/protection/uft_protection_api.c

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

exists(src/forensictab.h): HEADERS += src/forensictab.h
exists(src/nibbletab.h): HEADERS += src/nibbletab.h
exists(src/xcopytab.h): HEADERS += src/xcopytab.h

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
    forms/diskanalyzer_window.ui

exists(forms/dialog_validation.ui): FORMS += forms/dialog_validation.ui
exists(forms/tab_forensic.ui): FORMS += forms/tab_forensic.ui
exists(forms/tab_nibble.ui): FORMS += forms/tab_nibble.ui
exists(forms/tab_xcopy.ui): FORMS += forms/tab_xcopy.ui

exists(resources/resources.qrc): RESOURCES += resources/resources.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
