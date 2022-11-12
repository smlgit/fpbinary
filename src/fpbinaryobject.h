/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYOBJECT_H
#define FPBINARYOBJECT_H

#include "fpbinarycommon.h"

typedef struct
{
    PyObject_HEAD fpbinary_base_t *base_obj;
} FpBinaryObject;

#define PYOBJ_TO_BASE_FP(ob) (((FpBinaryObject *)ob)->base_obj)
#define PYOBJ_TO_BASE_FP_PYOBJ(ob) ((PyObject *)PYOBJ_TO_BASE_FP(ob))

extern PyTypeObject FpBinary_Type;

#define FpBinary_CheckExact(op) (Py_TYPE(op) == &FpBinary_Type)
#define FpBinary_Check(op) PyObject_TypeCheck(op, &FpBinary_Type)

FpBinaryObject *FpBinary_FromParams(long int_bits, long frac_bits,
                                    bool is_signed, double value,
                                    PyObject *bit_field,
                                    PyObject *format_instance);
FpBinaryObject *FpBinary_FromValue(PyObject *value);
void FpBinary_SetTwoInstToSameFormat(PyObject **op1, PyObject **op2);

/*
 * Functions for client objects to easily call FpBinary user-specified methods.
 * These tend to use the Python-like call interfaces rather than using an insider's
 * knowledge of the underlying structures of FpBinary, so should be safe to use
 * on objects that quack like an FpBinary...
 */
PyObject *FpBinary_ResizeWithCInts(PyObject *value, long int_bits, long frac_bits,
        long round_mode, long overflow_mode);
PyObject *FpBinary_ResizeWithFormatInstance(PyObject *value, PyObject *format_instance,
        long round_mode, long overflow_mode);

#endif
