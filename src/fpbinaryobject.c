/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinary
 *
 * This object wraps the _FpBinarySmall and _FpBinaryLarge objects. The intent
 * is for FpBinary to do the following tasks:
 *
 *     - select which object to use based on the number of bits required (use
 *       _FpBinarySmall if at all possible)
 *     - ensure operands to binary/ternary operations are the base object type
 *       (i.e. _FpBinarySmall and _FpBinaryLarge do minimal type checking).
 *
 *****************************************************************************/

#include "fpbinaryobject.h"
#include "fpbinaryglobaldoc.h"
#include "fpbinarylarge.h"
#include "fpbinarysmall.h"
#include <math.h>

typedef enum {
    fp_op_type_none,
    fp_op_type_add,
    fp_op_type_mult,
    fp_op_type_div,
} fp_op_type_t;

static inline bool
check_fp_type(PyObject *obj)
{
    return (FpBinarySmall_CheckExact(obj) || FpBinaryLarge_CheckExact(obj));
}

static PyObject *
cast_builtin_to_fp(PyObject *obj)
{
    PyObject *scaled_bits = NULL;
    FP_UINT_TYPE int_bits, frac_bits;

    /* Convert PyInt and PyFloat to 2's complement
     * scaled value PyLong - this will be our bits for the new type.
     */
    if (FpBinary_IntCheck(obj) || PyLong_Check(obj))
    {
        calc_pyint_to_fp_params(obj, &scaled_bits, &int_bits);
        frac_bits = 0;
    }
    else if (PyFloat_Check(obj))
    {
        double scaled_value;
        calc_double_to_fp_params(PyFloat_AsDouble(obj), &scaled_value,
                                 &int_bits, &frac_bits);
        scaled_bits = PyLong_FromDouble(scaled_value);
    }

    if (scaled_bits)
    {
        PyObject *result = NULL;
        ;
        FP_UINT_TYPE total_bits = int_bits + frac_bits;

        if (total_bits <= FP_SMALL_MAX_BITS)
        {
            result = FpBinarySmall_FromBitsPylong(scaled_bits, int_bits,
                                                  frac_bits, true);
        }
        else
        {
            result = FpBinaryLarge_FromBitsPylong(scaled_bits, int_bits,
                                                  frac_bits, true);
        }

        Py_DECREF(scaled_bits);
        return result;
    }

    return NULL;
}
/*
 * Will attempt to create a FpBinary_SmallType object from obj.
 * If obj is a FpBinary_SmallType, its ref counter WILL be incremented
 * and the reference returned. Otherwise, a new object will be created.
 * NULL is returned if obj can't be cast.
 */
static PyObject *
cast_to_fpsmall(PyObject *obj)
{
    PyObject *scaled_bits = NULL;
    FP_UINT_TYPE int_bits, frac_bits;

    if (FpBinarySmall_CheckExact(obj))
    {
        Py_INCREF(obj);
        return obj;
    }
    else if (FpBinaryLarge_CheckExact(obj))
    {
        PyObject *bits = FpBinaryLarge_BitsAsPylong(obj);
        PyObject *result = NULL;
        bool is_signed = FpBinaryLarge_IsSigned(obj);
        FpBinaryLarge_FormatAsUints(obj, &int_bits, &frac_bits);
        result =
            FpBinarySmall_FromBitsPylong(bits, int_bits, frac_bits, is_signed);

        Py_DECREF(bits);

        return result;
    }

    /* Only builtins left. Convert PyInt and PyFloat to 2's complement
     * scaled value PyLong - this will be our bits for the new type.
     */
    if (FpBinary_IntCheck(obj) || PyLong_Check(obj))
    {
        calc_pyint_to_fp_params(obj, &scaled_bits, &int_bits);
        frac_bits = 0;
    }
    else if (PyFloat_Check(obj))
    {
        double scaled_value;
        calc_double_to_fp_params(PyFloat_AsDouble(obj), &scaled_value,
                                 &int_bits, &frac_bits);
        scaled_bits = PyLong_FromDouble(scaled_value);
    }

    if (scaled_bits)
    {
        PyObject *result = FpBinarySmall_FromBitsPylong(scaled_bits, int_bits,
                                                        frac_bits, true);
        Py_DECREF(scaled_bits);
        return result;
    }

    return NULL;
}

/*
 * Will attempt to create a FpBinary_LargeType object from obj.
 * If obj is a FpBinary_LargeType, its ref counter WILL be incremented
 * and the reference returned. Otherwise, a new object will be created.
 * NULL is returned if obj can't be cast.
 */
static PyObject *
cast_to_fplarge(PyObject *obj)
{
    PyObject *scaled_bits = NULL;
    FP_UINT_TYPE int_bits, frac_bits;

    if (FpBinaryLarge_CheckExact(obj))
    {
        Py_INCREF(obj);
        return obj;
    }
    else if (FpBinarySmall_CheckExact(obj))
    {
        PyObject *bits = FpBinarySmall_BitsAsPylong(obj);
        PyObject *result = NULL;
        FpBinarySmall_FormatAsUints(obj, &int_bits, &frac_bits);
        result = FpBinaryLarge_FromBitsPylong(
            bits, int_bits, frac_bits, FP_BASE_METHOD(obj, is_signed)(obj));

        Py_DECREF(bits);

        return result;
    }

    /* Only builtins left. Convert PyInt and PyFloat to 2's complement
     * scaled value PyLong - this will be our bits for the new type.
     */
    if (FpBinary_IntCheck(obj) || PyLong_Check(obj))
    {
        calc_pyint_to_fp_params(obj, &scaled_bits, &int_bits);
        frac_bits = 0;
    }
    else if (PyFloat_Check(obj))
    {
        double scaled_value;
        calc_double_to_fp_params(PyFloat_AsDouble(obj), &scaled_value,
                                 &int_bits, &frac_bits);
        scaled_bits = PyLong_FromDouble(scaled_value);
    }

    if (scaled_bits)
    {
        PyObject *result = FpBinaryLarge_FromBitsPylong(scaled_bits, int_bits,
                                                        frac_bits, true);
        Py_DECREF(scaled_bits);
        return result;
    }

    return NULL;
}

/*
 * Convenience function to make sure the operands of a two operand operation
 * are instances of the same type. At least one object must already be a
 * FpBinarySmallObject or FpBinaryLargeObject instance. If there is one
 * of each, the FpBinarySmallObject is cast to FpBinaryLargeObject. If
 * one is a PyFloat, PyInt or PyLong, a FpBinarySmallObject OR
 * FpBinaryLargeObject will first be created depending on its value and
 * the the other operand will be made to be the same type.
 *
 * If one operand is signed and the other unsigned, the unsigned is cast
 * to signed (without chance of overflow via incrementing the int_bits
 * value).
 *
 * NOTE: It is assumed the user will decrement a reference count on
 * *output_op1 and *output_op2 after calling this function. That is,
 * if an input object is returned in an output object, it will have
 * had its ref counter incremented.
 */
static bool
prepare_binary_ops(PyObject *in_op1, PyObject *in_op2, fp_op_type_t op_type,
                   PyObject **output_op1, PyObject **output_op2)
{
    PyObject *op1 = NULL, *op2 = NULL;

    /* Inital basic check */
    if (FpBinary_CheckExact(in_op1))
    {
        op1 = PYOBJ_TO_BASE_FP_PYOBJ(in_op1);
        Py_INCREF(op1);

        /* Check for int or float conversion of other operand. */
        if (check_supported_builtin(in_op2))
        {
            /* Have an int or float. Convert it to the most suitable type. */
            op2 = cast_builtin_to_fp(in_op2);
        }
        else if (check_fp_type(in_op2))
        {
            op2 = in_op2;
            Py_INCREF(op2);
        }
    }

    if (FpBinary_CheckExact(in_op2))
    {
        op2 = PYOBJ_TO_BASE_FP_PYOBJ(in_op2);
        Py_INCREF(op2);

        /* Check for int or float conversion of other operand. */
        if (check_supported_builtin(in_op1))
        {
            /* Have an int or float. Convert it to the most suitable type. */
            op1 = cast_builtin_to_fp(in_op1);
        }
        else if (check_fp_type(in_op1))
        {
            op1 = in_op1;
            Py_INCREF(op1);
        }
    }

    /* Should now have both operands set to fixed point types. */
    if (!op1 || !op2)
    {
        return false;
    }

    /* Check for both signed or both unsigned. */
    if ((!FP_BASE_METHOD(op1, is_signed)(op1)) &&
        FP_BASE_METHOD(op2, is_signed)(op2))
    {
        PyObject *old = op1;
        op1 = FP_BASE_METHOD(old, to_signed)(old, NULL);
        Py_DECREF(old);
    }
    else if ((!FP_BASE_METHOD(op2, is_signed)(op2)) &&
             FP_BASE_METHOD(op1, is_signed)(op1))
    {
        PyObject *old = op2;
        op2 = FP_BASE_METHOD(old, to_signed)(old, NULL);
        Py_DECREF(old);
    }

    /* Check for sizes. */
    if (op1->ob_type == op2->ob_type)
    {
        *output_op1 = op1;
        *output_op2 = op2;
    }
    else if (FpBinarySmall_Check(op1) && FpBinaryLarge_Check(op2))
    {
        /* One small, one large. Convert to large. */
        *output_op1 = cast_to_fplarge(op1);
        Py_DECREF(op1);
        *output_op2 = op2;
    }
    else if (FpBinarySmall_Check(op2) && FpBinaryLarge_Check(op1))
    {
        /* One small, one large. Convert to large. */
        *output_op2 = cast_to_fplarge(op2);
        Py_DECREF(op2);
        *output_op1 = op1;
    }

    if (*output_op1 && *output_op2)
    {
        /* Check for operation bit increase that requires large type. */
        if (FpBinarySmall_Check(*output_op1))
        {
            bool convert = false;
            FP_UINT_TYPE op1_total_bits =
                FP_BASE_METHOD(*output_op1, get_total_bits)(*output_op1);
            FP_UINT_TYPE op2_total_bits =
                FP_BASE_METHOD(*output_op2, get_total_bits)(*output_op2);

            if (op_type == fp_op_type_add)
            {
                convert = ((op1_total_bits + 1 > FP_SMALL_MAX_BITS) ||
                           (op2_total_bits + 1 > FP_SMALL_MAX_BITS));
            }
            else if (op_type == fp_op_type_mult)
            {
                convert = (op1_total_bits + op2_total_bits > FP_SMALL_MAX_BITS);
            }
            else if (op_type == fp_op_type_div)
            {
                convert = !fpbinarysmall_can_divide_ops(op1_total_bits,
                                                        op2_total_bits);
            }

            if (convert)
            {
                PyObject *old_op1 = *output_op1, *old_op2 = *output_op2;
                *output_op1 = cast_to_fplarge(old_op1);
                *output_op2 = cast_to_fplarge(old_op2);
                Py_DECREF(old_op1);
                Py_DECREF(old_op2);
            }
        }
    }

    return (*output_op1 && *output_op2);
}

static void
check_op_size_for_negating(PyObject *in_op1, PyObject **op1)
{
    if (FpBinarySmall_Check(in_op1))
    {
        if (FP_BASE_METHOD(in_op1, get_total_bits)(in_op1) + 1 >
            FP_SMALL_MAX_BITS)
        {
            *op1 = cast_to_fplarge(in_op1);
            return;
        }
    }

    /* No change required - set output objects */
    Py_INCREF(in_op1);
    *op1 = in_op1;
}

/*
 *
 *
 * ============================================================================
 */

static FpBinaryObject *
fpbinary_from_base_fp(fpbinary_base_t *base_obj)
{
    FpBinaryObject *self =
        (FpBinaryObject *)FpBinary_Type.tp_alloc(&FpBinary_Type, 0);
    if (self)
    {
        self->base_obj = base_obj;
    }
    return self;
}

static bool
fpbinary_populate_with_params(FpBinaryObject *self, long int_bits,
                              long frac_bits, bool is_signed, double value,
                              PyObject *bit_field, PyObject *format_instance)
{
    fpbinary_base_t *base_obj = NULL;

    if (format_instance)
    {
        if (FpBinary_Check(format_instance))
        {
            format_instance = PYOBJ_TO_BASE_FP_PYOBJ(format_instance);
        }

        if (check_fp_type(format_instance))
        {
            PyObject *format_tuple = FP_BASE_METHOD(
                format_instance, fp_getformat)(format_instance, NULL);
            PyObject *int_bits_py = NULL, *frac_bits_py = NULL;

            if (extract_fp_format_from_tuple(format_tuple, &int_bits_py,
                                             &frac_bits_py))
            {
                int_bits = pylong_as_fp_int(int_bits_py);
                frac_bits = pylong_as_fp_int(frac_bits_py);

                Py_DECREF(int_bits_py);
                Py_DECREF(frac_bits_py);
            }

            Py_DECREF(format_tuple);
        }
        else
        {
            PyErr_SetString(PyExc_TypeError,
                            "format_inst must be a FpBinary instance.");
            return false;
        }
    }

    if (int_bits + frac_bits < 1)
    {
        PyErr_SetString(PyExc_ValueError, "The total number of bits in an "
                                          "fpbinary instance must be greater "
                                          "than 0.");
        return false;
    }

    if (int_bits + frac_bits <= (long)FP_SMALL_MAX_BITS)
    {
        if (bit_field)
        {
            base_obj = (fpbinary_base_t *)FpBinarySmall_FromBitsPylong(
                bit_field, int_bits, frac_bits, is_signed);
        }
        else
        {
            base_obj = (fpbinary_base_t *)FpBinarySmall_FromDouble(
                value, int_bits, frac_bits, is_signed, OVERFLOW_SAT,
                ROUNDING_NEAR_POS_INF);
        }
    }
    else
    {
        if (bit_field)
        {
            base_obj = (fpbinary_base_t *)FpBinaryLarge_FromBitsPylong(
                bit_field, int_bits, frac_bits, is_signed);
        }
        else
        {
            base_obj = (fpbinary_base_t *)FpBinaryLarge_FromDouble(
                value, int_bits, frac_bits, is_signed, OVERFLOW_SAT,
                ROUNDING_NEAR_POS_INF);
        }
    }

    if (base_obj)
    {
        PyObject *old = (PyObject *)self->base_obj;
        self->base_obj = base_obj;
        Py_XDECREF(old);

        return true;
    }

    return false;
}

PyDoc_STRVAR(
    fpbinaryobject_doc,
    "FpBinary(int_bits=1, frac_bits=0, signed=True, value=0.0, bit_field=None, "
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

static int
fpbinary_init(PyObject *self_pyobj, PyObject *args, PyObject *kwds)
{
    long int_bits = 1, frac_bits = 0;
    bool is_signed = true;
    double value = 0.0;
    PyObject *bit_field = NULL, *format_instance = NULL;
    FpBinaryObject *self = (FpBinaryObject *)self_pyobj;

    if (self)
    {
        if (!fp_binary_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                        &is_signed, &value, &bit_field,
                                        &format_instance))
        {
            return -1;
        }

        if (fpbinary_populate_with_params(self, int_bits, frac_bits, is_signed,
                                          value, bit_field, format_instance))
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
fpbinary_resize(FpBinaryObject *self, PyObject *args, PyObject *kwds)
{
    /*
     * Need to extract the format object so we know what the final
     * base type needs to be.
     */
    PyObject *result = NULL;
    PyObject *format_inst;
    PyObject *new_int_bits_py, *new_frac_bits_py;
    long new_int_bits, new_frac_bits;
    fp_overflow_mode_t overflow_mode = OVERFLOW_WRAP;
    fp_round_mode_t round_mode = ROUNDING_DIRECT_NEG_INF;

    static char *kwlist[] = {"format", "overflow_mode", "round_mode", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|ii", kwlist, &format_inst,
                                     &overflow_mode, &round_mode))
        return NULL;

    if (check_fp_type(format_inst))
    {
        format_inst =
            FP_BASE_METHOD(format_inst, fp_getformat)(format_inst, NULL);
    }
    else if (FpBinary_Check(format_inst))
    {
        format_inst = FP_BASE_METHOD(((FpBinaryObject *)format_inst)->base_obj,
                                     fp_getformat)(
            (PyObject *)((FpBinaryObject *)format_inst)->base_obj, NULL);
    }
    else
    {
        /* Because we are decrefing at the end */
        Py_INCREF(format_inst);
    }

    if (!format_inst || !PyTuple_Check(format_inst))
    {
        PyErr_SetString(PyExc_TypeError, "Unsupported type for format.");
        return false;
        return NULL;
    }

    if (!extract_fp_format_from_tuple(format_inst, &new_int_bits_py,
                                      &new_frac_bits_py))
    {
        return NULL;
    }

    new_int_bits = pylong_as_fp_int(new_int_bits_py);
    new_frac_bits = pylong_as_fp_int(new_frac_bits_py);

    if (FpBinarySmall_Check(self->base_obj) &&
        ((unsigned long long)(new_int_bits + new_frac_bits)) >
            FP_SMALL_MAX_BITS)
    {
        PyObject *tmp = (PyObject *)self->base_obj;
        self->base_obj = (fpbinary_base_t *)cast_to_fplarge(tmp);
        Py_DECREF(tmp);
    }

    result = FP_BASE_METHOD(self->base_obj, resize)(
        (PyObject *)self->base_obj,
        Py_BuildValue("(Oii)", format_inst, overflow_mode, round_mode), NULL);
    Py_DECREF(format_inst);

    /* Now check if we can reduce down to a small type */
    if (result)
    {
        Py_DECREF(self->base_obj);
        self->base_obj = (fpbinary_base_t *)result;

        /* Change type if small enough. */
        if (FpBinaryLarge_Check(self->base_obj) &&
            FP_BASE_METHOD(self->base_obj, get_total_bits)(
                PYOBJ_TO_BASE_FP_PYOBJ(self)) <= FP_SMALL_MAX_BITS)
        {
            PyObject *new_obj = cast_to_fpsmall((PyObject *)self->base_obj);
            Py_DECREF(self->base_obj);
            self->base_obj = (fpbinary_base_t *)new_obj;
        }

        Py_INCREF(self);
        return (PyObject *)self;
    }

    return NULL;
}

/*
 * See bits_to_signed_doc
 */
static PyObject *
fpbinary_bits_to_signed(FpBinaryObject *self, PyObject *args)
{
    return FP_BASE_METHOD(self->base_obj,
                          bits_to_signed)((PyObject *)self->base_obj, args);
}

/*
 * See copy_doc
 */
static PyObject *
fpbinary_copy(FpBinaryObject *self, PyObject *args)
{
    return (PyObject *)fpbinary_from_base_fp((fpbinary_base_t *)FP_BASE_METHOD(
        self->base_obj, copy)((PyObject *)self->base_obj, args));
}

/*
 *
 * Numeric methods implementation
 *
 */
static PyObject *
fpbinary_add(PyObject *op1, PyObject *op2)
{
    FpBinaryObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    if (prepare_binary_ops(op1, op2, fp_op_type_add, &cast_op1, &cast_op2))
    {
        result = fpbinary_from_base_fp(
            PYOBJ_FP_BASE(FP_NUM_METHOD(cast_op1, nb_add)(cast_op1, cast_op2)));

        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinary_subtract(PyObject *op1, PyObject *op2)
{
    FpBinaryObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    if (prepare_binary_ops(op1, op2, fp_op_type_add, &cast_op1, &cast_op2))
    {

        result = fpbinary_from_base_fp(PYOBJ_FP_BASE(
            FP_NUM_METHOD(cast_op1, nb_subtract)(cast_op1, cast_op2)));

        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinary_multiply(PyObject *op1, PyObject *op2)
{
    FpBinaryObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    if (prepare_binary_ops(op1, op2, fp_op_type_mult, &cast_op1, &cast_op2))
    {
        result = fpbinary_from_base_fp(PYOBJ_FP_BASE(
            FP_NUM_METHOD(cast_op1, nb_multiply)(cast_op1, cast_op2)));

        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinary_divide(PyObject *op1, PyObject *op2)
{
    FpBinaryObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    if (prepare_binary_ops(op1, op2, fp_op_type_div, &cast_op1, &cast_op2))
    {
        result = fpbinary_from_base_fp(PYOBJ_FP_BASE(
            FP_NUM_METHOD(cast_op1, nb_true_divide)(cast_op1, cast_op2)));

        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        FPBINARY_RETURN_NOT_IMPLEMENTED;
    }

    return (PyObject *)result;
}

static PyObject *
fpbinary_negative(PyObject *self)
{
    FpBinaryObject *result = NULL;
    PyObject *op1 = NULL;

    check_op_size_for_negating(PYOBJ_TO_BASE_FP_PYOBJ(self), &op1);

    result = fpbinary_from_base_fp(
        PYOBJ_FP_BASE(FP_NUM_METHOD(op1, nb_negative)(op1)));
    Py_DECREF(op1);

    return (PyObject *)result;
}

static PyObject *
fpbinary_int(PyObject *self)
{
    return FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                         nb_int)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

#if PY_MAJOR_VERSION < 3

static PyObject *
fpbinary_long(PyObject *self)
{
    return FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                         nb_long)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

#endif

static PyObject *
fpbinary_index(PyObject *self)
{
    return FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                         nb_index)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

static PyObject *
fpbinary_float(PyObject *self)
{
    return FP_NUM_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                         nb_float)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

static PyObject *
fpbinary_abs(PyObject *self)
{
    FpBinaryObject *result = NULL;
    PyObject *op1 = NULL;

    /* If negative, abs will involve negating, which adds a bit.
     * So check if we need an object size change.
     */
    if (FpBinarySmall_Check(PYOBJ_TO_BASE_FP_PYOBJ(self)))
    {
        if (FpBinarySmall_IsNegative(PYOBJ_TO_BASE_FP_PYOBJ(self)))
        {
            check_op_size_for_negating(PYOBJ_TO_BASE_FP_PYOBJ(self), &op1);
        }
    }

    if (!op1)
    {
        op1 = PYOBJ_TO_BASE_FP_PYOBJ(self);
        Py_INCREF(op1);
    }

    result = fpbinary_from_base_fp(
        PYOBJ_FP_BASE(FP_NUM_METHOD(op1, nb_absolute)(op1)));
    Py_DECREF(op1);

    return (PyObject *)result;
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
 *
 * Sequence methods implementation
 *
 */

static Py_ssize_t
fpbinary_sq_length(PyObject *self)
{
    return FP_SQ_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                        sq_length)(PYOBJ_TO_BASE_FP_PYOBJ(self));
}

static PyObject *
fpbinary_sq_item(PyObject *self, Py_ssize_t py_index)
{
    return FP_SQ_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                        sq_item)(PYOBJ_TO_BASE_FP_PYOBJ(self), py_index);
}

/*
 * If slice notation is invoked on an fpbinaryobject, a new fpbinaryobject is
 * created
 * as an unsigned integer where the value is the value of the selected bits.
 *
 * This is useful for digital logic implementations of NCOs and trig lookup
 * tables.
 */
#if PY_MAJOR_VERSION < 3

static PyObject *
fpbinary_sq_slice(PyObject *self, Py_ssize_t index1, Py_ssize_t index2)
{
    PyObject *sliced_base_obj =
        FP_SQ_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                     sq_slice)(PYOBJ_TO_BASE_FP_PYOBJ(self), index1, index2);
    if (sliced_base_obj)
    {
        FpBinaryObject *result =
            fpbinary_from_base_fp(PYOBJ_FP_BASE(sliced_base_obj));
        return (PyObject *)result;
    }

    return NULL;
}

#endif

static PyObject *
fpbinary_subscript(PyObject *self, PyObject *item)
{
    PyObject *sliced_base_obj =
        FP_MP_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(self),
                     mp_subscript)(PYOBJ_TO_BASE_FP_PYOBJ(self), item);

    if (sliced_base_obj == NULL)
    {
        return NULL;
    }

    if (check_fp_type(sliced_base_obj))
    {
        FpBinaryObject *result =
            fpbinary_from_base_fp(PYOBJ_FP_BASE(sliced_base_obj));
        return (PyObject *)result;
    }

    /* Should be a PyBool (from a get item indexing function) */
    return sliced_base_obj;
}

static PyObject *
fpbinary_getitem(PyObject *self, PyObject *item)
{
    return FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self),
                          getitem)(PYOBJ_TO_BASE_FP_PYOBJ(self), item);
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

static PySequenceMethods fpbinary_as_sequence = {
    .sq_length = (lenfunc)fpbinary_sq_length,
    .sq_item = (ssizeargfunc)fpbinary_sq_item,

#if PY_MAJOR_VERSION < 3

    .sq_slice = (ssizessizeargfunc)fpbinary_sq_slice,

#endif
};

static PyMappingMethods fpbinary_as_mapping = {
    .mp_length = fpbinary_sq_length, .mp_subscript = fpbinary_subscript,
};

PyTypeObject FpBinary_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinary",
    .tp_doc = fpbinaryobject_doc,
    .tp_basicsize = sizeof(FpBinaryObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinary_methods,
    .tp_getset = fpbinary_getsetters,
    .tp_as_number = &fpbinary_as_number,
    .tp_as_sequence = &fpbinary_as_sequence,
    .tp_as_mapping = &fpbinary_as_mapping,
    .tp_new = (newfunc)PyType_GenericNew,
    .tp_init = (initproc)fpbinary_init,
    .tp_dealloc = (destructor)fpbinary_dealloc,
    .tp_str = fpbinary_str,
    .tp_repr = fpbinary_str,
    .tp_richcompare = fpbinary_richcompare,
};
