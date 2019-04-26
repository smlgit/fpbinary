#include "fpbinaryobject.h"
#include "fpbinarylarge.h"
#include "fpbinarysmall.h"
#include <math.h>

typedef enum {
    fp_op_type_none,
    fp_op_type_add,
    fp_op_type_mult,
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
            if (op_type == fp_op_type_add)
            {
                convert =
                    ((FP_BASE_METHOD(*output_op1, get_total_bits)(*output_op1) +
                          1 >
                      FP_SMALL_MAX_BITS) ||
                     (FP_BASE_METHOD(*output_op2, get_total_bits)(*output_op2) +
                          1 >
                      FP_SMALL_MAX_BITS));
            }
            else if (op_type == fp_op_type_mult)
            {
                convert =
                    (FP_BASE_METHOD(*output_op1, get_total_bits)(*output_op1) +
                         FP_BASE_METHOD(*output_op2,
                                        get_total_bits)(*output_op2) >
                     FP_SMALL_MAX_BITS);
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
                int_bits = pylong_as_fp_uint(int_bits_py);
                frac_bits = pylong_as_fp_uint(frac_bits_py);

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
        self->base_obj = base_obj;
    }

    return self->base_obj;
}

FpBinaryObject *
FpBinary_FromParams(long int_bits, long frac_bits, bool is_signed, double value,
                    PyObject *bit_field, PyObject *format_instance)
{
    FpBinaryObject *self =
        (FpBinaryObject *)FpBinary_Type.tp_alloc(&FpBinary_Type, 0);

    if (self)
    {
        if (fpbinary_populate_with_params(self, int_bits, frac_bits, is_signed,
                                          value, bit_field, format_instance))
        {
            return self;
        }
        else
        {
            Py_DECREF(self);
        }
    }

    return NULL;
}

static PyObject *
fpbinary_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    long int_bits = 1, frac_bits = 0;
    bool is_signed = true;
    double value = 0.0;
    PyObject *bit_field = NULL, *format_instance = NULL;
    FpBinaryObject *self = (FpBinaryObject *)type->tp_alloc(type, 0);

    if (self)
    {
        if (!fp_binary_new_params_parse(args, kwds, &int_bits, &frac_bits,
                                        &is_signed, &value, &bit_field,
                                        &format_instance))
        {
            return NULL;
        }

        if (fpbinary_populate_with_params(self, int_bits, frac_bits, is_signed,
                                          value, bit_field, format_instance))
        {
            return (PyObject *)self;
        }

        Py_DECREF(self);
    }

    return NULL;
}

static PyObject *
fpbinary_resize(FpBinaryObject *self, PyObject *args, PyObject *kwds)
{
    PyObject *result = FP_BASE_METHOD(self->base_obj, resize)(
        (PyObject *)self->base_obj, args, kwds);

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
 * The bits represented in the passed fixed point object are interpreted as
 * a signed 2's complement integer and returned as a PyLong.
 * NOTE: if self is an unsigned object, the MSB, as defined by the int_bits
 * and frac_bits values, will be considered a sign bit.
 */
static PyObject *
fpbinary_bits_to_signed(FpBinaryObject *self, PyObject *args)
{
    return FP_BASE_METHOD(self->base_obj,
                          bits_to_signed)((PyObject *)self->base_obj, args);
}

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

    if (prepare_binary_ops(op1, op2, fp_op_type_mult, &cast_op1, &cast_op2))
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

static PyObject *
fpbinary_str(PyObject *obj)
{
    return FP_METHOD(PYOBJ_TO_BASE_FP_PYOBJ(obj),
                     tp_str)(PYOBJ_TO_BASE_FP_PYOBJ(obj));
}

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

static PyObject *
fpbinary_getformat(PyObject *self, void *closure)
{
    return FP_BASE_METHOD(PYOBJ_TO_BASE_FP(self),
                          fp_getformat)(PYOBJ_TO_BASE_FP_PYOBJ(self), closure);
}

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

/*
 * Returns the maximum bit width of fixed point numbers this object can
 * represent. If there is no limit, None is returned.
 */
static PyObject *
fpbinary_get_max_bits(PyObject *cls)
{
    Py_RETURN_NONE;
}

static PyMethodDef fpbinary_methods[] = {
    {"resize", (PyCFunction)fpbinary_resize, METH_VARARGS | METH_KEYWORDS,
     "Resize the fixed point binary object."},
    {"str_ex", (PyCFunction)fpbinary_str_ex, METH_NOARGS,
     "Extended version of str that provides max precision."},
    {"bits_to_signed", (PyCFunction)fpbinary_bits_to_signed, METH_NOARGS,
     "Interpret the bits of the fixed point binary object as a 2's complement "
     "long integer."},
    {"__copy__", (PyCFunction)fpbinary_copy, METH_NOARGS,
     "Shallow copy the fixed point binary object."},
    {"get_max_bits", (PyCFunction)fpbinary_get_max_bits, METH_CLASS,
     "Returns max number of bits representable with this object."},

    {"__getitem__", (PyCFunction)fpbinary_getitem, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinary_getsetters[] = {
    {"format", (getter)fpbinary_getformat, NULL, "Format tuple", NULL},
    {"is_signed", (getter)fpbinary_is_signed, NULL, "Returns True if signed.",
     NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinary_as_number = {
    .nb_add = (binaryfunc)fpbinary_add,
    .nb_subtract = (binaryfunc)fpbinary_subtract,
    .nb_multiply = (binaryfunc)fpbinary_multiply,
    .nb_true_divide = (binaryfunc)fpbinary_divide,
    .nb_negative = (unaryfunc)fpbinary_negative,
    .nb_int = (unaryfunc)fpbinary_int,

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
    .tp_doc = "Fixed point binary objects",
    .tp_basicsize = sizeof(FpBinaryObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinary_methods,
    .tp_getset = fpbinary_getsetters,
    .tp_as_number = &fpbinary_as_number,
    .tp_as_sequence = &fpbinary_as_sequence,
    .tp_as_mapping = &fpbinary_as_mapping,
    .tp_new = (newfunc)fpbinary_new,
    .tp_dealloc = (destructor)fpbinary_dealloc,
    .tp_str = fpbinary_str,
    .tp_repr = fpbinary_str,
    .tp_richcompare = fpbinary_richcompare,
};
