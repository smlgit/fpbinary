/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinaryLarge object (not meant for Python users, FpBinary wraps it).
 *
 * PyLong objects are utilised to provide for arbitrary length fixed point
 * values. A real number is represented by the scaled_value field. This is the
 * real value * 2**frac_bits. scaled_value can then be used for math operations
 * by using integer arithmetic.
 *
 * All math operations result in a new object with the int_bits and frac_bits
 * fields expanded to ensure no overflow. The resize method can be used by
 * the user to reduce (or increase for some reason) the number of bits.
 * Multiple overflow and rounding modes are available (see OverflowEnumType
 * and RoundingEnumType).
 *
 *****************************************************************************/

#include "fpbinarylarge.h"
#include "fpbinaryglobaldoc.h"
#include <math.h>

/*
 * Often used values.
 */
static PyObject *fpbinarylarge_minus_one;

static FP_UINT_TYPE
get_total_bits_uint(PyObject *int_bits, PyObject *frac_bits)
{
    PyObject *total = FP_NUM_METHOD(int_bits, nb_add)(int_bits, frac_bits);
    FP_UINT_TYPE result = pylong_as_fp_uint(total);

    Py_DECREF(total);
    return result;
}

static PyObject *
get_total_bits_mask(PyObject *total_bits)
{
    /*
     * total bits mask is (1 << total_bits) - 1
     */
    PyObject *result = FP_NUM_METHOD(py_one, nb_lshift)(py_one, total_bits);
    FP_NUM_BIN_OP_INPLACE(result, py_one, nb_subtract);
    return result;
}

static PyObject *
get_lsb_mask(PyObject *num_lsb_bits)
{
    if (PyLong_AsLong(num_lsb_bits) == 0)
    {
        return py_zero;
    }

    return get_total_bits_mask(num_lsb_bits);
}

static PyObject *
get_sign_bit(PyObject *total_bits)
{
    /*
     * Sign bit is (1 << (total_bits - 1) )
     */
    PyObject *result = FP_NUM_METHOD(py_one, nb_lshift)(py_one, total_bits);
    FP_NUM_BIN_OP_INPLACE(result, py_one, nb_rshift);
    return result;
}

/*
* Returns a PyLong that gives the largest value representable (after conversion
* to scaled
* int representation) given total_bits.
*/
static PyObject *
get_max_scaled_value(PyObject *total_bits, bool is_signed)
{
    PyObject *result = get_total_bits_mask(total_bits);

    if (is_signed)
    {
        FP_NUM_BIN_OP_INPLACE(result, py_one, nb_rshift);
        return result;
    }

    return result;
}

/*
* Returns a PyLong that gives the smallest value representable (after conversion
* to scaled
* int representation) given total_bits.
*/
static PyObject *
get_min_scaled_value(PyObject *total_bits, bool is_signed)
{
    if (is_signed)
    {
        /*
         * For signed numbers, the min value is the unsigned value of the "sign
         * bit"
         * negated.
         */
        PyObject *result = get_sign_bit(total_bits);
        FP_NUM_BIN_OP_INPLACE(result, py_minus_one, nb_multiply);
        return result;
    }
    else
    {
        Py_INCREF(py_zero);
        return py_zero;
    }
}

static inline PyObject *
apply_overflow_wrap(PyObject *value, PyObject *min_value, PyObject *max_value,
                    PyObject *sign_bit, bool is_signed)
{
    /* If we overflowed into the negative range, add the min value
     * to the magnitude-masked value. If we overflowed into the
     * positive range, just mask with the magnitude bits only. */

    if (is_signed)
    {
        PyObject *sign_bit_value =
            FP_NUM_METHOD(value, nb_and)(value, sign_bit);

        if (sign_bit_value != NULL)
        {

            if (FpBinary_TpCompare(sign_bit_value, py_zero) != 0)
            {
                /*
                 * Wrapping into a negative value:
                 *
                 * return min_value + (value & max_value);
                 */
                PyObject *result =
                    FP_NUM_METHOD(value, nb_and)(value, max_value);
                FP_NUM_BIN_OP_INPLACE(result, min_value, nb_add);

                return result;
            }

            Py_DECREF(sign_bit_value);

            /* If wrapped into positive value, just need to mask out the
             * magnitude bits (same as unsigned).
             */
        }
    }

    return FP_NUM_METHOD(value, nb_and)(value, max_value);
}

/*
 * Sets the pointers in self to point to scaled_value, int_bits and frac_bits.
 * WILL increment the reference counters for these objects and WILL decrement
 * the reference counters for the old objects (if they exist).
 * Also sets the is_signed flag.
 */
static inline void
set_object_fields(FpBinaryLargeObject *self, PyObject *scaled_value,
                  PyObject *int_bits, PyObject *frac_bits, bool is_signed)
{
    FP_ASSIGN_PY_FIELD(self, scaled_value, scaled_value);
    FP_ASSIGN_PY_FIELD(self, int_bits, int_bits);
    FP_ASSIGN_PY_FIELD(self, frac_bits, frac_bits);
    self->is_signed = is_signed;
}

/*
 * Any PyObject pointers in from_obj will be directly applied to to_obj
 * and WILL have their ref counters incremented (and the old pointers
 * decremented).
 */
static void
copy_fields(FpBinaryLargeObject *from_obj, FpBinaryLargeObject *to_obj)
{
    set_object_fields(to_obj, from_obj->scaled_value, from_obj->int_bits,
                      from_obj->frac_bits, from_obj->is_signed);
}

/*
 * Will check the fields in obj for overflow and will modify act on the
 * fields of obj or raise an exception depending on overflow_mode.
 * If there was no exception raised, non-zero is returned.
 * Otherwise, 0 is returned.
 */
static int
check_overflow(FpBinaryLargeObject *self, fp_overflow_mode_t overflow_mode)
{
    bool result = true;
    PyObject *new_scaled_value = NULL;
    PyObject *min_value, *max_value;
    PyObject *total_bits =
        FP_NUM_METHOD(self->int_bits, nb_add)(self->int_bits, self->frac_bits);
    PyObject *sign_bit = get_sign_bit(total_bits);
    int value_compare;

    min_value = get_min_scaled_value(total_bits, self->is_signed);
    max_value = get_max_scaled_value(total_bits, self->is_signed);

    value_compare = FpBinary_TpCompare(self->scaled_value, max_value);

    if (value_compare > 0)
    {
        if (overflow_mode == OVERFLOW_WRAP)
        {
            new_scaled_value =
                apply_overflow_wrap(self->scaled_value, min_value, max_value,
                                    sign_bit, self->is_signed);
        }
        else if (overflow_mode == OVERFLOW_SAT)
        {
            Py_INCREF(max_value);
            new_scaled_value = max_value;
        }
        else
        {
            PyErr_SetString(FpBinaryOverflowException,
                            "Fixed point resize overflow.");
            result = false;
        }
    }
    else
    {
        value_compare = FpBinary_TpCompare(self->scaled_value, min_value);
        if (value_compare < 0)
        {
            if (overflow_mode == OVERFLOW_WRAP)
            {
                new_scaled_value =
                    apply_overflow_wrap(self->scaled_value, min_value,
                                        max_value, sign_bit, self->is_signed);
            }
            else if (overflow_mode == OVERFLOW_SAT)
            {
                Py_INCREF(min_value);
                new_scaled_value = min_value;
            }
            else
            {
                PyErr_SetString(FpBinaryOverflowException,
                                "Fixed point resize overflow.");
                result = false;
            }
        }
        else
        {
            Py_INCREF(self->scaled_value);
            new_scaled_value = self->scaled_value;
        }
    }

    if (result)
    {
        set_object_fields(self, new_scaled_value, self->int_bits,
                          self->frac_bits, self->is_signed);
        Py_DECREF(new_scaled_value);
    }

    Py_DECREF(min_value);
    Py_DECREF(max_value);
    Py_DECREF(total_bits);
    Py_DECREF(sign_bit);

    return result;
}

/*
 * Will convert the passed PyFloat to a fixed point object and apply
 * the result to output_obj.
 * Returns 0 if (there was an overflow AND round_mode is OVERFLOW_EXCEP) OR
 *              (the passed value can't be represented on this CPU using a
 * FP_INT_TYPE).
 * In this case, the PyErr_SetString WON"T BE SET. Callers must decide
 * if they care.
 * Otherwise, non zero is returned.
 */
static int
build_from_pyfloat(PyObject *value, PyObject *int_bits, PyObject *frac_bits,
                   bool is_signed, fp_overflow_mode_t overflow_mode,
                   fp_round_mode_t round_mode, FpBinaryLargeObject *output_obj)
{
    PyObject *py_scaled_value_long = NULL;

    double dbl_scaled_value =
        ldexp(PyFloat_AsDouble(value), PyLong_AsLong(frac_bits));

    if (round_mode == ROUNDING_NEAR_POS_INF)
    {
        dbl_scaled_value += 0.5;
    }

    dbl_scaled_value = floor(dbl_scaled_value);
    py_scaled_value_long = PyLong_FromDouble(dbl_scaled_value);

    set_object_fields(output_obj, py_scaled_value_long, int_bits, frac_bits,
                      is_signed);

    Py_DECREF(py_scaled_value_long);

    return check_overflow(output_obj, overflow_mode);
}

static double
fpbinarylarge_to_double(FpBinaryLargeObject *obj)
{
    FpBinaryLargeObject *cast_obj = (FpBinaryLargeObject *)obj;

    return ldexp(PyLong_AsDouble(cast_obj->scaled_value),
                 -PyLong_AsLong(cast_obj->frac_bits));
}

/*
 * Will resize self to the format specified by new_int_bits and
 * new_frac_bits and take action based on overflow_mode and
 * round_mode.
 * If overflow_mode is OVERFLOW_EXCEP, an exception string will
 * be written and 0 will be returned. Otherwise, non-zero is
 * returned.
 */
static int
resize_object(FpBinaryLargeObject *self, PyObject *new_int_bits,
              PyObject *new_frac_bits, fp_overflow_mode_t overflow_mode,
              fp_round_mode_t round_mode)
{
    PyObject *new_scaled_value = NULL;
    PyObject *right_shifts = FP_NUM_METHOD(self->frac_bits, nb_subtract)(
        self->frac_bits, new_frac_bits);

    /* Rounding */
    if (FpBinary_TpCompare(right_shifts, py_zero) > 0)
    {
        if (round_mode == ROUNDING_DIRECT_ZERO)
        {
            /* This is "floor" functionality. So if we are positive, truncation
             * works.
             * If we are negative, need to add 1 to the new lowest int bit if
             * the old frac bits are non zero, and then truncate.
             */
            PyObject *inc_value = py_zero;
            PyObject *initial_shifted_val =
                FP_NUM_METHOD(self->scaled_value, nb_rshift)(self->scaled_value,
                                                             right_shifts);

            if (FpBinary_TpCompare(self->scaled_value, py_zero) < 0)
            {
                PyObject *frac_bits_mask = get_lsb_mask(right_shifts);
                PyObject *frac_bits = FP_NUM_METHOD(self->scaled_value, nb_and)(
                    self->scaled_value, frac_bits_mask);
                if (FpBinary_TpCompare(frac_bits, py_zero) != 0)
                {
                    inc_value = py_one;
                }

                Py_DECREF(frac_bits_mask);
                Py_DECREF(frac_bits);
            }

            new_scaled_value = FP_NUM_METHOD(initial_shifted_val, nb_add)(
                initial_shifted_val, inc_value);

            Py_DECREF(initial_shifted_val);
        }
        else if (round_mode == ROUNDING_NEAR_POS_INF ||
                 round_mode == ROUNDING_NEAR_ZERO ||
                 round_mode == ROUNDING_NEAR_EVEN)
        {
            /*
             * "Near" rounding modes. This basically means we need to add
             * "0.5" to our value, conditioned on the specific near type.
             */

            bool chopped_bits_are_nonzero = false;
            bool is_negative =
                (FpBinary_TpCompare(self->scaled_value, py_zero) < 0);
            PyObject *new_lsb_bit_pos, *inc_value;

            new_lsb_bit_pos =
                FP_NUM_METHOD(py_one, nb_lshift)(py_one, right_shifts);
            inc_value = FP_NUM_METHOD(new_lsb_bit_pos,
                                      nb_rshift)(new_lsb_bit_pos, py_one);

            /*
             * Work out if our lower bits are non zero.
             */
            if (FpBinary_TpCompare(right_shifts, py_one) > 0)
            {
                PyObject *frac_bits_mask = get_lsb_mask(right_shifts);
                PyObject *frac_bits = FP_NUM_METHOD(self->scaled_value, nb_and)(
                    self->scaled_value, frac_bits_mask);

                /* Actually interested in the LSBs minus the most significant of
                 * them, so cheap way from here is to compare with our inc value
                 * (which should be the most significant bit only).
                 */
                if (FpBinary_TpCompare(frac_bits, inc_value) > 0)
                {
                    chopped_bits_are_nonzero = true;
                }

                Py_DECREF(frac_bits_mask);
                Py_DECREF(frac_bits);
            }

            if (round_mode == ROUNDING_NEAR_EVEN)
            {
                PyObject *new_lsb = FP_NUM_METHOD(self->scaled_value, nb_and)(
                    self->scaled_value, new_lsb_bit_pos);

                if (chopped_bits_are_nonzero ||
                    FpBinary_TpCompare(new_lsb, py_zero) != 0)
                {
                    new_scaled_value =
                        FP_NUM_METHOD(self->scaled_value,
                                      nb_add)(self->scaled_value, inc_value);
                }
                else
                {
                    new_scaled_value =
                        FP_NUM_METHOD(self->scaled_value,
                                      nb_add)(self->scaled_value, py_zero);
                }

                Py_DECREF(new_lsb);
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
                if (is_negative || chopped_bits_are_nonzero)
                {
                    new_scaled_value =
                        FP_NUM_METHOD(self->scaled_value,
                                      nb_add)(self->scaled_value, inc_value);
                }
                else
                {
                    new_scaled_value =
                        FP_NUM_METHOD(self->scaled_value,
                                      nb_add)(self->scaled_value, py_zero);
                }
            }
            else if (round_mode == ROUNDING_NEAR_POS_INF)
            {
                /* Add "new value 0.5" to the old sized value and then truncate
                 * Here, we are doing:
                 *     new_scaled_value = scaled_value + (1 << (right_shifts -
                 * 1) )
                 */
                new_scaled_value = FP_NUM_METHOD(self->scaled_value, nb_add)(
                    self->scaled_value, inc_value);
            }

            /*
             * And finally do the after-add truncation.
             */
            FP_NUM_BIN_OP_INPLACE(new_scaled_value, right_shifts, nb_rshift);

            Py_DECREF(new_lsb_bit_pos);
            Py_DECREF(inc_value);
        }
        else
        {
            /* Default to truncate (ROUNDING_DIRECT_NEG_INF) */
            new_scaled_value = FP_NUM_METHOD(self->scaled_value, nb_rshift)(
                self->scaled_value, right_shifts);
        }
    }
    else
    {
        PyObject *lshifts =
            FP_NUM_METHOD(right_shifts, nb_negative)(right_shifts);
        new_scaled_value = FP_NUM_METHOD(self->scaled_value, nb_lshift)(
            self->scaled_value, lshifts);
        Py_DECREF(lshifts);
    }

    set_object_fields(self, new_scaled_value, new_int_bits, new_frac_bits,
                      self->is_signed);

    Py_DECREF(new_scaled_value);
    Py_DECREF(right_shifts);

    return check_overflow(self, overflow_mode);
}

static FpBinaryLargeObject *
fpbinarylarge_create_mem(PyTypeObject *type)
{
    FpBinaryLargeObject *self = (FpBinaryLargeObject *)type->tp_alloc(type, 0);
    if (self)
    {
        self->fpbinary_base.private_iface = &FpBinary_LargePrvIface;
        set_object_fields(self, py_zero, py_one, py_zero, true);
    }

    return self;
}

PyDoc_STRVAR(fpbinarylarge_doc,
             "_FpBinaryLarge(int_bits=1, frac_bits=0, signed=True, value=0.0, "
             "bit_field=None, format_inst=None)\n"
             "\n"
             "Represents a real number using fixed point math and structure.\n"
             "NOTE: This object is not intended to be used directly!\n");
static PyObject *
fpbinarylarge_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    FpBinaryLargeObject *self = NULL;
    PyObject *bit_field = NULL, *format_instance = NULL;
    long int_bits = 1, frac_bits = 0;
    double value = 0.0;
    bool is_signed = true;

    if (!fp_binary_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                    &is_signed, &value, &bit_field,
                                    &format_instance))
    {
        return NULL;
    }

    if (format_instance)
    {
        if (!FpBinaryLarge_Check(format_instance))
        {
            PyErr_SetString(
                PyExc_TypeError,
                "format_inst must be an instance of FpBinaryLarge.");
            return NULL;
        }
    }

    if (format_instance)
    {
        int_bits = pylong_as_fp_int(
            ((FpBinaryLargeObject *)format_instance)->int_bits);
        frac_bits = pylong_as_fp_int(
            ((FpBinaryLargeObject *)format_instance)->frac_bits);
    }

    if (bit_field)
    {
        self = (FpBinaryLargeObject *)FpBinaryLarge_FromBitsPylong(
            bit_field, int_bits, frac_bits, is_signed);
    }
    else
    {
        self = (FpBinaryLargeObject *)FpBinaryLarge_FromDouble(
            value, int_bits, frac_bits, is_signed, OVERFLOW_SAT,
            ROUNDING_NEAR_POS_INF);
    }

    return (PyObject *)self;
}

/*
 * See copy_doc
 */
static PyObject *
fpbinarylarge_copy(FpBinaryLargeObject *self, PyObject *args)
{
    FpBinaryLargeObject *new_obj =
        fpbinarylarge_create_mem(&FpBinary_LargeType);
    if (new_obj)
    {
        copy_fields(self, new_obj);
    }

    return (PyObject *)new_obj;
}

/*
 * Returns a new FpBinaryLarge object where the value is the same
 * as obj but:
 *     if obj is unsigned, an extra bit is added to int_bits.
 *     if obj is signed, no change to value or format.
 */
static PyObject *
fpbinarylarge_to_signed(PyObject *obj, PyObject *args)
{
    FpBinaryLargeObject *result = NULL;
    FpBinaryLargeObject *cast_obj = (FpBinaryLargeObject *)obj;
    PyObject *new_int_bits = NULL;

    if (!FpBinaryLarge_Check(obj))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    if (cast_obj->is_signed)
    {
        return fpbinarylarge_copy(cast_obj, NULL);
    }

    /* Input is an unsigned FpBinarySmall object. */

    result = fpbinarylarge_create_mem(&FpBinary_LargeType);
    new_int_bits =
        FP_NUM_METHOD(cast_obj->int_bits, nb_add)(cast_obj->int_bits, py_one);
    set_object_fields(result, cast_obj->scaled_value, new_int_bits,
                      cast_obj->frac_bits, true);

    return (PyObject *)result;
}

/*
 * Convenience function to make sure the operands of a two operand operation
 * are FpBinaryLargeObject instances and of the same signed type.
 */
static bool
check_binary_ops(PyObject *op1, PyObject *op2)
{
    if (!FpBinaryLarge_Check(op1) || !FpBinaryLarge_Check(op2))
    {
        return false;
    }

    if (((FpBinaryLargeObject *)op1)->is_signed !=
        ((FpBinaryLargeObject *)op2)->is_signed)
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
make_binary_ops_same_format(PyObject *op1, PyObject *op2,
                            FpBinaryLargeObject **output_op1,
                            FpBinaryLargeObject **output_op2)
{
    FpBinaryLargeObject *cast_op1 = (FpBinaryLargeObject *)op1;
    FpBinaryLargeObject *cast_op2 = (FpBinaryLargeObject *)op2;
    PyObject *new_int_bits;

    int compare_val_int =
        FpBinary_TpCompare(cast_op1->int_bits, cast_op2->int_bits);
    int compare_val_frac =
        FpBinary_TpCompare(cast_op1->frac_bits, cast_op2->frac_bits);

    *output_op1 = fpbinarylarge_create_mem(&FpBinary_LargeType);
    *output_op2 = fpbinarylarge_create_mem(&FpBinary_LargeType);

    if (compare_val_int > 0)
    {
        new_int_bits = cast_op1->int_bits;
    }
    else
    {
        new_int_bits = cast_op2->int_bits;
    }

    /* Frac bits. */
    if (compare_val_frac > 0)
    {
        PyObject *frac_bits_diff =
            FP_NUM_METHOD(cast_op1->frac_bits, nb_subtract)(
                cast_op1->frac_bits, cast_op2->frac_bits);
        PyObject *new_scaled_value =
            FP_NUM_METHOD(cast_op2->scaled_value,
                          nb_lshift)(cast_op2->scaled_value, frac_bits_diff);

        set_object_fields(*output_op2, new_scaled_value, new_int_bits,
                          cast_op1->frac_bits, cast_op2->is_signed);
        set_object_fields(*output_op1, cast_op1->scaled_value, new_int_bits,
                          cast_op1->frac_bits, cast_op1->is_signed);

        Py_DECREF(frac_bits_diff);
        Py_DECREF(new_scaled_value);
    }
    else if (compare_val_frac < 0)
    {
        PyObject *frac_bits_diff =
            FP_NUM_METHOD(cast_op2->frac_bits, nb_subtract)(
                cast_op2->frac_bits, cast_op1->frac_bits);
        PyObject *new_scaled_value =
            FP_NUM_METHOD(cast_op1->scaled_value,
                          nb_lshift)(cast_op1->scaled_value, frac_bits_diff);

        set_object_fields(*output_op1, new_scaled_value, new_int_bits,
                          cast_op2->frac_bits, cast_op1->is_signed);
        set_object_fields(*output_op2, cast_op2->scaled_value, new_int_bits,
                          cast_op2->frac_bits, cast_op2->is_signed);

        Py_DECREF(frac_bits_diff);
        Py_DECREF(new_scaled_value);
    }
    else
    {
        set_object_fields(*output_op1, cast_op1->scaled_value, new_int_bits,
                          cast_op1->frac_bits, cast_op1->is_signed);
        set_object_fields(*output_op2, cast_op2->scaled_value, new_int_bits,
                          cast_op2->frac_bits, cast_op2->is_signed);
    }
}

/*
 * See resize_doc
 */
static PyObject *
fpbinarylarge_resize(FpBinaryLargeObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *format;
    PyObject *result = NULL;
    int overflow_mode = OVERFLOW_WRAP;
    int round_mode = ROUNDING_DIRECT_NEG_INF;
    static char *kwlist[] = {"format", "overflow_mode", "round_mode", NULL};
    PyObject *new_int_bits = self->int_bits, *new_frac_bits = self->frac_bits;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ii", kwlist, &format,
                                     &overflow_mode, &round_mode))
        return NULL;

    /* FP format is defined by a python tuple: (int_bits, frac_bits) */
    if (PyTuple_Check(format))
    {
        if (!extract_fp_format_from_tuple(format, &new_int_bits,
                                          &new_frac_bits))
        {
            return NULL;
        }
    }
    else if (FpBinaryLarge_Check(format))
    {
        /* Format is an instance of FpBinaryLarge, so use its format */
        Py_INCREF(((FpBinaryLargeObject *)format)->int_bits);
        Py_INCREF(((FpBinaryLargeObject *)format)->frac_bits);
        new_int_bits = ((FpBinaryLargeObject *)format)->int_bits;
        new_frac_bits = ((FpBinaryLargeObject *)format)->frac_bits;
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
        result = (PyObject *)self;
    }

    Py_DECREF(new_int_bits);
    Py_DECREF(new_frac_bits);
    return result;
}

/*
 * See bits_to_signed_doc
 */
static PyObject *
fpbinarylarge_bits_to_signed(FpBinaryLargeObject *self, PyObject *args)
{
    PyObject *result = NULL;

    if (self->is_signed)
    {
        Py_INCREF(self->scaled_value);
        result = self->scaled_value;
    }
    else
    {
        PyObject *total_bits = FP_NUM_METHOD(self->int_bits, nb_add)(
            self->int_bits, self->frac_bits);
        PyObject *sign_bit = get_sign_bit(total_bits);

        if (FpBinary_TpCompare(self->scaled_value, sign_bit) < 0)
        {
            Py_INCREF(self->scaled_value);
            result = self->scaled_value;
        }
        else
        {
            /* If scaled_value is >= sign bit, it must have its sign bit set.
             * This means we
             * have to interpret the value as negative, but with the "magnitude"
             * bits
             * unchanged. We can do this by subtracting the next highest sign
             * bit value
             * from the value.
             */
            FP_NUM_BIN_OP_INPLACE(sign_bit, py_one, nb_lshift);
            result = FP_NUM_METHOD(self->scaled_value,
                                   nb_subtract)(self->scaled_value, sign_bit);
        }

        Py_DECREF(total_bits);
        Py_DECREF(sign_bit);
    }

    return result;
}

/*
 * Creating indexes from a fixed point number number is just returning
 * an unsigned int from the bits in the number.
 */
static PyObject *
fpbinarylarge_index(PyObject *self)
{
    PyObject *result = NULL;
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;

    /* Just mask scaled_value - this should convert the bits to an unsigned
     * value. */
    PyObject *total_bits = FP_NUM_METHOD(cast_self->int_bits, nb_add)(
        cast_self->int_bits, cast_self->frac_bits);
    PyObject *mask = get_total_bits_mask(total_bits);

    result = FP_NUM_METHOD(cast_self->scaled_value,
                           nb_and)(cast_self->scaled_value, mask);

    Py_DECREF(total_bits);
    Py_DECREF(mask);
    return result;
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
fpbinarylarge_add(PyObject *op1, PyObject *op2)
{
    FpBinaryLargeObject *cast_op1, *cast_op2;
    FpBinaryLargeObject *result;
    PyObject *new_scaled_value = NULL, *new_int_bits = NULL;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /* Add requires the fractional bits to be lined up */
    make_binary_ops_same_format(op1, op2, &cast_op1, &cast_op2);

    new_scaled_value = FP_NUM_METHOD(cast_op1->scaled_value, nb_add)(
        cast_op1->scaled_value, cast_op2->scaled_value);
    new_int_bits =
        FP_NUM_METHOD(cast_op1->int_bits, nb_add)(cast_op1->int_bits, py_one);

    result =
        (FpBinaryLargeObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    set_object_fields(result, new_scaled_value, new_int_bits,
                      cast_op1->frac_bits, cast_op1->is_signed);

    Py_DECREF(new_scaled_value);
    Py_DECREF(new_int_bits);
    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarylarge_subtract(PyObject *op1, PyObject *op2)
{
    FpBinaryLargeObject *cast_op1, *cast_op2;
    FpBinaryLargeObject *result;
    PyObject *new_scaled_value = NULL, *new_int_bits = NULL;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /* Add requires the fractional bits to be lined up */
    make_binary_ops_same_format(op1, op2, &cast_op1, &cast_op2);

    new_scaled_value = FP_NUM_METHOD(cast_op1->scaled_value, nb_subtract)(
        cast_op1->scaled_value, cast_op2->scaled_value);
    new_int_bits =
        FP_NUM_METHOD(cast_op1->int_bits, nb_add)(cast_op1->int_bits, py_one);

    result =
        (FpBinaryLargeObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    set_object_fields(result, new_scaled_value, new_int_bits,
                      cast_op1->frac_bits, cast_op1->is_signed);

    /* Need to deal with negative numbers and wrapping if we are unsigned type.
     */
    if (!result->is_signed)
    {
        check_overflow(result, OVERFLOW_WRAP);
    }

    Py_DECREF(new_scaled_value);
    Py_DECREF(new_int_bits);
    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarylarge_multiply(PyObject *op1, PyObject *op2)
{
    FpBinaryLargeObject *cast_op1 = (FpBinaryLargeObject *)op1;
    FpBinaryLargeObject *cast_op2 = (FpBinaryLargeObject *)op2;
    PyObject *new_scaled_value = NULL, *new_int_bits = NULL,
             *new_frac_bits = NULL;
    FpBinaryLargeObject *result = NULL;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    /* Multiply produces the addition of the int/frac bit format */

    new_int_bits = FP_NUM_METHOD(cast_op1->int_bits, nb_add)(
        cast_op1->int_bits, cast_op2->int_bits);
    new_frac_bits = FP_NUM_METHOD(cast_op1->frac_bits, nb_add)(
        cast_op1->frac_bits, cast_op2->frac_bits);

    /* Do multiply */
    new_scaled_value = FP_NUM_METHOD(cast_op1->scaled_value, nb_multiply)(
        cast_op1->scaled_value, cast_op2->scaled_value);

    result =
        (FpBinaryLargeObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    set_object_fields(result, new_scaled_value, new_int_bits, new_frac_bits,
                      cast_op1->is_signed);

    Py_DECREF(new_scaled_value);
    Py_DECREF(new_int_bits);
    Py_DECREF(new_frac_bits);

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarylarge_divide(PyObject *op1, PyObject *op2)
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
     * numerator by (denom int bits + denom_frac_bits) and then divide by the
     * untouched denominator.
     *
     *
     * The standard VHDL library appears to do "towards zero" truncation on
     * divide. This is the same as C. So this is what we will do. Unfortunately,
     * the python long only does a floor divide. So we need to convert negative
     * numbers to positive, do our division, and then convert back.
     */

    FpBinaryLargeObject *cast_op1 = (FpBinaryLargeObject *)op1;
    FpBinaryLargeObject *cast_op2 = (FpBinaryLargeObject *)op2;
    PyObject *op1_abs, *op2_abs;
    PyObject *extra_scale = NULL, *result_scaled_value = NULL,
             *result_int_bits = NULL, *result_frac_bits = NULL;
    FpBinaryLargeObject *result = NULL;
    bool op1_neg, op2_neg;

    if (!check_binary_ops(op1, op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    op1_neg = FpBinary_TpCompare(cast_op1->scaled_value, py_zero) < 0;
    op2_neg = FpBinary_TpCompare(cast_op2->scaled_value, py_zero) < 0;

    /* Need positive ints so our divide turns out to be toward zero truncated */
    op1_abs = FP_NUM_METHOD(cast_op1->scaled_value,
                            nb_absolute)(cast_op1->scaled_value);
    op2_abs = FP_NUM_METHOD(cast_op2->scaled_value,
                            nb_absolute)(cast_op2->scaled_value);

    extra_scale = FP_NUM_METHOD(cast_op2->int_bits, nb_add)(
        cast_op2->int_bits, cast_op2->frac_bits);

    result_scaled_value =
        FP_NUM_METHOD(op1_abs, nb_lshift)(op1_abs, extra_scale);
    FP_NUM_BIN_OP_INPLACE(result_scaled_value, op2_abs, nb_floor_divide);

    /* Now convert back to negative if needed */
    if (op1_neg != op2_neg)
    {
        FP_NUM_UNI_OP_INPLACE(result_scaled_value, nb_negative);
    }

    result_int_bits = FP_NUM_METHOD(cast_op1->int_bits, nb_add)(
        cast_op1->int_bits, cast_op2->frac_bits);
    if (cast_op1->is_signed)
    {
        FP_NUM_BIN_OP_INPLACE(result_int_bits, py_one, nb_add);
    }
    result_frac_bits = FP_NUM_METHOD(cast_op1->frac_bits, nb_add)(
        cast_op1->frac_bits, cast_op2->int_bits);

    result = fpbinarylarge_create_mem(&FpBinary_LargeType);
    set_object_fields(result, result_scaled_value, result_int_bits,
                      result_frac_bits, cast_op1->is_signed);

    Py_DECREF(op1_abs);
    Py_DECREF(op2_abs);
    Py_DECREF(extra_scale);
    Py_DECREF(result_scaled_value);
    Py_DECREF(result_int_bits);
    Py_DECREF(result_frac_bits);

    return (PyObject *)result;
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarylarge_negative(PyObject *self)
{
    return fpbinarylarge_multiply(self, fpbinarylarge_minus_one);
}

static PyObject *
fpbinarylarge_long(PyObject *self)
{
    PyObject *result = NULL;

    /*
     * Just resize to just the int bits with towards zero rounding
     * and return the scaled value;
     */
    FpBinaryLargeObject *resized = (FpBinaryLargeObject *)fpbinarylarge_copy(
        (FpBinaryLargeObject *)self, NULL);
    resize_object(resized, resized->int_bits, py_zero, OVERFLOW_WRAP,
                  ROUNDING_DIRECT_ZERO);

    Py_INCREF(resized->scaled_value);
    result = resized->scaled_value;
    Py_DECREF(resized);

    return result;
}

static PyObject *
fpbinarylarge_int(PyObject *self)
{
    return fpbinarylarge_long(self);
}

static PyObject *
fpbinarylarge_float(PyObject *self)
{
    return PyFloat_FromDouble(
        fpbinarylarge_to_double((FpBinaryLargeObject *)self));
}

/*
 * See fpbinaryobject_doc for official requirements.
 */
static PyObject *
fpbinarylarge_abs(PyObject *self)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
    PyObject *copied = fpbinarylarge_copy(cast_self, NULL);

    if (FpBinary_TpCompare(((FpBinaryLargeObject *)copied)->scaled_value,
                           py_zero) < 0)
    {
        /* Negative -> abs = self * -1. */
        FP_NUM_UNI_OP_INPLACE(copied, nb_negative);
    }

    return copied;
}

static PyObject *
fpbinarylarge_lshift(PyObject *self, PyObject *lshift)
{
    FpBinaryLargeObject *result = NULL;

    if (!PyLong_Check(lshift))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    if (lshift)
    {
        FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
        PyObject *masked_shifted_value = NULL;
        PyObject *shifted_value_sign = NULL;
        PyObject *total_bits = FP_NUM_METHOD(cast_self->int_bits, nb_add)(
            cast_self->int_bits, cast_self->frac_bits);
        PyObject *sign_bit = get_sign_bit(total_bits);
        PyObject *mask = get_total_bits_mask(total_bits);

        PyObject *shifted_value =
            FP_NUM_METHOD(cast_self->scaled_value,
                          nb_lshift)(cast_self->scaled_value, lshift);

        /*
         * For left shifting, we need to make sure the bits above our sign
         * bit are the correct value. I.e. zeros if the result is positive
         * and ones if the result is negative. This is because we rely on
         * the signed value of the underlying scaled_value integer.
         */
        shifted_value_sign =
            FP_NUM_METHOD(shifted_value, nb_and)(shifted_value, sign_bit);

        if (cast_self->is_signed &&
            (FpBinary_TpCompare(shifted_value_sign, py_zero) != 0))
        {
            PyObject *mask_complement = FP_NUM_METHOD(mask, nb_invert)(mask);
            masked_shifted_value = FP_NUM_METHOD(shifted_value, nb_or)(
                shifted_value, mask_complement);

            Py_DECREF(mask_complement);
        }
        else
        {
            masked_shifted_value =
                FP_NUM_METHOD(shifted_value, nb_and)(shifted_value, mask);
        }

        result = (FpBinaryLargeObject *)fpbinarylarge_create_mem(
            &FpBinary_LargeType);
        set_object_fields(result, masked_shifted_value, cast_self->int_bits,
                          cast_self->frac_bits, cast_self->is_signed);

        Py_DECREF(masked_shifted_value);
        Py_DECREF(shifted_value_sign);
        Py_DECREF(total_bits);
        Py_DECREF(sign_bit);
        Py_DECREF(mask);
        Py_DECREF(shifted_value);

        return (PyObject *)result;
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static PyObject *
fpbinarylarge_rshift(PyObject *self, PyObject *rshift)
{
    FpBinaryLargeObject *result = NULL;

    if (!PyLong_Check(rshift))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    if (rshift)
    {
        FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
        PyObject *shifted_value =
            FP_NUM_METHOD(cast_self->scaled_value,
                          nb_rshift)(cast_self->scaled_value, rshift);

        result = (FpBinaryLargeObject *)fpbinarylarge_create_mem(
            &FpBinary_LargeType);
        set_object_fields(result, shifted_value, cast_self->int_bits,
                          cast_self->frac_bits, cast_self->is_signed);

        Py_DECREF(shifted_value);

        return (PyObject *)result;
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

int
fpbinarylarge_nonzero(PyObject *self)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
    return (self && FpBinary_TpCompare(cast_self->scaled_value, py_zero) != 0);
}

/*
 *
 * Sequence methods implementation
 *
 */

static Py_ssize_t
fpbinarylarge_sq_length(PyObject *self)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
    PyObject *len_pylong = FP_NUM_METHOD(cast_self->int_bits, nb_add)(
        cast_self->int_bits, cast_self->frac_bits);
    Py_ssize_t result = PyLong_AsSsize_t(len_pylong);

    Py_DECREF(len_pylong);
    return result;
}

/*
 * A get item on an fpbinaryobject returns a bool (True for 1, False for 0).
 */
static PyObject *
fpbinarylarge_sq_item(PyObject *self, Py_ssize_t py_index)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;
    PyObject *index = PyLong_FromSize_t(py_index);
    PyObject *anded = FP_NUM_METHOD(py_one, nb_lshift)(py_one, index);
    FP_NUM_BIN_OP_INPLACE(anded, cast_self->scaled_value, nb_and);
    int compare = FpBinary_TpCompare(anded, py_zero);

    Py_DECREF(index);
    Py_DECREF(anded);

    if (compare == 0)
    {
        Py_RETURN_FALSE;
    }

    Py_RETURN_TRUE;
}

/*
 * If slice notation is invoked on an fpbinarylargeobject, a new
 * fpbinarylargeobject is created as an unsigned integer where the value is the
 * value of the selected bits.
 *
 * This is useful for digital logic implementations of NCOs and trig lookup
 * tables.
 */
static PyObject *
fpbinarylarge_sq_slice(PyObject *self, Py_ssize_t index1, Py_ssize_t index2)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self, *result;
    PyObject *total_bits, *mask, *masked_val;
    PyObject *low_index;
    Py_ssize_t total_bits_ssize, high_index_ssize, low_index_ssize;

    total_bits = FP_NUM_METHOD(cast_self->int_bits, nb_add)(
        cast_self->int_bits, cast_self->frac_bits);
    total_bits_ssize = PyLong_AsSsize_t(total_bits);
    Py_DECREF(total_bits);

    /* To allow for the (reasonably) common convention of "high-to-low" bit
     * array ordering in languages like VHDL, the user can have index 1 higher
     * than index 2 - we always just assume the highest value is the MSB
     * desired. */
    if (index1 > index2)
    {
        high_index_ssize = index1;
        low_index_ssize = index2;
    }
    else
    {
        high_index_ssize = index2;
        low_index_ssize = index1;
    }

    if (high_index_ssize > low_index_ssize + total_bits_ssize - 1)
    {
        /* Rail index to max possible. */
        high_index_ssize = low_index_ssize + total_bits_ssize - 1;
    }

    low_index = PyLong_FromSsize_t(low_index_ssize);
    masked_val = FP_NUM_METHOD(cast_self->scaled_value,
                               nb_rshift)(cast_self->scaled_value, low_index);
    total_bits = PyLong_FromSsize_t(high_index_ssize - low_index_ssize + 1);
    mask = get_total_bits_mask(total_bits);
    FP_NUM_BIN_OP_INPLACE(masked_val, mask, nb_and);

    result = fpbinarylarge_create_mem(&FpBinary_LargeType);
    set_object_fields(result, masked_val, total_bits, py_zero, false);

    Py_DECREF(total_bits);
    Py_DECREF(mask);
    Py_DECREF(masked_val);
    Py_DECREF(low_index);

    return (PyObject *)result;
}

static PyObject *
fpbinarylarge_subscript(PyObject *self, PyObject *item)
{
    FpBinaryLargeObject *cast_self = ((FpBinaryLargeObject *)self);
    Py_ssize_t index, start, stop;

    if (fp_binary_subscript_get_item_index(item, &index))
    {
        return fpbinarylarge_sq_item(self, index);
    }

    if (fp_binary_subscript_get_item_start_stop(
            item, &start, &stop,
            get_total_bits_uint(cast_self->int_bits, cast_self->frac_bits)))
    {
        return fpbinarylarge_sq_slice(self, start, stop);
    }

    return NULL;
}

static PyObject *
fpbinarylarge_str(PyObject *obj)
{
    PyObject *result;
    PyObject *double_val = fpbinarylarge_float(obj);
    result = Py_TYPE(double_val)->tp_str(double_val);

    Py_DECREF(double_val);
    return result;
}

/*
 * See str_ex_doc
 */
static PyObject *
fpbinarylarge_str_ex(PyObject *self)
{
    FpBinaryLargeObject *cast_self = ((FpBinaryLargeObject *)self);
    return scaled_long_to_float_str(cast_self->scaled_value,
                                    cast_self->int_bits, cast_self->frac_bits);
}

static PyObject *
fpbinarylarge_richcompare(PyObject *op1, PyObject *op2, int operator)
{
    bool eval = false;
    int compare;
    FpBinaryLargeObject *resized_op1, *resized_op2;

    if (!FpBinaryLarge_Check(op1) || !FpBinaryLarge_Check(op2))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    make_binary_ops_same_format(op1, op2, &resized_op1, &resized_op2);
    compare = FpBinary_TpCompare(resized_op1->scaled_value,
                                 resized_op2->scaled_value);

    switch (operator)
    {
        case Py_LT: eval = (compare < 0); break;
        case Py_LE: eval = (compare <= 0); break;
        case Py_EQ: eval = (compare == 0); break;
        case Py_NE: eval = (compare != 0); break;
        case Py_GT: eval = (compare > 0); break;
        case Py_GE: eval = (compare >= 0); break;
        default: FPBINARY_RETURN_NOT_IMPLEMENTED; break;
    }

    Py_DECREF(resized_op1);
    Py_DECREF(resized_op2);

    if (eval)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static void
fpbinarylarge_dealloc(FpBinaryLargeObject *self)
{
    Py_XDECREF(self->int_bits);
    Py_XDECREF(self->frac_bits);
    Py_XDECREF(self->scaled_value);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

/*
 * See format_doc
 */
static PyObject *
fpbinarylarge_getformat(PyObject *self, void *closure)
{
    PyObject *result_tuple;

    Py_INCREF(((FpBinaryLargeObject *)self)->int_bits);
    Py_INCREF(((FpBinaryLargeObject *)self)->frac_bits);

    result_tuple = PyTuple_Pack(2, ((FpBinaryLargeObject *)self)->int_bits,
                                ((FpBinaryLargeObject *)self)->frac_bits);

    if (!result_tuple)
    {
        Py_DECREF(((FpBinaryLargeObject *)self)->int_bits);
        Py_DECREF(((FpBinaryLargeObject *)self)->frac_bits);
    }

    return result_tuple;
}

/*
 * See is_signed_doc
 */
static PyObject *
fpbinarylarge_is_signed(PyObject *self, void *closure)
{
    if (((FpBinaryLargeObject *)self)->is_signed)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

/* Helper functions for use of top client object. */

static FP_UINT_TYPE
fpbinarylarge_get_total_bits(PyObject *obj)
{
    FpBinaryLargeObject *cast_obj = (FpBinaryLargeObject *)obj;
    /* Assume the number of int and frac bits aren't insanely massive
     * and just convert.
     */
    return (FP_UINT_TYPE)(PyLong_AsLongLong(cast_obj->int_bits) +
                          PyLong_AsLongLong(cast_obj->frac_bits));
}

void
FpBinaryLarge_FormatAsUints(PyObject *self, FP_UINT_TYPE *out_int_bits,
                            FP_UINT_TYPE *out_frac_bits)
{
    *out_int_bits = (FP_UINT_TYPE)PyLong_AsUnsignedLongLong(
        ((FpBinaryLargeObject *)self)->int_bits);
    *out_frac_bits = (FP_UINT_TYPE)PyLong_AsUnsignedLongLong(
        ((FpBinaryLargeObject *)self)->frac_bits);
}

/*
 * Returns a PyLong* who's bits are those of the underlying FpBinaryLargeObject
 * instance. This means that if the object represents a negative value, the
 * sign bit (as defined by int_bits and frac_bits) will be 1 (i.e. the bits
 * will be in 2's complement format). However, don't assume the PyLong returned
 * will or won't be negative.
 *
 * No need to increment the reference counter on the returned object.
 */
PyObject *
FpBinaryLarge_BitsAsPylong(PyObject *obj)
{
    Py_INCREF(((FpBinaryLargeObject *)obj)->scaled_value);
    return ((FpBinaryLargeObject *)obj)->scaled_value;
}

bool
FpBinaryLarge_IsSigned(PyObject *obj)
{
    return ((FpBinaryLargeObject *)obj)->is_signed;
}

/*
 * Will return a new FpBinaryLargeObject with the underlying fixed point value
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
FpBinaryLarge_FromBitsPylong(PyObject *bits, FP_INT_TYPE int_bits,
                             FP_INT_TYPE frac_bits, bool is_signed)
{
    PyObject *result =
        (PyObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    PyObject *int_bits_py = PyLong_FromLongLong(int_bits);
    PyObject *frac_bits_py = PyLong_FromLongLong(frac_bits);
    PyObject *total_bits =
        FP_NUM_METHOD(int_bits_py, nb_add)(int_bits_py, frac_bits_py);
    PyObject *mask = get_total_bits_mask(total_bits);
    PyObject *masked_val = FP_NUM_METHOD(bits, nb_and)(bits, mask);
    PyObject *sign_bit = get_sign_bit(total_bits);
    PyObject *scaled_value = NULL;

    if (is_signed && FpBinary_TpCompare(masked_val, sign_bit) >= 0)
    {
        /* scaled_value represents a negative value. Subtract "next" sign bit
         * to convert to negative pylong.
         */
        FP_NUM_BIN_OP_INPLACE(sign_bit, py_one, nb_lshift);
        scaled_value =
            FP_NUM_METHOD(masked_val, nb_subtract)(masked_val, sign_bit);
    }
    else
    {
        Py_INCREF(masked_val);
        scaled_value = masked_val;
    }

    set_object_fields((FpBinaryLargeObject *)result, scaled_value, int_bits_py,
                      frac_bits_py, is_signed);

    Py_DECREF(total_bits);
    Py_DECREF(mask);
    Py_DECREF(masked_val);
    Py_DECREF(sign_bit);
    Py_DECREF(scaled_value);
    Py_DECREF(int_bits_py);
    Py_DECREF(frac_bits_py);

    return result;
}

PyObject *
FpBinaryLarge_FromDouble(double value, FP_INT_TYPE int_bits,
                         FP_INT_TYPE frac_bits, bool is_signed,
                         fp_overflow_mode_t overflow_mode,
                         fp_round_mode_t round_mode)
{
    PyObject *value_py = PyFloat_FromDouble(value);
    PyObject *int_bits_py = PyLong_FromLong(int_bits);
    PyObject *frac_bits_py = PyLong_FromLong(frac_bits);
    FpBinaryLargeObject *result = fpbinarylarge_create_mem(&FpBinary_LargeType);

    build_from_pyfloat(value_py, int_bits_py, frac_bits_py, is_signed,
                       overflow_mode, round_mode, result);

    Py_DECREF(value_py);
    Py_DECREF(int_bits_py);
    Py_DECREF(frac_bits_py);

    return (PyObject *)result;
}

PyObject *
FpBinaryLarge_FromPickleDict(PyObject *dict)
{
    PyObject *result =
        (PyObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    PyObject *int_bits, *frac_bits, *scaled_value, *is_signed;

    int_bits = PyDict_GetItemString(dict, "ib");
    frac_bits = PyDict_GetItemString(dict, "fb");
    scaled_value = PyDict_GetItemString(dict, "sv");
    is_signed = PyDict_GetItemString(dict, "sgn");

    if (int_bits && frac_bits && scaled_value && is_signed)
    {
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

        set_object_fields((FpBinaryLargeObject *)result, scaled_value, int_bits,
                          frac_bits, (is_signed == Py_True) ? true : false);

        Py_DECREF(int_bits);
        Py_DECREF(frac_bits);
        Py_DECREF(scaled_value);
    }
    else
    {
        Py_XDECREF(result);
        result = NULL;
        PyErr_SetString(PyExc_KeyError,
                        "Pickle dict didn't have a required key.");
    }

    return result;
}

bool
FpBinaryLarge_UpdatePickleDict(PyObject *self, PyObject *dict)
{
    FpBinaryLargeObject *cast_self = (FpBinaryLargeObject *)self;

    if (!dict)
    {
        return false;
    }

    if (cast_self->int_bits && cast_self->frac_bits && cast_self->scaled_value)
    {
        PyDict_SetItemString(dict, "ib", cast_self->int_bits);
        PyDict_SetItemString(dict, "fb", cast_self->frac_bits);
        PyDict_SetItemString(dict, "sv", cast_self->scaled_value);
        PyDict_SetItemString(dict, "sgn",
                             cast_self->is_signed ? Py_True : Py_False);
        PyDict_SetItemString(dict, "bid", fp_large_type_id);

        return true;
    }

    return false;
}

static PyMethodDef fpbinarylarge_methods[] = {
    {"resize", (PyCFunction)fpbinarylarge_resize, METH_VARARGS | METH_KEYWORDS,
     resize_doc},
    {"str_ex", (PyCFunction)fpbinarylarge_str_ex, METH_NOARGS, str_ex_doc},
    {"bits_to_signed", (PyCFunction)fpbinarylarge_bits_to_signed, METH_NOARGS,
     bits_to_signed_doc},
    {"__copy__", (PyCFunction)fpbinarylarge_copy, METH_NOARGS, copy_doc},

    {"__getitem__", (PyCFunction)fpbinarylarge_subscript, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinarylarge_getsetters[] = {
    {"format", (getter)fpbinarylarge_getformat, NULL, format_doc, NULL},
    {"is_signed", (getter)fpbinarylarge_is_signed, NULL, is_signed_doc, NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinarylarge_as_number = {
    .nb_add = (binaryfunc)fpbinarylarge_add,
    .nb_subtract = (binaryfunc)fpbinarylarge_subtract,
    .nb_multiply = (binaryfunc)fpbinarylarge_multiply,
    .nb_true_divide = (binaryfunc)fpbinarylarge_divide,
    .nb_negative = (unaryfunc)fpbinarylarge_negative,
    .nb_int = (unaryfunc)fpbinarylarge_int,
    .nb_index = (unaryfunc)fpbinarylarge_index,

#if PY_MAJOR_VERSION < 3
    .nb_divide = (binaryfunc)fpbinarylarge_divide,
    .nb_long = (unaryfunc)fpbinarylarge_long,
#endif

    .nb_float = (unaryfunc)fpbinarylarge_float,
    .nb_absolute = (unaryfunc)fpbinarylarge_abs,
    .nb_lshift = (binaryfunc)fpbinarylarge_lshift,
    .nb_rshift = (binaryfunc)fpbinarylarge_rshift,
    .nb_nonzero = (inquiry)fpbinarylarge_nonzero,
};

static PySequenceMethods fpbinarylarge_as_sequence = {
    .sq_length = (lenfunc)fpbinarylarge_sq_length,
    .sq_item = (ssizeargfunc)fpbinarylarge_sq_item,

#if PY_MAJOR_VERSION < 3

    .sq_slice = (ssizessizeargfunc)fpbinarylarge_sq_slice,

#endif
};

static PyMappingMethods fpbinarylarge_as_mapping = {
    .mp_length = fpbinarylarge_sq_length,
    .mp_subscript = (binaryfunc)fpbinarylarge_subscript,
};

PyTypeObject FpBinary_LargeType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinaryLarge",
    .tp_doc = fpbinarylarge_doc,
    .tp_basicsize = sizeof(FpBinaryLargeObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinarylarge_methods,
    .tp_getset = fpbinarylarge_getsetters,
    .tp_as_number = &fpbinarylarge_as_number,
    .tp_as_sequence = &fpbinarylarge_as_sequence,
    .tp_as_mapping = &fpbinarylarge_as_mapping,
    .tp_new = (newfunc)fpbinarylarge_new,
    .tp_dealloc = (destructor)fpbinarylarge_dealloc,
    .tp_str = fpbinarylarge_str,
    .tp_repr = fpbinarylarge_str,
    .tp_richcompare = fpbinarylarge_richcompare,
};

fpbinary_private_iface_t FpBinary_LargePrvIface = {
    .get_total_bits = fpbinarylarge_get_total_bits,
    .is_signed = FpBinaryLarge_IsSigned,
    .resize = (PyCFunctionWithKeywords)fpbinarylarge_resize,
    .str_ex = fpbinarylarge_str_ex,
    .to_signed = fpbinarylarge_to_signed,
    .bits_to_signed = (PyCFunction)fpbinarylarge_bits_to_signed,
    .copy = (PyCFunction)fpbinarylarge_copy,
    .fp_getformat = fpbinarylarge_getformat,

    .fp_from_double = FpBinaryLarge_FromDouble,
    .fp_from_bits_pylong = FpBinaryLarge_FromBitsPylong,

    .build_pickle_dict = FpBinaryLarge_UpdatePickleDict,

    .getitem = fpbinarylarge_subscript,
};

void
FpBinaryLarge_InitModule(void)
{
    PyObject *minus_one_pyfloat = PyFloat_FromDouble(-1.0);

    fpbinarylarge_minus_one =
        (PyObject *)fpbinarylarge_create_mem(&FpBinary_LargeType);
    build_from_pyfloat(minus_one_pyfloat, py_one, py_zero, true, OVERFLOW_WRAP,
                       ROUNDING_DIRECT_NEG_INF,
                       (FpBinaryLargeObject *)fpbinarylarge_minus_one);

    Py_DECREF(minus_one_pyfloat);
}
