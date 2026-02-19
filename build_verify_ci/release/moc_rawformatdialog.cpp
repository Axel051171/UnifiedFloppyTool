/****************************************************************************
** Meta object code from reading C++ file 'rawformatdialog.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/rawformatdialog.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'rawformatdialog.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15RawFormatDialogE_t {};
} // unnamed namespace

template <> constexpr inline auto RawFormatDialog::qt_create_metaobjectdata<qt_meta_tag_ZN15RawFormatDialogE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RawFormatDialog",
        "configurationApplied",
        "",
        "RawConfig",
        "config",
        "loadRawFile",
        "path",
        "createEmptyFloppy",
        "onTrackTypeChanged",
        "index",
        "onGeometryChanged",
        "onLayoutPresetChanged",
        "onAutoGap3Toggled",
        "checked",
        "onSaveConfig",
        "onLoadConfig",
        "onLoadRawFile",
        "onCreateEmpty",
        "updateCalculatedValues"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'configurationApplied'
        QtMocHelpers::SignalData<void(const RawConfig &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'loadRawFile'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'createEmptyFloppy'
        QtMocHelpers::SignalData<void(const RawConfig &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'onTrackTypeChanged'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onGeometryChanged'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLayoutPresetChanged'
        QtMocHelpers::SlotData<void(int)>(11, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onAutoGap3Toggled'
        QtMocHelpers::SlotData<void(bool)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 13 },
        }}),
        // Slot 'onSaveConfig'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoadConfig'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoadRawFile'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCreateEmpty'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'updateCalculatedValues'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RawFormatDialog, qt_meta_tag_ZN15RawFormatDialogE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RawFormatDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15RawFormatDialogE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15RawFormatDialogE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15RawFormatDialogE_t>.metaTypes,
    nullptr
} };

void RawFormatDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RawFormatDialog *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->configurationApplied((*reinterpret_cast<std::add_pointer_t<RawConfig>>(_a[1]))); break;
        case 1: _t->loadRawFile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->createEmptyFloppy((*reinterpret_cast<std::add_pointer_t<RawConfig>>(_a[1]))); break;
        case 3: _t->onTrackTypeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onGeometryChanged(); break;
        case 5: _t->onLayoutPresetChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onAutoGap3Toggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->onSaveConfig(); break;
        case 8: _t->onLoadConfig(); break;
        case 9: _t->onLoadRawFile(); break;
        case 10: _t->onCreateEmpty(); break;
        case 11: _t->updateCalculatedValues(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RawFormatDialog::*)(const RawConfig & )>(_a, &RawFormatDialog::configurationApplied, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RawFormatDialog::*)(const QString & )>(_a, &RawFormatDialog::loadRawFile, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (RawFormatDialog::*)(const RawConfig & )>(_a, &RawFormatDialog::createEmptyFloppy, 2))
            return;
    }
}

const QMetaObject *RawFormatDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RawFormatDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15RawFormatDialogE_t>.strings))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int RawFormatDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void RawFormatDialog::configurationApplied(const RawConfig & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void RawFormatDialog::loadRawFile(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void RawFormatDialog::createEmptyFloppy(const RawConfig & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
