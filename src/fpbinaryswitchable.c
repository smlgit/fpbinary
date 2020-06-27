/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * FpBinarySwitchable
 *
 * This object is composed of an FpBinary object (fp_mode_value) and a float
 * (float_value). The user specifies the fixed point mode (fp_mode) at
 * constructor time - this dictates whether fixed point or floating point mode
 * is used for math operations. This also dictates how operands and input
 * values are cast.
 *
 * The object also provides a value tracking mechanism via the value property.
 * The user can set the current "value" and the object tracks the min and max
 * values over the lifetime of the object.
 *
 * The intent is for this object to be used in simulation environments and
 * only where it makes sense to switch between fixed and floating point math.
 * So not all services of FpBinary are implemented by FpBinarySwitchable.
 *
 *****************************************************************************/

#include "fpbinaryswitchable.h"
#include "fpbinaryglobaldoc.h"
#include <math.h>

static PyObject *resize_method_name_str = NULL;
static PyObject *get_format_method_name_str = NULL;
static PyObject *py_default_format_tuple = NULL;

static PyObject *
forward_call_with_args(PyObject *obj, PyObject *method_name, PyObject *args,
                       PyObject *kwds)
{
    PyObject *callable = PyObject_GetAttr(obj, method_name);
    if (callable)
    {
        if (!args)
        {
            PyObject *dummy_tup = PyTuple_New(0);
            PyObject *result = PyObject_Call(callable, dummy_tup, kwds);

            Py_DECREF(dummy_tup);
            return result;
        }
        else
        {
            return PyObject_Call(callable, args, kwds);
        }
    }

    return NULL;
}

static bool
extract_double(PyObject *op1, double *op1_double)
{
    if (FpBinarySwitchable_Check(op1))
    {
        *op1_double = ((FpBinarySwitchableObject *)op1)->dbl_mode_value;
    }
    else
    {
        PyObject *py_float = FP_NUM_METHOD(op1, nb_float)(op1);
        if (!py_float)
        {
            return false;
        }

        *op1_double = PyFloat_AsDouble(py_float);
        Py_DECREF(py_float);
    }

    return true;
}

/*
 * Will produce a PyObject from op1. If op1 is a FpBinarySwitchableObject
 * type, its underlying fp object will be returned if it is in fp_mode,
 * otherwise a PyFloat will be created from its double mode value.
 *
 * If op1 is not a FpBinarySwitchableObject type, its ref count will be
 * incremented and it will be returned.
 *
 * Returns a NEW reference.
 */
static PyObject *
extract_fp_op_object(PyObject *op1)
{
    if (FpBinarySwitchable_Check(op1))
    {
        if (((FpBinarySwitchableObject *)op1)->fp_mode)
        {
            Py_INCREF(((FpBinarySwitchableObject *)op1)->fp_mode_value);
            return ((FpBinarySwitchableObject *)op1)->fp_mode_value;
        }
        else
        {
            return PyFloat_FromDouble(
                ((FpBinarySwitchableObject *)op1)->dbl_mode_value);
        }
    }
    else
    {
        Py_INCREF(op1);
        return op1;
    }
}

/*
 * Converts op1 and op2 to doubles. Returns true if successful.
 */
static bool
prepare_binary_ops_double(PyObject *op1, PyObject *op2, double *op1_double,
                          double *op2_double)
{
    return (extract_double(op1, op1_double) && extract_double(op2, op2_double));
}

/*
 * Extracts the actual PyObject instances to apply as parameters to the
 * binary operation. These will be applied to op1_out/op2_out. In addition,
 * the correct PyObject instance that acts at the function master will be
 * returned.
 *
 * If NULL is returned, this means that a double operation is required and
 * the parameters should be extracted via prepare_binary_ops_double.
 *
 * NOTE: If an object is returned, the references in op1_out/op2_out MUST
 * be decremented by the calling function.
 */
static PyObject *
prepare_binary_ops_fp(PyObject *op1, PyObject *op2, PyObject **op1_out,
                      PyObject **op2_out)
{
    /*
     *
     *First determine if we should use fixed point mode.
     *
     *Rules:
     * - If both operands are FpBinarySwitchObject types, the mode is
     * obvious if they share fp_mode. If only one is in fp_mode,
     * fixed point operation takes precedence.
     *
     * - If one operand is a FpBinarySwitchObject type, its mode is used.
     */
    if (!((FpBinarySwitchable_Check(op1) &&
           ((FpBinarySwitchableObject *)op1)->fp_mode) ||
          (FpBinarySwitchable_Check(op2) &&
           ((FpBinarySwitchableObject *)op2)->fp_mode)))
    {
        return NULL;
    }

    *op1_out = *op2_out = NULL;

    *op1_out = extract_fp_op_object(op1);
    *op2_out = extract_fp_op_object(op2);

    /* Fixed point type functions take priority
     * TODO: This is a bit dodgy. If someone creates a new fixed point type that
     * doesn't extend from FpBinary, this could actually call a non-fixed point
     * type instead.
     */
    if (FpBinary_Check(*op1_out))
        return *op1_out;
    if (FpBinary_Check(*op2_out))
        return *op2_out;

    return *op1_out;
}

static void
fpbinaryswitchable_populate_new(FpBinarySwitchableObject *self, bool fp_mode,
                                PyObject *fp_mode_value, double dbl_mode_value)
{
    if (self)
    {
        self->fp_mode = fp_mode;

        /* If we are replacing the FP object, dec the ref counter for the
         * old object. Note that the generic allocation method does set
         * all objects to NULL, so this should be safe when called from
         * __new__.*/
        Py_XDECREF(self->fp_mode_value);
        self->fp_mode_value = NULL;

        if (fp_mode)
        {
            self->fp_mode_value = fp_mode_value;
            self->dbl_mode_value = self->dbl_mode_min_value =
                self->dbl_mode_max_value = 0.0;
        }
        else
        {
            self->dbl_mode_value = self->dbl_mode_min_value =
                self->dbl_mode_max_value = dbl_mode_value;
        }
    }
}

/*
 * fp_mode_value ref count IS NOT incremented.
 */
static void
fpbinaryswitchable_populate_all(FpBinarySwitchableObject *self, bool fp_mode,
                                PyObject *fp_mode_value, double dbl_mode_value,
                                double dbl_mode_min_value,
                                double dbl_mode_max_value)
{
    if (self)
    {
        /* If we are replacing the FP object, dec the ref counter for the
         * old object. Note that the generic allocation method does set
         * all objects to NULL, so this should be safe when called from
         * __new__.*/
        Py_XDECREF(self->fp_mode_value);
        self->fp_mode_value = fp_mode_value;

        self->fp_mode = fp_mode;
        self->dbl_mode_value = dbl_mode_value;
        self->dbl_mode_min_value = dbl_mode_min_value;
        self->dbl_mode_max_value = dbl_mode_max_value;
    }
}

static FpBinarySwitchableObject *
fpbinaryswitchable_from_params(bool fp_mode, PyObject *fp_mode_value,
                               double dbl_mode_value)
{
    FpBinarySwitchableObject *self =
        (FpBinarySwitchableObject *)FpBinarySwitchable_Type.tp_alloc(
            &FpBinarySwitchable_Type, 0);

    if (self)
    {
        fpbinaryswitchable_populate_new(self, fp_mode, fp_mode_value,
                                        dbl_mode_value);
    }

    return self;
}

PyDoc_STRVAR(
    fpbinaryswitchable_doc,
    "FpBinarySwitchable(fp_mode, fp_value=None, float_value=0.0)\n"
    "--\n\n"
    "Represents a fixed point OR floating point value depending on the `fp_mode` constructor parameter.\n"
    "Also enables tracking of minimum and maximum values during the lifetime of the object via the `value` property.\n"
    "This class can be used in math and resize operations and act like an FpBinary instance when `fp_mode` is \n"
    "True and act sensibly (normally do nothing) when `fp_mode` is False.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "fp_mode : bool\n"
    "    If True, the underlying data/operations are fixed point.\n"
    "    If False, a native double type will be used.\n"
    "    If True, the `fp_value` parameter must be set to an FpBinary instance.\n"
    "    This instance will define the fixed point data and initial value.\n"
    "\n"
    "fp_value : FpBinary, optional if `fp_mode` is False.\n"
    "    This instance will define the fixed point data and initial value.\n"
    "\n"
    "float_value : float, optional\n"
    "    Sets the initial value when `fp_mode` is False. Defaults to 0.0.\n"
    "\n");
static int
fpbinaryswitchable_init(PyObject *self, PyObject *args, PyObject *kwds)
{
    /*
     * Parameters for FpBinarySwitchableObject:
     * fp_mode - required. true is fixed point mode, false is floating point
     * mode.
     * fp_value and float_value - these can be set to initialise the object when
     * running in the corresponding mode. float_value isn't required if fp_value
     * is set (in float mode, float_value will be set to float(fp_value) ).
     */

    PyObject *fp_mode_in = NULL, *fp_value_in = NULL, *float_value_in = NULL,
             *value_obj = NULL;
    bool fp_mode = true;
    double dbl_val = 0.0;

    static char *kwlist[] = {"fp_mode", "fp_value", "float_value", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|OO", kwlist, &fp_mode_in,
                                     &fp_value_in, &float_value_in))
        return -1;

    if (!fp_mode_in && PyBool_Check(fp_mode_in))
    {
        PyErr_SetString(PyExc_TypeError, "fp_mode must be True or False.");
        return -1;
    }

    if (fp_mode_in == Py_False)
        fp_mode = false;

    if (fp_mode)
    {
        if (fp_value_in && FpBinary_Check(fp_value_in))
        {
            Py_INCREF(fp_value_in);
            value_obj = fp_value_in;
        }
        else
        {
            PyErr_SetString(
                PyExc_TypeError,
                "Fixed point mode value must be an instance of FpBinary.");
            return -1;
        }
    }
    else
    {
        if (FP_NUM_METHOD_PRESENT(float_value_in, nb_float))
        {
            FP_NUM_UNI_OP_INPLACE(float_value_in, nb_float);
            dbl_val = PyFloat_AsDouble(float_value_in);
        }
        else if (!float_value_in)
        {
            /* If the user didn't give any args, default to zero */
            dbl_val = 0.0;
        }
        else
        {
            PyErr_SetString(
                PyExc_TypeError,
                "Floating point mode value must be convertable to float.");
            return -1;
        }
    }

    fpbinaryswitchable_populate_new((FpBinarySwitchableObject *)self, fp_mode,
                                    value_obj, dbl_val);
    return 0;
}

FP_GLOBAL_Doc_STRVAR(
    fpswitchable_resize_doc,
    "resize(format, overflow_mode=fpbinary.OverflowEnum.wrap, "
    "round_mode=fpbinary.RoundingEnum.direct_neg_inf)\n"
    "--\n"
    "\n"
    "If `fp_mode` is True, this method does what FpBinary.resize() does.\n"
    "If `fp_mode` is False, nothing is done.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "See FpBinary.resize()\n"
    "\n"
    "Returns\n"
    "----------\n"
    "FpBinarySwitchable\n"
    "    `self`\n");
static PyObject *
fpbinaryswitchable_resize(FpBinarySwitchableObject *self, PyObject *args,
                          PyObject *kwds)
{
    PyObject *call_result = NULL;

    if (!self->fp_mode)
    {
        Py_INCREF(self);
        return (PyObject *)self;
    }

    call_result = forward_call_with_args(self->fp_mode_value,
                                         resize_method_name_str, args, kwds);

    /* Don't use the resize return reference (it is the same as base_obj). */
    if (call_result)
    {
        Py_DECREF(call_result);
        Py_INCREF(self);
        return (PyObject *)self;
    }

    return NULL;
}

/*
 * Picking functions __getstate__ and __setstate__ .
 *
 * The pickling strategy is the same as that for FpBinary.
 * This means that protocols < 2 are not supported.
 */

static PyObject *
fpbinaryswitchable_getstate(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    PyObject *dict = PyDict_New();

    if (dict != NULL)
    {
        PyObject *dbl_mode_value_py =
            PyFloat_FromDouble(cast_self->dbl_mode_value);
        PyObject *dbl_mode_max_value_py =
            PyFloat_FromDouble(cast_self->dbl_mode_max_value);
        PyObject *dbl_mode_min_value_py =
            PyFloat_FromDouble(cast_self->dbl_mode_min_value);

        PyDict_SetItemString(dict, "fpm",
                             (cast_self->fp_mode == true) ? Py_True : Py_False);
        PyDict_SetItemString(dict, "dv", dbl_mode_value_py);
        PyDict_SetItemString(dict, "dmax", dbl_mode_max_value_py);
        PyDict_SetItemString(dict, "dmin", dbl_mode_min_value_py);

        if (cast_self->fp_mode_value != NULL)
        {
            PyDict_SetItemString(dict, "fpv", cast_self->fp_mode_value);
        }

        Py_DECREF(dbl_mode_value_py);
        Py_DECREF(dbl_mode_max_value_py);
        Py_DECREF(dbl_mode_min_value_py);
    }

    return dict;
}

static PyObject *
fpbinaryswitchable_setstate(PyObject *self, PyObject *dict)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    PyObject *fp_mode_py, *fp_mode_value_py, *dbl_mode_value_py,
        *dbl_mode_min_value_py, *dbl_mode_max_value_py;

    fp_mode_py = PyDict_GetItemString(dict, "fpm");
    fp_mode_value_py = PyDict_GetItemString(dict, "fpv");
    dbl_mode_value_py = PyDict_GetItemString(dict, "dv");
    dbl_mode_min_value_py = PyDict_GetItemString(dict, "dmin");
    dbl_mode_max_value_py = PyDict_GetItemString(dict, "dmax");

    if (fp_mode_py && dbl_mode_value_py && dbl_mode_min_value_py &&
        dbl_mode_max_value_py && (fp_mode_py == Py_False || fp_mode_value_py))
    {
        if (fp_mode_value_py)
        {
            Py_INCREF(fp_mode_value_py);
        }

        fpbinaryswitchable_populate_all(
            cast_self, (fp_mode_py == Py_True) ? true : false, fp_mode_value_py,
            PyFloat_AsDouble(dbl_mode_value_py),
            PyFloat_AsDouble(dbl_mode_min_value_py),
            PyFloat_AsDouble(dbl_mode_max_value_py));
    }
    else
    {
        PyErr_SetString(PyExc_KeyError,
                        "Pickle dict didn't have a required key.");
    }

    Py_RETURN_NONE;
}

/*
 *
 * Numeric methods implementation
 *
 */
static PyObject *
fpbinaryswitchable_add(PyObject *op1, PyObject *op2)
{
    FpBinarySwitchableObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    PyObject *function_op =
        prepare_binary_ops_fp(op1, op2, &cast_op1, &cast_op2);

    if (function_op)
    {
        result = fpbinaryswitchable_from_params(
            true, FP_NUM_METHOD(function_op, nb_add)(cast_op1, cast_op2), 0.0);
        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        double dbl_op1, dbl_op2;
        prepare_binary_ops_double(op1, op2, &dbl_op1, &dbl_op2);
        result = fpbinaryswitchable_from_params(false, NULL, dbl_op1 + dbl_op2);
    }

    return (PyObject *)result;
}

static PyObject *
fpbinaryswitchable_subtract(PyObject *op1, PyObject *op2)
{
    FpBinarySwitchableObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    PyObject *function_op =
        prepare_binary_ops_fp(op1, op2, &cast_op1, &cast_op2);

    if (function_op)
    {
        result = fpbinaryswitchable_from_params(
            true, FP_NUM_METHOD(function_op, nb_subtract)(cast_op1, cast_op2),
            0.0);
        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        double dbl_op1, dbl_op2;
        prepare_binary_ops_double(op1, op2, &dbl_op1, &dbl_op2);
        result = fpbinaryswitchable_from_params(false, NULL, dbl_op1 - dbl_op2);
    }

    return (PyObject *)result;
}

static PyObject *
fpbinaryswitchable_multiply(PyObject *op1, PyObject *op2)
{
    FpBinarySwitchableObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    PyObject *function_op =
        prepare_binary_ops_fp(op1, op2, &cast_op1, &cast_op2);

    if (function_op)
    {
        result = fpbinaryswitchable_from_params(
            true, FP_NUM_METHOD(function_op, nb_multiply)(cast_op1, cast_op2),
            0.0);
        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        double dbl_op1, dbl_op2;
        prepare_binary_ops_double(op1, op2, &dbl_op1, &dbl_op2);
        result = fpbinaryswitchable_from_params(false, NULL, dbl_op1 * dbl_op2);
    }

    return (PyObject *)result;
}

static PyObject *
fpbinaryswitchable_divide(PyObject *op1, PyObject *op2)
{
    FpBinarySwitchableObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    PyObject *function_op =
        prepare_binary_ops_fp(op1, op2, &cast_op1, &cast_op2);

    if (function_op)
    {
        result = fpbinaryswitchable_from_params(
            true,
            FP_NUM_METHOD(function_op, nb_true_divide)(cast_op1, cast_op2),
            0.0);
        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);
    }
    else
    {
        double dbl_op1, dbl_op2;
        prepare_binary_ops_double(op1, op2, &dbl_op1, &dbl_op2);
        result = fpbinaryswitchable_from_params(false, NULL, dbl_op1 / dbl_op2);
    }

    return (PyObject *)result;
}

static PyObject *
fpbinaryswitchable_negative(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        return (PyObject *)fpbinaryswitchable_from_params(
            true, FP_NUM_METHOD(cast_self->fp_mode_value,
                                nb_negative)(cast_self->fp_mode_value),
            0.0);
    }

    return (PyObject *)fpbinaryswitchable_from_params(
        false, NULL, -cast_self->dbl_mode_value);
}

static PyObject *
fpbinaryswitchable_int(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        return FP_NUM_METHOD(cast_self->fp_mode_value,
                             nb_int)(cast_self->fp_mode_value);
    }

    return PyLong_FromDouble(cast_self->dbl_mode_value);
}

#if PY_MAJOR_VERSION < 3

static PyObject *
fpbinaryswitchable_long(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        return FP_NUM_METHOD(cast_self->fp_mode_value,
                             nb_long)(cast_self->fp_mode_value);
    }

    return PyLong_FromDouble(cast_self->dbl_mode_value);
}

#endif

static PyObject *
fpbinaryswitchable_float(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        return FP_NUM_METHOD(cast_self->fp_mode_value,
                             nb_float)(cast_self->fp_mode_value);
    }

    return PyFloat_FromDouble(cast_self->dbl_mode_value);
}

static PyObject *
fpbinaryswitchable_abs(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        return (PyObject *)fpbinaryswitchable_from_params(
            true, FP_NUM_METHOD(cast_self->fp_mode_value,
                                nb_absolute)(cast_self->fp_mode_value),
            0.0);
    }

    return (PyObject *)fpbinaryswitchable_from_params(
        false, NULL, (cast_self->dbl_mode_value < 0.0)
                         ? -cast_self->dbl_mode_value
                         : cast_self->dbl_mode_value);
}

/*
 * Left shift is often used in digital hardware as a multiply by 2. So this
 * makes sense for double values.
 */
static PyObject *
fpbinaryswitchable_lshift(PyObject *self, PyObject *pyobj_lshift)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        if (FP_NUM_METHOD_PRESENT(cast_self->fp_mode_value, nb_lshift))
        {
            return (PyObject *)fpbinaryswitchable_from_params(
                true, FP_NUM_METHOD(cast_self->fp_mode_value, nb_lshift)(
                          cast_self->fp_mode_value, pyobj_lshift),
                0.0);
        }
        else
        {
            FPBINARY_RETURN_NOT_IMPLEMENTED;
        }
    }
    else
    {
        PyObject *result;
        PyObject *shift_pyfloat =
            FP_NUM_METHOD(pyobj_lshift, nb_float)(pyobj_lshift);
        result = (PyObject *)fpbinaryswitchable_from_params(
            false, NULL, cast_self->dbl_mode_value *
                             pow(2.0, PyFloat_AsDouble(shift_pyfloat)));
        Py_DECREF(shift_pyfloat);
        return result;
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

/*
 * Right shift is often used in digital hardware as a divide by 2. So this makes
 * sense for double values.
 */
static PyObject *
fpbinaryswitchable_rshift(PyObject *self, PyObject *pyobj_rshift)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        if (FP_NUM_METHOD(cast_self->fp_mode_value, nb_rshift))
        {
            return (PyObject *)fpbinaryswitchable_from_params(
                true, FP_NUM_METHOD(cast_self->fp_mode_value, nb_rshift)(
                          cast_self->fp_mode_value, pyobj_rshift),
                0.0);
        }
        else
        {
            FPBINARY_RETURN_NOT_IMPLEMENTED;
        }
    }
    else
    {
        PyObject *result;
        PyObject *shift_pyfloat =
            FP_NUM_METHOD(pyobj_rshift, nb_float)(pyobj_rshift);
        result = (PyObject *)fpbinaryswitchable_from_params(
            false, NULL, cast_self->dbl_mode_value /
                             pow(2.0, PyFloat_AsDouble(shift_pyfloat)));
        Py_DECREF(shift_pyfloat);
        return result;
    }

    FPBINARY_RETURN_NOT_IMPLEMENTED;
}

static int
fpbinaryswitchable_nonzero(PyObject *self)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        if (FP_NUM_METHOD_PRESENT(cast_self->fp_mode_value, nb_nonzero))
        {
            return FP_NUM_METHOD(cast_self->fp_mode_value,
                                 nb_nonzero)(cast_self->fp_mode_value);
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return cast_self->dbl_mode_value != 0.0;
    }

    return 0;
}

static PyObject *
fpbinaryswitchable_str(PyObject *obj)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)obj;
    if (cast_self->fp_mode)
    {
        return FP_METHOD(cast_self->fp_mode_value,
                         tp_str)(cast_self->fp_mode_value);
    }
    else
    {
        PyObject *float_py = PyFloat_FromDouble(cast_self->dbl_mode_value);
        PyObject *result = FP_METHOD(float_py, tp_str)(float_py);
        Py_DECREF(float_py);
        return result;
    }
}

static PyObject *
fpbinaryswitchable_richcompare(PyObject *op1, PyObject *op2, int operator)
{
    PyObject *result = NULL;
    PyObject *cast_op1 = NULL, *cast_op2 = NULL;

    PyObject *function_op =
        prepare_binary_ops_fp(op1, op2, &cast_op1, &cast_op2);

    if (function_op)
    {
        if (FP_METHOD(function_op, tp_richcompare))
        {
            result = FP_METHOD(function_op, tp_richcompare)(cast_op1, cast_op2,
                                                                      operator);
        }
        Py_DECREF(cast_op1);
        Py_DECREF(cast_op2);

        if (result)
            return result;
        else
            FPBINARY_RETURN_NOT_IMPLEMENTED;
    }
    else
    {
        PyObject *op1_pyfloat, *op2_pyfloat;
        double dbl_op1, dbl_op2;
        prepare_binary_ops_double(op1, op2, &dbl_op1, &dbl_op2);
        op1_pyfloat = PyFloat_FromDouble(dbl_op1);
        op2_pyfloat = PyFloat_FromDouble(dbl_op2);

        result = FP_METHOD(op1_pyfloat, tp_richcompare)(op1_pyfloat,
                                                        op2_pyfloat, operator);
        Py_DECREF(op1_pyfloat);
        Py_DECREF(op2_pyfloat);

        return result;
    }
}

static void
fpbinaryswitchable_dealloc(FpBinarySwitchableObject *self)
{
    Py_XDECREF((PyObject *)self->fp_mode_value);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

PyDoc_STRVAR(fp_mode_doc,
             "bool : Read-only. True if the object is in fixed point mode.");
static PyObject *
fpbinaryswitchable_get_fp_mode(PyObject *self, void *closure)
{
    if (((FpBinarySwitchableObject *)self)->fp_mode)
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

PyDoc_STRVAR(fpbinaryswitchable_format_doc,
        "tuple : Read-only (int_bits, frac_bits) tuple representing the fixed point format. \n"
        "If `fp_mode` is False, (1, 0) is returned.\n");
static PyObject *
fpbinaryswitchable_getformat(PyObject *self, void *closure)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;

    if (cast_self->fp_mode && cast_self->fp_mode_value &&
        PyObject_HasAttr(cast_self->fp_mode_value, get_format_method_name_str))
    {
        return PyObject_GetAttr(cast_self->fp_mode_value,
                                get_format_method_name_str);
    }

    Py_INCREF(py_default_format_tuple);
    return py_default_format_tuple;
}

PyDoc_STRVAR(value_doc,
        "float-castable : If `fp_mode` is True, may set to a FpBinary or FpBinarySwitchable type. \n"
        "If `fp_mode` is False may set any object that is castable to a float. \n"
        "This property can be used to set the underlying value of a monitoring variable \n"
        "for the purposes of profiling the min and max values at a given point in execution. \n");
static PyObject *
fpbinaryswitchable_getvalue(PyObject *self, void *closure)
{
    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;

    if (cast_self->fp_mode)
    {
        Py_INCREF(cast_self->fp_mode_value);
        return cast_self->fp_mode_value;
    }
    else
    {
        return PyFloat_FromDouble(cast_self->dbl_mode_value);
    }

    return NULL;
}

static int
fpbinaryswitchable_setvalue(PyObject *self, PyObject *value, void *closure)
{
    /*
     * If in fp_mode, the set value must be a FpBinary or FpBinarySwitchable
     * type. If not in fp_mode, the set value must have a float() conversion
     * function.
     */

    FpBinarySwitchableObject *cast_self = (FpBinarySwitchableObject *)self;
    if (cast_self->fp_mode)
    {
        PyObject *new_val = NULL;

        if (FpBinarySwitchable_Check(value))
        {
            new_val = fpbinaryswitchable_getvalue(value, NULL);
        }
        else if (FpBinary_Check(value))
        {
            new_val = value;
            Py_INCREF(value);
        }

        if (new_val)
        {
            PyObject *tmp = cast_self->fp_mode_value;
            cast_self->fp_mode_value = new_val;
            Py_DECREF(tmp);
        }
        else
        {
            PyErr_SetString(PyExc_TypeError, "When in fixed point mode, set "
                                             "value must be an instance of "
                                             "FpBinary or FpBinarySwitchable.");
            return -1;
        }
    }
    else
    {
        double new_value;

        if (FpBinarySwitchable_Check(value))
        {
            new_value = ((FpBinarySwitchableObject *)value)->dbl_mode_value;
        }
        else if (FP_NUM_METHOD_PRESENT(value, nb_float))
        {
            PyObject *value_pyfloat = FP_NUM_METHOD(value, nb_float)(value);
            new_value = PyFloat_AsDouble(value_pyfloat);
            Py_DECREF(value_pyfloat);
        }
        else
        {
            PyErr_SetString(PyExc_TypeError, "When not in fixed point mode, "
                                             "set value must be convertable to "
                                             "float.");
            return -1;
        }

        cast_self->dbl_mode_value = new_value;

        /* Check min and max values */
        if (new_value < cast_self->dbl_mode_min_value)
        {
            cast_self->dbl_mode_min_value = new_value;
        }

        if (new_value > cast_self->dbl_mode_max_value)
        {
            cast_self->dbl_mode_max_value = new_value;
        }
    }

    return 0;
}

PyDoc_STRVAR(
    minvalue_doc,
    "float : Will return the lowest value the `value` property has been set to. \n"
    "This only applies when `fp_mode` is False. If `fp_mode` is True, this property will return 0.0 .\n");
static PyObject *
fpbinaryswitchable_getminvalue(PyObject *self, void *closure)
{
    return PyFloat_FromDouble(
        ((FpBinarySwitchableObject *)self)->dbl_mode_min_value);
}

PyDoc_STRVAR(maxvalue_doc,
        "float : Will return the highest value the `value` property has been set to. \n"
        "This only applies when `fp_mode` is False. If `fp_mode` is True, this property will return 0.0 .\n");
static PyObject *
fpbinaryswitchable_getmaxvalue(PyObject *self, void *closure)
{
    return PyFloat_FromDouble(
        ((FpBinarySwitchableObject *)self)->dbl_mode_max_value);
}

void
FpBinarySwitchable_InitModule(void)
{
    resize_method_name_str = PyUnicode_FromString("resize");
    get_format_method_name_str = PyUnicode_FromString("format");
    py_default_format_tuple = PyTuple_Pack(2, py_one, py_zero);
}

static PyMethodDef fpbinaryswitchable_methods[] = {
    {"resize", (PyCFunction)fpbinaryswitchable_resize,
     METH_VARARGS | METH_KEYWORDS, fpswitchable_resize_doc},

    /* Pickling functions */
    {"__getstate__", (PyCFunction)fpbinaryswitchable_getstate, METH_NOARGS,
     NULL},
    {"__setstate__", (PyCFunction)fpbinaryswitchable_setstate, METH_O, NULL},

    {NULL} /* Sentinel */
};

static PyGetSetDef fpbinaryswitchable_getsetters[] = {
    {"fp_mode", (getter)fpbinaryswitchable_get_fp_mode, NULL, fp_mode_doc,
     NULL},
    {"format", (getter)fpbinaryswitchable_getformat, NULL, fpbinaryswitchable_format_doc, NULL},
    {"value", (getter)fpbinaryswitchable_getvalue,
     (setter)fpbinaryswitchable_setvalue, value_doc, NULL},
    {"min_value", (getter)fpbinaryswitchable_getminvalue, NULL, minvalue_doc,
     NULL},
    {"max_value", (getter)fpbinaryswitchable_getmaxvalue, NULL, maxvalue_doc,
     NULL},
    {NULL} /* Sentinel */
};

static PyNumberMethods fpbinaryswitchable_as_number = {
    .nb_add = (binaryfunc)fpbinaryswitchable_add,
    .nb_subtract = (binaryfunc)fpbinaryswitchable_subtract,
    .nb_multiply = (binaryfunc)fpbinaryswitchable_multiply,
    .nb_true_divide = (binaryfunc)fpbinaryswitchable_divide,
    .nb_negative = (unaryfunc)fpbinaryswitchable_negative,
    .nb_int = (unaryfunc)fpbinaryswitchable_int,

#if PY_MAJOR_VERSION < 3
    .nb_divide = (binaryfunc)fpbinaryswitchable_divide,
    .nb_long = (unaryfunc)fpbinaryswitchable_long,
#endif

    .nb_float = (unaryfunc)fpbinaryswitchable_float,
    .nb_absolute = (unaryfunc)fpbinaryswitchable_abs,
    .nb_lshift = (binaryfunc)fpbinaryswitchable_lshift,
    .nb_rshift = (binaryfunc)fpbinaryswitchable_rshift,
    .nb_nonzero = (inquiry)fpbinaryswitchable_nonzero,
};

PyTypeObject FpBinarySwitchable_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.FpBinarySwitchable",
    .tp_doc = fpbinaryswitchable_doc,
    .tp_basicsize = sizeof(FpBinarySwitchableObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES,
    .tp_methods = fpbinaryswitchable_methods,
    .tp_getset = fpbinaryswitchable_getsetters,
    .tp_as_number = &fpbinaryswitchable_as_number,
    .tp_new = (newfunc)PyType_GenericNew,
    .tp_init = (initproc)fpbinaryswitchable_init,
    .tp_dealloc = (destructor)fpbinaryswitchable_dealloc,
    .tp_str = fpbinaryswitchable_str,
    .tp_repr = fpbinaryswitchable_str,
    .tp_richcompare = fpbinaryswitchable_richcompare,
};
