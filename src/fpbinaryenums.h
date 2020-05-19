/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYENUMS_H_
#define FPBINARYENUMS_H_

#include "fpbinarycommon.h"

typedef struct
{
    PyObject_HEAD long wrap;
    long sat;
    long excep;
} OverflowEnumObject;

extern PyTypeObject OverflowEnumType;

PyObject *overflowenum_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

typedef struct
{
    PyObject_HEAD long near_pos_inf;
    long direct_neg_inf;
    long near_zero;
    long direct_zero;
    long near_even;
} RoundingEnumObject;

extern PyTypeObject RoundingEnumType;

PyObject *roundingenum_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

#endif /* FPBINARYENUMS_H_ */
