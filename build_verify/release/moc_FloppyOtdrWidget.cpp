/****************************************************************************
** Meta object code from reading C++ file 'FloppyOtdrWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/analysis/otdr/FloppyOtdrWidget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FloppyOtdrWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13OtdrTraceViewE_t {};
} // unnamed namespace

template <> constexpr inline auto OtdrTraceView::qt_create_metaobjectdata<qt_meta_tag_ZN13OtdrTraceViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OtdrTraceView",
        "cursorPositionChanged",
        "",
        "uint32_t",
        "bitcell",
        "quality_db",
        "eventClicked",
        "const otdr_event_t*",
        "event",
        "zoomChanged",
        "start",
        "end"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'cursorPositionChanged'
        QtMocHelpers::SignalData<void(uint32_t, float)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Float, 5 },
        }}),
        // Signal 'eventClicked'
        QtMocHelpers::SignalData<void(const otdr_event_t *)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 8 },
        }}),
        // Signal 'zoomChanged'
        QtMocHelpers::SignalData<void(uint32_t, uint32_t)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 10 }, { 0x80000000 | 3, 11 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OtdrTraceView, qt_meta_tag_ZN13OtdrTraceViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OtdrTraceView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OtdrTraceViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OtdrTraceViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13OtdrTraceViewE_t>.metaTypes,
    nullptr
} };

void OtdrTraceView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OtdrTraceView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->cursorPositionChanged((*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<float>>(_a[2]))); break;
        case 1: _t->eventClicked((*reinterpret_cast<std::add_pointer_t<const otdr_event_t*>>(_a[1]))); break;
        case 2: _t->zoomChanged((*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OtdrTraceView::*)(uint32_t , float )>(_a, &OtdrTraceView::cursorPositionChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (OtdrTraceView::*)(const otdr_event_t * )>(_a, &OtdrTraceView::eventClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (OtdrTraceView::*)(uint32_t , uint32_t )>(_a, &OtdrTraceView::zoomChanged, 2))
            return;
    }
}

const QMetaObject *OtdrTraceView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OtdrTraceView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13OtdrTraceViewE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int OtdrTraceView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void OtdrTraceView::cursorPositionChanged(uint32_t _t1, float _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void OtdrTraceView::eventClicked(const otdr_event_t * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void OtdrTraceView::zoomChanged(uint32_t _t1, uint32_t _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}
namespace {
struct qt_meta_tag_ZN15OtdrHeatmapViewE_t {};
} // unnamed namespace

template <> constexpr inline auto OtdrHeatmapView::qt_create_metaobjectdata<qt_meta_tag_ZN15OtdrHeatmapViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OtdrHeatmapView",
        "trackClicked",
        "",
        "uint16_t",
        "trackNum"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'trackClicked'
        QtMocHelpers::SignalData<void(uint16_t)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OtdrHeatmapView, qt_meta_tag_ZN15OtdrHeatmapViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OtdrHeatmapView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15OtdrHeatmapViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15OtdrHeatmapViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN15OtdrHeatmapViewE_t>.metaTypes,
    nullptr
} };

void OtdrHeatmapView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OtdrHeatmapView *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->trackClicked((*reinterpret_cast<std::add_pointer_t<uint16_t>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (OtdrHeatmapView::*)(uint16_t )>(_a, &OtdrHeatmapView::trackClicked, 0))
            return;
    }
}

const QMetaObject *OtdrHeatmapView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OtdrHeatmapView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN15OtdrHeatmapViewE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int OtdrHeatmapView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void OtdrHeatmapView::trackClicked(uint16_t _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
namespace {
struct qt_meta_tag_ZN17OtdrHistogramViewE_t {};
} // unnamed namespace

template <> constexpr inline auto OtdrHistogramView::qt_create_metaobjectdata<qt_meta_tag_ZN17OtdrHistogramViewE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "OtdrHistogramView"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<OtdrHistogramView, qt_meta_tag_ZN17OtdrHistogramViewE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject OtdrHistogramView::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17OtdrHistogramViewE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17OtdrHistogramViewE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17OtdrHistogramViewE_t>.metaTypes,
    nullptr
} };

void OtdrHistogramView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OtdrHistogramView *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *OtdrHistogramView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OtdrHistogramView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17OtdrHistogramViewE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int OtdrHistogramView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN16FloppyOtdrWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto FloppyOtdrWidget::qt_create_metaobjectdata<qt_meta_tag_ZN16FloppyOtdrWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FloppyOtdrWidget",
        "onTrackSelected",
        "",
        "index",
        "onHeatmapTrackClicked",
        "uint16_t",
        "trackNum",
        "onExportReport",
        "onExportHeatmap",
        "onCursorPosition",
        "uint32_t",
        "bitcell",
        "db"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onTrackSelected'
        QtMocHelpers::SlotData<void(int)>(1, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'onHeatmapTrackClicked'
        QtMocHelpers::SlotData<void(uint16_t)>(4, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'onExportReport'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onExportHeatmap'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCursorPosition'
        QtMocHelpers::SlotData<void(uint32_t, float)>(9, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 10, 11 }, { QMetaType::Float, 12 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FloppyOtdrWidget, qt_meta_tag_ZN16FloppyOtdrWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FloppyOtdrWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16FloppyOtdrWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16FloppyOtdrWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16FloppyOtdrWidgetE_t>.metaTypes,
    nullptr
} };

void FloppyOtdrWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FloppyOtdrWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onTrackSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->onHeatmapTrackClicked((*reinterpret_cast<std::add_pointer_t<uint16_t>>(_a[1]))); break;
        case 2: _t->onExportReport(); break;
        case 3: _t->onExportHeatmap(); break;
        case 4: _t->onCursorPosition((*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<float>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject *FloppyOtdrWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FloppyOtdrWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16FloppyOtdrWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FloppyOtdrWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
