/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYSMALL_H
#define FPBINARYSMALL_H

#include "fpbinarycommon.h"

typedef struct
{
    fpbinary_base_t fpbinary_base;
    FP_INT_TYPE int_bits;
    FP_INT_TYPE frac_bits;
    FP_UINT_TYPE scaled_value;
    bool is_signed;
} FpBinarySmallObject;

extern PyTypeObject FpBinary_SmallType;
extern fpbinary_private_iface_t FpBinary_SmallPrvIface;

#define FpBinarySmall_CheckExact(op) (Py_TYPE(op) == &FpBinary_SmallType)
/* TODO: */
#define FpBinarySmall_Check(op) FpBinarySmall_CheckExact(op)

#define FP_SMALL_MAX_BITS FP_UINT_NUM_BITS

/* Helper functions for use of top client object. */

void FpBinarySmall_FormatAsUints(PyObject *self, FP_UINT_TYPE *out_int_bits,
                                 FP_UINT_TYPE *out_frac_bits);
PyObject *FpBinarySmall_BitsAsPylong(PyObject *obj);
PyObject *FpBinarySmall_FromDouble(double value, FP_INT_TYPE int_bits,
                                   FP_INT_TYPE frac_bits, bool is_signed,
                                   fp_overflow_mode_t overflow_mode,
                                   fp_round_mode_t round_mode);
PyObject *FpBinarySmall_FromBitsPylong(PyObject *scaled_value,
                                       FP_INT_TYPE int_bits,
                                       FP_INT_TYPE frac_bits, bool is_signed);

PyObject *FpBinarySmall_FromPickleDict(PyObject *dict);
bool FpBinarySmall_UpdatePickleDict(PyObject *self, PyObject *dict);

bool FpBinarySmall_IsNegative(PyObject *obj);
FP_UINT_TYPE fpbinarysmall_can_divide_ops(FP_UINT_TYPE op1_total_bits,
                                          FP_UINT_TYPE op2_total_bits);

#endif
