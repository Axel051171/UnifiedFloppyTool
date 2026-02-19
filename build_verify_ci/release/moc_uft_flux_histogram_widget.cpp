/****************************************************************************
** Meta object code from reading C++ file 'uft_flux_histogram_widget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/uft_flux_histogram_widget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'uft_flux_histogram_widget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN22UftFluxHistogramWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto UftFluxHistogramWidget::qt_create_metaobjectdata<qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UftFluxHistogramWidget",
        "encodingDetected",
        "",
        "EncodingType",
        "encoding",
        "cellTime",
        "histogramUpdated",
        "binClicked",
        "binIndex",
        "nsValue",
        "uint32_t",
        "count",
        "clear",
        "setDisplayMode",
        "DisplayMode",
        "mode",
        "setEncodingHint",
        "enc",
        "setBinWidth",
        "nsPerBin",
        "setVisibleRange",
        "minNs",
        "maxNs",
        "autoFitRange",
        "setShowPeaks",
        "show",
        "setShowGrid",
        "exportImage",
        "filename",
        "exportCSV"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'encodingDetected'
        QtMocHelpers::SignalData<void(enum EncodingType, double)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Double, 5 },
        }}),
        // Signal 'histogramUpdated'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'binClicked'
        QtMocHelpers::SignalData<void(int, int, uint32_t)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::Int, 9 }, { 0x80000000 | 10, 11 },
        }}),
        // Slot 'clear'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setDisplayMode'
        QtMocHelpers::SlotData<void(enum DisplayMode)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 14, 15 },
        }}),
        // Slot 'setEncodingHint'
        QtMocHelpers::SlotData<void(enum EncodingType)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 17 },
        }}),
        // Slot 'setBinWidth'
        QtMocHelpers::SlotData<void(int)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 19 },
        }}),
        // Slot 'setVisibleRange'
        QtMocHelpers::SlotData<void(int, int)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 21 }, { QMetaType::Int, 22 },
        }}),
        // Slot 'autoFitRange'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setShowPeaks'
        QtMocHelpers::SlotData<void(bool)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 25 },
        }}),
        // Slot 'setShowGrid'
        QtMocHelpers::SlotData<void(bool)>(26, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 25 },
        }}),
        // Slot 'exportImage'
        QtMocHelpers::SlotData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Slot 'exportCSV'
        QtMocHelpers::SlotData<void(const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UftFluxHistogramWidget, qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UftFluxHistogramWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>.metaTypes,
    nullptr
} };

void UftFluxHistogramWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UftFluxHistogramWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->encodingDetected((*reinterpret_cast<std::add_pointer_t<enum EncodingType>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2]))); break;
        case 1: _t->histogramUpdated(); break;
        case 2: _t->binClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<uint32_t>>(_a[3]))); break;
        case 3: _t->clear(); break;
        case 4: _t->setDisplayMode((*reinterpret_cast<std::add_pointer_t<enum DisplayMode>>(_a[1]))); break;
        case 5: _t->setEncodingHint((*reinterpret_cast<std::add_pointer_t<enum EncodingType>>(_a[1]))); break;
        case 6: _t->setBinWidth((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->setVisibleRange((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->autoFitRange(); break;
        case 9: _t->setShowPeaks((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->setShowGrid((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->exportImage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->exportCSV((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (UftFluxHistogramWidget::*)(EncodingType , double )>(_a, &UftFluxHistogramWidget::encodingDetected, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftFluxHistogramWidget::*)()>(_a, &UftFluxHistogramWidget::histogramUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (UftFluxHistogramWidget::*)(int , int , uint32_t )>(_a, &UftFluxHistogramWidget::binClicked, 2))
            return;
    }
}

const QMetaObject *UftFluxHistogramWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UftFluxHistogramWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22UftFluxHistogramWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UftFluxHistogramWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void UftFluxHistogramWidget::encodingDetected(EncodingType _t1, double _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void UftFluxHistogramWidget::histogramUpdated()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void UftFluxHistogramWidget::binClicked(int _t1, int _t2, uint32_t _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}
namespace {
struct qt_meta_tag_ZN21UftFluxHistogramPanelE_t {};
} // unnamed namespace

template <> constexpr inline auto UftFluxHistogramPanel::qt_create_metaobjectdata<qt_meta_tag_ZN21UftFluxHistogramPanelE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "UftFluxHistogramPanel",
        "onTrackChanged",
        "",
        "track",
        "head",
        "onEncodingChanged",
        "index",
        "onModeChanged",
        "onBinWidthChanged",
        "value",
        "onExportImage",
        "onExportCSV"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onTrackChanged'
        QtMocHelpers::SlotData<void(int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Int, 4 },
        }}),
        // Slot 'onEncodingChanged'
        QtMocHelpers::SlotData<void(int)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onModeChanged'
        QtMocHelpers::SlotData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 6 },
        }}),
        // Slot 'onBinWidthChanged'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onExportImage'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onExportCSV'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<UftFluxHistogramPanel, qt_meta_tag_ZN21UftFluxHistogramPanelE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject UftFluxHistogramPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UftFluxHistogramPanelE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UftFluxHistogramPanelE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN21UftFluxHistogramPanelE_t>.metaTypes,
    nullptr
} };

void UftFluxHistogramPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<UftFluxHistogramPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onTrackChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->onEncodingChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->onModeChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->onBinWidthChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->onExportImage(); break;
        case 5: _t->onExportCSV(); break;
        default: ;
        }
    }
}

const QMetaObject *UftFluxHistogramPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *UftFluxHistogramPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN21UftFluxHistogramPanelE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int UftFluxHistogramPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
QT_WARNING_POP
