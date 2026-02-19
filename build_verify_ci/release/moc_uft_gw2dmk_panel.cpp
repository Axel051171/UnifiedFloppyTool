/****************************************************************************
** Meta object code from reading C++ file 'uft_gw2dmk_panel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/uft_gw2dmk_panel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uft_gw2dmk_panel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN15UftGw2DmkWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto UftGw2DmkWorker::qt_create_metaobjectdata<qt_meta_tag_ZN15UftGw2DmkWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UftGw2DmkWorker",
        "deviceDetected",
        "",
        "info",
        "deviceError",
        "error",
        "progressChanged",
        "track",
        "head",
        "total",
        "message",
        "trackRead",
        "sectors",
        "errors",
        "operationComplete",
        "success",
        "fluxDataReady",
        "data"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'deviceDetected'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'deviceError'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'progressChanged'
        QtMocHelpers::SignalData<void(int, int, int, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 8 }, { QMetaType::Int, 9 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'trackRead'
        QtMocHelpers::SignalData<void(int, int, int, int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 8 }, { QMetaType::Int, 12 }, { QMetaType::Int, 13 },
        }}),
        // Signal 'operationComplete'
        QtMocHelpers::SignalData<void(bool, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 15 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'fluxDataReady'
        QtMocHelpers::SignalData<void(int, int, const QByteArray &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 }, { QMetaType::Int, 8 }, { QMetaType::QByteArray, 17 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UftGw2DmkWorker, qt_meta_tag_ZN15UftGw2DmkWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UftGw2DmkWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QThread::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UftGw2DmkWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UftGw2DmkWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15UftGw2DmkWorkerE_t>.metaTypes,
    nullptr
} };

void UftGw2DmkWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UftGw2DmkWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->deviceDetected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->deviceError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->progressChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 3: _t->trackRead((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 4: _t->operationComplete((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->fluxDataReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(const QString & )>(_a, &UftGw2DmkWorker::deviceDetected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(const QString & )>(_a, &UftGw2DmkWorker::deviceError, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(int , int , int , const QString & )>(_a, &UftGw2DmkWorker::progressChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(int , int , int , int )>(_a, &UftGw2DmkWorker::trackRead, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(bool , const QString & )>(_a, &UftGw2DmkWorker::operationComplete, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkWorker::*)(int , int , const QByteArray & )>(_a, &UftGw2DmkWorker::fluxDataReady, 5))
            return;
    }
}

const QMetaObject *UftGw2DmkWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UftGw2DmkWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15UftGw2DmkWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QThread::qt_metacast(_clname);
}

int UftGw2DmkWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void UftGw2DmkWorker::deviceDetected(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void UftGw2DmkWorker::deviceError(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void UftGw2DmkWorker::progressChanged(int _t1, int _t2, int _t3, const QString & _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 3
void UftGw2DmkWorker::trackRead(int _t1, int _t2, int _t3, int _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 4
void UftGw2DmkWorker::operationComplete(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void UftGw2DmkWorker::fluxDataReady(int _t1, int _t2, const QByteArray & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}
namespace {
struct qt_meta_tag_ZN14UftGw2DmkPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto UftGw2DmkPanel::qt_create_metaobjectdata<qt_meta_tag_ZN14UftGw2DmkPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UftGw2DmkPanel",
        "trackReadComplete",
        "",
        "track",
        "head",
        "diskReadComplete",
        "filename",
        "fluxHistogramRequested",
        "data",
        "detectDevice",
        "readDisk",
        "readTrack",
        "stopOperation",
        "browseOutput",
        "setPreset",
        "index",
        "onDeviceDetected",
        "info",
        "onDeviceError",
        "error",
        "onProgressChanged",
        "total",
        "message",
        "onTrackRead",
        "sectors",
        "errors",
        "onOperationComplete",
        "success",
        "onFluxDataReady",
        "onDiskTypeChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'trackReadComplete'
        QtMocHelpers::SignalData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
        // Signal 'diskReadComplete'
        QtMocHelpers::SignalData<void(const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 },
        }}),
        // Signal 'fluxHistogramRequested'
        QtMocHelpers::SignalData<void(const QByteArray &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 8 },
        }}),
        // Slot 'detectDevice'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'readDisk'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'readTrack'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'stopOperation'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'browseOutput'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setPreset'
        QtMocHelpers::SlotData<void(int)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
        // Slot 'onDeviceDetected'
        QtMocHelpers::SlotData<void(const QString &)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 17 },
        }}),
        // Slot 'onDeviceError'
        QtMocHelpers::SlotData<void(const QString &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'onProgressChanged'
        QtMocHelpers::SlotData<void(int, int, int, const QString &)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 }, { QMetaType::Int, 21 }, { QMetaType::QString, 22 },
        }}),
        // Slot 'onTrackRead'
        QtMocHelpers::SlotData<void(int, int, int, int)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 }, { QMetaType::Int, 24 }, { QMetaType::Int, 25 },
        }}),
        // Slot 'onOperationComplete'
        QtMocHelpers::SlotData<void(bool, const QString &)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 27 }, { QMetaType::QString, 22 },
        }}),
        // Slot 'onFluxDataReady'
        QtMocHelpers::SlotData<void(int, int, const QByteArray &)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 }, { QMetaType::QByteArray, 8 },
        }}),
        // Slot 'onDiskTypeChanged'
        QtMocHelpers::SlotData<void(int)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 15 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UftGw2DmkPanel, qt_meta_tag_ZN14UftGw2DmkPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UftGw2DmkPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftGw2DmkPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftGw2DmkPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14UftGw2DmkPanelE_t>.metaTypes,
    nullptr
} };

void UftGw2DmkPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UftGw2DmkPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->trackReadComplete((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->diskReadComplete((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->fluxHistogramRequested((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 3: _t->detectDevice(); break;
        case 4: _t->readDisk(); break;
        case 5: _t->readTrack(); break;
        case 6: _t->stopOperation(); break;
        case 7: _t->browseOutput(); break;
        case 8: _t->setPreset((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->onDeviceDetected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->onDeviceError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->onProgressChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 12: _t->onTrackRead((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[4]))); break;
        case 13: _t->onOperationComplete((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 14: _t->onFluxDataReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[3]))); break;
        case 15: _t->onDiskTypeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkPanel::*)(int , int )>(_a, &UftGw2DmkPanel::trackReadComplete, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkPanel::*)(const QString & )>(_a, &UftGw2DmkPanel::diskReadComplete, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftGw2DmkPanel::*)(const QByteArray & )>(_a, &UftGw2DmkPanel::fluxHistogramRequested, 2))
            return;
    }
}

const QMetaObject *UftGw2DmkPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UftGw2DmkPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftGw2DmkPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UftGw2DmkPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void UftGw2DmkPanel::trackReadComplete(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void UftGw2DmkPanel::diskReadComplete(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void UftGw2DmkPanel::fluxHistogramRequested(const QByteArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
