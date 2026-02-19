/****************************************************************************
** Meta object code from reading C++ file 'uft_cart7_panel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/gui/uft_cart7_panel.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uft_cart7_panel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN11Cart7WorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto Cart7Worker::qt_create_metaobjectdata<qt_meta_tag_ZN11Cart7WorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Cart7Worker",
        "progressChanged",
        "",
        "current",
        "total",
        "speed",
        "finished",
        "success",
        "message",
        "statusChanged",
        "status"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progressChanged'
        QtMocHelpers::SignalData<void(quint64, quint64, quint32)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 3 }, { QMetaType::ULongLong, 4 }, { QMetaType::UInt, 5 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 7 }, { QMetaType::QString, 8 },
        }}),
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void(const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Cart7Worker, qt_meta_tag_ZN11Cart7WorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Cart7Worker::staticMetaObject = { {
    QMetaObject::SuperData::link<QThread::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11Cart7WorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11Cart7WorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN11Cart7WorkerE_t>.metaTypes,
    nullptr
} };

void Cart7Worker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Cart7Worker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progressChanged((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[3]))); break;
        case 1: _t->finished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->statusChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Cart7Worker::*)(quint64 , quint64 , quint32 )>(_a, &Cart7Worker::progressChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Cart7Worker::*)(bool , const QString & )>(_a, &Cart7Worker::finished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Cart7Worker::*)(const QString & )>(_a, &Cart7Worker::statusChanged, 2))
            return;
    }
}

const QMetaObject *Cart7Worker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Cart7Worker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN11Cart7WorkerE_t>.strings))
        return static_cast<void*>(this);
    return QThread::qt_metacast(_clname);
}

int Cart7Worker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QThread::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void Cart7Worker::progressChanged(quint64 _t1, quint64 _t2, quint32 _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void Cart7Worker::finished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void Cart7Worker::statusChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN10Cart7PanelE_t {};
} // unnamed namespace

template <> constexpr inline auto Cart7Panel::qt_create_metaobjectdata<qt_meta_tag_ZN10Cart7PanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Cart7Panel",
        "refreshDevices",
        "",
        "connectDevice",
        "disconnectDevice",
        "selectSlot",
        "index",
        "startDump",
        "startSaveBackup",
        "startSaveRestore",
        "abortOperation",
        "browseOutputPath",
        "browseInputPath",
        "onPollTimer",
        "onWorkerProgress",
        "current",
        "total",
        "speed",
        "onWorkerFinished",
        "success",
        "message",
        "onWorkerStatus",
        "status"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'refreshDevices'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'connectDevice'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'disconnectDevice'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'selectSlot'
        QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'startDump'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startSaveBackup'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startSaveRestore'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'abortOperation'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'browseOutputPath'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'browseInputPath'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onPollTimer'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onWorkerProgress'
        QtMocHelpers::SlotData<void(quint64, quint64, quint32)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::ULongLong, 15 }, { QMetaType::ULongLong, 16 }, { QMetaType::UInt, 17 },
        }}),
        // Slot 'onWorkerFinished'
        QtMocHelpers::SlotData<void(bool, const QString &)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 19 }, { QMetaType::QString, 20 },
        }}),
        // Slot 'onWorkerStatus'
        QtMocHelpers::SlotData<void(const QString &)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Cart7Panel, qt_meta_tag_ZN10Cart7PanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Cart7Panel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10Cart7PanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10Cart7PanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10Cart7PanelE_t>.metaTypes,
    nullptr
} };

void Cart7Panel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Cart7Panel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->refreshDevices(); break;
        case 1: _t->connectDevice(); break;
        case 2: _t->disconnectDevice(); break;
        case 3: _t->selectSlot((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->startDump(); break;
        case 5: _t->startSaveBackup(); break;
        case 6: _t->startSaveRestore(); break;
        case 7: _t->abortOperation(); break;
        case 8: _t->browseOutputPath(); break;
        case 9: _t->browseInputPath(); break;
        case 10: _t->onPollTimer(); break;
        case 11: _t->onWorkerProgress((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint32>>(_a[3]))); break;
        case 12: _t->onWorkerFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 13: _t->onWorkerStatus((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *Cart7Panel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Cart7Panel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10Cart7PanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Cart7Panel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
