/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * _FpBinarySmall object (not meant for Python users, FpBinary wraps it).
 *
 * This object exists to maximise speed. Most use cases will not require more
 * than 64 bit fixed point numbers. So instead of using the arbitrary length
 * PyLong object, this object does most things using native C types.
 *
 * A real number is represented by the scaled_value field, type unsigned long
 *long.
 * This is the real value * 2**frac_bits. scaled_value can then be used for math
 * operations by using integer arithmetic. Negative numbers are converted to
 * 2's complement bit representation on entry. From there, the maths comes out
 * in the wash. Note that the C standard guarantees wrapping behavior for
 * unsigned types.
 *
 * All math operations result in a new object with the int_bits and frac_bits
 * fields expanded to ensure no overflow. The resize method can be used by
 * the user to reduce (or increase for some reason) the number of bits.
 * Multiple overflow and rounding modes are available (see OverflowEnumType
 * and RoundingEnumType).
 *
 *****************************************************************************/

#include "fpbinarysmall.h"
#include "fpbinarycommon.h"
#include "fpbinaryglobaldoc.h"
#include <math.h>

#define FP_BINARY_SMALL_FORMAT_SUPPORTED(int_bits, frac_bits) (((FP_UINT_TYPE)(int_bits + frac_bits)) <= FP_UINT_NUM_BITS)

static int
check_new_bit_len_ok(FpBinarySmallObject *new_obj)
{
    if (!FP_BINARY_SMALL_FORMAT_SUPPORTED(new_obj->int_bits, new_obj->frac_bits))
    {
        PyErr_SetString(PyExc_OverflowError,
                        "New FpBinary object has too many bits for this CPU.");
        return false;
    }

    return true;
}

static inline FP_UINT_TYPE
get_sign_bit(FP_UINT_TYPE total_bits)
{
    return fp_uint_lshift(1, (total_bits - 1));
}

/*
 * Assumes the passed value is properly sign extended.
 */
static bool
scaled_value_is_negative(FP_UINT_TYPE value, bool is_signed)
{
    if (!is_signed)
    {
        return false;
    }

    return ((FP_UINT_MAX_SIGN_BIT & value) != 0);
}

/*
 * Modifies the bits in scaled_value to represent the negative 2's complement
 * value.
 */
static inline FP_UINT_TYPE
negate_scaled_value(FP_UINT_TYPE scaled_value)
{
    return ~scaled_value + 1;
}

static inline FP_UINT_TYPE
sign_extend_scaled_value(FP_UINT_TYPE scaled_value, FP_UINT_TYPE total_bits,
                         bool is_signed)
{
    if (is_signed && (scaled_value & get_sign_bit(total_bits)))
    {
        /* Need to shift with 1's. Calculate mask that will OR out the newly
         * shifted zeros. */
        return scaled_value - fp_uint_lshift(1, total_bits);
    }

    return scaled_value;
}

/*
* Returns an int that gives the largest value representable (after conversion to
* scaled
* int representation) given total_bits.
*/
static FP_UINT_TYPE
get_max_scaled_value(FP_UINT_TYPE total_bits, bool is_signed)
{
    if (is_signed)
    {
        return fp_uint_lshift(1, (total_bits - 1)) - 1;
    }

    return fp_uint_rshift(FP_UINT_MAX_VAL, FP_UINT_NUM_BITS - total_bits);
}

/*
* Returns an int that gives the smallest value representable (after conversion
* to scaled
* int representation) given total_bits. Beware comparing with this directly.
* Usually, it
* needs to be compared via the compare_scaled_values function.
*/
static FP_UINT_TYPE
get_min_scaled_value(FP_UINT_TYPE total_bits, bool is_signed)
{
    if (is_signed)
    {
        return fp_uint_lshift(FP_UINT_MAX_VAL, total_bits - 1);
    }

    return 0;
}

static FP_UINT_TYPE
get_mag_of_min_scaled_value(FP_UINT_TYPE total_bits, bool is_signed)
{
    if (is_signed)
    {
        return get_sign_bit(total_bits);
    }

    return 0;
}

/*
 * Interprets scaled_value as a 2's complement signed integer.
 */
static FP_INT_TYPE
scaled_value_to_int(FP_UINT_TYPE scaled_value)
{
    /* I'm not simply casting to a signed pointer because the C standard doesn't
     * guarantee signed integers are 2's complement.
     */
    if (scaled_value & FP_UINT_MAX_SIGN_BIT)
    {
        /* Negative. Convert to magnitude and multiply by -1. */
        return ((FP_INT_TYPE)(~scaled_value + 1)) * -1;
    }
    else
    {
        return (FP_INT_TYPE)scaled_value;
    }
}

static PyObject *
scaled_value_to_pylong(FP_UINT_TYPE scaled_value, bool is_signed)
{
    if (is_signed)
    {

        return fp_int_as_pylong(scaled_value_to_int(scaled_value));
    }

    return fp_uint_as_pylong(scaled_value);
}

/*
* Returns 1, 0 or -1 depending on whether op1 is larger, equal to or smaller
* than op2. It is assumed the ops are taken from fpbinarysmallobject
* scaled_value fields and that the total number of bits are the same for both
* ops.
*/
static inline int
compare_scaled_values(bool are_signed, FP_UINT_TYPE op1, FP_UINT_TYPE op2)
{
    if (are_signed)
    {
        bool op1_negative = ((op1 & FP_UINT_MAX_SIGN_BIT) != 0);
        bool op2_negative = ((op2 & FP_UINT_MAX_SIGN_BIT) != 0);

        if ((op1_negative == op2_negative && op1 > op2) ||
            (!op1_negative && op2_negative))
        {
            return 1;
        }
        else if ((op1_negative == op2_negative && op1 < op2) ||
                 (op1_negative && !op2_negative))
        {
            return -1;
        }
        else
        {
            return 0;
        }
    }

    if (op1 == op2)
        return 0;
    return (op1 > op2) ? 1 : -1;
}

static FP_UINT_TYPE
get_total_bits_mask(FP_UINT_TYPE total_bits)
{
    return get_max_scaled_value(total_bits, false);
}

static FP_UINT_TYPE
apply_rshift(FP_UINT_TYPE value, FP_UINT_TYPE num_shifts, bool is_signed)
{
    if (num_shifts == 0)
        return value;

    /* Using unsigned integers to represent possible signed values, so need
     * to manually ensure sign is extended on shift.
     */
    if (is_signed)
    {
        if (value & FP_UINT_MAX_SIGN_BIT)
        {
            /* Need to shift with 1's. Calculate mask that will OR out the newly
             * shifted zeros. */
            return (fp_uint_rshift(value, num_shifts) |
                    ~fp_uint_rshift(FP_UINT_MAX_VAL, num_shifts));
        }
    }

    return fp_uint_rshift(value, num_shifts);
}

static inline FP_UINT_TYPE
apply_overflow_wrap(FP_UINT_TYPE value, bool is_signed, FP_UINT_TYPE max_value,
                    FP_UINT_TYPE sign_bit)
{
    /* If we overflowed into the negative range, subtract the sign bit
     * from the magnitude-masked value. If we overflowed into the
     * positive range, just mask with the magnitude bits only. */

    if (is_signed && (value & sign_bit))
    {
        return (value & max_value) - sign_bit;
    }

    return (value & max_value);
}

static inline void
set_object_fields(FpBinarySmallObject *self, FP_UINT_TYPE scaled_value,
                  FP_INT_TYPE int_bits, FP_INT_TYPE frac_bits, bool is_signed)
{
    self->scaled_value = scaled_value;
    self->int_bits = int_bits;
    self->frac_bits = frac_bits;
    self->is_signed = is_signed;
}

static void
copy_fields(FpBinarySmallObject *from_obj, FpBinarySmallObject *to_obj)
{
    set_object_fields(to_obj, from_obj->scaled_value, from_obj->int_bits,
                      from_obj->frac_bits, from_obj->is_signed);
}

/*
 * Will check the fields in obj for overflow and will modify act on the
 * fields of obj or raise an exception depending on overflow_mode.
 *
 * The force_positive_overflow and force_negative_overflow flags allow
 * a calling function to force check_overflow to act as though an overflow
 * occured. This is useful if an overflow occured during a previous operation on
 * self but isn't detectable from the current value of self. I.e. if
 * a scaling due to an increase in fractional bits resulted in an overflow.
 *
 * If there was no exception raised, non-zero is returned.
 * Otherwise, 0 is returned.
 */
static int
check_overflow(FpBinarySmallObject *self, fp_overflow_mode_t overflow_mode,
               bool force_positive_overflow, bool force_negative_overflow)
{
    FP_UINT_TYPE new_scaled_value = self->scaled_value;
    FP_UINT_TYPE min_value, max_value;
    FP_UINT_TYPE total_bits = self->int_bits + self->frac_bits;
    FP_UINT_TYPE sign_bit = get_sign_bit(total_bits);

    min_value = get_min_scaled_value(total_bits, self->is_signed);
    max_value = get_max_scaled_value(total_bits, self->is_signed);

    if (compare_scaled_values(self->is_signed, new_scaled_value, max_value) >
            0 ||
        force_positive_overflow)
    {
        if (overflow_mode == OVERFLOW_WRAP)
        {
            new_scaled_value = apply_overflow_wrap(
                new_scaled_value, self->is_signed, max_value, sign_bit);
        }
        else if (overflow_mode == OVERFLOW_SAT)
        {
            new_scaled_value = max_value;
        }
        else
        {
            PyErr_SetString(FpBinaryOverflowException,
                            "Fixed point resize overflow.");
            return false;
        }
    }
    else if (compare_scaled_values(self->is_signed, new_scaled_value,
                                   min_value) < 0 ||
             force_negative_overflow)
    {
        if (overflow_mode == OVERFLOW_WRAP)
        {
            new_scaled_value = apply_overflow_wrap(
                new_scaled_value, self->is_signed, max_value, sign_bit);
        }
        else if (overflow_mode == OVERFLOW_SAT)
        {
            new_scaled_value = min_value;
        }
        else
        {
            PyErr_SetString(FpBinaryOverflowException,
                            "Fixed point resize overflow.");
            return false;
        }
    }

    set_object_fields(self, new_scaled_value, self->int_bits, self->frac_bits,
                      self->is_signed);
    return true;
}

/*
 * Will convert the passed float to a fixed point object and apply
 * the result to output_obj.
 * Returns 0 if (there was an overflow AND round_mode is OVERFLOW_EXCEP)
 * Otherwise, non zero is returned.
 */
static bool
build_from_float(double value, FP_INT_TYPE int_bits, FP_INT_TYPE frac_bits,
                 bool is_signed, fp_overflow_mode_t overflow_mode,
                 fp_round_mode_t round_mode, FpBinarySmallObject *output_obj)
{
    FP_UINT_TYPE scaled_value = 0;
    FP_UINT_TYPE max_scaled_value =
        get_max_scaled_value(FP_UINT_NUM_BITS, is_signed);
    FP_UINT_TYPE min_scaled_value =
        get_min_scaled_value(FP_UINT_NUM_BITS, is_signed);
    FP_UINT_TYPE min_scaled_value_mag =
        get_mag_of_min_scaled_value(FP_UINT_NUM_BITS, is_signed);

    /* Can't use shifts if the number of frac_bits is the system word length
     * (which occurs here if the user specifies an fpbinary format of (0,
     * word_length).
     * So use ldexp, which should be fast enough.
     */
    double scaled_value_dbl = ldexp(value, frac_bits);

    if (round_mode == ROUNDING_NEAR_POS_INF)
    {
        scaled_value_dbl += 0.5;
    }

    scaled_value_dbl = floor(scaled_value_dbl);

    /*
     * Because we only have a limited number of bits to play with,
     * we need to check that the scaled value won't exceed the max
     * magnitude when we cast from the float. So we are really doing
     * a pre-saturate to the platform max magnitude before we do the
     * actual overflow check (which uses the int_bits/frac_bits limitations).
     */

    if (scaled_value_dbl >= max_scaled_value)
    {
        scaled_value = max_scaled_value;
    }
    else if (scaled_value_dbl <= -1.0 * min_scaled_value_mag)
    {
        scaled_value = min_scaled_value;
    }
    else
    {
        /* Safe to cast to int */
        if (is_signed && scaled_value_dbl < 0.0)
        {
            double abs_scaled_value = -scaled_value_dbl;
            scaled_value = ~((FP_UINT_TYPE)abs_scaled_value) + 1;
        }
        else
        {
            scaled_value = (FP_UINT_TYPE)scaled_value_dbl;
        }
    }

    set_object_fields(output_obj, scaled_value, int_bits, frac_bits, is_signed);
    return check_overflow(output_obj, overflow_mode, false, false);
}

/*
 * Will resize self to the format specified by new_int_bits and
 * new_frac_bits and take action based on overflow_mode and
 * round_mode.
 * If overflow_mode is OVERFLOW_EXCEP, an exception string will
 * be written and 0 will be returned if an overflow occurs.
 * Otherwise, non-zero is returned.
 */
static int
resize_object(FpBinarySmallObject *self, FP_INT_TYPE new_int_bits,
              FP_INT_TYPE new_frac_bits, fp_overflow_mode_t overflow_mode,
              fp_round_mode_t round_mode)
{
    bool manual_pos_overflow = false, manual_neg_overflow = false;

    FP_UINT_TYPE new_scaled_value = self->scaled_value;

    bool original_is_negative =
        scaled_value_is_negative(self->scaled_value, self->is_signed);

    /* Rounding */
    if (new_frac_bits < self->frac_bits)
    {
        FP_UINT_TYPE right_shifts = self->frac_bits - new_frac_bits;

        /* We do the main shift first and then add any round inc if the
         * chopped msb was 1. This avoids the underlying datatype
         * overflowing if we are using the max number of bits.
         */
        new_scaled_value =
            apply_rshift(self->scaled_value, right_shifts, self->is_signed);

        if (round_mode == ROUNDING_DIRECT_ZERO)
        {
            /* Go toward zero to the nearest representable value.
             * If positive, this is just a normal truncate.
             * If negative, we need to add "1.0" unless we are at a
             * "round number". That is, if the chopped bits aren't zero,
             * add NEW LSB 1 to the scaled value and then truncate.
             */
            if (self->is_signed && original_is_negative &&
                (self->scaled_value & get_total_bits_mask(right_shifts)))
            {
                new_scaled_value += 1;
            }
        }
        else if (round_mode == ROUNDING_NEAR_POS_INF ||
                 round_mode == ROUNDING_NEAR_ZERO ||
                 round_mode == ROUNDING_NEAR_EVEN)
        {
            /* "Near" rounding modes. This basically means we need to add
             * "0.5" to our value, conditioned on the specific near type.
             */

            FP_UINT_TYPE chopped_lsbs_value = 0;
            FP_UINT_TYPE num_chopped_minus_1 = right_shifts - 1;
            FP_UINT_TYPE chopped_msb =
                self->scaled_value & fp_uint_lshift(1, num_chopped_minus_1);

            if (right_shifts > 1)
            {
                chopped_lsbs_value = self->scaled_value &
                                     get_total_bits_mask(num_chopped_minus_1);
            }

            if (round_mode == ROUNDING_NEAR_EVEN)
            {
                FP_UINT_TYPE new_lsb =
                    self->scaled_value & fp_uint_lshift(1, right_shifts);

                if (chopped_msb != 0 &&
                    (chopped_lsbs_value != 0 || new_lsb != 0))
                {
                    new_scaled_value += 1;
                }
            }
            else if (round_mode == ROUNDING_NEAR_ZERO)
            {
                /*
                 * This is a "near" round but ties are settled towards zero.
                 * So if negative, a normal add of "0.5" and then truncate
                 * works.
                 * It also works for positive unless we are on an exact "0.5"
                 * boundary (i.e. the chopped LSBs except the MSB are zero). In
                 * that case, we must truncate WITHOUT the add.
                 */
                if ((chopped_msb != 0) &&
                    (original_is_negative || chopped_lsbs_value != 0))
                {
                    new_scaled_value += 1;
                }
            }
            else if (round_mode == ROUNDING_NEAR_POS_INF)
            {
                /* Add "new value 0.5" to the old sized value and then truncate
                 */
                if (chopped_msb != 0)
                {
                    new_scaled_value += 1;
                }
            }
        }
        /* else Default to truncate (ROUNDING_DIRECT_NEG_INF) */
    }
    else if (new_frac_bits > self->frac_bits)
    {
        FP_UINT_TYPE lshifts = new_frac_bits - self->frac_bits;
        new_scaled_value = fp_uint_lshift(new_scaled_value, lshifts);

        /*
         * Calling classes must ensure that the new format after a resize
         * has <= the system word length. This means that adding fractional
         * bits will not result in an overflow UNLESS the user also REDUCES
         * the number of int bits. In this instance, it is possible for the
         * scaling for the extra fractional bits will cause the chopped int
         * bit information to be lost (because the left shift is done on a
         * native word). This will cause a wrap, regardless of whether the
         * user asked for a wrap or a saturation/exception. In order to provide
         * the saturation and exception modes, we need to see if we lost data
         * in the left shift.
         *
         * We need to look at the int bits that will be shifted out and if
         * they are non zero (for positive values) or non one (for negative)
         * values, then we overflowed. In addition, if the newly scaled value
         * is of a different sign than the original, we overflowed.
         */
        if (overflow_mode != OVERFLOW_WRAP && new_int_bits < self->int_bits)
        {
            bool new_is_negative =
                scaled_value_is_negative(new_scaled_value, self->is_signed);

            FP_UINT_TYPE overflow_mask =
                ~fp_uint_rshift(FP_UINT_ALL_BITS_MASK, lshifts);
            FP_UINT_TYPE overflow = self->scaled_value & overflow_mask;

            if (original_is_negative)
            {
                if (!new_is_negative || ((~overflow & overflow_mask) != 0))
                {
                    manual_neg_overflow = true;
                }
            }
            else
            {
                if (new_is_negative || overflow != 0)
                {
                    manual_pos_overflow = true;
                }
            }
        }
    }

    set_object_fields(self, new_scaled_value, new_int_bits, new_frac_bits,
                      self->is_signed);
    return check_overflow(self, overflow_mode, manual_pos_overflow,
                          manual_neg_overflow);
}

static double
fpbinarysmall_to_double(FpBinarySmallObject *obj)
{
    double result;

    if (obj->is_signed && (obj->scaled_value & FP_UINT_MAX_SIGN_BIT))
    {
        /* Negative - create double with magnitude and mult by -1.0 */
        double magnitude = (double)(~obj->scaled_value + 1);
        result = ldexp(-magnitude, -obj->frac_bits);
    }
    else
    {
        result = ldexp(((double)obj->scaled_value), -obj->frac_bits);
    }

    return result;
}

static void
fpsmall_format_as_pylongs(PyObject *self, PyObject **out_int_bits,
                          PyObject **out_frac_bits)
{
    *out_int_bits = PyLong_FromLong(((FpBinarySmallObject *)self)->int_bits);
    *out_frac_bits = PyLong_FromLong(((FpBinarySmallObject *)self)->frac_bits);
}

static FpBinarySmallObject *
fpbinarysmall_create_mem(PyTypeObject *type)
{
    FpBinarySmallObject *self = (FpBinarySmallObject *)type->tp_alloc(type, 0);

    if (self)
    {
        self->fpbinary_base.private_iface = &FpBinary_SmallPrvIface;
        set_object_fields(self, 0, 1, 0, true);
    }

    return self;
}

PyDoc_STRVAR(fpbinarysmall_doc,
             "_FpBinarySmall(int_bits=1, frac_bits=0, signed=True, value=0.0, "
             "bit_field=None, format_inst=None)\n"
             "\n"
             "Represents a real number using fixed point math and structure.\n"
             "NOTE: This object is not intended to be used directly!\n");
static PyObject *
fpbinarysmall_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FpBinarySmallObject *self = NULL;
    long int_bits = 1, frac_bits = 0;
    bool is_signed = true;
    double value = 0.0;
    PyObject *bit_field = NULL, *format_instance = NULL;

    if (!fp_binary_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                    &is_signed, &value, &bit_field,
                                    &format_instance))
    {
        return NULL;
    }

    if (format_instance)
    {
        if (!FpBinarySmall_Check(format_instance))
        {
            PyErr_SetString(
                PyExc_TypeError,
                "format_inst must be an instance of FpBinarySmall.");
            return NULL;
        }
    }

    if (format_instance)
    {
        int_bits = ((FpBinarySmallObject *)format_instance)->int_bits;
        frac_bits = ((FpBinarySmallObject *)format_instance)->frac_bits;
    }

    if (bit_field)
    {
        self = (FpBinarySmallObject *)FpBinarySmall_FromBitsPylong(
            bit_field, int_bits, frac_bits, is_signed);
    }
    else
    {
        self = (FpBinarySmallObject *)FpBinarySmall_FromDouble(
            value, int_bits, frac_bits, is_signed, OVERFLOW_SAT,
            ROUNDING_NEAR_POS_INF);
    }

    return (PyObject *)self;
}

/*
 * See copy_doc
 */
static PyObject *
fpbinarysmall_copy(FpBinarySmallObject *self, PyObject *args)
{
    FpBinarySmallObject *new_obj =
        fpbinarysmall_create_mem(&FpBinary_SmallType);
    if (new_obj)
    {
        copy_fields(self, new_obj);
    }

    return (PyObject *)new_obj;
}

/*
 * Returns a new FpBinarySmall object where the value is the same
 * as obj but:
 *     if obj is unsigned, an extra bit is added to int_bits.
 *     if obj is signed, no change to value or format.
 */
static PyObject *
fpbinarysmall_to_signed(PyObject *obj, PyObject *args)
{
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_obj = (FpBinarySmallObject *)obj;

    if (!FpBinarySmall_Check(obj))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    if (cast_obj->is_signed)
    {
        return fpbinarysmall_copy(cast_obj, NULL);
    }

    /* Input is an unsigned FpBinarySmall object. */

    result = fpbinarysmall_create_mem(&FpBinary_SmallType);
    /* TODO: Shouldn't this be int_bits + 1 ??? */
    set_object_fields(result, cast_obj->scaled_value, cast_obj->int_bits,
                      cast_obj->frac_bits, true);

    return (PyObject *)result;
}

/*
 * See resize_doc
 */
static PyObject *
fpbinarysmall_resize(FpBinarySmallObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *format;
    int overflow_mode = OVERFLOW_WRAP;
    int round_mode = ROUNDING_DIRECT_NEG_INF;
    static char *kwlist[] = {"format", "overflow_mode", "round_mode", NULL};
    FP_INT_TYPE new_int_bits = self->int_bits, new_frac_bits = self->frac_bits;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ii", kwlist, &format,
                                     &overflow_mode, &round_mode))
        return NULL;

    /* FP format is defined by a python tuple: (int_bits, frac_bits) */
    if (PyTuple_Check(format))
    {
        PyObject *int_bits_obj, *frac_bits_obj;

        if (!extract_fp_format_from_tuple(format, &int_bits_obj,
                                          &frac_bits_obj))
        {
            return NULL;
        }

        new_int_bits = (FP_INT_TYPE)(long)PyLong_AsLong(int_bits_obj);
        new_frac_bits = (FP_INT_TYPE)(long)PyLong_AsLong(frac_bits_obj);

        Py_DECREF(int_bits_obj);
        Py_DECREF(frac_bits_obj);
    }
    else if (FpBinarySmall_Check(format))
    {
        /* Format is an instance of FpBinary, so use its format */
        new_int_bits = ((FpBinarySmallObject *)format)->int_bits;
        new_frac_bits = ((FpBinarySmallObject *)format)->frac_bits;
    }
    else
    {
        PyErr_SetString(PyExc_TypeError,
                        "The format parameter type is not supported.");
        return NULL;
    }

    if (resize_object(self, new_int_bits, new_frac_bits, overflow_mode,
                      round_mode))
    {
        Py_INCREF(self);
        return (PyObject *)self;
    }

    return NULL;
}

/*
 * See bits_to_signed_doc
 */
static PyObject *
fpbinarysmall_bits_to_signed(FpBinarySmallObject *self, PyObject *args)
{
    FP_UINT_TYPE scaled_value;

    if (self->is_signed)
    {
        scaled_value = self->scaled_value;
    }
    else
    {
        /*
         * If the MSB is one, need to interpret the bits as negative 2's
         * complement. This requires a sign extension.
         */
        FP_UINT_TYPE total_bits = self->int_bits + self->frac_bits;
        if (self->scaled_value & get_sign_bit(total_bits))
        {
            scaled_value =
                self->scaled_value | ~(get_total_bits_mask(total_bits));
        }
        else
        {
            scaled_value = self->scaled_value;
        }
    }

    return PyLong_FromLongLong(scaled_value_to_int(scaled_value));
}

/*
 * Convenience function to make sure the operands of a two operand operation
 * are FpBinarySmallObject instances and of the same signed type.
 */
static bool
check_binary_ops(PyObject *op1, PyObject *op2)
{
    if (!FpBinarySmall_Check(op1) || !FpBinarySmall_Check(op2))
    {
        return false;
    }

    if (((FpBinarySmallObject *)op1)->is_signed !=
        ((FpBinarySmallObject *)op2)->is_signed)
    {
        return false;
    }

    return true;
}

/*
 * NOTE: The calling function is expected to decrement the reference counters
 * of output_op1, output_op2.
 */
static void
make_binary_ops_same_frac_size(PyObject *op1, PyObject *op2,
                               FpBinarySmallObject **output_op1,
                               FpBinarySmallObject **output_op2)
{
    FpBinarySmallObject *cast_op1 = (FpBinarySmallObject *)op1;
    FpBinarySmallObject *cast_op2 = (FpBinarySmallObject *)op2;

    if (cast_op1->frac_bits > cast_op2->frac_bits)
    {
        FpBinarySmallObject *new_op =
            fpbinarysmall_create_mem(&FpBinary_SmallType);
        set_object_fields(
            new_op, fp_uint_lshift(cast_op2->scaled_value,
                                   cast_op1->frac_bits - cast_op2->frac_bits),
            cast_op2->int_bits, cast_op1->frac_bits, cast_op2->is_signed);

        *output_op2 = new_op;
        Py_INCREF(cast_op1);
        *output_op1 = cast_op1;
    }
    else if (cast_op2->frac_bits > cast_op1->frac_bits)
    {
        FpBinarySmallObject *new_op =
            fpbinarysmall_create_mem(&FpBinary_SmallType);
        set_object_fields(
            new_op, fp_uint_lshift(cast_op1->scaled_value,
                                   cast_op2->frac_bits - cast_op1->frac_bits),
            cast_op1->int_bits, cast_op2->frac_bits, cast_op1->is_signed);

        *output_op1 = new_op;
        Py_INCREF(cast_op2);
        *output_op2 = cast_op2;
    }
    else
    {
        Py_INCREF(cast_op1);
        Py_INCREF(cast_op2);
        *output_op1 = cast_op1;
        *output_op2 = cast_op2;
    }
}

/*
 *
 * Numeric methods implementation
 *
 */

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_add(PyObject *op1, PyObject *op2)
{
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_op1, *cast_op2;
    FP_INT_TYPE result_int_bits;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /* Add requires the fractional bits to be lined up */
    make_binary_ops_same_frac_size(op1, op2, &cast_op1, &cast_op2);

    result_int_bits = (cast_op1->int_bits > cast_op2->int_bits)
                          ? cast_op1->int_bits
                          : cast_op2->int_bits;

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);

    /* Do add and add int_bit. */
    set_object_fields(result, cast_op1->scaled_value + cast_op2->scaled_value,
                      result_int_bits + 1, cast_op1->frac_bits,
                      cast_op1->is_signed);

    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);

    /* Check for overflow of our underlying word size. */
    if (!check_new_bit_len_ok(result))
    {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_subtract(PyObject *op1, PyObject *op2)
{
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_op1, *cast_op2;
    FP_INT_TYPE result_int_bits;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /* Add requires the fractional bits to be lined up */
    make_binary_ops_same_frac_size(op1, op2, &cast_op1, &cast_op2);

    result_int_bits = (cast_op1->int_bits > cast_op2->int_bits)
                          ? cast_op1->int_bits
                          : cast_op2->int_bits;

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);

    /* Do add and add int_bit. */
    set_object_fields(result, cast_op1->scaled_value - cast_op2->scaled_value,
                      result_int_bits + 1, cast_op1->frac_bits,
                      cast_op1->is_signed);

    /* Need to deal with negative numbers and wrapping if we are unsigned type.
     */
    if (!result->is_signed)
    {
        check_overflow(result, OVERFLOW_WRAP, false, false);
    }

    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);

    /* Check for overflow of our underlying word size. */
    if (!check_new_bit_len_ok(result))
    {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_multiply(PyObject *op1, PyObject *op2)
{
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_op1, *cast_op2;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    cast_op1 = (FpBinarySmallObject *)op1;
    cast_op2 = (FpBinarySmallObject *)op2;

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
    /* Do multiply and add format bits. */
    set_object_fields(result, cast_op1->scaled_value * cast_op2->scaled_value,
                      cast_op1->int_bits + cast_op2->int_bits,
                      cast_op1->frac_bits + cast_op2->frac_bits,
                      cast_op1->is_signed);

    /* Check for overflow of our underlying word size. */
    if (!check_new_bit_len_ok(result))
    {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *)result;
}

/*
 * Returns true if the FpBinarySmall object has enough bits in its native
 * type to divide op1 by op2.
 */
FP_UINT_TYPE
fpbinarysmall_can_divide_ops(FP_UINT_TYPE op1_total_bits,
                             FP_UINT_TYPE op2_total_bits)
{
    /* Need to shift the numerator by the total bits in the denom to do the
     * integer divide,
     * and allow for extra sign bit. */
    return (op1_total_bits + op2_total_bits + 1) <= FP_SMALL_MAX_BITS;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_divide(PyObject *op1, PyObject *op2)
{
    /*
     * Given the nature of division (i.e. the int bits in the denominator make
     * the result smaller and the frac bits in the denomintor make the result
     * larger), the convention is to have:
     *     result frac bits = numerator frac bits + denominator int bits
     *
     * Similarly, in order to avoid overflow:
     *     result int bits = numerator int bits + denominator frac bits + 1
     *     (the + 1 is only required for signed (e.g. -8 / -0.125) )
     *
     *
     * We just divide the scaled values but in order to maintain precision,
     * we scale the numerator further by denom_frac_bits + denom_int_bits since:
     *     result = (actual_num << num_frac_bits_adjusted) / (actual_denom <<
     * denom_frac_bits)
     *            = (actual_num / actual_denom) << (num_frac_bits_adjusted -
     * denom_frac_bits)
     *
     * So, to get (num frac bits + denom int bits) frac bits in our result:
     *     num_frac_bits_adjusted - denom_frac_bits = num frac bits + denom int
     * bits
     *     num_frac_bits_adjusted = num frac bits + denom int bits +
     * denom_frac_bits
     *
     * So, all this is a long-winded way of saying that we just left shift the
     * numerator by
     * (denom int bits + denom_frac_bits) and then divide by the untouched
     * denominator.
     *
     *
     */
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_op1 = (FpBinarySmallObject *)op1;
    FpBinarySmallObject *cast_op2 = (FpBinarySmallObject *)op2;

    FP_INT_TYPE op2_total_bits = cast_op2->int_bits + cast_op2->frac_bits;

    bool op1_neg =
        scaled_value_is_negative(cast_op1->scaled_value, cast_op1->is_signed);
    bool op2_neg =
        scaled_value_is_negative(cast_op2->scaled_value, cast_op2->is_signed);

    FP_UINT_TYPE op1_scaled_val_mag, op2_scaled_val_mag;
    FP_UINT_TYPE new_scaled_value;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /*
     * We use unsigned ints for the scaled value. This means direct division
     * will not work negative numbers will always be larger than positive,
     * so division by a neg number will result in integer 0. So convert to
     * magnitude and sign extend later (note that overflow isn't an issue
     * with magnitude conversion here because the unsigned result will be
     * the same except for the sign - e.g 8 / 1 or -8 / 1 both look like
     * 0b1000 / 0b0001).
     */

    if (op1_neg)
    {
        op1_scaled_val_mag = negate_scaled_value(cast_op1->scaled_value);
    }
    else
    {
        op1_scaled_val_mag = cast_op1->scaled_value;
    }

    if (op2_neg)
    {
        op2_scaled_val_mag = negate_scaled_value(cast_op2->scaled_value);
    }
    else
    {
        op2_scaled_val_mag = cast_op2->scaled_value;
    }

    /* Extra scale for final fractional precision */
    op1_scaled_val_mag = fp_uint_lshift(op1_scaled_val_mag, op2_total_bits);

    new_scaled_value = op1_scaled_val_mag / op2_scaled_val_mag;
    if (op1_neg != op2_neg)
    {
        new_scaled_value = negate_scaled_value(new_scaled_value);
    }

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);

    set_object_fields(
        result, new_scaled_value,
        cast_op1->is_signed ? cast_op1->int_bits + cast_op2->frac_bits + 1
                            : cast_op1->int_bits + cast_op2->frac_bits,
        cast_op1->frac_bits + cast_op2->int_bits, cast_op1->is_signed);

    /* Check for overflow of our underlying word size. */
    if (!check_new_bit_len_ok(result))
    {
        Py_DECREF(result);
        return NULL;
    }

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_negative(PyObject *self)
{
    PyObject *result;
    PyObject *minus_one =
        (PyObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);

    set_object_fields((FpBinarySmallObject *)minus_one, -1, 1, 0, true);
    result = fpbinarysmall_multiply(self, minus_one);

    Py_DECREF(minus_one);

    return result;
}

static PyObject *
fpbinarysmall_long(PyObject *self)
{
    FpBinarySmallObject *cast_self = (FpBinarySmallObject *)self;
    FpBinarySmallObject *result_fp =
        fpbinarysmall_create_mem(&FpBinary_SmallType);
    PyObject *result = NULL;
    copy_fields(cast_self, result_fp);
    resize_object(result_fp, cast_self->int_bits, 0, OVERFLOW_WRAP,
                  ROUNDING_DIRECT_ZERO);

    result = PyLong_FromLongLong(scaled_value_to_int(result_fp->scaled_value));
    Py_DECREF(result_fp);

    return result;
}

static PyObject *
fpbinarysmall_int(PyObject *self)
{
    PyObject *result_pylong = fpbinarysmall_long(self);
    PyObject *result = NULL;

    result = FpBinary_TryConvertToPyInt(result_pylong);
    Py_DECREF(result_pylong);

    return result;
}

/*
 * Creating indexes from a fixed point number number is just returning
 * an unsigned int from the bits in the number.
 */
static PyObject *
fpbinarysmall_index(PyObject *self)
{
    FpBinarySmallObject *cast_self = (FpBinarySmallObject *)self;
    return fp_uint_as_pylong(
        cast_self->scaled_value &
        get_total_bits_mask(cast_self->int_bits + cast_self->frac_bits));
}

static PyObject *
fpbinarysmall_float(PyObject *self)
{
    return PyFloat_FromDouble(
        fpbinarysmall_to_double((FpBinarySmallObject *)self));
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarysmall_abs(PyObject *self)
{
    FpBinarySmallObject *cast_self = (FpBinarySmallObject *)self;

    if (!cast_self->is_signed ||
        !(cast_self->scaled_value & FP_UINT_MAX_SIGN_BIT))
    {
        /* Positive already */
        return fpbinarysmall_copy((FpBinarySmallObject *)self, NULL);
    }

    /* Negative value. Just negate */
    return FP_NUM_METHOD(self, nb_negative)(self);
}

static PyObject *
fpbinarysmall_lshift(PyObject *self, PyObject *pyobj_lshift)
{
    long lshift;
    FpBinarySmallObject *cast_self = (FpBinarySmallObject *)self;
    FP_INT_TYPE total_bits = cast_self->int_bits + cast_self->frac_bits;
    FP_UINT_TYPE sign_bit = get_sign_bit(total_bits);
    FP_UINT_TYPE mask = get_total_bits_mask(total_bits);
    FP_UINT_TYPE shifted_value;
    FpBinarySmallObject *result = NULL;

    if (!PyLong_Check(pyobj_lshift))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    lshift = PyLong_AsLong(pyobj_lshift);
    shifted_value = fp_uint_lshift(cast_self->scaled_value, lshift);

    /*
     * For left shifting, we need to make sure the bits above our sign
     * bit are the correct value. I.e. zeros if the result is positive
     * and ones if the result is negative. This is because we rely on
     * the signed value of the underlying scaled_value integer.
     */
    if (cast_self->is_signed && (shifted_value & sign_bit))
    {
        shifted_value |= (~mask);
    }
    else
    {
        shifted_value &= (mask);
    }

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
    set_object_fields(result, shifted_value, cast_self->int_bits,
                      cast_self->frac_bits, cast_self->is_signed);

    return (PyObject *)result;
}

static PyObject *
fpbinarysmall_rshift(PyObject *self, PyObject *pyobj_rshift)
{
    long rshift;
    FpBinarySmallObject *cast_self = ((FpBinarySmallObject *)self);
    FpBinarySmallObject *result = NULL;

    if (!PyLong_Check(pyobj_rshift))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    rshift = PyLong_AsLong(pyobj_rshift);

    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
    set_object_fields(
        result,
        apply_rshift(cast_self->scaled_value, rshift, cast_self->is_signed),
        cast_self->int_bits, cast_self->frac_bits, cast_self->is_signed);

    return (PyObject *)result;
}

static int
fpbinarysmall_nonzero(PyObject *self)
{
    return (self && ((FpBinarySmallObject *)self)->scaled_value != 0);
}

/*
 *
 * Sequence methods implementation
 *
 */

static Py_ssize_t
fpbinarysmall_sq_length(PyObject *self)
{
    return (Py_ssize_t)(((FpBinarySmallObject *)self)->int_bits +
                        ((FpBinarySmallObject *)self)->frac_bits);
}

/*
 * A get item on an fpbinarysmallobject returns a bool (True for 1, False for
 * 0).
 */
static PyObject *
fpbinarysmall_sq_item(PyObject *self, Py_ssize_t py_index)
{
    FpBinarySmallObject *cast_self = ((FpBinarySmallObject *)self);

    if (py_index < ((Py_ssize_t)(cast_self->int_bits + cast_self->frac_bits)))
    {
        if (cast_self->scaled_value & fp_uint_lshift(1, py_index))
        {
            Py_RETURN_TRUE;
        }
        else
        {
            Py_RETURN_FALSE;
        }
    }

    return NULL;
}

/*
 * If slice notation is invoked on an fpbinarysmallobject, a new
 * fpbinarysmallobject is created as an unsigned integer where the value is the
 * value of the selected bits.
 *
 * This is useful for digital logic implementations of NCOs and trig lookup
 * tables.
 */
static PyObject *
fpbinarysmall_sq_slice(PyObject *self, Py_ssize_t index1, Py_ssize_t index2)
{
    FpBinarySmallObject *result = NULL;
    FpBinarySmallObject *cast_self = ((FpBinarySmallObject *)self);
    Py_ssize_t low_index, high_index;
    FP_UINT_TYPE mask;
    FP_INT_TYPE max_index = cast_self->int_bits + cast_self->frac_bits - 1;

    /* To allow for the (reasonably) common convention of "high-to-low" bit
     * array ordering in languages like VHDL, the user can have index 1 higher
     * than index 2 - we always just assume the highest value is the MSB
     * desired. */
    if (index1 < index2)
    {
        low_index = index1;
        high_index = index2;
    }
    else
    {
        low_index = index2;
        high_index = index1;
    }

    if (high_index > max_index)
        high_index = max_index;

    mask = fp_uint_lshift(1, (high_index + 1)) - 1;
    result =
        (FpBinarySmallObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
    set_object_fields(result,
                      fp_uint_rshift(cast_self->scaled_value & mask, low_index),
                      high_index - low_index + 1, 0, false);

    return (PyObject *)result;
}

static PyObject *
fpbinarysmall_subscript(PyObject *self, PyObject *item)
{
    FpBinarySmallObject *cast_self = ((FpBinarySmallObject *)self);
    Py_ssize_t index, start, stop;

    if (fp_binary_subscript_get_item_index(item, &index))
    {
        return fpbinarysmall_sq_item(self, index);
    }
    else if (fp_binary_subscript_get_item_start_stop(item, &start, &stop,
                                                     cast_self->int_bits +
                                                         cast_self->frac_bits))
    {
        return fpbinarysmall_sq_slice(self, start, stop);
    }

    return NULL;
}

static PyObject *
fpbinarysmall_str(PyObject *obj)
{
    PyObject *result;
    PyObject *double_val = fpbinarysmall_float(obj);
    result = Py_TYPE(double_val)->tp_str(double_val);

    Py_DECREF(double_val);
    return result;
}

/*
 * See str_ex_doc
 */
static PyObject *
fpbinarysmall_str_ex(PyObject *self)
{
    FpBinarySmallObject *cast_self = ((FpBinarySmallObject *)self);
    PyObject *scaled_value =
        scaled_value_to_pylong(cast_self->scaled_value, cast_self->is_signed);
    PyObject *result = scaled_long_to_float_str(
        scaled_value, fp_int_as_pylong(cast_self->int_bits),
        fp_int_as_pylong(cast_self->frac_bits));

    Py_DECREF(scaled_value);
    return result;
}

/*
 * Because are are using the native word length and because two different
 * fpbinary objects can have wildly different formats, we can't compare
 * their values just by setting both to have the same number of fractional
 * bits. E.g. if op1 has format (64, 0) and op2 has format (-100, 164),
 * setting op1 to have 164 fract bits is impossible.
 *
 * So we have to compare the operands in two blocks. We first compare the
 * bits DOWN TO the fractional bit with the lowest number (or the highest
 * fractional place) of the two operands. If there is a difference, we
 * are done. If they are the same, we then check the left over lower
 * fractional bits of the operand with the lowest fractional place. If
 * this is non zero, it must be the larger number (regardless of sign).
 */
static int
fpbinarysmall_compare(PyObject *obj1, PyObject *obj2)
{
    FpBinarySmallObject *cast_op1 = (FpBinarySmallObject *)obj1;
    FpBinarySmallObject *cast_op2 = (FpBinarySmallObject *)obj2;

    int current_compare;
    FP_INT_TYPE op1_right_shift, op2_right_shift;
    FP_UINT_TYPE op1_value_shifted, op2_value_shifted;
    FP_INT_TYPE lowest_frac_bits = (cast_op1->frac_bits < cast_op2->frac_bits)
                                       ? cast_op1->frac_bits
                                       : cast_op2->frac_bits;

    /* Compare highest bit blocks */
    op1_right_shift = cast_op1->frac_bits - lowest_frac_bits;
    op2_right_shift = cast_op2->frac_bits - lowest_frac_bits;

    op1_value_shifted = apply_rshift(cast_op1->scaled_value, op1_right_shift,
                                     cast_op1->is_signed);
    op2_value_shifted = apply_rshift(cast_op2->scaled_value, op2_right_shift,
                                     cast_op2->is_signed);

    current_compare = compare_scaled_values(
        cast_op1->is_signed, op1_value_shifted, op2_value_shifted);

    if (current_compare != 0)
    {
        return current_compare;
    }

    /* First block of bits are equal.  This means that ops are the same sign
     * and so the op that still has bits uncompared has smaller fractional
     * bits to check - if any of these are non-zero, it must be bigger, else
     * equal.
     * So we just do a standard mask on the basis on what bits have already been
     * checked.
     */

    op1_value_shifted =
        cast_op1->scaled_value &
        (~fp_uint_lshift(FP_UINT_ALL_BITS_MASK, op1_right_shift));
    op2_value_shifted =
        cast_op2->scaled_value &
        (~fp_uint_lshift(FP_UINT_ALL_BITS_MASK, op2_right_shift));

    if (op1_value_shifted > op2_value_shifted)
        return 1;
    else if (op1_value_shifted < op2_value_shifted)
        return -1;
    return 0;
}

static PyObject *
fpbinarysmall_richcompare(PyObject *obj1, PyObject *obj2, int operator)
{
    bool eval = false;
    int compare;

    if (!check_binary_ops(obj1, obj2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    compare = fpbinarysmall_compare(obj1, obj2);

    switch (operator)
    {
        case Py_LT: eval = (compare < 0); break;
        case Py_LE: eval = (compare <= 0); break;
        case Py_EQ: eval = (compare == 0); break;
        case Py_NE: eval = (compare != 0); break;
        case Py_GT: eval = (compare > 0); break;
        case Py_GE: eval = (compare >= 0); break;
        default: eval = false; break;
    }

    if (eval)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static void
fpbinarysmall_dealloc(FpBinarySmallObject *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
}

/*
 * See format_doc
 */
static PyObject *
fpbinarysmall_getformat(PyObject *self, void *closure)
{
    PyObject *result_tuple = NULL;
    PyObject *pylong_int_bits;
    PyObject *pylong_frac_bits;

    fpsmall_format_as_pylongs(self, &pylong_int_bits, &pylong_frac_bits);

    if (pylong_int_bits && pylong_frac_bits)
    {
        result_tuple = PyTuple_Pack(2, pylong_int_bits, pylong_frac_bits);
    }

    if (!result_tuple)
    {
        Py_XDECREF(pylong_int_bits);
        Py_XDECREF(pylong_frac_bits);
    }

    return result_tuple;
}

/*
 * See is_signed_doc
 */
static PyObject *
fpbinarysmall_is_signed(PyObject *self, void *closure)
{
    if (((FpBinarySmallObject *)self)->is_signed)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

/* Helper functions for use of top client object. */
static FP_UINT_TYPE
fpbinarysmall_get_total_bits(PyObject *obj)
{
    FpBinarySmallObject *cast_obj = (FpBinarySmallObject *)obj;
    return cast_obj->int_bits + cast_obj->frac_bits;
}

void
FpBinarySmall_FormatAsUints(PyObject *self, FP_UINT_TYPE *out_int_bits,
                            FP_UINT_TYPE *out_frac_bits)
{
    *out_int_bits = ((FpBinarySmallObject *)self)->int_bits;
    *out_frac_bits = ((FpBinarySmallObject *)self)->frac_bits;
}

/*
 * Returns a PyLong* who's bits are those of the underlying FpBinarySmallObject
 * instance. This means that if the object represents a negative value, the
 * sign bit (as defined by int_bits and frac_bits) will be 1 (i.e. the bits
 * will be in 2's complement format). However, don't assume the PyLong returned
 * will or won't be negative.
 *
 * No need to increment the reference counter on the returned object.
 */
PyObject *
FpBinarySmall_BitsAsPylong(PyObject *obj)
{
    return PyLong_FromUnsignedLongLong(
        ((FpBinarySmallObject *)obj)->scaled_value);
}

static bool
fpsmall_is_signed(PyObject *obj)
{
    return ((FpBinarySmallObject *)obj)->is_signed;
}

bool
FpBinarySmall_IsNegative(PyObject *obj)
{
    FpBinarySmallObject *cast_obj = ((FpBinarySmallObject *)obj);
    return scaled_value_is_negative(cast_obj->scaled_value,
                                    cast_obj->is_signed);
}

PyObject *
FpBinarySmall_FromDouble(double value, FP_INT_TYPE int_bits,
                         FP_INT_TYPE frac_bits, bool is_signed,
                         fp_overflow_mode_t overflow_mode,
                         fp_round_mode_t round_mode)
{
    FpBinarySmallObject *self = fpbinarysmall_create_mem(&FpBinary_SmallType);
    if (!build_from_float(value, int_bits, frac_bits, is_signed, overflow_mode,
                          round_mode, self))
    {
        Py_DECREF(self);
        self = NULL;
    }

    return (PyObject *)self;
}

/*
 * Will return a new FpBinarySmallObject with the underlying fixed point value
 * defined by bits, int_bits and frac_bits. Note that bits is
 * expected to be a group of bits that reflect the 2's complement representation
 * of the fixed point value * 2^frac_bits. I.e. the signed bit would be 1 for
 * a negative fixed point number. However, it is NOT assumed that bits
 * is negative (i.e. only int_bits + frac_bits bits will be used so sign
 * extension is not required).
 *
 * This function is useful for creating an object that has a large number of
 * fixed point bits and the double type is too small to represent the the
 * init value.
 *
 * NOTE: It is assumed that the inputs are correct such that the number of
 * format bits are enough to represent the size of bits.
 */
PyObject *
FpBinarySmall_FromBitsPylong(PyObject *scaled_value, FP_INT_TYPE int_bits,
                             FP_INT_TYPE frac_bits, bool is_signed)
{
    PyObject *result;
    FP_UINT_TYPE total_bits = int_bits + frac_bits;
    PyObject *mask =
        PyLong_FromUnsignedLongLong(get_total_bits_mask(total_bits));
    PyObject *masked_val =
        FP_NUM_METHOD(scaled_value, nb_and)(scaled_value, mask);
    FP_UINT_TYPE scaled_value_uint = pylong_as_fp_uint(masked_val);

    /* If underlying value is negative, ensure bits are sign extended. */
    scaled_value_uint =
        sign_extend_scaled_value(scaled_value_uint, total_bits, is_signed);

    result = (PyObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
    set_object_fields((FpBinarySmallObject *)result, scaled_value_uint,
                      int_bits, frac_bits, is_signed);

    Py_DECREF(mask);
    Py_DECREF(masked_val);

    return result;
}

/*
 * Will rebuilt an FpBinarySmall instance and return IF the number of
 * bits in the instance format are supportable on the current platform.
 * If not (i.e. user has opened a pickle from a larger wordlength machine),
 * a Dict will be returned with data that can be used to build an FpBinary
 * instance (presumably via a FpBinaryLarge instance) using the bitfield
 * method of creation. The Dict format is:
 *
 * {'ib': int_bits_pylong, 'fb': frac_bits_pylong, 'sv': bitfield_pylong, 'sgn': is_signed_pybool}
 */
PyObject *
FpBinarySmall_FromPickleDict(PyObject *dict)
{
    PyObject *result = NULL;
    PyObject *int_bits, *frac_bits, *scaled_value, *is_signed;

    int_bits = PyDict_GetItemString(dict, "ib");
    frac_bits = PyDict_GetItemString(dict, "fb");
    scaled_value = PyDict_GetItemString(dict, "sv");
    is_signed = PyDict_GetItemString(dict, "sgn");

    if (int_bits && frac_bits && scaled_value && is_signed)
    {
        FP_INT_TYPE int_bits_native, frac_bits_native;

        /* Make sure the objects are actually PyLongs. I.e. if we are in Python
         * 2.7, the unpickler may have decided to create a PyInt. Note that
         * after
         * these calls, we have created a new/incremented reference, so we need
         * to decrement when done (note that up to this point, the objects are
         * just a borrowed references from the dict).
         */
        int_bits = FpBinary_EnsureIsPyLong(int_bits);
        frac_bits = FpBinary_EnsureIsPyLong(frac_bits);
        scaled_value = FpBinary_EnsureIsPyLong(scaled_value);

        int_bits_native = pylong_as_fp_int(int_bits);
        frac_bits_native = pylong_as_fp_int(frac_bits);

        if (FP_BINARY_SMALL_FORMAT_SUPPORTED(int_bits_native, frac_bits_native))
        {
            result = (PyObject *)fpbinarysmall_create_mem(&FpBinary_SmallType);
            set_object_fields(
                (FpBinarySmallObject *)result, pylong_as_fp_uint(scaled_value),
                int_bits_native, frac_bits_native,
                (is_signed == Py_True) ? true : false);
        }
        else
        {
            /* This pickled value must have been pickled on a system
             * with a larger word length. Return a dictionary so that
             * the calling function can created an FpBinaryLarge instance.
             *
             * Just using the original dict format.
             */
            Py_INCREF(dict);
            result = dict;
        }

        Py_DECREF(int_bits);
        Py_DECREF(frac_bits);
        Py_DECREF(scaled_value);
    }
    else
    {
        PyErr_SetString(PyExc_KeyError,
                        "Pickle dict didn't have a required key.");
    }

    return result;
}

bool
FpBinarySmall_UpdatePickleDict(PyObject *self, PyObject *dict)
{
    bool result = false;
    FpBinarySmallObject *cast_self = (FpBinarySmallObject *)self;
    PyObject *int_bits, *frac_bits, *scaled_value, *is_signed;

    if (!dict)
    {
        return false;
    }

    int_bits = fp_int_as_pylong(cast_self->int_bits);
    frac_bits = fp_int_as_pylong(cast_self->frac_bits);
    scaled_value = fp_uint_as_pylong(cast_self->scaled_value);
    is_signed = cast_self->is_signed ? Py_True : Py_False;

    if (int_bits && frac_bits && scaled_value)
    {
        PyDict_SetItemString(dict, "ib", int_bits);
        PyDict_SetItemString(dict, "fb", frac_bits);
        PyDict_SetItemString(dict, "sv", scaled_value);
        PyDict_SetItemString(dict, "sgn", is_signed);
        PyDict_SetItemString(dict, "bid", fp_small_type_id);

        result = true;
    }

    Py_XDECREF(int_bits);
    Py_XDECREF(frac_bits);
    Py_XDECREF(scaled_value);
    // Didn't create a new bool, no need to decref is_signed

    return result;
}

static PyMethodDef fpbinarysmall_methods[] = {
    {"resize", (PyCFunction)fpbinarysmall_resize, METH_VARARGS | METH_KEYWORDS,
     resize_doc},
    {"str_ex", (PyCFunction)fpbinarysmall_str_ex, METH_NOARGS, str_ex_doc},
    {"bits_to_signed", (PyCFunction)fpbinarysmall_bits_to_signed, METH_NOARGS,
     bits_to_signed_doc},
    {"__copy__", (PyCFunction)fpbinarysmall_copy, METH_NOARGS, copy_doc},

    {"__getitem__", (PyCFunction)fpbinarysmall_subscript, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinarysmall_getsetters[] = {
    {"format", (getter)fpbinarysmall_getformat, NULL, format_doc, NULL},
    {"is_signed", (getter)fpbinarysmall_is_signed, NULL, is_signed_doc, NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinarysmall_as_number = {
    .nb_add = (binaryfunc)fpbinarysmall_add,
    .nb_subtract = (binaryfunc)fpbinarysmall_subtract,
    .nb_multiply = (binaryfunc)fpbinarysmall_multiply,
    .nb_true_divide = (binaryfunc)fpbinarysmall_divide,
    .nb_negative = (unaryfunc)fpbinarysmall_negative,
    .nb_int = (unaryfunc)fpbinarysmall_int,
    .nb_index = (unaryfunc)fpbinarysmall_index,

#if PY_MAJOR_VERSION < 3
    .nb_divide = (binaryfunc)fpbinarysmall_divide,
    .nb_long = (unaryfunc)fpbinarysmall_long,
#endif

    .nb_float = (unaryfunc)fpbinarysmall_float,
    .nb_absolute = (unaryfunc)fpbinarysmall_abs,
    .nb_lshift = (binaryfunc)fpbinarysmall_lshift,
    .nb_rshift = (binaryfunc)fpbinarysmall_rshift,
    .nb_nonzero = (inquiry)fpbinarysmall_nonzero,
};

static PySequenceMethods fpbinarysmall_as_sequence = {
    .sq_length = (lenfunc)fpbinarysmall_sq_length,
    .sq_item = (ssizeargfunc)fpbinarysmall_sq_item,

#if PY_MAJOR_VERSION < 3

    .sq_slice = (ssizessizeargfunc)fpbinarysmall_sq_slice,

#endif
};

static PyMappingMethods fpbinarysmall_as_mapping = {
    .mp_length = fpbinarysmall_sq_length,
    .mp_subscript = (binaryfunc)fpbinarysmall_subscript,
};

PyTypeObject FpBinary_SmallType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinarySmall",
    .tp_doc = fpbinarysmall_doc,
    .tp_basicsize = sizeof(FpBinarySmallObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinarysmall_methods,
    .tp_getset = fpbinarysmall_getsetters,
    .tp_as_number = &fpbinarysmall_as_number,
    .tp_as_sequence = &fpbinarysmall_as_sequence,
    .tp_as_mapping = &fpbinarysmall_as_mapping,
    .tp_new = (newfunc)fpbinarysmall_new,
    .tp_dealloc = (destructor)fpbinarysmall_dealloc,
    .tp_str = fpbinarysmall_str,
    .tp_repr = fpbinarysmall_str,
    .tp_richcompare = fpbinarysmall_richcompare,
};

fpbinary_private_iface_t FpBinary_SmallPrvIface = {
    .get_total_bits = fpbinarysmall_get_total_bits,
    .is_signed = fpsmall_is_signed,
    .resize = (PyCFunctionWithKeywords)fpbinarysmall_resize,
    .str_ex = fpbinarysmall_str_ex,
    .to_signed = fpbinarysmall_to_signed,
    .bits_to_signed = (PyCFunction)fpbinarysmall_bits_to_signed,
    .copy = (PyCFunction)fpbinarysmall_copy,
    .fp_getformat = fpbinarysmall_getformat,

    .fp_from_double = FpBinarySmall_FromDouble,
    .fp_from_bits_pylong = FpBinarySmall_FromBitsPylong,

    .build_pickle_dict = FpBinarySmall_UpdatePickleDict,

    .getitem = fpbinarysmall_subscript,
};
