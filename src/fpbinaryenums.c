/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Basic objects to make the use of enumerated values easier pre python3.
 * For each enum type, a PyTypeObject is defined which has an int field for
 * each possible value. The fields are exposed to python users as get
 * properties.
 *
 *****************************************************************************/

#include "fpbinaryenums.h"
#include "structmember.h"

/*
 *
 * Overflow enumeration class.
 *
 */

PyDoc_STRVAR(fpbinaryoverflow_enum_doc, "This type is not meant to be "
                                        "instantiated. Use the global instance "
                                        "OverflowEnum.\n");

PyDoc_STRVAR(fpbinaryoverflow_wrap_doc,
             "This is essentially the truncation of any int bits thatare being "
             "removed (usually \n"
             "via a resize() call). For signed types, this mayresult in a "
             "positive number becoming\n"
             "negative and vice versa.\n");

PyDoc_STRVAR(fpbinaryoverflow_sat_doc, "If an overflow occurs, the value is "
                                       "railed to the min or max value of the "
                                       "new bit format.\n");

PyDoc_STRVAR(
    fpbinaryoverflow_excep_doc,
    "If an overflow occurs, an FpBinaryOverflowException is raised.\n");

static void
overflowenum_dealloc(OverflowEnumObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMemberDef overflowenum_members[] = {
    {"wrap", T_LONG, offsetof(OverflowEnumObject, wrap), 0,
     fpbinaryoverflow_wrap_doc},
    {"sat", T_LONG, offsetof(OverflowEnumObject, sat), 0,
     fpbinaryoverflow_sat_doc},
    {"excep", T_LONG, offsetof(OverflowEnumObject, excep), 0,
     fpbinaryoverflow_excep_doc},
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
    .tp_doc = fpbinaryoverflow_enum_doc,
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

PyDoc_STRVAR(fpbinaryrounding_enum_doc, "This type is not meant to be "
                                        "instantiated. Use the global instance "
                                        "RoundingEnum.\n");

PyDoc_STRVAR(
    fpbinaryrounding_npi_doc,
    "The value is rounded towards the nearest value representable by "
    "the new format.\n"
    "Ties (i.e. X.5) are rounded towards positive infinity.\n"
    "The IEEE 754 standard does not have an equivalent, but this is "
    "the mode used by the VHDL\n"
    "fixed point library when using the 'round' mode.\n"
    "Examples.\n"
    "    5.5 and 5.6 both go to 6.0 (assuming resizing to zero "
    "fract_bits).\n"
    "    -5.25 goes to -5.0, -5.375 goes to -5.5 (assuming resizing to one "
    "fract_bit).\n");

PyDoc_STRVAR(
    fpbinaryrounding_nz_doc,
    "The value is rounded towards the "
    "nearest value representable by the new "
    "format. Ties\n"
    "(i.e. X.5) are rounded towards zero.\n"
    "The IEEE 754 standard does not have an equivalent, but python uses this\n"
    "mode when converting floats to ints.\n"
    "Examples.\n"
    "    5.5 goes to 5.0, 5.6 goes to 6.0 (assuming resizing to zero "
    "fract_bits).\n"
    "    -5.25 goes to -5.0, -5.375 goes to -5.5 (assuming resizing to one "
    "fract_bit).\n");

PyDoc_STRVAR(
    fpbinaryrounding_dni_doc,
    "The value is rounded in the negative direction to the nearest "
    "value representable by the\n"
    "new format. This is often called 'flooring' or, for hardware people, "
    "'truncation'.\n"
    "The IEEE 754 standard calls this 'Round toward -infinity'.\n"
    "Examples.\n"
    "    5.5 and 5.6 both go to 5.0 (assuming resizing to zero fract_bits).\n"
    "    -5.25 and -5.375 both go to -5.5 (assuming resizing to one "
    "fract_bit).\n");

PyDoc_STRVAR(
    fpbinaryrounding_dz_doc,
    "The value is rounded in the direction towards zero to the nearest "
    "value representable by the\n"
    "new format.\n"
    "The IEEE 754 standard calls this 'Round toward 0' or 'truncation'.\n"
    "Examples.\n"
    "    5.5 and 5.6 both go to 5.0 (assuming resizing to zero fract_bits).\n"
    "    -5.25 and -5.375 both go to -5.0 (assuming resizing to one "
    "fract_bit).\n");

static void
roundingenum_dealloc(RoundingEnumObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyMemberDef roundingenum_members[] = {
    {"near_pos_inf", T_LONG, offsetof(RoundingEnumObject, near_pos_inf), 0,
     fpbinaryrounding_npi_doc},
    {"direct_neg_inf", T_LONG, offsetof(RoundingEnumObject, direct_neg_inf), 0,
     fpbinaryrounding_dni_doc},
    {"near_zero", T_LONG, offsetof(RoundingEnumObject, near_zero), 0,
     fpbinaryrounding_nz_doc},
    {"direct_zero", T_LONG, offsetof(RoundingEnumObject, direct_zero), 0,
     fpbinaryrounding_dz_doc},
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
        self->direct_zero = (long)ROUNDING_DIRECT_ZERO;
    }

    return (PyObject *)self;
}

PyTypeObject RoundingEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.RoundingEnumType",
    .tp_doc = fpbinaryrounding_enum_doc,
    .tp_basicsize = sizeof(RoundingEnumObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = (newfunc)roundingenum_new,
    .tp_dealloc = (destructor)roundingenum_dealloc,
    .tp_members = roundingenum_members,
};
