/****************************************************************************
** Meta object code from reading C++ file 'protectiontab.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/protectiontab.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'protectiontab.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13ProtectionTabE_t {};
} // unnamed namespace

template <> constexpr inline auto ProtectionTab::qt_create_metaobjectdata<qt_meta_tag_ZN13ProtectionTabE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ProtectionTab",
        "configChanged",
        "",
        "profileChanged",
        "profileName",
        "platformChanged",
        "uft_platform_t",
        "platform",
        "loadSettings",
        "saveSettings",
        "resetDefaults",
        "applyProfile",
        "onProfileChanged",
        "index",
        "onSaveProfile",
        "onLoadProfile",
        "onDeleteProfile",
        "onAutoDetectToggled",
        "checked",
        "onPreserveToggled",
        "onC64EnableToggled",
        "onC64ExpertToggled",
        "onDDEnableToggled",
        "onDDExpertToggled",
        "onXCopyEnableToggled",
        "onAnyCheckboxChanged",
        "onSliderChanged",
        "value",
        "onSpinboxChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'configChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'profileChanged'
        QtMocHelpers::SignalData<void(const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Signal 'platformChanged'
        QtMocHelpers::SignalData<void(uft_platform_t)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Slot 'loadSettings'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'saveSettings'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'resetDefaults'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'applyProfile'
        QtMocHelpers::SlotData<void(const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Slot 'onProfileChanged'
        QtMocHelpers::SlotData<void(int)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 13 },
        }}),
        // Slot 'onSaveProfile'
        QtMocHelpers::SlotData<void()>(14, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLoadProfile'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDeleteProfile'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onAutoDetectToggled'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onPreserveToggled'
        QtMocHelpers::SlotData<void(bool)>(19, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onC64EnableToggled'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onC64ExpertToggled'
        QtMocHelpers::SlotData<void(bool)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onDDEnableToggled'
        QtMocHelpers::SlotData<void(bool)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onDDExpertToggled'
        QtMocHelpers::SlotData<void(bool)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onXCopyEnableToggled'
        QtMocHelpers::SlotData<void(bool)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 18 },
        }}),
        // Slot 'onAnyCheckboxChanged'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSliderChanged'
        QtMocHelpers::SlotData<void(int)>(26, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 27 },
        }}),
        // Slot 'onSpinboxChanged'
        QtMocHelpers::SlotData<void(int)>(28, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 27 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ProtectionTab, qt_meta_tag_ZN13ProtectionTabE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ProtectionTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ProtectionTabE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ProtectionTabE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13ProtectionTabE_t>.metaTypes,
    nullptr
} };

void ProtectionTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ProtectionTab *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->configChanged(); break;
        case 1: _t->profileChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->platformChanged((*reinterpret_cast<std::add_pointer_t<uft_platform_t>>(_a[1]))); break;
        case 3: _t->loadSettings(); break;
        case 4: _t->saveSettings(); break;
        case 5: _t->resetDefaults(); break;
        case 6: _t->applyProfile((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onProfileChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onSaveProfile(); break;
        case 9: _t->onLoadProfile(); break;
        case 10: _t->onDeleteProfile(); break;
        case 11: _t->onAutoDetectToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 12: _t->onPreserveToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->onC64EnableToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->onC64ExpertToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->onDDEnableToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 16: _t->onDDExpertToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 17: _t->onXCopyEnableToggled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 18: _t->onAnyCheckboxChanged(); break;
        case 19: _t->onSliderChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 20: _t->onSpinboxChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ProtectionTab::*)()>(_a, &ProtectionTab::configChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtectionTab::*)(const QString & )>(_a, &ProtectionTab::profileChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (ProtectionTab::*)(uft_platform_t )>(_a, &ProtectionTab::platformChanged, 2))
            return;
    }
}

const QMetaObject *ProtectionTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ProtectionTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ProtectionTabE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ProtectionTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 21)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 21;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 21)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 21;
    }
    return _id;
}

// SIGNAL 0
void ProtectionTab::configChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ProtectionTab::profileChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void ProtectionTab::platformChanged(uft_platform_t _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
