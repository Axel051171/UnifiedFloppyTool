/****************************************************************************
** Meta object code from reading C++ file 'hardwareprovider.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/hardware_providers/hardwareprovider.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'hardwareprovider.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16HardwareProviderE_t {};
} // unnamed namespace

template <> constexpr inline auto HardwareProvider::qt_create_metaobjectdata<qt_meta_tag_ZN16HardwareProviderE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "HardwareProvider",
        "driveDetected",
        "",
        "DetectedDriveInfo",
        "info",
        "hardwareInfoUpdated",
        "HardwareInfo",
        "statusMessage",
        "message",
        "devicePathSuggested",
        "path",
        "connectionStateChanged",
        "connected",
        "operationError",
        "error",
        "progressChanged",
        "current",
        "total",
        "trackRead",
        "cylinder",
        "head",
        "success",
        "trackWritten",
        "trackReadComplete",
        "trackWriteComplete"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'driveDetected'
        QtMocHelpers::SignalData<void(const DetectedDriveInfo &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'hardwareInfoUpdated'
        QtMocHelpers::SignalData<void(const HardwareInfo &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 4 },
        }}),
        // Signal 'statusMessage'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 },
        }}),
        // Signal 'devicePathSuggested'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Signal 'connectionStateChanged'
        QtMocHelpers::SignalData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Signal 'operationError'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'progressChanged'
        QtMocHelpers::SignalData<void(int, int)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 16 }, { QMetaType::Int, 17 },
        }}),
        // Signal 'trackRead'
        QtMocHelpers::SignalData<void(int, int, bool)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 }, { QMetaType::Bool, 21 },
        }}),
        // Signal 'trackWritten'
        QtMocHelpers::SignalData<void(int, int, bool)>(22, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 }, { QMetaType::Bool, 21 },
        }}),
        // Signal 'trackReadComplete'
        QtMocHelpers::SignalData<void(int, int, bool)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 }, { QMetaType::Bool, 21 },
        }}),
        // Signal 'trackWriteComplete'
        QtMocHelpers::SignalData<void(int, int, bool)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 }, { QMetaType::Int, 20 }, { QMetaType::Bool, 21 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<HardwareProvider, qt_meta_tag_ZN16HardwareProviderE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject HardwareProvider::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16HardwareProviderE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16HardwareProviderE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16HardwareProviderE_t>.metaTypes,
    nullptr
} };

void HardwareProvider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<HardwareProvider *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->driveDetected((*reinterpret_cast<std::add_pointer_t<DetectedDriveInfo>>(_a[1]))); break;
        case 1: _t->hardwareInfoUpdated((*reinterpret_cast<std::add_pointer_t<HardwareInfo>>(_a[1]))); break;
        case 2: _t->statusMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->devicePathSuggested((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->connectionStateChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->operationError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->progressChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 7: _t->trackRead((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 8: _t->trackWritten((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 9: _t->trackReadComplete((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 10: _t->trackWriteComplete((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< DetectedDriveInfo >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< HardwareInfo >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(const DetectedDriveInfo & )>(_a, &HardwareProvider::driveDetected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(const HardwareInfo & )>(_a, &HardwareProvider::hardwareInfoUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(const QString & )>(_a, &HardwareProvider::statusMessage, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(const QString & )>(_a, &HardwareProvider::devicePathSuggested, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(bool )>(_a, &HardwareProvider::connectionStateChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(const QString & )>(_a, &HardwareProvider::operationError, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(int , int )>(_a, &HardwareProvider::progressChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(int , int , bool )>(_a, &HardwareProvider::trackRead, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(int , int , bool )>(_a, &HardwareProvider::trackWritten, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(int , int , bool )>(_a, &HardwareProvider::trackReadComplete, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (HardwareProvider::*)(int , int , bool )>(_a, &HardwareProvider::trackWriteComplete, 10))
            return;
    }
}

const QMetaObject *HardwareProvider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *HardwareProvider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16HardwareProviderE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int HardwareProvider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void HardwareProvider::driveDetected(const DetectedDriveInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void HardwareProvider::hardwareInfoUpdated(const HardwareInfo & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void HardwareProvider::statusMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void HardwareProvider::devicePathSuggested(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void HardwareProvider::connectionStateChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void HardwareProvider::operationError(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void HardwareProvider::progressChanged(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void HardwareProvider::trackRead(int _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2, _t3);
}

// SIGNAL 8
void HardwareProvider::trackWritten(int _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2, _t3);
}

// SIGNAL 9
void HardwareProvider::trackReadComplete(int _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2, _t3);
}

// SIGNAL 10
void HardwareProvider::trackWriteComplete(int _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
