/****************************************************************************
** Meta object code from reading C++ file 'recoveryworkflowwidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/widgets/recoveryworkflowwidget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'recoveryworkflowwidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto RecoveryWorkflowWidget::qt_create_metaobjectdata<qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "RecoveryWorkflowWidget",
        "startRecoveryClicked",
        "",
        "cancelRecoveryClicked",
        "applyRecommendationsClicked",
        "skipRecoveryClicked",
        "customSettingsClicked",
        "trackSelected",
        "track",
        "head",
        "problemSelected",
        "index",
        "onProblemTableClicked",
        "row",
        "column",
        "onPassTableClicked",
        "onStartClicked",
        "onCancelClicked",
        "onApplyClicked",
        "onSkipClicked",
        "onCustomClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'startRecoveryClicked'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'cancelRecoveryClicked'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'applyRecommendationsClicked'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'skipRecoveryClicked'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'customSettingsClicked'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'trackSelected'
        QtMocHelpers::SignalData<void(int, int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { QMetaType::Int, 9 },
        }}),
        // Signal 'problemSelected'
        QtMocHelpers::SignalData<void(int)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 11 },
        }}),
        // Slot 'onProblemTableClicked'
        QtMocHelpers::SlotData<void(int, int)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'onPassTableClicked'
        QtMocHelpers::SlotData<void(int, int)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 13 }, { QMetaType::Int, 14 },
        }}),
        // Slot 'onStartClicked'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCancelClicked'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onApplyClicked'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSkipClicked'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onCustomClicked'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<RecoveryWorkflowWidget, qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject RecoveryWorkflowWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>.metaTypes,
    nullptr
} };

void RecoveryWorkflowWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<RecoveryWorkflowWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->startRecoveryClicked(); break;
        case 1: _t->cancelRecoveryClicked(); break;
        case 2: _t->applyRecommendationsClicked(); break;
        case 3: _t->skipRecoveryClicked(); break;
        case 4: _t->customSettingsClicked(); break;
        case 5: _t->trackSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->problemSelected((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->onProblemTableClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 8: _t->onPassTableClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->onStartClicked(); break;
        case 10: _t->onCancelClicked(); break;
        case 11: _t->onApplyClicked(); break;
        case 12: _t->onSkipClicked(); break;
        case 13: _t->onCustomClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)()>(_a, &RecoveryWorkflowWidget::startRecoveryClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)()>(_a, &RecoveryWorkflowWidget::cancelRecoveryClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)()>(_a, &RecoveryWorkflowWidget::applyRecommendationsClicked, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)()>(_a, &RecoveryWorkflowWidget::skipRecoveryClicked, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)()>(_a, &RecoveryWorkflowWidget::customSettingsClicked, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)(int , int )>(_a, &RecoveryWorkflowWidget::trackSelected, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (RecoveryWorkflowWidget::*)(int )>(_a, &RecoveryWorkflowWidget::problemSelected, 6))
            return;
    }
}

const QMetaObject *RecoveryWorkflowWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *RecoveryWorkflowWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22RecoveryWorkflowWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int RecoveryWorkflowWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void RecoveryWorkflowWidget::startRecoveryClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void RecoveryWorkflowWidget::cancelRecoveryClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void RecoveryWorkflowWidget::applyRecommendationsClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void RecoveryWorkflowWidget::skipRecoveryClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void RecoveryWorkflowWidget::customSettingsClicked()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void RecoveryWorkflowWidget::trackSelected(int _t1, int _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void RecoveryWorkflowWidget::problemSelected(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}
QT_WARNING_POP
