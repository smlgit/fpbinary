#include "fpbinaryenums.h"
#include "structmember.h"

/*
 *
 * Overflow enumeration class.
 *
 */
static void
overflowenum_dealloc(OverflowEnumObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMemberDef overflowenum_members[] = {
    {"wrap", T_LONG, offsetof(OverflowEnumObject, wrap), 0, ""},
    {"sat", T_LONG, offsetof(OverflowEnumObject, sat), 0, ""},
    {"excep", T_LONG, offsetof(OverflowEnumObject, excep), 0, ""},
    {NULL} /* Sentinel */
};

PyObject *
overflowenum_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    OverflowEnumObject *self = (OverflowEnumObject *)type->tp_alloc(type, 0);

    if (self != NULL)
    {
        self->wrap = (long)OVERFLOW_WRAP;
        self->sat = (long)OVERFLOW_SAT;
        self->excep = (long)OVERFLOW_EXCEP;
    }

    return (PyObject *)self;
}

PyTypeObject OverflowEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.OverflowEnumType",
    .tp_doc = "Fixed point binary overflow type.",
    .tp_basicsize = sizeof(OverflowEnumObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = (newfunc)overflowenum_new,
    .tp_dealloc = (destructor)overflowenum_dealloc,
    .tp_members = overflowenum_members,
};

/*
 *
 * Rounding enumeration class.
 *
 */
static void
roundingenum_dealloc(RoundingEnumObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMemberDef roundingenum_members[] = {
    {"near_pos_inf", T_LONG, offsetof(RoundingEnumObject, near_pos_inf), 0, ""},
    {"direct_neg_inf", T_LONG, offsetof(RoundingEnumObject, direct_neg_inf), 0,
     ""},
    {"near_zero", T_LONG, offsetof(RoundingEnumObject, near_zero), 0, ""},
    {NULL} /* Sentinel */
};

PyObject *
roundingenum_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    RoundingEnumObject *self = (RoundingEnumObject *)type->tp_alloc(type, 0);

    if (self != NULL)
    {
        self->near_pos_inf = (long)ROUNDING_NEAR_POS_INF;
        self->direct_neg_inf = (long)ROUNDING_DIRECT_NEG_INF;
        self->near_zero = (long)ROUNDING_NEAR_ZERO;
    }

    return (PyObject *)self;
}

PyTypeObject RoundingEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.RoundingEnumType",
    .tp_doc = "Fixed point binary rounding type.",
    .tp_basicsize = sizeof(RoundingEnumObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = (newfunc)roundingenum_new,
    .tp_dealloc = (destructor)roundingenum_dealloc,
    .tp_members = roundingenum_members,
};
