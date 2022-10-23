/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinaryComplex
 *
 *****************************************************************************/

#include "fpbinaryobject.h"
#include "fpbinaryglobaldoc.h"
#include <math.h>

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

    if (!FpBinaryComplex_CheckExact(in_op1) && FpBinaryComplex_CheckExact(in_op2))
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
                           PyObject **real_bit_field, PyObject **imag_bit_field,
                           PyObject **format_instance)
{
    static char *kwlist[] = {"int_bits",  "frac_bits",   "signed", "value",
                             "real_bit_field", "imag_bit_field", "format_inst", NULL};

    PyObject *py_is_signed = NULL;
    *real_bit_field = NULL, *imag_bit_field = NULL;
    *format_instance = NULL;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|llODOOO", kwlist, int_bits,
                                     frac_bits, &py_is_signed, value, real_bit_field,
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
fpbinarycomplex_from_params(FpBinaryComplexObject *real, FpBinaryComplexObject *imag)
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
    PyObject *real_bit_field = NULL, *imag_bit_field= NULL, *format_instance = NULL;
    FpBinaryComplexObject *self = (FpBinaryComplexObject *)self_pyobj;

    if (self)
    {
        if (!fp_binary_complex_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                        &is_signed, &value, &real_bit_field,
                                        &imag_bit_field, &format_instance))
        {
            return -1;
        }

        self->real = (PyObject *) FpBinary_FromParams(int_bits, frac_bits, is_signed,
                value.real, real_bit_field, format_instance);
        self->imag = (PyObject *) FpBinary_FromParams(int_bits, frac_bits, is_signed,
                        value.imag, imag_bit_field, format_instance);

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
        PyObject *real_result = NULL, *imag_result = NULL;

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
fpbinarycomplex_divide(PyObject *op1, PyObject *op2)
{
    FPBINARY_RETURN_NOT_IMPLEMENTED;
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
fpbinary_abs(PyObject *self)
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
fpbinary_lshift(PyObject *self, PyObject *pyobj_lshift)
{
    PyObject *shift_as_pylong =
        FP_NUM_METHOD(pyobj_lshift, nb_long)(pyobj_lshift);

    if (shift_as_pylong)
    {
        PyObject *shifted_base_obj =
            FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self), nb_lshift)(
                PYOBJ_TO_BASE_FP_PYOBJ(self), shift_as_pylong);
        if (shifted_base_obj)
        {
            FpBinaryObject *result =
                fpbinary_from_base_fp(PYOBJ_FP_BASE(shifted_base_obj));
            return FP_BASE_PYOBJ(result);
        }
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static PyObject *
fpbinary_rshift(PyObject *self, PyObject *pyobj_rshift)
{
    PyObject *shift_as_pylong =
        FP_NUM_METHOD(pyobj_rshift, nb_long)(pyobj_rshift);

    if (shift_as_pylong)
    {
        PyObject *shifted_base_obj =
            FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self), nb_rshift)(
                PYOBJ_TO_BASE_FP_PYOBJ(self), shift_as_pylong);
        if (shifted_base_obj)
        {
            FpBinaryObject *result =
                fpbinary_from_base_fp(PYOBJ_FP_BASE(shifted_base_obj));
            return FP_BASE_PYOBJ(result);
        }
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static int
fpbinary_nonzero(PyObject *self)
{
    return FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                         nb_nonzero)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

/*
 * Picking funcitons __getstate__ and __setstate__ .
 *
 * The pickling strategy is to not implement a __reduce__ method but to
 * implement
 * __getstate__ and __setstate__. This will cause all pickling code for
 * protocols
 * >= 2 to pickle the dictionary produced by __getstate__ on pickling and, on
 * unpickling, call the FpBinary __new__ function but NOT the __init__ function
 * and
 * instead call the __setstate__ function with the unpickled dict as parameter.
 *
 * This strategy does not work for pickle protocols 0 and 1.
 *
 * The underlying base classes (FpBinarySmall and FpBinaryLarge) are never
 * exposed
 * to the outside world, so we can easily get rid of them in future if we want.
 *
 * The underlying base classes populate the dict with whatever they want and
 * then
 * FpBinary returns it from the __getstate__ function. An id is put in the dict
 * so
 * FpBinary knows which function to call when __setstate__ is called.
 *
 * No version number is set in the dict. If changes are made to the objects in
 * future,
 * a version field can be added to the dict and an absence of a version field in
 * the
 * unpickled dict indicates the oldest version.
 */
static PyObject *
fpbinary_getstate(PyObject *self)
{
    PyObject *dict = PyDict_New();

    if (dict != NULL)
    {
        if (!FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self), build_pickle_dict)(
                PYOBJ_TO_BASE_FP_PYOBJ(self), dict))
        {
            Py_DECREF(dict);
            dict = NULL;
        }
    }

    return dict;
}

static PyObject *
fpbinary_setstate(PyObject *self, PyObject *dict)
{
    FpBinaryObject *cast_self = (FpBinaryObject *)self;
    PyObject *base_obj = NULL;
    PyObject *base_type_id = PyDict_GetItemString(dict, "bid");

    if (base_type_id != NULL)
    {
        /* Make sure the object is actually a PyLong. I.e. if we are in Python
         * 2.7, the unpickler may have decided to create a PyInt. Note that
         * after
         * this call, we have created a new/incremented reference, so we need
         * to decrement when done (note that up to this point, base_type_id is
         * just a borrowed reference from the dict).
         */
        base_type_id = FpBinary_EnsureIsPyLong(base_type_id);

        if (FpBinary_TpCompare(base_type_id, fp_small_type_id) == 0)
        {
            base_obj = FpBinarySmall_FromPickleDict(dict);

            /* FpBinarySmall_FromPickleDict may return a dict if it can't
             * represent the pickled value (i.e. it was pickled on a larger
             * word length system). In this case, we need to build an
             * FpBinaryLarge instead.
             */
            if (PyDict_Check(base_obj))
            {
                PyObject *returned_dict = base_obj;

                /* BORROWED references */
                PyObject *int_bits_py = PyDict_GetItemString(returned_dict, "ib");
                PyObject *frac_bits_py = PyDict_GetItemString(returned_dict, "fb");
                PyObject *scaled_value_py = PyDict_GetItemString(returned_dict, "sv");
                PyObject *is_signed_py = PyDict_GetItemString(returned_dict, "sgn");

                base_obj = FpBinaryLarge_FromBitsPylong(scaled_value_py,
                        pylong_as_fp_int(int_bits_py), pylong_as_fp_int(frac_bits_py),
                        (is_signed_py == Py_True) ? true : false);

                Py_DECREF(returned_dict);
            }
        }
        else if (FpBinary_TpCompare(base_type_id, fp_large_type_id) == 0)
        {
            base_obj = FpBinaryLarge_FromPickleDict(dict);
        }

        Py_DECREF(base_type_id);
    }

    if (base_obj != NULL)
    {
        PyObject *old = (PyObject *)cast_self->base_obj;
        cast_self->base_obj = (fpbinary_base_t *)base_obj;
        Py_XDECREF(old);
    }

    Py_RETURN_NONE;
}

static PyObject *
fpbinary_str(PyObject *obj)
{
    return FP_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(obj),
                     tp_str)(PYOBJ_TO_BASE_FP_PYOBJ(obj));
}

/*
 * See str_ex_doc
 */
static PyObject *
fpbinary_str_ex(PyObject *self)
{
    return FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self),
                          str_ex)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

static PyObject *
fpbinary_richcompare(PyObject *obj1, PyObject *obj2, int operator)
{
    PyObject *result = NULL;
    PyObject *obj1_cast = NULL, *obj2_cast = NULL;

    if (!prepare_binary_ops(obj1, obj2, fp_op_type_none, &obj1_cast,
                            &obj2_cast))
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    result =
        FP_METHOD(obj1_cast, tp_richcompare)(obj1_cast, obj2_cast, operator);
    Py_DECREF(obj1_cast);
    Py_DECREF(obj2_cast);

    return result;
}

static void
fpbinary_dealloc(FpBinaryObject *self)
{
    Py_XDECREF((PyObject *)self->base_obj);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

/*
 * See format_doc
 */
static PyObject *
fpbinary_getformat(PyObject *self, void *closure)
{
    return FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self),
                          fp_getformat)(PYOBJ_TO_BASE_FP_PYOBJ(self), closure);
}

/*
 * See is_signed_doc
 */
static PyObject *
fpbinary_is_signed(PyObject *self, void *closure)
{
    bool is_signed = FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self),
                                    is_signed)(PYOBJ_TO_BASE_FP_PYOBJ(self));
    if (is_signed)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyMethodDef fpbinary_methods[] = {
    {"resize", (PyCFunction)fpbinary_resize, METH_VARARGS | METH_KEYWORDS,
     resize_doc},
    {"str_ex", (PyCFunction)fpbinary_str_ex, METH_NOARGS, str_ex_doc},
    {"bits_to_signed", (PyCFunction)fpbinary_bits_to_signed, METH_NOARGS,
     bits_to_signed_doc},
    {"__copy__", (PyCFunction)fpbinary_copy, METH_NOARGS, copy_doc},

    {"__getitem__", (PyCFunction)fpbinary_getitem, METH_O, NULL},

    /* Pickling functions */
    {"__getstate__", (PyCFunction)fpbinary_getstate, METH_NOARGS, NULL},
    {"__setstate__", (PyCFunction)fpbinary_setstate, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinary_getsetters[] = {
    {"format", (getter)fpbinary_getformat, NULL, format_doc, NULL},
    {"is_signed", (getter)fpbinary_is_signed, NULL, is_signed_doc, NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinary_as_number = {
    .nb_add = (binaryfunc)fpbinary_add,
    .nb_subtract = (binaryfunc)fpbinary_subtract,
    .nb_multiply = (binaryfunc)fpbinary_multiply,
    .nb_true_divide = (binaryfunc)fpbinary_divide,
    .nb_negative = (unaryfunc)fpbinary_negative,
    .nb_int = (unaryfunc)fpbinary_int,
    .nb_index = (unaryfunc)fpbinary_index,

#if PY_MAJOR_VERSION < 3
    .nb_divide = (binaryfunc)fpbinary_divide,
    .nb_long = (unaryfunc)fpbinary_long,
#endif

    .nb_float = (unaryfunc)fpbinary_float,
    .nb_absolute = (unaryfunc)fpbinary_abs,
    .nb_lshift = (binaryfunc)fpbinary_lshift,
    .nb_rshift = (binaryfunc)fpbinary_rshift,
    .nb_nonzero = (inquiry)fpbinary_nonzero,
};

static PyMappingMethods fpbinary_as_mapping = {
    .mp_length = fpbinary_mp_length, .mp_subscript = fpbinary_subscript,
};

PyTypeObject FpBinaryComplex_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinaryComplex",
    .tp_doc = fpbinaryobject_doc,
    .tp_basicsize = sizeof(FpBinaryObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinary_methods,
    .tp_getset = fpbinary_getsetters,
    .tp_as_number = &fpbinary_as_number,
    .tp_as_mapping = &fpbinary_as_mapping,
    .tp_new = (newfunc)PyType_GenericNew,
    .tp_init = (initproc)fpbinary_init,
    .tp_dealloc = (destructor)fpbinary_dealloc,
    .tp_str = fpbinary_str,
    .tp_repr = fpbinary_str,
    .tp_richcompare = fpbinary_richcompare,
};
