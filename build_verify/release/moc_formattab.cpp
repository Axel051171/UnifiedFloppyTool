/****************************************************************************
** Meta object code from reading C++ file 'formattab.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/formattab.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'formattab.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN9FormatTabE_t {};
} // unnamed namespace

template <> constexpr inline auto FormatTab::qt_create_metaobjectdata<qt_meta_tag_ZN9FormatTabE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FormatTab",
        "protectionSettingsChanged",
        "",
        "formatSettingsChanged",
        "readOptionsChanged",
        "writeOptionsChanged",
        "systemChanged",
        "system",
        "formatChanged",
        "format",
        "loadSettings",
        "saveSettings",
        "onFluxAdvanced",
        "onPLLAdvanced",
        "onNibbleAdvanced",
        "onSystemChanged",
        "index",
        "onFormatChanged",
        "onVersionChanged",
        "onEncodingChanged",
        "onDetectAllToggled",
        "checked",
        "onPlatformChanged",
        "onProtectionCheckChanged",
        "onAdaptivePLLToggled",
        "onPreserveTimingToggled",
        "onCopyModeChanged",
        "onAllTracksToggled",
        "onGCRTypeChanged",
        "onRetryErrorsToggled",
        "onLogToFileToggled",
        "onBrowseLogPath",
        "onValidateStructureToggled",
        "onReportFormatChanged",
        "onGw2DmkOpenClicked",
        "onLoadPreset",
        "onSavePreset",
        "onPresetChanged",
        "onReadSpeedChanged",
        "onIgnoreReadErrorsChanged",
        "onFastErrorSkipChanged",
        "onAdvancedScanningChanged",
        "onScanFactorChanged",
        "value",
        "onReadTimingDataChanged",
        "onDPMAnalysisChanged",
        "onReadSubChannelChanged",
        "onVerifyAfterWriteChanged",
        "onIgnoreWriteErrorsChanged",
        "onWriteTimingDataChanged",
        "onCorrectSubChannelChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'protectionSettingsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'formatSettingsChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'readOptionsChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'writeOptionsChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'systemChanged'
        QtMocHelpers::SignalData<void(const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 7 },
        }}),
        // Signal 'formatChanged'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Slot 'loadSettings'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'saveSettings'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onFluxAdvanced'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPLLAdvanced'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onNibbleAdvanced'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSystemChanged'
        QtMocHelpers::SlotData<void(int)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onFormatChanged'
        QtMocHelpers::SlotData<void(int)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onVersionChanged'
        QtMocHelpers::SlotData<void(int)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onEncodingChanged'
        QtMocHelpers::SlotData<void(int)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onDetectAllToggled'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onPlatformChanged'
        QtMocHelpers::SlotData<void(int)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onProtectionCheckChanged'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAdaptivePLLToggled'
        QtMocHelpers::SlotData<void(bool)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onPreserveTimingToggled'
        QtMocHelpers::SlotData<void(bool)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onCopyModeChanged'
        QtMocHelpers::SlotData<void(int)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onAllTracksToggled'
        QtMocHelpers::SlotData<void(bool)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onGCRTypeChanged'
        QtMocHelpers::SlotData<void(int)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onRetryErrorsToggled'
        QtMocHelpers::SlotData<void(bool)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onLogToFileToggled'
        QtMocHelpers::SlotData<void(bool)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onBrowseLogPath'
        QtMocHelpers::SlotData<void()>(31, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onValidateStructureToggled'
        QtMocHelpers::SlotData<void(bool)>(32, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onReportFormatChanged'
        QtMocHelpers::SlotData<void(int)>(33, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onGw2DmkOpenClicked'
        QtMocHelpers::SlotData<void()>(34, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoadPreset'
        QtMocHelpers::SlotData<void()>(35, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSavePreset'
        QtMocHelpers::SlotData<void()>(36, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPresetChanged'
        QtMocHelpers::SlotData<void(int)>(37, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onReadSpeedChanged'
        QtMocHelpers::SlotData<void(int)>(38, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 16 },
        }}),
        // Slot 'onIgnoreReadErrorsChanged'
        QtMocHelpers::SlotData<void(bool)>(39, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onFastErrorSkipChanged'
        QtMocHelpers::SlotData<void(bool)>(40, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onAdvancedScanningChanged'
        QtMocHelpers::SlotData<void(bool)>(41, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onScanFactorChanged'
        QtMocHelpers::SlotData<void(int)>(42, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 43 },
        }}),
        // Slot 'onReadTimingDataChanged'
        QtMocHelpers::SlotData<void(bool)>(44, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onDPMAnalysisChanged'
        QtMocHelpers::SlotData<void(bool)>(45, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onReadSubChannelChanged'
        QtMocHelpers::SlotData<void(bool)>(46, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onVerifyAfterWriteChanged'
        QtMocHelpers::SlotData<void(bool)>(47, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onIgnoreWriteErrorsChanged'
        QtMocHelpers::SlotData<void(bool)>(48, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onWriteTimingDataChanged'
        QtMocHelpers::SlotData<void(bool)>(49, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'onCorrectSubChannelChanged'
        QtMocHelpers::SlotData<void(bool)>(50, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FormatTab, qt_meta_tag_ZN9FormatTabE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FormatTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9FormatTabE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9FormatTabE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9FormatTabE_t>.metaTypes,
    nullptr
} };

void FormatTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FormatTab *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->protectionSettingsChanged(); break;
        case 1: _t->formatSettingsChanged(); break;
        case 2: _t->readOptionsChanged(); break;
        case 3: _t->writeOptionsChanged(); break;
        case 4: _t->systemChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->formatChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->loadSettings(); break;
        case 7: _t->saveSettings(); break;
        case 8: _t->onFluxAdvanced(); break;
        case 9: _t->onPLLAdvanced(); break;
        case 10: _t->onNibbleAdvanced(); break;
        case 11: _t->onSystemChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->onFormatChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 13: _t->onVersionChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->onEncodingChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 15: _t->onDetectAllToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->onPlatformChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 17: _t->onProtectionCheckChanged(); break;
        case 18: _t->onAdaptivePLLToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 19: _t->onPreserveTimingToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 20: _t->onCopyModeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 21: _t->onAllTracksToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 22: _t->onGCRTypeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 23: _t->onRetryErrorsToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 24: _t->onLogToFileToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 25: _t->onBrowseLogPath(); break;
        case 26: _t->onValidateStructureToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 27: _t->onReportFormatChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 28: _t->onGw2DmkOpenClicked(); break;
        case 29: _t->onLoadPreset(); break;
        case 30: _t->onSavePreset(); break;
        case 31: _t->onPresetChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 32: _t->onReadSpeedChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 33: _t->onIgnoreReadErrorsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 34: _t->onFastErrorSkipChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 35: _t->onAdvancedScanningChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 36: _t->onScanFactorChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 37: _t->onReadTimingDataChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 38: _t->onDPMAnalysisChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 39: _t->onReadSubChannelChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 40: _t->onVerifyAfterWriteChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 41: _t->onIgnoreWriteErrorsChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 42: _t->onWriteTimingDataChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 43: _t->onCorrectSubChannelChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)()>(_a, &FormatTab::protectionSettingsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)()>(_a, &FormatTab::formatSettingsChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)()>(_a, &FormatTab::readOptionsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)()>(_a, &FormatTab::writeOptionsChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)(const QString & )>(_a, &FormatTab::systemChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (FormatTab::*)(const QString & )>(_a, &FormatTab::formatChanged, 5))
            return;
    }
}

const QMetaObject *FormatTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FormatTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9FormatTabE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FormatTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 44)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 44;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 44)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 44;
    }
    return _id;
}

// SIGNAL 0
void FormatTab::protectionSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void FormatTab::formatSettingsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void FormatTab::readOptionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void FormatTab::writeOptionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void FormatTab::systemChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void FormatTab::formatChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
