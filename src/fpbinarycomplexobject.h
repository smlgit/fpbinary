/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYCOMPLEXOBJECT_H
#define FPBINARYCOMPLEXOBJECT_H

#include "fpbinarycommon.h"

typedef struct
{
    PyObject_HEAD
    PyObject *real;
    PyObject *imag;
} FpBinaryComplexObject;

extern PyTypeObject FpBinaryComplex_Type;

#define PYOBJ_TO_REAL_FP(ob) (((FpBinaryComplexObject *)ob)->real)
#define PYOBJ_TO_IMAG_FP(ob) (((FpBinaryComplexObject *)ob)->imag)
#define PYOBJ_TO_REAL_FP_PYOBJ(ob) ((PyObject *) PYOBJ_TO_REAL_FP(ob))
#define PYOBJ_TO_IMAG_FP_PYOBJ(ob) ((PyObject *) PYOBJ_TO_IMAG_FP(ob))

#define FpBinaryComplex_CheckExact(op) (Py_TYPE(op) == &FpBinaryComplex_Type)
#define FpBinaryComplex_Check(op) PyObject_TypeCheck(op, &FpBinaryComplex_Type)


#endif
