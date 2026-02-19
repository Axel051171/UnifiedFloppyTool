/****************************************************************************
** Meta object code from reading C++ file 'fluxvisualizerwidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/widgets/fluxvisualizerwidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'fluxvisualizerwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN20FluxVisualizerWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto FluxVisualizerWidget::qt_create_metaobjectdata<qt_meta_tag_ZN20FluxVisualizerWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FluxVisualizerWidget",
        "sampleClicked",
        "",
        "size_t",
        "index",
        "uint32_t",
        "time_ns",
        "regionSelected",
        "startIndex",
        "endIndex",
        "zoomChanged",
        "zoom",
        "viewModeChanged",
        "FluxViewMode",
        "mode"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'sampleClicked'
        QtMocHelpers::SignalData<void(size_t, uint32_t)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'regionSelected'
        QtMocHelpers::SignalData<void(size_t, size_t)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 8 }, { 0x80000000 | 3, 9 },
        }}),
        // Signal 'zoomChanged'
        QtMocHelpers::SignalData<void(double)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Double, 11 },
        }}),
        // Signal 'viewModeChanged'
        QtMocHelpers::SignalData<void(FluxViewMode)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FluxVisualizerWidget, qt_meta_tag_ZN20FluxVisualizerWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FluxVisualizerWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20FluxVisualizerWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20FluxVisualizerWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20FluxVisualizerWidgetE_t>.metaTypes,
    nullptr
} };

void FluxVisualizerWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FluxVisualizerWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->sampleClicked((*reinterpret_cast<std::add_pointer_t<size_t>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[2]))); break;
        case 1: _t->regionSelected((*reinterpret_cast<std::add_pointer_t<size_t>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<size_t>>(_a[2]))); break;
        case 2: _t->zoomChanged((*reinterpret_cast<std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->viewModeChanged((*reinterpret_cast<std::add_pointer_t<FluxViewMode>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FluxVisualizerWidget::*)(size_t , uint32_t )>(_a, &FluxVisualizerWidget::sampleClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FluxVisualizerWidget::*)(size_t , size_t )>(_a, &FluxVisualizerWidget::regionSelected, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FluxVisualizerWidget::*)(double )>(_a, &FluxVisualizerWidget::zoomChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FluxVisualizerWidget::*)(FluxViewMode )>(_a, &FluxVisualizerWidget::viewModeChanged, 3))
            return;
    }
}

const QMetaObject *FluxVisualizerWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FluxVisualizerWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20FluxVisualizerWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int FluxVisualizerWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void FluxVisualizerWidget::sampleClicked(size_t _t1, uint32_t _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void FluxVisualizerWidget::regionSelected(size_t _t1, size_t _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void FluxVisualizerWidget::zoomChanged(double _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void FluxVisualizerWidget::viewModeChanged(FluxViewMode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
