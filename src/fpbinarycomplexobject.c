/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinaryComplex
 *
 *****************************************************************************/

#include "fpbinarycomplexobject.h"
#include "fpbinaryobject.h"
#include "fpbinaryglobaldoc.h"
#include <math.h>

static PyObject* cast_to_complex(PyObject *obj)
{
    FpBinaryObject *real = NULL, *imag = NULL;
    FpBinaryComplexObject *result = NULL;
    PyObject *is_signed = NULL;

    if (FpBinaryComplex_Check(obj))
    {
        Py_INCREF(obj);
        return obj;
    }

    real = FpBinary_FromValue(obj);

    if (!real)
    {
        return NULL;
    }

    is_signed = PyObject_GetAttr(real, get_is_signed_method_name_str);
    imag = FpBinary_FromParams(1, 0, is_signed == Py_True, 0.0, NULL, real);

    if (imag)
    {
        result = fpbinarycomplex_from_params(real, imag);
    }

    Py_DECREF(is_signed);

    return (PyObject *)result;
}

/*
 * Checks that at least one of the operands is a FpBinaryComplexObject type.
 * Also sets the real and imag op objects to be applied to the desired FpBinary operation.
 * If the other operand is not an instance of FpBinaryComplexObject, it will be assumed it
 * is a supported builtin type and will be used as the real of the other operand. The imaginary
 * will be set to the integer zero.
 *
 * Returns pointer to one of the PyObjects that is an instance of FpBinary for the purposes
 * of calling the correct function.
 *
 * NOTE: If an object is returned, the references in op1_out/op2_out MUST
 * be decremented by the calling function.
 */
static PyObject*
prepare_binary_real_ops(PyObject *in_op1, PyObject *in_op2,
        PyObject **op1_real_out, PyObject **op1_imag_out, PyObject **op2_real_out,
        PyObject **op2_imag_out)
{
    PyObject *result = NULL;

    if (!FpBinaryComplex_CheckExact(in_op1) && !FpBinaryComplex_CheckExact(in_op2))
    {
        return NULL;
    }

    if (FpBinaryComplex_CheckExact(in_op1))
    {
        *op1_real_out = PYOBJ_TO_REAL_FP_PYOBJ(in_op1);
        *op1_imag_out = PYOBJ_TO_IMAG_FP_PYOBJ(in_op1);
        result = *op1_real_out;
    }
    else
    {
        *op1_real_out = in_op1;
        *op1_imag_out = py_zero;
    }

    if (FpBinaryComplex_CheckExact(in_op2))
    {
        *op1_real_out = PYOBJ_TO_REAL_FP_PYOBJ(in_op2);
        *op1_imag_out = PYOBJ_TO_IMAG_FP_PYOBJ(in_op2);
        result = *op2_real_out;
    }
    else
    {
        *op2_real_out = in_op2;
        *op2_imag_out = py_zero;
    }


    Py_INCREF(*op1_real_out);
    Py_INCREF(*op1_imag_out);
    Py_INCREF(*op2_real_out);
    Py_INCREF(*op2_imag_out);

    return result;
}

/*
 *
 *
 * ============================================================================
 */


PyDoc_STRVAR(
    fpbinarycomplex_doc,
    "FpBinaryComplex(int_bits=1, frac_bits=0, signed=True, value=0.0, bit_field=None, "
    "format_inst=None)\n"
    "--\n\n"
    "Represents a real number using fixed point math and structure.\n"
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
    "signed : bool\n"
    "    Specifies whether the data represented is signed (True) or unsigned.\n"
    "    This effects min/max values and wrapping/saturation behaviour.\n"
    "\n"
    "value : float\n"
    "    The value to initialise the fixed point object to. If int_bits and "
    "frac_bits\n"
    "    do not provide enough precision to represent value fully, rounding "
    "will be\n"
    "    done using RoundingEnum.near_pos_inf and overflow will be handled "
    "using\n"
    "    OverflowEnum.sat.\n"
    "\n"
    "bit_field : int\n"
    "    If the precision of the desired initialise value is too great for the "
    "native\n"
    "    float type, bit_field can be set to a 2's complement representation "
    "of the\n"
    "    desired value * 2**frac_bits. Note that bit_field overrides the value "
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
    "The resultant fixed point number has the following format::\n\n"
    "    int_bits  = max(op1.int_bits, op2.int_bits) + 1 \n"
    "    frac_bits = max(op1.frac_bits, op2.frac_bits) \n"
    "\n"
    "*Multiply:*\n"
    "If op2 is not a fixed point type, an attempt will be made to convert\n"
    "it to a fixed point object using as few bits as necessary.\n"
    "Overflow is guaranteed to NOT happen. "
    "The resultant fixed point number has the following format::\n\n"
    "    int_bits  = op1.int_bits + op2.int_bits \n"
    "    frac_bits = op1.frac_bits + op2.frac_bits \n"
    "\n"
    "*Divide:*\n"
    "If op2 is not a fixed point type, an attempt will be made to convert\n"
    "it to a fixed point object using as few bits as necessary.\n"
    "\n"
    "The divide operation is carried out on the fixed point representations. "
    "This\n"
    "is essentially an integer divide on the values scaled by 2**frac_bits.\n"
    "However, the numerator is scaled further so that the result has "
    "self.int_bits +\n"
    "value.frac bits fractional bits of precision. Once the divide is done,\n"
    "the result is direct rounded TOWARD ZERO."
    "\n"
    "Enough int bits are in the result to ensure there is never an overflow.\n"
    "In short, the resultant fixed point number has the following format::\n\n"
    "    int_bits = op1.int_bits + op2.frac_bits + 1 if signed or\n"
    "    int_bits = op1.int_bits + op2.frac_bits     if unsigned\n"
    "    frac_bits = op1.frac_bits + op2.int_bits\n"
    "\n"
    "If the user wants to implement a different type of rounding or increase "
    "the\n"
    "precision of the result, they need to resize the operands first, do the "
    "divide\n"
    "and then resize to the desired length with the desired rounding mode.\n"
    "\n"
    "*Negate:*\n"
    "Because a negate is a multiply by -1, the output has one extra integer "
    "bit than\n"
    "the input operand.\n"
    "\n"
    "*Absolute value:*\n"
    "If the input operand is negative, the operation requires a negate, so the "
    "output\n"
    "will have one extra integer bit than the input operand. Otherwise, the "
    "format will\n"
    "remain the same.\n");

bool
fp_binary_complex_new_params_parse(PyObject *args, PyObject *kwds, long *int_bits,
                           long *frac_bits, bool *is_signed, Py_complex *value,
                           PyObject **real_fp_binary, PyObject **imag_fp_binary,
                           PyObject **real_bit_field, PyObject **imag_bit_field,
                           PyObject **format_instance)
{
    static char *kwlist[] = {"int_bits",  "frac_bits",   "signed", "value",
            "real_fp_binary", "imag_fp_binary", "real_bit_field", "imag_bit_field", "format_inst", NULL};

    PyObject *py_is_signed = NULL;
    *real_bit_field = NULL, *imag_bit_field = NULL;
    *format_instance = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|llODOOOOO", kwlist, int_bits,
                                     frac_bits, &py_is_signed, value, real_fp_binary,
                                     imag_fp_binary, real_bit_field,
                                     imag_bit_field, format_instance))
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

    if (*real_fp_binary)
    {
        if (!FpBinary_Check(*real_fp_binary))
        {
            PyErr_SetString(PyExc_TypeError,
                            "real_fp_binary must be an instance of FpBinary.");
            return false;
        }
    }

    if (*imag_fp_binary)
    {
        if (!FpBinary_Check(*imag_fp_binary))
        {
            PyErr_SetString(PyExc_TypeError,
                            "imag_fp_binary must be an instance of FpBinary.");
            return false;
        }
    }

    if (*real_bit_field)
    {
        if (!PyLong_Check(*real_bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "real_bit_field must be a long integer.");
            return false;
        }
    }

    if (*imag_bit_field)
    {
        if (!PyLong_Check(*imag_bit_field))
        {
            PyErr_SetString(PyExc_TypeError,
                            "imag_bit_field must be a long integer.");
            return false;
        }
    }

    return true;
}

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
        self->real = (PyObject *) real;
        self->imag = (PyObject *) imag;
    }

    return self;
}

static int
fpbinarycomplex_init(PyObject *self_pyobj, PyObject *args, PyObject *kwds)
{
    long int_bits = 1, frac_bits = 0;
    bool is_signed = true;
    Py_complex value = 0.0;
    PyObject *real_fp_binary = NULL, *imag_fp_binary = NULL;
    PyObject *real_bit_field = NULL, *imag_bit_field= NULL, *format_instance = NULL;
    FpBinaryComplexObject *self = (FpBinaryComplexObject *)self_pyobj;

    if (self)
    {
        if (!fp_binary_complex_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                        &is_signed, &value, &real_fp_binary,
                                        &imag_fp_binary, &real_bit_field,
                                        &imag_bit_field, &format_instance))
        {
            return -1;
        }

        /* Explicit set of FpBinary object takes precendence */
        if (real_fp_binary)
        {
            self->real = forward_call_with_args(real_fp_binary, copy_method_name_str, NULL, NULL);
        }
        else
        {
            self->real = (PyObject *) FpBinary_FromParams(int_bits, frac_bits, is_signed,
                    value.real, real_bit_field, format_instance);
        }

        /* Explicit set of FpBinary object takes precendence */
        if (imag_fp_binary)
        {
            self->imag = forward_call_with_args(imag_fp_binary, copy_method_name_str, NULL, NULL);
        }
        else
        {
            self->imag = (PyObject *) FpBinary_FromParams(int_bits, frac_bits, is_signed,
                        value.imag, imag_bit_field, format_instance);
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

    call_result_real = forward_call_with_args(self->real,
                                         resize_method_name_str, args, kwds);
    call_result_imag = forward_call_with_args(self->imag,
                                             resize_method_name_str, args, kwds);

    /* Don't use the resize return references (these are the inc refs to the underlying
     * FpBinary objects). We need to inc the upper FpBinaryComplex object (for returning
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
    PyObject *cast_op1_real, *cast_op1_imag, *cast_op2_real, *cast_op2_imag;

    PyObject *function_op = prepare_binary_real_ops(op1, op2, &cast_op1_real,
            &cast_op1_imag, &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        FpBinaryObject *func_fp = (FpBinaryObject *) function_op;
        FpBinaryObject *real_result = NULL, *imag_result = NULL;

        real_result = FP_NUM_METHOD(function_op, nb_add)(cast_op1_real, cast_op2_real);
        imag_result = FP_NUM_METHOD(function_op, nb_add)(cast_op1_imag, cast_op2_imag);

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
    PyObject *cast_op1_real, *cast_op1_imag, *cast_op2_real, *cast_op2_imag;

    PyObject *function_op = prepare_binary_real_ops(op1, op2, &cast_op1_real,
            &cast_op1_imag, &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        FpBinaryObject *func_fp = (FpBinaryObject *) function_op;
        PyObject *real_result = NULL, *imag_result = NULL;

        real_result = FP_NUM_METHOD(function_op, nb_subtract)(cast_op1_real, cast_op2_real);
        imag_result = FP_NUM_METHOD(function_op, nb_subtract)(cast_op1_imag, cast_op2_imag);

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
    PyObject *cast_op1_real, *cast_op1_imag, *cast_op2_real, *cast_op2_imag;

    PyObject *function_op = prepare_binary_real_ops(op1, op2, &cast_op1_real,
            &cast_op1_imag, &cast_op2_real, &cast_op2_imag);

    if (function_op)
    {
        FpBinaryObject *func_fp = (FpBinaryObject *) function_op;
        PyObject *ac = NULL, *ad = NULL, *bc = NULL, *bd = NULL;
        PyObject *real_result = NULL, *imag_result = NULL;

        ac = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_real, cast_op2_real);
        ad = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_real, cast_op2_imag);
        bc = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_imag, cast_op2_real);
        bd = FP_NUM_METHOD(function_op, nb_multiply)(cast_op1_imag, cast_op2_imag);

        real_result = FP_NUM_METHOD(function_op, nb_subtract)(ac, bd);
        imag_result = FP_NUM_METHOD(function_op, nb_add)(ad, bc);

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
    PyObject *real = forward_call_with_args(cast_self->real, copy_method_name_str, NULL, NULL);
    PyObject *imag = FP_NUM_METHOD(cast_self->imag, nb_negative)(cast_self->imag);
    return fpbinarycomplex_from_params(real, imag);
}

static PyObject *
fpbinarycomplex_divide(PyObject *op1, PyObject *op2)
{
    FpBinaryComplexObject *result = NULL, *cast_op1 = NULL, *cast_op2 = NULL;
    FpBinaryComplexObject *mult_result = NULL, *conjugate = NULL;
    PyObject *result_real = NULL, *result_imag = NULL;
    FpBinaryObject *energy = NULL;

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

    conjugate = fpbinarycomplex_conjugate(cast_op2);
    mult_result = FP_NUM_METHOD(cast_op1, nb_multiply)(cast_op1, conjugate);
    energy = fpbinarycomplex_energy(cast_op2);

    result_real = FP_NUM_METHOD(mult_result->real, nb_divide)(mult_result->real, energy);
    result_imag = FP_NUM_METHOD(mult_result->imag, nb_divide)(mult_result->imag, energy);
    result = fpbinarycomplex_from_params((FpBinaryObject *) result_real, (FpBinaryObject *) result_imag);

    Py_DECREF(cast_op1);
    Py_DECREF(cast_op2);
    Py_DECREF(mult_result);
    Py_DECREF(conjugate);
    Py_DECREF(energy);

    return (PyObject *)result;
}

static PyObject *
fpbinarycomplex_negative(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;

    return (PyObject *)fpbinarycomplex_from_params(
        FP_NUM_METHOD(cast_self->real, nb_negative)(cast_self->real),
        FP_NUM_METHOD(cast_self->imag, nb_negative)(cast_self->imag));
}

static PyObject *
fpbinarycomplex_energy(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *real_square = FP_NUM_METHOD(cast_self->real, nb_multiply)(cast_self->real, cast_self->real);
    PyObject *imag_square = FP_NUM_METHOD(cast_self->imag, nb_multiply)(cast_self->imag, cast_self->imag);
    PyObject *result = FP_NUM_METHOD(real_square, nb_add)(real_square, imag_square);
    Py_DECREF(real_square);
    Py_DECREF(imag_square);

    return result;
}

static PyObject *
fpbinarycomplex_abs(PyObject *self)
{
    /* As divides are a bit tricky with fixed point numbers, we are currently
     * calculating the absolute value of a complex number by calculating the energy
     * and then converting to a float and square rooting, then converting back to
     * fixed point with the same format as the energy result. This should give us
     * a good estimate of a hardware implementation.
     */
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *energy = fpbinarycomplex_energy(self);
    PyObject *energy_pyfloat = FP_NUM_METHOD(energy, nb_float)(energy);
    double energy_double = PyFloat_AsDouble(energy_pyfloat);
    double abs_double = sqrt(energy_double);
    PyObject *is_signed = PyObject_GetAttr(energy, get_is_signed_method_name_str);
    bool is_signed_bool = (is_signed == Py_True);
    PyObject *result = FpBinary_FromParams(1, 0, is_signed_bool, abs_double, NULL, energy);

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
        PyObject *shifted_real =
            FP_NUM_METHOD(cast_self->real, nb_lshift)(cast_self->real, shift_as_pylong);
        PyObject *shifted_imag =
                    FP_NUM_METHOD(cast_self->imag, nb_lshift)(cast_self->imag, shift_as_pylong);

        Py_DECREF(shift_as_pylong);

        if (shifted_real && shifted_imag)
        {
            return (PyObject *) fpbinarycomplex_from_params(shifted_real, shifted_imag);
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
        PyObject *shifted_real =
            FP_NUM_METHOD(cast_self->real, nb_rshift)(cast_self->real, shift_as_pylong);
        PyObject *shifted_imag =
                    FP_NUM_METHOD(cast_self->imag, nb_rshift)(cast_self->imag, shift_as_pylong);

        Py_DECREF(shift_as_pylong);

        if (shifted_real && shifted_imag)
        {
            return (PyObject *) fpbinarycomplex_from_params(shifted_real, shifted_imag);
        }
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static int
fpbinarycomplex_nonzero(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *real_nonzero = FP_NUM_METHOD(cast_self->real, nb_nonzero)(cast_self->real);
    PyObject *imag_nonzero = FP_NUM_METHOD(cast_self->imag, nb_nonzero)(cast_self->imag);
    bool result_bool = ((PyObject_IsTrue(real_nonzero) == 1) && (PyObject_IsTrue(imag_nonzero) == 1));

    Py_DECREF(real_nonzero);
    Py_DECREF(imag_nonzero);

    if (result_bool)
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_FALSE;
    }
}

static PyObject *
fpbinarycomplex_str(PyObject *obj)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)obj;
    PyObject *real_str = FP_METHOD(cast_self->real, tp_str)(cast_self->real);
    PyObject *imag_str = FP_METHOD(cast_self->imag, tp_str)(cast_self->imag);
    PyObject *result = unicode_concat(&real_str, add_sign_str);

    result = unicode_concat(&result, imag_str);
    result = unicode_concat(&result, j_str);

    /* Don't decrement the real_str ref because it was stolen by the unicode_concat function */
    Py_DECREF(imag_str);

    return result;
}

/*
 * See str_ex_doc
 */
static PyObject *
fpbinarycomplex_str_ex(PyObject *self)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    PyObject *real_str = forward_call_with_args(cast_self->real, str_ex_method_name_str, NULL, NULL);
    PyObject *imag_str = forward_call_with_args(cast_self->imag, str_ex_method_name_str, NULL, NULL);
    PyObject *result = unicode_concat(&real_str, add_sign_str);

    result = unicode_concat(&result, imag_str);
    result = unicode_concat(&result, j_str);

    /* Don't decrement the real_str ref because it was stolen by the unicode_concat function */
    Py_DECREF(imag_str);

    return result;
}

static PyObject *
fpbinarycomplex_richcompare(PyObject *obj1, PyObject *obj2, int operator)
{
    PyObject *result = NULL, *result_real = NULL, *result_imag = NULL;
    PyObject *cast_op1_real, *cast_op1_imag, *cast_op2_real, *cast_op2_imag;
    PyObject *function_op = prepare_binary_real_ops(obj1, obj2, &cast_op1_real,
            &cast_op1_imag, &cast_op2_real, &cast_op2_imag);
    bool result_bool;

    if (!function_op || (operator != Py_EQ && operator != Py_NE))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    result_real =
            FP_METHOD(cast_op1_real, tp_richcompare)(cast_op1_real, cast_op2_real, operator);
    result_imag =
            FP_METHOD(cast_op1_imag, tp_richcompare)(cast_op1_imag, cast_op2_imag, operator);
    result_bool = ((PyObject_IsTrue(result_real) == 1) && (PyObject_IsTrue(result_imag) == 1));

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

/*
 * See is_signed_doc
 */
static PyObject *
fpbinarycomplex_is_signed(PyObject *self, void *closure)
{
    FpBinaryComplexObject *cast_self = (FpBinaryComplexObject *)self;
    return PyObject_GetAttr(cast_self->real, get_is_signed_method_name_str);
}

static PyObject *
fpbinarycomplex_copy(FpBinaryComplexObject *self, PyObject *args)
{
    PyObject *real = forward_call_with_args(self->real, copy_method_name_str, NULL, NULL);
    PyObject *imag = forward_call_with_args(self->imag, copy_method_name_str, NULL, NULL);
    return fpbinarycomplex_from_params(real, imag);
}

static PyMethodDef fpbinarycomplex_methods[] = {
    {"resize", (PyCFunction)fpbinarycomplex_resize, METH_VARARGS | METH_KEYWORDS,
     resize_doc},
    {"str_ex", (PyCFunction)fpbinarycomplex_str_ex, METH_NOARGS, str_ex_doc},
    {"conjugate", (PyCFunction)fpbinarycomplex_conjugate, METH_NOARGS, NULL},
    {"__copy__", (PyCFunction)fpbinarycomplex_copy, METH_NOARGS, copy_doc},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinarycomplex_getsetters[] = {
    {"format", (getter)fpbinarycomplex_getformat, NULL, format_doc, NULL},
    {"is_signed", (getter)fpbinarycomplex_is_signed, NULL, is_signed_doc, NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinarycomplex_as_number = {
    .nb_add = (binaryfunc)fpbinarycomplex_add,
    .nb_subtract = (binaryfunc)fpbinarycomplex_subtract,
    .nb_multiply = (binaryfunc)fpbinarycomplex_multiply,
    .nb_true_divide = (binaryfunc)fpbinarycomplex_divide,
    .nb_negative = (unaryfunc)fpbinarycomplex_negative,
    .nb_absolute = (unaryfunc)fpbinarycomplex_abs,
    .nb_lshift = (binaryfunc)fpbinarycomplex_lshift,
    .nb_rshift = (binaryfunc)fpbinarycomplex_rshift,
    .nb_nonzero = (inquiry)fpbinarycomplex_nonzero,
};

PyTypeObject FpBinaryComplex_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinaryComplex",
    .tp_doc = NULL,
    .tp_basicsize = sizeof(FpBinaryObject),
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
