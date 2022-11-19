/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinaryComplex
 *
 *****************************************************************************/

#include "fpbinarycomplexobject.h"
#include "fpbinaryglobaldoc.h"
#include "fpbinaryobject.h"
#include <math.h>

/*
 * Creates new object. Assumes refs to real and imag ALREADY EXIST.
 */
static FpBinaryComplexObject *
fpbinarycomplex_from_params(FpBinaryObject *real, FpBinaryObject *imag)
{
    FpBinaryComplexObject *self =
        (FpBinaryComplexObject *)FpBinaryComplex_Type.tp_alloc(
            &FpBinaryComplex_Type, 0);

    if (self)
    {
        self->real = (PyObject *)real;
        self->imag = (PyObject *)imag;
    }

    return self;
}

static FpBinaryComplexObject *
cast_c_complex_to_complex(Py_complex c_complex)
{
    /* Calculate the required int and frac bits */
    double scaled_value;
    FP_INT_TYPE real_int_bits_uint, real_frac_bits_uint;
    FP_INT_TYPE imag_int_bits_uint, imag_frac_bits_uint;
    FP_INT_TYPE final_int_bits_uint, final_frac_bits_uint;
    FpBinaryObject *real_fp_binary, *imag_fp_binary;

    calc_double_to_fp_params(c_complex.real, &scaled_value, &real_int_bits_uint,
                             &real_frac_bits_uint);
    calc_double_to_fp_params(c_complex.imag, &scaled_value, &imag_int_bits_uint,
                             &imag_frac_bits_uint);
    final_int_bits_uint = (real_int_bits_uint > imag_int_bits_uint)
                              ? real_int_bits_uint
                              : imag_int_bits_uint;
    final_frac_bits_uint = (real_frac_bits_uint > imag_frac_bits_uint)
                               ? real_frac_bits_uint
                               : imag_frac_bits_uint;

    real_fp_binary =
        FpBinary_FromParams(final_int_bits_uint, final_frac_bits_uint, true,
                            c_complex.real, NULL, NULL);
    imag_fp_binary =
        FpBinary_FromParams(final_int_bits_uint, final_frac_bits_uint, true,
                            c_complex.imag, NULL, NULL);
    return fpbinarycomplex_from_params(real_fp_binary, imag_fp_binary);
}

static FpBinaryComplexObject *
cast_to_complex(PyObject *obj)
{
    FpBinaryComplexObject *result = NULL;

    if (FpBinaryComplex_CheckExact(obj))
    {
        result = (FpBinaryComplexObject *)obj;
        Py_INCREF(obj);
    }
    else if (PyObject_HasAttr(obj, complex_real_property_name_str) &&
             PyObject_HasAttr(obj, complex_imag_property_name_str))
    {
        PyObject *real_out =
            PyObject_GetAttr(obj, complex_real_property_name_str);
        PyObject *imag_out =
            PyObject_GetAttr(obj, complex_imag_property_name_str);
        result = fpbinarycomplex_from_params(FpBinary_FromValue(real_out),
                                             FpBinary_FromValue(imag_out));
        if (result)
        {
            FpBinary_SetTwoInstToSameFormat(&result->real, &result->imag);
        }
        Py_DECREF(real_out);
        Py_DECREF(imag_out);
    }
    else
    {
        result = fpbinarycomplex_from_params(FpBinary_FromValue(obj),
                                             FpBinary_FromValue(py_zero));
        if (result)
        {
            FpBinary_SetTwoInstToSameFormat(&result->real, &result->imag);
        }
    }

    return result;
}

/*
 * Checks that at least one of the operands is a FpBinaryComplexObject type.
 * Also sets the real and imag op objects to be applied to the desired FpBinary
 * operation.
 * If the other operand is not an instance of FpBinaryComplexObject, it will be
 * assumed it
 * is a supported builtin type and will be used as the real of the other
 * operand. The imaginary
 * will be set to the integer zero.
 *
 * Returns pointer to one of the PyObjects that is an instance of FpBinary for
 * the purposes
 * of calling the correct function.
 *
 * NOTE: If an object is returned, the references in op1_out/op2_out MUST
 * be decremented by the calling function.
 */
static PyObject *
prepare_binary_real_ops(PyObject *in_op1, PyObject *in_op2,
                        PyObject **op1_real_out, PyObject **op1_imag_out,
                        PyObject **op2_real_out, PyObject **op2_imag_out)
{
    PyObject *result = NULL;

    if (!FpBinaryComplex_CheckExact(in_op1) &&
        !FpBinaryComplex_CheckExact(in_op2))
    {
        return NULL;
    }

    if (FpBinaryComplex_CheckExact(in_op1))
    {
        *op1_real_out = PYOBJ_TO_REAL_FP_PYOBJ(in_op1);
        *op1_imag_out = PYOBJ_TO_IMAG_FP_PYOBJ(in_op1);
        result = *op1_real_out;

        Py_INCREF(*op1_real_out);
        Py_INCREF(*op1_imag_out);
    }
    else
    {
        FpBinaryComplexObject *comp = NULL;

        comp = cast_to_complex(in_op1);
        if (!comp)
        {
            return NULL;
        }

        *op1_real_out = comp->real;
        *op1_imag_out = comp->imag;

        Py_INCREF(*op1_real_out);
        Py_INCREF(*op1_imag_out);
        Py_DECREF(comp);
    }

    if (FpBinaryComplex_CheckExact(in_op2))
    {
        *op2_real_out = PYOBJ_TO_REAL_FP_PYOBJ(in_op2);
        *op2_imag_out = PYOBJ_TO_IMAG_FP_PYOBJ(in_op2);
        result = *op2_real_out;

        Py_INCREF(*op2_real_out);
        Py_INCREF(*op2_imag_out);
    }
    else
    {
        FpBinaryComplexObject *comp = NULL;

        comp = cast_to_complex(in_op2);
        if (!comp)
        {
            return NULL;
        }

        *op2_real_out = comp->real;
        *op2_imag_out = comp->imag;

        Py_INCREF(*op2_real_out);
        Py_INCREF(*op2_imag_out);
        Py_DECREF(comp);
    }

    return result;
}

/*
 *
 *
 * ============================================================================
 */

PyDoc_STRVAR(
    fpbinarycomplex_doc,
    "FpBinaryComplex(int_bits=1, frac_bits=0, value=0.0+0.0j, "
    "real_fp_binary=None, "
    "imag_fp_binary=None, real_bit_field=None, imag_bit_field=None, "
    "format_inst=None)\n"
    "--\n\n"
    "Represents a complex number using fixed point math and structure.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "int_bits : int\n"
    "    The number of bits to use to represent the integer part.\n"
    "    This value may be negative - this simply removes that number of bits\n"
    "    from the fractional bits. The frac_bits param still specifies the "
    "position\n"
    "    of the least significant fractional bit but the total bits are\n"
    "    int_bits + fract_bits. For example, a format of (-3, 6) would produce "
    "an\n"
    "    instance with 3 fractional bits with a maximum value (assuming "
    "unsigned) of\n"
    "    2.0**-4 + 2.0**-5 + 2.0**-6.\n"
    "\n"
    "frac_bits : int\n"
    "    The number of bits to use to represent the fractional part.\n"
    "    This value may be negative - this simply removes that number of bits\n"
    "    from the int bits. The int_bits param still specifies the position\n"
    "    of the most significant integer bit but the total bits are\n"
    "    int_bits + fract_bits. For example, a format of (6, -3) would "
    "produce\n"
    "    an instance with 3 integer bits with a maximum value (assuming "
    "unsigned) of\n"
    "    2.0**5 + 2.0**4 + 2.0**3.\n"
    "    (Note that integer powers start at 0).\n"
    "\n"
    "value : float/complex\n"
    "    The value to initialise the fixed point object to. If int_bits and "
    "frac_bits\n"
    "    do not provide enough precision to represent value fully, rounding "
    "will be\n"
    "    done using RoundingEnum.near_pos_inf and overflow will be handled "
    "using\n"
    "    OverflowEnum.sat.\n"
    "\n"
    "real_fp_binary : FpBinary\n"
    "    The real part of the FpBinaryComplex instance can be set to the "
    "value\n"
    "    of an FpBinary instance. The format will also be used if it isn't "
    "specified\n"
    "    explicitly.\n"
    "\n"
    "imag_fp_binary : FpBinary\n"
    "    The imag part of the FpBinaryComplex instance can be set to the "
    "value\n"
    "    of an FpBinary instance. The format will also be used if it isn't "
    "specified\n"
    "    explicitly.\n"
    "\n"
    "real_bit_field : int\n"
    "    If the precision of the desired initialise value is too great for the "
    "native\n"
    "    float type, real_bit_field can be set to a 2's complement "
    "representation "
    "of the\n"
    "    desired real value * 2**frac_bits. Note that real_bit_field overrides "
    "the value "
    "parameter.\n"
    "\n"
    "imag_bit_field : int\n"
    "    If the precision of the desired initialise value is too great for the "
    "native\n"
    "    float type, real_bit_field can be set to a 2's complement "
    "representation "
    "of the\n"
    "    desired imaginary value * 2**frac_bits. Note that imag_bit_field "
    "overrides the value "
    "parameter.\n"
    "\n"
    "format_inst : FpBinary\n"
    "    If set, the int_bits and frac_bits values will be taken from the "
    "format of\n"
    "    format_inst.\n"
    "\n"
    "Notes\n"
    "-----\n"
    "\n"
    "*Add and Subtract:*\n"
    "If op2 is not a fixed point type, an attempt will be made to convert\n"
    "it to a fixed point object using as few bits as necessary.\n"
    "Overflow is guaranteed to NOT happen. "
    "The resultant real/imag fixed point numbers have the following "
    "format::\n\n"
    "    int_bits  = max(op1.int_bits, op2.int_bits) + 1 \n"
    "    frac_bits = max(op1.frac_bits, op2.frac_bits) \n"
    "\n"
    "*Multiply:*\n"
    "If op2 is not a fixed point type, an attempt will be made to convert\n"
    "it to a fixed point object using as few bits as necessary.\n"
    "Overflow is guaranteed to NOT happen. "
    "The resultant real/imag fixed point numbers have the following "
    "format::\n\n"
    "    int_bits  = op1.int_bits + op2.int_bits + 1 \n"
    "    frac_bits = op1.frac_bits + op2.frac_bits\n"
    "\n"
    "*Divide:*\n"
    "Complex divide is implemented by multiplying by the conjugate of the "
    "denominator "
    "and dividing by the denominator.real**2 + denominator.imag**2."
    "\n"
    "*pow():*\n"
    "Only raising an FpBinaryComplex object to the power of 2 is supported.\n"
    "\n"
    "*Negate:*\n"
    "Because a negate is a multiply by -1, the output has one extra integer "
    "bit than\n"
    "the input operand.\n"
    "\n"
    "*Absolute value:*\n"
    "Estimates the absolute value by calculating the energy, converting to "
    "float,\n"
    "squaring and converting back to FpBinaryComplex.\n");

bool
fp_binary_complex_new_params_parse(
    PyObject *args, PyObject *kwds, PyObject **int_bits, PyObject **frac_bits,
    bool *is_signed, Py_complex *value, PyObject **real_fp_binary,
    PyObject **imag_fp_binary, PyObject **real_bit_field,
    PyObject **imag_bit_field, PyObject **format_instance)
{
    static char *kwlist[] = {
        "int_bits",       "frac_bits",      "value",
        "real_fp_binary", "imag_fp_binary", "real_bit_field",
        "imag_bit_field", "format_inst",    NULL};

    *is_signed = true;
    *real_bit_field = NULL, *imag_bit_field = NULL;
    *format_instance = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OODOOOOO", kwlist, int_bits,
                                     frac_bits, value, real_fp_binary,
                                     imag_fp_binary, real_bit_field,
                                     imag_bit_field, format_instance))
        return false;

    if (*int_bits || *frac_bits)
    {
        /* Must specify zero or both, not just one */
        if (!*int_bits || !*frac_bits)
        {
            PyErr_SetString(PyExc_TypeError,
                            "Both int_bits and frac_bits must be specified.");
            return false;
        }

        if (!check_supported_builtin_int(*int_bits))
        {
            PyErr_SetString(PyExc_TypeError, "int_bits must be an integer.");
            return false;
        }

        if (!check_supported_builtin_int(*frac_bits))
        {
            PyErr_SetString(PyExc_TypeError, "frac_bits must be an integer.");
            return false;
        }
    }

    /* Fixed point binary instances - must specify zero or both */
    if (*real_fp_binary || *imag_fp_binary)
    {
        PyObject *real_is_signed = NULL, *imag_is_signed = NULL;
        bool signed_state_same;

        /* Must specify zero or both, not just one */
        if (!(*real_fp_binary) || !(*imag_fp_binary))
        {
            PyErr_SetString(
                PyExc_TypeError,
                "Both real_fp_binary and imag_fp_binary must be specified.");
            return false;
        }

        if (!FpBinary_Check(*real_fp_binary))
        {
            PyErr_SetString(PyExc_TypeError,
                            "real_fp_binary must be an instance of FpBinary.");
            return false;
        }

        if (!FpBinary_Check(*imag_fp_binary))
        {
            PyErr_SetString(PyExc_TypeError,
                            "imag_fp_binary must be an instance of FpBinary.");
            return false;
        }

        real_is_signed = PyObject_GetAttr((PyObject *)*real_fp_binary,
                                          get_is_signed_method_name_str);
        imag_is_signed = PyObject_GetAttr((PyObject *)*imag_fp_binary,
                                          get_is_signed_method_name_str);
        signed_state_same = (real_is_signed == imag_is_signed);
        Py_DECREF(real_is_signed);
        Py_DECREF(imag_is_signed);

        if (!signed_state_same)
        {
            PyErr_SetString(PyExc_ValueError, "real_fp_binary and "
                                              "imag_fp_binary must have the "
                                              "same signed state.");
            return false;
        }
    }

    if (*real_bit_field || *imag_bit_field)
    {
        /* Must specify zero or both, not just one */
        if (!*real_bit_field || !*imag_bit_field)
        {
            PyErr_SetString(
                PyExc_TypeError,
                "Both real_bit_field and imag_bit_field must be specified.");
            return false;
        }

        /* Must also specify int_bits/frac_bits or format_instance for use with
         * bit fields */
        if (!*int_bits && !*format_instance)
        {
            PyErr_SetString(PyExc_TypeError,
                            "int_bits/frac_bits or format_instance must be "
                            "specified when using bit fields.");
            return false;
        }

        if (!PyLong_Check(*real_bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "real_bit_field must be a long integer.");
            return false;
        }

        if (!PyLong_Check(*imag_bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "imag_bit_field must be a long integer.");
            return false;
        }
    }

    if (*format_instance)
    {
        /* If the format instance is a complex fp binary, use the real for
         * formatting */
        if (!FpBinaryComplex_Check(*format_instance) &&
            !FpBinary_Check(*format_instance))
        {
            PyErr_SetString(
                PyExc_TypeError,
                "format_inst must be a FpBinary or FpBinaryComplex instance.");
            return false;
        }

        if (FpBinaryComplex_Check(*format_instance))
        {
            /* Change to FpBinary type so FpBinary functions can be used down
             * the line. */
            *format_instance =
                ((FpBinaryComplexObject *)(*format_instance))->real;
        }
    }

    return true;
}

static int
fpbinarycomplex_init(PyObject *self_pyobj, PyObject *args, PyObject *kwds)
{
    long int_bits = 1, frac_bits = 0;
    bool is_signed = true;
    PyObject *int_bits_py = NULL, *frac_bits_py = NULL;
    Py_complex value = {.real = 0.0, .imag = 0.0};
    PyObject *real_fp_binary = NULL, *imag_fp_binary = NULL;
    PyObject *real_bit_field = NULL, *imag_bit_field = NULL,
             *format_instance = NULL;
    FpBinaryComplexObject *self = (FpBinaryComplexObject *)self_pyobj;

    if (self)
    {
        if (!fp_binary_complex_new_params_parse(
                args, kwds, &int_bits_py, &frac_bits_py, &is_signed, &value,
                &real_fp_binary, &imag_fp_binary, &real_bit_field,
                &imag_bit_field, &format_instance))
        {
            return -1;
        }

        if (int_bits_py)
        {
            int_bits = PyLong_AsLong(int_bits_py);
        }

        if (frac_bits_py)
        {
            frac_bits = PyLong_AsLong(frac_bits_py);
        }

        /* Explicit set of FpBinary object takes precedence */
        if (real_fp_binary)
        {
            self->real = forward_call_with_args(
                real_fp_binary, copy_method_name_str, NULL, NULL);
            self->imag = forward_call_with_args(
                imag_fp_binary, copy_method_name_str, NULL, NULL);

            /* Format determined first by format_instance, then by
             * int/frac_bits, then via the
             * highest values from the value instances.
             */
            if (format_instance)
            {
                FpBinary_ResizeWithFormatInstance(self->real, format_instance,
                                                  ROUNDING_NEAR_POS_INF,
                                                  OVERFLOW_SAT);
                FpBinary_ResizeWithFormatInstance(self->imag, format_instance,
                                                  ROUNDING_NEAR_POS_INF,
                                                  OVERFLOW_SAT);

                /* Resize function increments reference - dec back */
                Py_DECREF(self->real);
                Py_DECREF(self->imag);
            }
            else if (int_bits_py)
            {
                FpBinary_ResizeWithCInts(self->real, int_bits, frac_bits,
                                         ROUNDING_NEAR_POS_INF, OVERFLOW_SAT);
                FpBinary_ResizeWithCInts(self->imag, int_bits, frac_bits,
                                         ROUNDING_NEAR_POS_INF, OVERFLOW_SAT);

                /* Resize function increments reference - dec back */
                Py_DECREF(self->real);
                Py_DECREF(self->imag);
            }
            else
            {
                /* Find largest format values from the two fp_binary value
                 * instances */
                FP_INT_TYPE real_int_bits_uint, real_frac_bits_uint;
                FP_INT_TYPE imag_int_bits_uint, imag_frac_bits_uint;
                PyObject *real_format_tuple =
                    PyObject_GetAttr(self->real, get_format_method_name_str);
                PyObject *imag_format_tuple =
                    PyObject_GetAttr(self->imag, get_format_method_name_str);

                if (extract_fp_format_ints_from_tuple(real_format_tuple,
                                                      &real_int_bits_uint,
                                                      &real_frac_bits_uint) &&
                    extract_fp_format_ints_from_tuple(imag_format_tuple,
                                                      &imag_int_bits_uint,
                                                      &imag_frac_bits_uint))
                {
                    FP_INT_TYPE best_int_bits =
                        (real_int_bits_uint > imag_int_bits_uint)
                            ? real_int_bits_uint
                            : imag_int_bits_uint;
                    FP_INT_TYPE best_frac_bits =
                        (real_frac_bits_uint > imag_frac_bits_uint)
                            ? real_frac_bits_uint
                            : imag_frac_bits_uint;
                    FpBinary_ResizeWithCInts(
                        self->real, best_int_bits, best_frac_bits,
                        ROUNDING_NEAR_POS_INF, OVERFLOW_SAT);
                    FpBinary_ResizeWithCInts(
                        self->imag, best_int_bits, best_frac_bits,
                        ROUNDING_NEAR_POS_INF, OVERFLOW_SAT);

                    /* Resize function increments reference - dec back */
                    Py_DECREF(self->real);
                    Py_DECREF(self->imag);
                }
                else
                {
                    return -1;
                }
            }
        }
        else if (int_bits_py || format_instance)
        {
            /* If a format is explicitly set, we can use the FpBinary
             * constructor directly on
             * the real and imag value or bit_field params
             */
            self->real = (PyObject *)FpBinary_FromParams(
                int_bits, frac_bits, is_signed, value.real, real_bit_field,
                format_instance);
            self->imag = (PyObject *)FpBinary_FromParams(
                int_bits, frac_bits, is_signed, value.imag, imag_bit_field,
                format_instance);
        }
        else
        {
            /* If still haven't been able to ascertain value, try and cast */
            FpBinaryComplexObject *cast_complex =
                cast_c_complex_to_complex(value);
            self->real = cast_complex->real;
            self->imag = cast_complex->imag;
            Py_INCREF(self->real);
            Py_INCREF(self->imag);
            Py_DECREF(cast_complex);
        }

        if (self->real && self->imag)
        {
            return 0;
        }
    }

    return -1;
}

/*
 * See resize_doc
 */
static PyObject *
fpbinarycomplex_resize(FpBinaryComplexObject *self, PyObject *args,
                       PyObject *kwds)
{
    PyObject *call_result_real, *call_result_imag = NULL;

    call_result_real =
        forward_call_with_args(self->real, resize_method_name_str, args, kwds);
    if (!call_result_real)
    {
        return NULL;
    }

    call_result_imag =
        forward_call_with_args(self->imag, resize_method_name_str, args, kwds);

    if (!call_result_imag)
    {
        return NULL;
    }

    /* Don't use the resize return references (these are the inc refs to the
     * underlying
     * FpBinary objects). We need to inc the upper FpBinaryComplex object (for
     * returning
     * from the resize function). */
    if (call_result_real)
    {
        Py_DECREF(call_result_real);
    }

    if (call_result_imag)
    {
        Py_DECREF(call_result_imag);
    }

    if (call_result_real && call_result_imag)
    {
        Py_INCREF(self);
        return (PyObject *)self;
    }

    return NULL;
}

/*
 *
 * Numeric methods implementation
 *
 */
static PyObject *
fpbinarycomplex_add(PyObject *op1, PyObject *op2)
{
    FpBinaryComplexObject *result = NULL;
    PyObject *cast_op1_real = NULL, *cast_op1_imag = NULL,
             *cast_op2_real = NULL, *cast_op2_imag = NULL;

    PyObject *function_op =
        prepare_binary_real_ops(op1, op2, &cast_op1_real, &cast_op1_imag,
                                &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        FpBinaryObject *real_result = NULL, *imag_result = NULL;

        real_result = (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_add)(
            cast_op1_real, cast_op2_real);
        imag_result = (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_add)(
            cast_op1_imag, cast_op2_imag);

        result = fpbinarycomplex_from_params(real_result, imag_result);

        Py_DECREF(cast_op1_real);
        Py_DECREF(cast_op1_imag);
        Py_DECREF(cast_op2_real);
        Py_DECREF(cast_op2_imag);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinarycomplex_subtract(PyObject *op1, PyObject *op2)
{
    FpBinaryComplexObject *result = NULL;
    PyObject *cast_op1_real = NULL, *cast_op1_imag = NULL,
             *cast_op2_real = NULL, *cast_op2_imag = NULL;

    PyObject *function_op =
        prepare_binary_real_ops(op1, op2, &cast_op1_real, &cast_op1_imag,
                                &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        FpBinaryObject *real_result = NULL, *imag_result = NULL;

        real_result = (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_subtract)(
            cast_op1_real, cast_op2_real);
        imag_result = (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_subtract)(
            cast_op1_imag, cast_op2_imag);

        result = fpbinarycomplex_from_params(real_result, imag_result);

        Py_DECREF(cast_op1_real);
        Py_DECREF(cast_op1_imag);
        Py_DECREF(cast_op2_real);
        Py_DECREF(cast_op2_imag);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinarycomplex_multiply(PyObject *op1, PyObject *op2)
{
    FpBinaryComplexObject *result = NULL;
    PyObject *cast_op1_real = NULL, *cast_op1_imag = NULL,
             *cast_op2_real = NULL, *cast_op2_imag = NULL;

    PyObject *function_op =
        prepare_binary_real_ops(op1, op2, &cast_op1_real, &cast_op1_imag,
                                &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        PyObject *ac = NULL, *ad = NULL, *bc = NULL, *bd = NULL;
        FpBinaryObject *real_result = NULL, *imag_result = NULL;

        ac = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_real,
                                                     cast_op2_real);
        ad = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_real,
                                                     cast_op2_imag);
        bc = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_imag,
                                                     cast_op2_real);
        bd = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_imag,
                                                     cast_op2_imag);

        real_result =
            (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_subtract)(ac, bd);
        imag_result =
            (FpBinaryObject *)FP_NUM_METHOD(function_op, nb_add)(ad, bc);

        result = fpbinarycomplex_from_params(real_result, imag_result);

        Py_DECREF(cast_op1_real);
        Py_DECREF(cast_op1_imag);
        Py_DECREF(cast_op2_real);
        Py_DECREF(cast_op2_imag);
        Py_DECREF(ac);
        Py_DECREF(ad);
        Py_DECREF(bc);
        Py_DECREF(bd);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinarycomplex_conjugate(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *real = forward_call_with_args(cast_self->real,
                                            copy_method_name_str, NULL, NULL);
    PyObject *imag =
        FP_NUM_METHOD(cast_self->imag, nb_negative)(cast_self->imag);
    FpBinary_ResizeWithFormatInstance(real, imag, ROUNDING_DIRECT_NEG_INF,
                                      OVERFLOW_WRAP);
    return (PyObject *)fpbinarycomplex_from_params((FpBinaryObject *)real,
                                                   (FpBinaryObject *)imag);
}

static PyObject *
fpbinarycomplex_energy(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *real_square = FP_NUM_METHOD(cast_self->real, nb_multiply)(
        cast_self->real, cast_self->real);
    PyObject *imag_square = FP_NUM_METHOD(cast_self->imag, nb_multiply)(
        cast_self->imag, cast_self->imag);
    PyObject *result =
        FP_NUM_METHOD(real_square, nb_add)(real_square, imag_square);
    Py_DECREF(real_square);
    Py_DECREF(imag_square);

    return result;
}

static PyObject *
fpbinarycomplex_divide(PyObject *op1, PyObject *op2)
{
    FpBinaryComplexObject *result = NULL, *cast_op1 = NULL, *cast_op2 = NULL;
    FpBinaryComplexObject *mult_result = NULL, *conjugate = NULL;
    PyObject *result_real = NULL, *result_imag = NULL;
    PyObject *energy = NULL;

    cast_op1 = cast_to_complex(op1);
    if (!cast_op1)
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    cast_op2 = cast_to_complex(op2);
    if (!cast_op2)
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    conjugate = (FpBinaryComplexObject *)fpbinarycomplex_conjugate(
        (PyObject *)cast_op2);
    mult_result = (FpBinaryComplexObject *)FP_NUM_METHOD(cast_op1, nb_multiply)(
        (PyObject *)cast_op1, (PyObject *)conjugate);
    energy = fpbinarycomplex_energy((PyObject *)cast_op2);

    result_real = FP_NUM_METHOD(mult_result->real,
                                nb_true_divide)(mult_result->real, energy);
    result_imag = FP_NUM_METHOD(mult_result->imag,
                                nb_true_divide)(mult_result->imag, energy);
    result = fpbinarycomplex_from_params((FpBinaryObject *)result_real,
                                         (FpBinaryObject *)result_imag);

    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);
    Py_DECREF(mult_result);
    Py_DECREF(conjugate);
    Py_DECREF(energy);

    return (PyObject *)result;
}

static PyObject *
fpbinarycomplex_complex(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *py_real =
        FP_NUM_METHOD(cast_self->real, nb_float)(cast_self->real);
    PyObject *py_imag =
        FP_NUM_METHOD(cast_self->imag, nb_float)(cast_self->imag);
    double double_real = PyFloat_AsDouble(py_real);
    double double_imag = PyFloat_AsDouble(py_imag);
    PyObject *result = PyComplex_FromDoubles(double_real, double_imag);

    Py_DECREF(py_real);
    Py_DECREF(py_imag);
    return result;
}

/*
 * When the first operand is FpBinaryComplex, currently only support squaring.
 */
static PyObject *
fpbinarycomplex_power(PyObject *o1, PyObject *o2, PyObject *o3)
{
    if (FpBinaryComplex_Check(o1))
    {
        PyObject *py_equals_2 = PyObject_RichCompare(o2, py_two, Py_EQ);

        if (PyObject_IsTrue(py_equals_2) == 1)
        {
            return FP_NUM_METHOD(o1, nb_multiply)(o1, o1);
        }
        else
        {
            FPBINARY_RETURN_NOT_IMPLEMENTED;
        }
    }
    else if (FpBinaryComplex_Check(o2))
    {
        PyObject *py_exp_complex = fpbinarycomplex_complex(o2);
        PyObject *result = PyNumber_Power(o1, py_exp_complex, o3);

        Py_DECREF(py_exp_complex);
        return result;
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static PyObject *
fpbinarycomplex_negative(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;

    return (PyObject *)fpbinarycomplex_from_params(
        (FpBinaryObject *)FP_NUM_METHOD(cast_self->real,
                                        nb_negative)(cast_self->real),
        (FpBinaryObject *)FP_NUM_METHOD(cast_self->imag,
                                        nb_negative)(cast_self->imag));
}

static PyObject *
fpbinarycomplex_abs(PyObject *self)
{
    /* As divides are a bit tricky with fixed point numbers, we are currently
     * calculating the absolute value of a complex number by calculating the
     * energy
     * and then converting to a float and square rooting, then converting back
     * to
     * fixed point with the same format as the energy result. This should give
     * us
     * a good estimate of a hardware implementation.
     */
    PyObject *energy = fpbinarycomplex_energy(self);
    PyObject *energy_pyfloat = FP_NUM_METHOD(energy, nb_float)(energy);
    double energy_double = PyFloat_AsDouble(energy_pyfloat);
    double abs_double = sqrt(energy_double);
    PyObject *is_signed =
        PyObject_GetAttr(energy, get_is_signed_method_name_str);
    bool is_signed_bool = (is_signed == Py_True);
    PyObject *result = (PyObject *)FpBinary_FromParams(
        1, 0, is_signed_bool, abs_double, NULL, energy);

    Py_DECREF(energy);
    Py_DECREF(energy_pyfloat);
    Py_DECREF(is_signed);

    return result;
}

static PyObject *
fpbinarycomplex_lshift(PyObject *self, PyObject *pyobj_lshift)
{
    PyObject *shift_as_pylong =
        FP_NUM_METHOD(pyobj_lshift, nb_long)(pyobj_lshift);

    if (shift_as_pylong)
    {
        FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
        PyObject *shifted_real = FP_NUM_METHOD(cast_self->real, nb_lshift)(
            cast_self->real, shift_as_pylong);
        PyObject *shifted_imag = FP_NUM_METHOD(cast_self->imag, nb_lshift)(
            cast_self->imag, shift_as_pylong);

        Py_DECREF(shift_as_pylong);

        if (shifted_real && shifted_imag)
        {
            return (PyObject *)fpbinarycomplex_from_params(
                (FpBinaryObject *)shifted_real, (FpBinaryObject *)shifted_imag);
        }
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static PyObject *
fpbinarycomplex_rshift(PyObject *self, PyObject *pyobj_lshift)
{
    PyObject *shift_as_pylong =
        FP_NUM_METHOD(pyobj_lshift, nb_long)(pyobj_lshift);

    if (shift_as_pylong)
    {
        FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
        PyObject *shifted_real = FP_NUM_METHOD(cast_self->real, nb_rshift)(
            cast_self->real, shift_as_pylong);
        PyObject *shifted_imag = FP_NUM_METHOD(cast_self->imag, nb_rshift)(
            cast_self->imag, shift_as_pylong);

        Py_DECREF(shift_as_pylong);

        if (shifted_real && shifted_imag)
        {
            return (PyObject *)fpbinarycomplex_from_params(
                (FpBinaryObject *)shifted_real, (FpBinaryObject *)shifted_imag);
        }
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static int
fpbinarycomplex_nonzero(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    int real_nonzero =
        FP_NUM_METHOD(cast_self->real, nb_nonzero)(cast_self->real);
    int imag_nonzero =
        FP_NUM_METHOD(cast_self->imag, nb_nonzero)(cast_self->imag);
    return ((real_nonzero == 1) || (imag_nonzero == 1)) ? 1 : 0;
}

static PyObject *
fpbinarycomplex_str(PyObject *obj)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)obj;
    PyObject *result = PyUnicode_FromString("(");
    PyObject *real_str = FP_METHOD(cast_self->real, tp_str)(cast_self->real);
    PyObject *imag_str = FP_METHOD(cast_self->imag, tp_str)(cast_self->imag);
    PyObject *imag_negative = FP_METHOD(cast_self->imag, tp_richcompare)(
        cast_self->imag, py_zero, Py_LT);

    unicode_concat(&result, real_str);

    if (imag_negative != Py_True)
    {
        unicode_concat(&result, add_sign_str);
    }

    unicode_concat(&result, imag_str);
    unicode_concat(&result, j_str);
    unicode_concat(&result, close_bracket_str);

    Py_DECREF(real_str);
    Py_DECREF(imag_str);
    Py_DECREF(imag_negative);

    return result;
}

/*
 * See str_ex_doc
 */
static PyObject *
fpbinarycomplex_str_ex(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *result = PyUnicode_FromString("(");
    PyObject *real_str = forward_call_with_args(
        cast_self->real, str_ex_method_name_str, NULL, NULL);
    PyObject *imag_str = forward_call_with_args(
        cast_self->imag, str_ex_method_name_str, NULL, NULL);
    PyObject *imag_negative = FP_METHOD(cast_self->imag, tp_richcompare)(
        cast_self->imag, py_zero, Py_LT);

    unicode_concat(&result, real_str);

    if (imag_negative != Py_True)
    {
        unicode_concat(&result, add_sign_str);
    }
    unicode_concat(&result, imag_str);
    unicode_concat(&result, j_str);
    unicode_concat(&result, close_bracket_str);

    Py_DECREF(real_str);
    Py_DECREF(imag_str);
    Py_DECREF(imag_negative);

    return result;
}

static PyObject *
fpbinarycomplex_richcompare(PyObject *obj1, PyObject *obj2, int operator)
{
    PyObject *result_real = NULL, *result_imag = NULL;
    PyObject *cast_op1_real = NULL, *cast_op1_imag = NULL,
             *cast_op2_real = NULL, *cast_op2_imag = NULL;
    PyObject *function_op =
        prepare_binary_real_ops(obj1, obj2, &cast_op1_real, &cast_op1_imag,
                                &cast_op2_real, &cast_op2_imag);
    bool result_bool;

    if (!function_op || (operator!= Py_EQ && operator!= Py_NE))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    result_real = FP_METHOD(cast_op1_real, tp_richcompare)(
        cast_op1_real, cast_op2_real, operator);
    result_imag = FP_METHOD(cast_op1_imag, tp_richcompare)(
        cast_op1_imag, cast_op2_imag, operator);
    result_bool = ((PyObject_IsTrue(result_real) == 1) &&
                   (PyObject_IsTrue(result_imag) == 1));

    Py_DECREF(cast_op1_real);
    Py_DECREF(cast_op1_imag);
    Py_DECREF(cast_op2_real);
    Py_DECREF(cast_op2_imag);

    if (result_bool)
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_FALSE;
    }
}

static void
fpbinarycomplex_dealloc(FpBinaryComplexObject *self)
{
    Py_XDECREF((PyObject *)self->real);
    Py_XDECREF((PyObject *)self->imag);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

/*
 * See format_doc
 */
static PyObject *
fpbinarycomplex_getformat(PyObject *self, void *closure)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    return PyObject_GetAttr(cast_self->real, get_format_method_name_str);
}

static PyObject *
fpbinarycomplex_real(PyObject *self, void *closure)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    Py_INCREF(cast_self->real);
    return (PyObject *)cast_self->real;
}

static PyObject *
fpbinarycomplex_imag(PyObject *self, void *closure)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    Py_INCREF(cast_self->imag);
    return (PyObject *)cast_self->imag;
}

static PyObject *
fpbinarycomplex_copy(FpBinaryComplexObject *self, PyObject *args)
{
    PyObject *real =
        forward_call_with_args(self->real, copy_method_name_str, NULL, NULL);
    PyObject *imag =
        forward_call_with_args(self->imag, copy_method_name_str, NULL, NULL);
    return (PyObject *)fpbinarycomplex_from_params((FpBinaryObject *)real,
                                                   (FpBinaryObject *)imag);
}

static PyObject *
fpbinarycomplex_getstate(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *dict = PyDict_New();

    if (dict)
    {
        PyDict_SetItemString(dict, "real", cast_self->real);
        PyDict_SetItemString(dict, "imag", cast_self->imag);
    }

    return dict;
}

static PyObject *
fpbinarycomplex_setstate(PyObject *self, PyObject *dict)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;

    cast_self->real = PyDict_GetItemString(dict, "real");
    cast_self->imag = PyDict_GetItemString(dict, "imag");

    Py_INCREF(cast_self->real);
    Py_INCREF(cast_self->imag);

    Py_RETURN_NONE;
}

static PyMethodDef fpbinarycomplex_methods[] = {
    {"resize", (PyCFunction)fpbinarycomplex_resize,
     METH_VARARGS | METH_KEYWORDS, resize_doc},
    {"str_ex", (PyCFunction)fpbinarycomplex_str_ex, METH_NOARGS, str_ex_doc},
    {"conjugate", (PyCFunction)fpbinarycomplex_conjugate, METH_NOARGS, NULL},
    {"__copy__", (PyCFunction)fpbinarycomplex_copy, METH_NOARGS, copy_doc},
    {"__complex__", (PyCFunction)fpbinarycomplex_complex, METH_NOARGS, NULL},

    /* Pickling functions */
    {"__getstate__", (PyCFunction)fpbinarycomplex_getstate, METH_NOARGS, NULL},
    {"__setstate__", (PyCFunction)fpbinarycomplex_setstate, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinarycomplex_getsetters[] = {
    {"format", (getter)fpbinarycomplex_getformat, NULL, format_doc, NULL},
    {"real", (getter)fpbinarycomplex_real, NULL, NULL, NULL},
    {"imag", (getter)fpbinarycomplex_imag, NULL, NULL, NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinarycomplex_as_number = {
    .nb_add = (binaryfunc)fpbinarycomplex_add,
    .nb_subtract = (binaryfunc)fpbinarycomplex_subtract,
    .nb_multiply = (binaryfunc)fpbinarycomplex_multiply,
    .nb_true_divide = (binaryfunc)fpbinarycomplex_divide,
    .nb_power = (ternaryfunc)fpbinarycomplex_power,

#if PY_MAJOR_VERSION < 3
    .nb_divide = (binaryfunc)fpbinarycomplex_divide,
#endif

    .nb_negative = (unaryfunc)fpbinarycomplex_negative,
    .nb_absolute = (unaryfunc)fpbinarycomplex_abs,
    .nb_lshift = (binaryfunc)fpbinarycomplex_lshift,
    .nb_rshift = (binaryfunc)fpbinarycomplex_rshift,
    .nb_nonzero = (inquiry)fpbinarycomplex_nonzero,
};

PyTypeObject FpBinaryComplex_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinaryComplex",
    .tp_doc = fpbinarycomplex_doc,
    .tp_basicsize = sizeof(FpBinaryComplexObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinarycomplex_methods,
    .tp_getset = fpbinarycomplex_getsetters,
    .tp_as_number = &fpbinarycomplex_as_number,
    .tp_new = (newfunc)PyType_GenericNew,
    .tp_init = (initproc)fpbinarycomplex_init,
    .tp_dealloc = (destructor)fpbinarycomplex_dealloc,
    .tp_str = fpbinarycomplex_str,
    .tp_repr = fpbinarycomplex_str,
    .tp_richcompare = fpbinarycomplex_richcompare,
};
