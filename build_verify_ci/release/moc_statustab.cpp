/****************************************************************************
** Meta object code from reading C++ file 'statustab.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../src/statustab.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'statustab.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9StatusTabE_t {};
} // unnamed namespace

template <> constexpr inline auto StatusTab::qt_create_metaobjectdata<qt_meta_tag_ZN9StatusTabE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "StatusTab",
        "decodeCompleted",
        "",
        "success",
        "requestLabelEditor",
        "requestBAMViewer",
        "requestBootblock",
        "requestProtectionAnalysis",
        "onProgress",
        "percentage",
        "onStageChanged",
        "stage",
        "onSectorUpdate",
        "track",
        "sector",
        "status",
        "onImageInfo",
        "DecodeResult",
        "info",
        "onDecodeFinished",
        "message",
        "onDecodeError",
        "error",
        "onLabelEditorClicked",
        "onBAMViewerClicked",
        "onBootblockClicked",
        "onProtectionClicked",
        "onDmkAnalysisClicked"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'decodeCompleted'
        QtMocHelpers::SignalData<void(bool)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 3 },
        }}),
        // Signal 'requestLabelEditor'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestBAMViewer'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestBootblock'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'requestProtectionAnalysis'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onProgress'
        QtMocHelpers::SlotData<void(int)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 9 },
        }}),
        // Slot 'onStageChanged'
        QtMocHelpers::SlotData<void(const QString &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 },
        }}),
        // Slot 'onSectorUpdate'
        QtMocHelpers::SlotData<void(int, int, const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 13 }, { QMetaType::Int, 14 }, { QMetaType::QString, 15 },
        }}),
        // Slot 'onImageInfo'
        QtMocHelpers::SlotData<void(const DecodeResult &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 17, 18 },
        }}),
        // Slot 'onDecodeFinished'
        QtMocHelpers::SlotData<void(const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 20 },
        }}),
        // Slot 'onDecodeError'
        QtMocHelpers::SlotData<void(const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Slot 'onLabelEditorClicked'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onBAMViewerClicked'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onBootblockClicked'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onProtectionClicked'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onDmkAnalysisClicked'
        QtMocHelpers::SlotData<void()>(27, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<StatusTab, qt_meta_tag_ZN9StatusTabE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject StatusTab::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9StatusTabE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9StatusTabE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9StatusTabE_t>.metaTypes,
    nullptr
} };

void StatusTab::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<StatusTab *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->decodeCompleted((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->requestLabelEditor(); break;
        case 2: _t->requestBAMViewer(); break;
        case 3: _t->requestBootblock(); break;
        case 4: _t->requestProtectionAnalysis(); break;
        case 5: _t->onProgress((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onStageChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->onSectorUpdate((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 8: _t->onImageInfo((*reinterpret_cast<std::add_pointer_t<DecodeResult>>(_a[1]))); break;
        case 9: _t->onDecodeFinished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 10: _t->onDecodeError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->onLabelEditorClicked(); break;
        case 12: _t->onBAMViewerClicked(); break;
        case 13: _t->onBootblockClicked(); break;
        case 14: _t->onProtectionClicked(); break;
        case 15: _t->onDmkAnalysisClicked(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (StatusTab::*)(bool )>(_a, &StatusTab::decodeCompleted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (StatusTab::*)()>(_a, &StatusTab::requestLabelEditor, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (StatusTab::*)()>(_a, &StatusTab::requestBAMViewer, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (StatusTab::*)()>(_a, &StatusTab::requestBootblock, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (StatusTab::*)()>(_a, &StatusTab::requestProtectionAnalysis, 4))
            return;
    }
}

const QMetaObject *StatusTab::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StatusTab::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9StatusTabE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int StatusTab::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void StatusTab::decodeCompleted(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void StatusTab::requestLabelEditor()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void StatusTab::requestBAMViewer()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void StatusTab::requestBootblock()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void StatusTab::requestProtectionAnalysis()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
