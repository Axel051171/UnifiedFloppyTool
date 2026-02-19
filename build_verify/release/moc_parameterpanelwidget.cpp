/****************************************************************************
** Meta object code from reading C++ file 'parameterpanelwidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/widgets/parameterpanelwidget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'parameterpanelwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN20ParameterPanelWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto ParameterPanelWidget::qt_create_metaobjectdata<qt_meta_tag_ZN20ParameterPanelWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ParameterPanelWidget",
        "profileChanged",
        "",
        "ProfileType",
        "profile",
        "parameterChanged",
        "key",
        "QVariant",
        "value",
        "advancedModeChanged",
        "enabled",
        "expertModeChanged",
        "validationFailed",
        "errors",
        "exportRequested",
        "importRequested",
        "onProfileSelected",
        "index",
        "onAdvancedToggled",
        "checked",
        "onExpertToggled",
        "onParameterChanged",
        "onExportClicked",
        "onImportClicked",
        "onResetClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'profileChanged'
        QtMocHelpers::SignalData<void(ProfileType)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'parameterChanged'
        QtMocHelpers::SignalData<void(const QString &, const QVariant &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 6 }, { 0x80000000 | 7, 8 },
        }}),
        // Signal 'advancedModeChanged'
        QtMocHelpers::SignalData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 },
        }}),
        // Signal 'expertModeChanged'
        QtMocHelpers::SignalData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 },
        }}),
        // Signal 'validationFailed'
        QtMocHelpers::SignalData<void(const QStringList &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 13 },
        }}),
        // Signal 'exportRequested'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'importRequested'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onProfileSelected'
        QtMocHelpers::SlotData<void(int)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 17 },
        }}),
        // Slot 'onAdvancedToggled'
        QtMocHelpers::SlotData<void(bool)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Slot 'onExpertToggled'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Slot 'onParameterChanged'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onExportClicked'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onImportClicked'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onResetClicked'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ParameterPanelWidget, qt_meta_tag_ZN20ParameterPanelWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ParameterPanelWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20ParameterPanelWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20ParameterPanelWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20ParameterPanelWidgetE_t>.metaTypes,
    nullptr
} };

void ParameterPanelWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ParameterPanelWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->profileChanged((*reinterpret_cast<std::add_pointer_t<ProfileType>>(_a[1]))); break;
        case 1: _t->parameterChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QVariant>>(_a[2]))); break;
        case 2: _t->advancedModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->expertModeChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->validationFailed((*reinterpret_cast<std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 5: _t->exportRequested(); break;
        case 6: _t->importRequested(); break;
        case 7: _t->onProfileSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onAdvancedToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 9: _t->onExpertToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->onParameterChanged(); break;
        case 11: _t->onExportClicked(); break;
        case 12: _t->onImportClicked(); break;
        case 13: _t->onResetClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)(ProfileType )>(_a, &ParameterPanelWidget::profileChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)(const QString & , const QVariant & )>(_a, &ParameterPanelWidget::parameterChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)(bool )>(_a, &ParameterPanelWidget::advancedModeChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)(bool )>(_a, &ParameterPanelWidget::expertModeChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)(const QStringList & )>(_a, &ParameterPanelWidget::validationFailed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)()>(_a, &ParameterPanelWidget::exportRequested, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (ParameterPanelWidget::*)()>(_a, &ParameterPanelWidget::importRequested, 6))
            return;
    }
}

const QMetaObject *ParameterPanelWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ParameterPanelWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20ParameterPanelWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ParameterPanelWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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

// SIGNAL 0
void ParameterPanelWidget::profileChanged(ProfileType _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void ParameterPanelWidget::parameterChanged(const QString & _t1, const QVariant & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void ParameterPanelWidget::advancedModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void ParameterPanelWidget::expertModeChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void ParameterPanelWidget::validationFailed(const QStringList & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void ParameterPanelWidget::exportRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void ParameterPanelWidget::importRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}
QT_WARNING_POP
