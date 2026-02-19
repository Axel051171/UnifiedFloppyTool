/****************************************************************************
** Meta object code from reading C++ file 'uft_switch_panel.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/gui/uft_switch_panel.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uft_switch_panel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN14UftSwitchPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto UftSwitchPanel::qt_create_metaobjectdata<qt_meta_tag_ZN14UftSwitchPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UftSwitchPanel",
        "deviceConnected",
        "",
        "connected",
        "cartridgeInserted",
        "inserted",
        "dumpStarted",
        "dumpProgress",
        "percent",
        "speedMbps",
        "dumpFinished",
        "success",
        "message",
        "logMessage",
        "refreshDevices",
        "connectDevice",
        "disconnectDevice",
        "startDump",
        "abortDump",
        "browseXCI",
        "extractPartition",
        "onDeviceTimer",
        "onDumpProgress",
        "bytesRead",
        "totalBytes",
        "onDumpFinished",
        "onXciSelected",
        "path",
        "onPartitionSelected",
        "index"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'deviceConnected'
        QtMocHelpers::SignalData<void(bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Signal 'cartridgeInserted'
        QtMocHelpers::SignalData<void(bool)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 5 },
        }}),
        // Signal 'dumpStarted'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'dumpProgress'
        QtMocHelpers::SignalData<void(int, double)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::Double, 9 },
        }}),
        // Signal 'dumpFinished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 11 }, { QMetaType::QString, 12 },
        }}),
        // Signal 'logMessage'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 12 },
        }}),
        // Slot 'refreshDevices'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'connectDevice'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'disconnectDevice'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startDump'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'abortDump'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'browseXCI'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'extractPartition'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onDeviceTimer'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDumpProgress'
        QtMocHelpers::SlotData<void(int, quint64, quint64, float)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::ULongLong, 23 }, { QMetaType::ULongLong, 24 }, { QMetaType::Float, 9 },
        }}),
        // Slot 'onDumpFinished'
        QtMocHelpers::SlotData<void(bool)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onXciSelected'
        QtMocHelpers::SlotData<void(const QString &)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 27 },
        }}),
        // Slot 'onPartitionSelected'
        QtMocHelpers::SlotData<void(int)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 29 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UftSwitchPanel, qt_meta_tag_ZN14UftSwitchPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UftSwitchPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftSwitchPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftSwitchPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN14UftSwitchPanelE_t>.metaTypes,
    nullptr
} };

void UftSwitchPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UftSwitchPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->deviceConnected((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->cartridgeInserted((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->dumpStarted(); break;
        case 3: _t->dumpProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 4: _t->dumpFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->logMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->refreshDevices(); break;
        case 7: _t->connectDevice(); break;
        case 8: _t->disconnectDevice(); break;
        case 9: _t->startDump(); break;
        case 10: _t->abortDump(); break;
        case 11: _t->browseXCI(); break;
        case 12: _t->extractPartition(); break;
        case 13: _t->onDeviceTimer(); break;
        case 14: _t->onDumpProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<float>>(_a[4]))); break;
        case 15: _t->onDumpFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->onXciSelected((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->onPartitionSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)(bool )>(_a, &UftSwitchPanel::deviceConnected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)(bool )>(_a, &UftSwitchPanel::cartridgeInserted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)()>(_a, &UftSwitchPanel::dumpStarted, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)(int , double )>(_a, &UftSwitchPanel::dumpProgress, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)(bool , const QString & )>(_a, &UftSwitchPanel::dumpFinished, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftSwitchPanel::*)(const QString & )>(_a, &UftSwitchPanel::logMessage, 5))
            return;
    }
}

const QMetaObject *UftSwitchPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UftSwitchPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN14UftSwitchPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UftSwitchPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void UftSwitchPanel::deviceConnected(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void UftSwitchPanel::cartridgeInserted(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void UftSwitchPanel::dumpStarted()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void UftSwitchPanel::dumpProgress(int _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void UftSwitchPanel::dumpFinished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void UftSwitchPanel::logMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN10DumpWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto DumpWorker::qt_create_metaobjectdata<qt_meta_tag_ZN10DumpWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "DumpWorker",
        "progress",
        "",
        "percent",
        "bytesRead",
        "totalBytes",
        "speedMbps",
        "finished",
        "success",
        "message",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progress'
        QtMocHelpers::SignalData<void(int, quint64, quint64, float)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::ULongLong, 4 }, { QMetaType::ULongLong, 5 }, { QMetaType::Float, 6 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(bool, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 }, { QMetaType::QString, 9 },
        }}),
        // Signal 'error'
        QtMocHelpers::SignalData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DumpWorker, qt_meta_tag_ZN10DumpWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject DumpWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QThread::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DumpWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DumpWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10DumpWorkerE_t>.metaTypes,
    nullptr
} };

void DumpWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DumpWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<quint64>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<float>>(_a[4]))); break;
        case 1: _t->finished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->error((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DumpWorker::*)(int , quint64 , quint64 , float )>(_a, &DumpWorker::progress, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DumpWorker::*)(bool , const QString & )>(_a, &DumpWorker::finished, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DumpWorker::*)(const QString & )>(_a, &DumpWorker::error, 2))
            return;
    }
}

const QMetaObject *DumpWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DumpWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10DumpWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QThread::qt_metacast(_clname);
}

int DumpWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void DumpWorker::progress(int _t1, quint64 _t2, quint64 _t3, float _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 1
void DumpWorker::finished(bool _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void DumpWorker::error(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
