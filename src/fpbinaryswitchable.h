/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYSWITCHABLE_H
#define FPBINARYSWITCHABLE_H

#include "fpbinaryobject.h"

typedef struct
{
    PyObject_HEAD bool fp_mode;
    PyObject *fp_mode_value;
    double dbl_mode_value;
    double dbl_mode_min_value;
    double dbl_mode_max_value;
} FpBinarySwitchableObject;

extern PyTypeObject FpBinarySwitchable_Type;

#define FpBinarySwitchable_CheckExact(op)                                      \
    (Py_TYPE(op) == &FpBinarySwitchable_Type)
#define FpBinarySwitchable_Check(op)                                           \
    PyObject_TypeCheck(op, &FpBinarySwitchable_Type)

void FpBinarySwitchable_InitModule(void);

#endif
