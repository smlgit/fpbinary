/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYLARGE_H
#define FPBINARYLARGE_H

#include "fpbinarycommon.h"

typedef struct
{
    fpbinary_base_t fpbinary_base;
    PyObject *int_bits;
    PyObject *frac_bits;
    PyObject *scaled_value;
    bool is_signed;
} FpBinaryLargeObject;

extern PyTypeObject FpBinary_LargeType;
extern fpbinary_private_iface_t FpBinary_LargePrvIface;

#define FpBinaryLarge_CheckExact(op) (Py_TYPE(op) == &FpBinary_LargeType)
/* TODO: */
#define FpBinaryLarge_Check(op) FpBinaryLarge_CheckExact(op)

void FpBinaryLarge_InitModule(void);

/* Helper functions for use of top client object. */

void FpBinaryLarge_FormatAsUints(PyObject *self, FP_UINT_TYPE *out_int_bits,
                                 FP_UINT_TYPE *out_frac_bits);
PyObject *FpBinaryLarge_BitsAsPylong(PyObject *obj);
bool FpBinaryLarge_IsSigned(PyObject *obj);

PyObject *FpBinaryLarge_FromDouble(double value, FP_INT_TYPE int_bits,
                                   FP_INT_TYPE frac_bits, bool is_signed,
                                   fp_overflow_mode_t overflow_mode,
                                   fp_round_mode_t round_mode);
PyObject *FpBinaryLarge_FromBitsPylong(PyObject *scaled_value,
                                       FP_INT_TYPE int_bits,
                                       FP_INT_TYPE frac_bits, bool is_signed);

PyObject *FpBinaryLarge_FromPickleDict(PyObject *dict);
bool FpBinaryLarge_UpdatePickleDict(PyObject *self, PyObject *dict);

#endif
