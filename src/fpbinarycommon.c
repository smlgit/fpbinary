/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Useful functions available to all fpbinary source.
 *
 *****************************************************************************/

#include "fpbinarycommon.h"

#include <float.h>
#include <math.h>

PyObject *py_zero;
PyObject *py_one;
PyObject *py_minus_one;
PyObject *py_ten;
PyObject *py_five;

/* For pickling base objects */
PyObject *fp_small_type_id;
PyObject *fp_large_type_id;

/*
 * Does a left shift SAFELY (shifting by more than the length of the
 * type is undefined).
 */
FP_UINT_TYPE
fp_uint_lshift(FP_UINT_TYPE value, FP_UINT_TYPE num_shifts)
{
    if (num_shifts == 0)
    {
        return value;
    }

    if (num_shifts >= FP_UINT_NUM_BITS)
    {
        return 0;
    }

    return value << num_shifts;
}

/*
 * Does a right shift SAFELY (shifting by more than the length of the
 * type is undefined).
 */
FP_UINT_TYPE
fp_uint_rshift(FP_UINT_TYPE value, FP_UINT_TYPE num_shifts)
{
    if (num_shifts == 0)
    {
        return value;
    }

    if (num_shifts >= FP_UINT_NUM_BITS)
    {
        return 0;
    }

    return value >> num_shifts;
}

/*
 * FFS, this does what the 2.7 PyString concat does...
 *
 * Probably could change all code to use unicode.
 *
 * The reference in left is stolen and reassigned to the result of the
 * concatenation.
 */
static void
unicode_concat(PyObject **left, PyObject *right)
{
    PyObject *tmp = *left;
    *left = PyUnicode_Concat(tmp, right);
    Py_DECREF(tmp);
}

/*
 * To minimise impact of v2->v3 ...
 */
int
FpBinary_TpCompare(PyObject *op1, PyObject *op2)
{
#if PY_MAJOR_VERSION >= 3
    int result = -1;
    PyObject *gt = FP_METHOD(op1, tp_richcompare)(op1, op2, Py_GT);

    if (gt == Py_True)
    {
        result = 1;
    }
    else
    {
        PyObject *eq = FP_METHOD(op1, tp_richcompare)(op1, op2, Py_EQ);

        if (eq == Py_True)
        {
            result = 0;
        }

        Py_DECREF(eq);
    }

    Py_DECREF(gt);
    return result;

#else

    return FP_METHOD(op1, tp_compare)(op1, op2);

#endif
}

bool
fp_binary_new_params_parse(PyObject *args, PyObject *kwds, long *int_bits,
                           long *frac_bits, bool *is_signed, double *value,
                           PyObject **bit_field, PyObject **format_instance)
{
    static char *kwlist[] = {"int_bits",  "frac_bits",   "signed", "value",
                             "bit_field", "format_inst", NULL};

    PyObject *py_is_signed = NULL;
    *bit_field = NULL;
    *format_instance = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|llOdOO", kwlist, int_bits,
                                     frac_bits, &py_is_signed, value, bit_field,
                                     format_instance))
        return false;

    if (py_is_signed)
    {
        if (!PyBool_Check(py_is_signed))
        {
            PyErr_SetString(PyExc_TypeError, "signed must be True or False.");
            return false;
        }

        if (py_is_signed == Py_True)
        {
            *is_signed = true;
        }
        else
        {
            *is_signed = false;
        }
    }

    if (*bit_field)
    {
        if (!PyLong_Check(*bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "bit_field must be a long integer.");
            return false;
        }
    }

    return true;
}

bool
fp_binary_subscript_get_item_index(PyObject *item, Py_ssize_t *index)
{
    if (PyIndex_Check(item))
    {
        *index = PyNumber_AsSsize_t(item, NULL);
        return true;
    }

    return false;
}

bool
fp_binary_subscript_get_item_start_stop(PyObject *item, Py_ssize_t *start,
                                        Py_ssize_t *stop,
                                        Py_ssize_t assumed_length)
{
    if (PySlice_Check(item))
    {
        Py_ssize_t step;

/* I ain't giving up a clean record now... */
#if PY_MAJOR_VERSION >= 3
        PyObject *cast_item = item;
#else
        PySliceObject *cast_item = (PySliceObject *)item;
#endif

        /*
         * In Python 2, the slice unpacking methods are pretty bad. To make this
         * as clean as possible (i.e. only upper functions care about length, we
         * have a bit of a hack here.
         */
        Py_ssize_t length;
        if (PySlice_GetIndicesEx(cast_item, PY_SSIZE_T_MAX, start, stop, &step,
                                 &length) < 0)
        {
            return false;
        }

        if (step > 0)
        {
            return true;
        }
        else
        {
            PyErr_SetString(PyExc_TypeError,
                            "Steps in subscripts are not supported.");
        }
    }

    return false;
}

/*
 * This function basically converts a double to a fpbinary without creating
 * the actual fpbinary object. This is so a user can decide which type to
 * use based on the magnitude of scaled_value. If scaled value is too large
 * to be represented by a 64 bit fpbinary object, the values can be applied
 * to a fpbinarylarge object (this is why the output parameter scaled_value is
 * a double).
 *
 * We could have decided to just use the exp value and DBL_MANT_DIG but in
 * most cases, I wouldn't expect raw doubles/floats to be used with fpbinary
 * objects unless they were quite small and of limited precision, and if we
 * minimise the number of bits used, we reduce the need to use the slower
 * fpbinarylarge object after math operations.
 */
void
calc_double_to_fp_params(double input_value, double *scaled_value,
                         FP_UINT_TYPE *int_bits, FP_UINT_TYPE *frac_bits)
{
    int exp;
    double mantissa = frexp(input_value, &exp);

    if (mantissa == 0)
    {
        *int_bits = 1;
        *frac_bits = 0;
        *scaled_value = 0.0;
    }
    else
    {
        FP_UINT_TYPE i;
        double shifted_mant = mantissa;

        /* Multiply the mantissa by two and subtract the new integer part.
         * Continue until
         * the remaining value is zero. I'm doing this instead of (say)
         * converting to an
         * integer and left shifting because we can't guarantee a double will
         * have less bits
         * than a long long int.
         *
         * I'm using a for loop that WILL exit if it gets to
         * the max number of bits possible in the mantissa. This is so the code
         * doesn't
         * lock up if I've made a mistake...
         */
        for (i = 1; i <= DBL_MANT_DIG; i++)
        {
            shifted_mant *= 2;
            shifted_mant -= (int)shifted_mant;

            if (shifted_mant == 0.0)
            {
                break;
            }
        }

        /* i should now be the total number of PRECISION bits required. */
        if (exp > 0)
        {
            *int_bits = exp;
        }
        else
            *int_bits = 0;

        *frac_bits = 0;

        /* Negative exponent means its magnitude is the inital number of
         * FRACTIONAL bits.
         */
        if (exp < 0)
        {
            *frac_bits = abs(exp);
        }

        if (i > *int_bits)
        {
            *frac_bits += i - *int_bits;
        }

        /* And calculate the scaled_value for fixed point representation. */
        *scaled_value = ldexp(mantissa, exp + *frac_bits);

        /* We always assume a signed type, so add an extra bit for the sign. */
        *int_bits += 1;
    }
}

/*
 * This function basically converts a PyInt or PyLong to a fpbinary without
 * creating
 * the actual fpbinary object. This is so a user can decide which type to
 * use based on the magnitude of scaled_value. If scaled value is too large
 * to be represented by a 64 bit fpbinary object, the values can be applied
 * to a fpbinarylarge object.
 *
 * input_value must be a pointer to a PyInt or PyLong.
 */
void
calc_pyint_to_fp_params(PyObject *input_value, PyObject **scaled_value,
                        FP_UINT_TYPE *int_bits)
{
    *scaled_value = NULL;
    *int_bits = 0;

    if (FpBinary_IntCheck(input_value) || PyLong_Check(input_value))
    {
        *scaled_value = FpBinary_EnsureIsPyLong(input_value);
    }

    if (*scaled_value)
    {
        size_t num_bits = _PyLong_NumBits(*scaled_value);
        /* Assume signed - need extra bit (_PyLong_NumBits returns number
         * of bits for magnitude).
         */
        num_bits++;

        *int_bits = num_bits;
    }
}

PyObject *
fp_uint_as_pylong(FP_UINT_TYPE value)
{
    return PyLong_FromUnsignedLongLong(value);
}

PyObject *
fp_int_as_pylong(FP_INT_TYPE value)
{
    return PyLong_FromLongLong(value);
}

FP_INT_TYPE
pylong_as_fp_int(PyObject *val) { return (FP_INT_TYPE)PyLong_AsLongLong(val); }

FP_UINT_TYPE
pylong_as_fp_uint(PyObject *val)
{
    return (FP_UINT_TYPE)PyLong_AsUnsignedLongLong(val);
}

/*
 * Will build a PyLong object whose bits are the scaled value as defined
 * by the float value and frac_bits. Rounding will be taken care of on
 * the basis of round_mode.
 *
 * NOTE: Overflow is not checked for (that is why there is no int_bits param).
 */
void
build_scaled_bits_from_pyfloat(PyObject *value, PyObject *frac_bits,
                               fp_round_mode_t round_mode,
                               PyObject **output_obj)
{
    PyObject *py_scale_factor =
        FP_NUM_METHOD(py_one, nb_lshift)(py_one, frac_bits);
    PyObject *py_scaled_value =
        FP_NUM_METHOD(value, nb_multiply)(value, py_scale_factor);
    double dbl_scaled_value = PyFloat_AsDouble(py_scaled_value);

    if (round_mode == ROUNDING_NEAR_POS_INF)
    {
        dbl_scaled_value += 0.5;
    }

    dbl_scaled_value = floor(dbl_scaled_value);
    *output_obj = PyLong_FromDouble(dbl_scaled_value);

    Py_DECREF(py_scale_factor);
    Py_DECREF(py_scaled_value);
}

/* Will attempt to convert the format_tuple_param int_bits and frac_bits PyLong
 * objects.
 *
 * It is assumed the calling function will decrement the reference to
 * *int_bits and *frac_bits.
 *
 * Returns false if the parameter could not be converted.
 */
bool
extract_fp_format_from_tuple(PyObject *format_tuple_param, PyObject **int_bits,
                             PyObject **frac_bits)
{
    *int_bits = NULL;
    *frac_bits = NULL;

    /* FP format is defined by a python tuple: (int_bits, frac_bits) */
    if (PyTuple_Check(format_tuple_param))
    {
        PyObject *new_int_bits_borrowed = NULL, *new_frac_bits_borrowed = NULL;

        if (PyTuple_Size(format_tuple_param) != 2)
        {
            PyErr_SetString(PyExc_TypeError, "Format tuple must be length 2.");
            return false;
        }

        new_int_bits_borrowed = PyTuple_GetItem(format_tuple_param, 0);
        if (new_int_bits_borrowed)
        {
            if (FpBinary_IntCheck(new_int_bits_borrowed) || PyLong_Check(new_int_bits_borrowed))
            {
                /* Need to convert to long. */
                *int_bits = FpBinary_EnsureIsPyLong(new_int_bits_borrowed);
            }
        }

        new_frac_bits_borrowed = PyTuple_GetItem(format_tuple_param, 1);
        if (new_frac_bits_borrowed)
        {
            if (FpBinary_IntCheck(new_frac_bits_borrowed) || PyLong_Check(new_frac_bits_borrowed))
            {
                /* Need to convert to long. */
                *frac_bits = FpBinary_EnsureIsPyLong(new_frac_bits_borrowed);
            }
        }

        if (!*int_bits || !*frac_bits)
        {
            PyErr_SetString(PyExc_TypeError,
                            "The values in the format tuple must be integers.");
        }
    }

    return (*int_bits && *frac_bits);
}

/*
 * Checks the parameters to an FpBinary new method are the correct types.
 * Returns false if one or more params are the wrong type.
 */
bool
check_new_method_input_types(PyObject *py_is_signed, PyObject *bit_field)
{
    if (py_is_signed)
    {
        if (!PyBool_Check(py_is_signed))
        {
            PyErr_SetString(PyExc_TypeError, "signed must be True or False.");
            return false;
        }
    }

    if (bit_field)
    {
        if (!PyLong_Check(bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "bit_field must be a long integer.");
            return false;
        }
    }

    return true;
}

/*
 * Produces a string representation of the arbitrary length fixed point number
 * as defined by scaled_value, int_bits, and frac_bits. Scientific notation is
 * NOT used.
 *
 * scaled_value must be a 2's compliment representation of the fixed point
 * number
 * multiplied by 2**frac bits.
 */
PyObject *
scaled_long_to_float_str(PyObject *scaled_value, PyObject *int_bits,
                         PyObject *frac_bits)
{
    /*
     * scaled_value: this is the number multiplied by 2**frac_bits. So
     * to be converted to the correct value while staying as an integer, we
     * multiply
     * by 10**frac bits and then divide by 2**frac_bits (note that each negative
     * power of 2 can only produce, at most, a single decimal equivalent because
     * 1 >> 2 produces 0.5). So this is (10/2)**frac_bits = 5**frac_bits. This
     * will
     * give us an integer that can be assessed using the standard % 10 logic.
     */
    PyObject *int_bits_is_negative, *frac_bits_is_negative;
    PyObject *int_string, *frac_string, *final_string;
    PyObject *frac_format_string, *frac_value_tuple;
    PyObject *scaled_value_padded;
    PyObject *is_negative, *scaled_value_mag, *frac_mask1, *frac_mask;
    PyObject *frac_part, *int_part, *frac_scale, *frac_part_corrected;

    long frac_bits_long, frac_dec_places, count = 0;
    PyObject *modulus, *modulus_is_zero;

    /* If we have negative int_bits, pad out the extra fractional spaces */
    int_bits_is_negative = PyObject_RichCompare(int_bits, py_zero, Py_LT);

    if (int_bits_is_negative == Py_True)
    {
        int_bits = py_zero; // No need to inc/dec this
    }

    /* If we have negative frac_bits, pad out the extra int spaces */
    frac_bits_is_negative = PyObject_RichCompare(frac_bits, py_zero, Py_LT);

    if (frac_bits_is_negative == Py_True)
    {
        PyObject *left_shift = PyNumber_Absolute(frac_bits);
        scaled_value_padded = PyNumber_Lshift(scaled_value, left_shift);
        Py_DECREF(left_shift);
        frac_bits = py_zero; // No need to inc/dec this
    }
    else
    {
        Py_INCREF(scaled_value);
        scaled_value_padded = scaled_value;
    }

    is_negative = PyObject_RichCompare(scaled_value_padded, py_zero, Py_LT);
    scaled_value_mag = PyNumber_Absolute(scaled_value_padded);
    frac_mask1 = PyNumber_Lshift(py_one, frac_bits);
    frac_mask = PyNumber_Subtract(frac_mask1, py_one);
    frac_part = PyNumber_And(scaled_value_mag, frac_mask);
    int_part = PyNumber_Rshift(scaled_value_mag, frac_bits);

    frac_scale = PyNumber_Power(py_five, frac_bits, Py_None);
    frac_part_corrected = PyNumber_Multiply(frac_part, frac_scale);

    /* Need to get rid of any trailing zeros before creating string */
    frac_bits_long = PyLong_AsLong(frac_bits), count = 0;
    frac_dec_places = frac_bits_long;
    modulus = PyNumber_Remainder(frac_part_corrected, py_ten);
    modulus_is_zero = PyObject_RichCompare(modulus, py_zero, Py_EQ);

    while (count < frac_bits_long && modulus_is_zero == Py_True)
    {
        PyObject *tmp = frac_part_corrected;
        frac_part_corrected = PyNumber_FloorDivide(tmp, py_ten);
        Py_DECREF(tmp);

        Py_DECREF(modulus);
        Py_DECREF(modulus_is_zero);
        modulus = PyNumber_Remainder(frac_part_corrected, py_ten);
        modulus_is_zero = PyObject_RichCompare(modulus, py_zero, Py_EQ);

        frac_dec_places--;
        count++;
    }

    Py_DECREF(modulus);
    Py_DECREF(modulus_is_zero);

    int_string = FP_METHOD(int_part, tp_str)(int_part);

    frac_format_string = PyUnicode_FromFormat("%%0%ldd", frac_dec_places);
    frac_value_tuple = PyTuple_Pack(1, frac_part_corrected);
    frac_string = PyUnicode_Format(frac_format_string, frac_value_tuple);

    if (is_negative == Py_True)
    {
        final_string = PyUnicode_FromString("-");
        unicode_concat(&final_string, int_string);
        Py_DECREF(int_string);
    }
    else
    {
        final_string = int_string;
    }

    unicode_concat(&final_string, PyUnicode_FromString("."));
    unicode_concat(&final_string, frac_string);

    Py_DECREF(scaled_value_padded);
    Py_DECREF(frac_string);
    Py_DECREF(is_negative);
    Py_DECREF(int_bits_is_negative);
    Py_DECREF(frac_bits_is_negative);
    Py_DECREF(scaled_value_mag);
    Py_DECREF(frac_mask1);
    Py_DECREF(frac_mask);
    Py_DECREF(frac_part);
    Py_DECREF(int_part);
    Py_DECREF(frac_scale);
    Py_DECREF(frac_part_corrected);
    Py_DECREF(frac_format_string);
    Py_DECREF(frac_value_tuple);

    return final_string;
}

bool
FpBinary_IntCheck(PyObject *ob)
{
#if PY_MAJOR_VERSION >= 3

    return false;

#else

    return PyInt_Check(ob);

#endif
}

/*
 * Will take the input and:
 *     - if it is NOT a PyLong, will attempt to convert to a PyLong
 *     - if it IS a PyLong, will increment the ref count and return it
 *
 * Note that this function should only be called if the input ob is either
 * a PyLong or a PyInt.
 */
PyObject *
FpBinary_EnsureIsPyLong(PyObject *ob)
{
#if PY_MAJOR_VERSION >= 3

    Py_INCREF(ob);
    return ob;

#else

    if (PyLong_Check(ob))
    {
        Py_INCREF(ob);
        return ob;
    }
    else
    {
        return PyLong_FromLong(PyInt_AsLong(ob));
    }

#endif
}

/*
 * Will take the input and:
 *     - if it is NOT a PyInt AND platform supports distinction between Int and Long,
 *       will convert to a PyInt
 *     - if it IS a PyLong, will increment the ref count and return it
 *
 * Note that this function should only be called if the input ob is either
 * a PyLong or a PyInt.
 */
PyObject *
FpBinary_TryConvertToPyInt(PyObject *ob)
{
#if PY_MAJOR_VERSION >= 3

    Py_INCREF(ob);
    return ob;

#else

    if (PyInt_Check(ob))
    {
        Py_INCREF(ob);
        return ob;
    }
    else
    {
        return PyInt_FromLong(PyLong_AsLong(ob));
    }

#endif
}

void
FpBinaryCommon_InitModule(void)
{
    py_zero = PyLong_FromLong(0);
    py_one = PyLong_FromLong(1);
    py_minus_one = PyLong_FromLong(-1);
    py_five = PyLong_FromLong(5);
    py_ten = PyLong_FromLong(10);

    /* Tells us what type of base object was pickled */
    fp_small_type_id = PyLong_FromLong(1);
    fp_large_type_id = PyLong_FromLong(2);
}
