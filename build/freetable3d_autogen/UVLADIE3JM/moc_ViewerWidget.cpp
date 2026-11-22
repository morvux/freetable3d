/****************************************************************************
** Meta object code from reading C++ file 'ViewerWidget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ViewerWidget.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ViewerWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
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
struct qt_meta_tag_ZN12ViewerWidgetE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN12ViewerWidgetE = QtMocHelpers::stringData(
    "ViewerWidget",
    "selectionChangee",
    "",
    "deplacementTermine",
    "QList<int>",
    "ids",
    "dx",
    "dy",
    "rotationTerminee",
    "rx",
    "ry",
    "rz",
    "etirementTermine",
    "idPiece",
    "TopoDS_Shape",
    "face",
    "distance",
    "messageApercu",
    "message",
    "apercuGeste",
    "texte"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN12ViewerWidgetE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   50,    2, 0x06,    1 /* Public */,
       3,    3,   51,    2, 0x06,    2 /* Public */,
       8,    4,   58,    2, 0x06,    6 /* Public */,
      12,    3,   67,    2, 0x06,   11 /* Public */,
      17,    1,   74,    2, 0x06,   15 /* Public */,
      19,    1,   77,    2, 0x06,   17 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4, QMetaType::Double, QMetaType::Double,    5,    6,    7,
    QMetaType::Void, 0x80000000 | 4, QMetaType::Double, QMetaType::Double, QMetaType::Double,    5,    9,   10,   11,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 14, QMetaType::Double,   13,   15,   16,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void, QMetaType::QString,   20,

       0        // eod
};

Q_CONSTINIT const QMetaObject ViewerWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN12ViewerWidgetE.offsetsAndSizes,
    qt_meta_data_ZN12ViewerWidgetE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN12ViewerWidgetE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<ViewerWidget, std::true_type>,
        // method 'selectionChangee'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deplacementTermine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVector<int> &, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'rotationTerminee'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QVector<int> &, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'etirementTermine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const TopoDS_Shape &, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'messageApercu'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'apercuGeste'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void ViewerWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ViewerWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->selectionChangee(); break;
        case 1: _t->deplacementTermine((*reinterpret_cast< std::add_pointer_t<QList<int>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3]))); break;
        case 2: _t->rotationTerminee((*reinterpret_cast< std::add_pointer_t<QList<int>>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[4]))); break;
        case 3: _t->etirementTermine((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<TopoDS_Shape>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<double>>(_a[3]))); break;
        case 4: _t->messageApercu((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->apercuGeste((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<int> >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<int> >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (ViewerWidget::*)();
            if (_q_method_type _q_method = &ViewerWidget::selectionChangee; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (ViewerWidget::*)(const QVector<int> & , double , double );
            if (_q_method_type _q_method = &ViewerWidget::deplacementTermine; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (ViewerWidget::*)(const QVector<int> & , double , double , double );
            if (_q_method_type _q_method = &ViewerWidget::rotationTerminee; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (ViewerWidget::*)(int , const TopoDS_Shape & , double );
            if (_q_method_type _q_method = &ViewerWidget::etirementTermine; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (ViewerWidget::*)(const QString & );
            if (_q_method_type _q_method = &ViewerWidget::messageApercu; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (ViewerWidget::*)(const QString & );
            if (_q_method_type _q_method = &ViewerWidget::apercuGeste; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }
}

const QMetaObject *ViewerWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ViewerWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN12ViewerWidgetE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ViewerWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void ViewerWidget::selectionChangee()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ViewerWidget::deplacementTermine(const QVector<int> & _t1, double _t2, double _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ViewerWidget::rotationTerminee(const QVector<int> & _t1, double _t2, double _t3, double _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void ViewerWidget::etirementTermine(int _t1, const TopoDS_Shape & _t2, double _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void ViewerWidget::messageApercu(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void ViewerWidget::apercuGeste(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
