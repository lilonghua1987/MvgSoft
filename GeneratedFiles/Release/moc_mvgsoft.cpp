/****************************************************************************
** Meta object code from reading C++ file 'mvgsoft.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.3.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../mvgsoft.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mvgsoft.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.3.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_MvgSoft_t {
    QByteArrayData data[20];
    char stringdata[219];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MvgSoft_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MvgSoft_t qt_meta_stringdata_MvgSoft = {
    {
QT_MOC_LITERAL(0, 0, 7),
QT_MOC_LITERAL(1, 8, 9),
QT_MOC_LITERAL(2, 18, 0),
QT_MOC_LITERAL(3, 19, 14),
QT_MOC_LITERAL(4, 34, 12),
QT_MOC_LITERAL(5, 47, 6),
QT_MOC_LITERAL(6, 54, 6),
QT_MOC_LITERAL(7, 61, 6),
QT_MOC_LITERAL(8, 68, 25),
QT_MOC_LITERAL(9, 94, 20),
QT_MOC_LITERAL(10, 115, 12),
QT_MOC_LITERAL(11, 128, 14),
QT_MOC_LITERAL(12, 143, 15),
QT_MOC_LITERAL(13, 159, 4),
QT_MOC_LITERAL(14, 164, 4),
QT_MOC_LITERAL(15, 169, 4),
QT_MOC_LITERAL(16, 174, 7),
QT_MOC_LITERAL(17, 182, 13),
QT_MOC_LITERAL(18, 196, 7),
QT_MOC_LITERAL(19, 204, 14)
    },
    "MvgSoft\0openFiles\0\0selectInputDir\0"
    "computeFocal\0matchE\0matchF\0matchH\0"
    "incrementalReconstruction\0"
    "globalReconstruction\0exportToPMVS\0"
    "exportToCMPMVS\0exportToMESHLAB\0cmvs\0"
    "pmvs\0mesh\0libViso\0updateStatues\0message\0"
    "updateFinished"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MvgSoft[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   99,    2, 0x08 /* Private */,
       3,    0,  100,    2, 0x08 /* Private */,
       4,    0,  101,    2, 0x08 /* Private */,
       5,    0,  102,    2, 0x08 /* Private */,
       6,    0,  103,    2, 0x08 /* Private */,
       7,    0,  104,    2, 0x08 /* Private */,
       8,    0,  105,    2, 0x08 /* Private */,
       9,    0,  106,    2, 0x08 /* Private */,
      10,    0,  107,    2, 0x08 /* Private */,
      11,    0,  108,    2, 0x08 /* Private */,
      12,    0,  109,    2, 0x08 /* Private */,
      13,    0,  110,    2, 0x08 /* Private */,
      14,    0,  111,    2, 0x08 /* Private */,
      15,    0,  112,    2, 0x08 /* Private */,
      16,    0,  113,    2, 0x08 /* Private */,
      17,    1,  114,    2, 0x0a /* Public */,
      19,    0,  117,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Void, QMetaType::QString,   18,
    QMetaType::Void,

       0        // eod
};

void MvgSoft::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        MvgSoft *_t = static_cast<MvgSoft *>(_o);
        switch (_id) {
        case 0: { bool _r = _t->openFiles();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 1: { bool _r = _t->selectInputDir();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 2: { bool _r = _t->computeFocal();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 3: { bool _r = _t->matchE();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 4: { bool _r = _t->matchF();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 5: { bool _r = _t->matchH();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 6: { bool _r = _t->incrementalReconstruction();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 7: { bool _r = _t->globalReconstruction();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 8: { bool _r = _t->exportToPMVS();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 9: { bool _r = _t->exportToCMPMVS();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 10: { bool _r = _t->exportToMESHLAB();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 11: { bool _r = _t->cmvs();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 12: { bool _r = _t->pmvs();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 13: { bool _r = _t->mesh();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 14: { bool _r = _t->libViso();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = _r; }  break;
        case 15: _t->updateStatues((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 16: _t->updateFinished(); break;
        default: ;
        }
    }
}

const QMetaObject MvgSoft::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_MvgSoft.data,
      qt_meta_data_MvgSoft,  qt_static_metacall, 0, 0}
};


const QMetaObject *MvgSoft::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MvgSoft::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_MvgSoft.stringdata))
        return static_cast<void*>(const_cast< MvgSoft*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int MvgSoft::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 17;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
