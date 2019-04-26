#ifndef FPBINARYSMALL_H
#define FPBINARYSMALL_H

#include "fpbinarycommon.h"

typedef struct
{
    fpbinary_base_t fpbinary_base;
    FP_UINT_TYPE int_bits;
    FP_UINT_TYPE frac_bits;
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
PyObject *FpBinarySmall_FromDouble(double value, FP_UINT_TYPE int_bits,
                                   FP_UINT_TYPE frac_bits, bool is_signed,
                                   fp_overflow_mode_t overflow_mode,
                                   fp_round_mode_t round_mode);
PyObject *FpBinarySmall_FromBitsPylong(PyObject *scaled_value,
                                       FP_UINT_TYPE int_bits,
                                       FP_UINT_TYPE frac_bits, bool is_signed);

bool FpBinarySmall_IsNegative(PyObject *obj);

#endif
