/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Functions to create or modify lists or arrays of fixed point objects.
 *
 *****************************************************************************/

#include "fpbinaryarrayfuncs.h"
#include "fpbinarycomplexobject.h"
#include "fpbinaryobject.h"

/*
 * Assumes kwds is NOT NULL.
 */
static PyObject *
fpbinary_from_array_nested(PyObject *array, PyObject *fpbinary_args,
                           PyObject *kwds)
{
    Py_ssize_t len = PySequence_Length(array);
    PyObject *result_list = NULL;

    if (len < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Could not determine array length "
                                         "when creating FpBinary from array.");
        return NULL;
    }

    result_list = PyList_New(len);

    for (int i = 0; i < len; i++)
    {
        PyObject *cur_item = PySequence_GetItem(array, i);

        if (PySequence_Check(cur_item))
        {
            PyObject *nested_list =
                fpbinary_from_array_nested(cur_item, fpbinary_args, kwds);
            if (nested_list)
            {
                PyList_SET_ITEM(result_list, i, nested_list);
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            PyObject *new_fp_object = NULL;

            /* Set the fpbinary keyword argument "value" to the current array
             * item */
            PyDict_SetItemString(kwds, "value", cur_item);

            new_fp_object =
                PyObject_Call((PyObject *)&FpBinary_Type, fpbinary_args, kwds);

            if (new_fp_object)
            {
                PyList_SET_ITEM(result_list, i, new_fp_object);
            }
            else
            {
                return NULL;
            }
        }

        Py_DECREF(cur_item);
    }

    return result_list;
}

/*
 * Assumes kwds is NOT NULL.
 */
static PyObject *
fpbinarycomplex_from_array_nested(PyObject *array,
                                  PyObject *fpbinarycomplex_args,
                                  PyObject *kwds)
{
    Py_ssize_t len = PySequence_Length(array);
    PyObject *result_list = NULL;

    if (len < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Could not determine array length "
                                         "when creating FpBinary from array.");
        return NULL;
    }

    result_list = PyList_New(len);

    for (int i = 0; i < len; i++)
    {
        PyObject *cur_item = PySequence_GetItem(array, i);

        if (PySequence_Check(cur_item))
        {
            PyObject *nested_list = fpbinarycomplex_from_array_nested(
                cur_item, fpbinarycomplex_args, kwds);
            if (nested_list)
            {
                PyList_SET_ITEM(result_list, i, nested_list);
            }
            else
            {
                return NULL;
            }
        }
        else
        {
            PyObject *new_fp_object = NULL;

            /* Set the fpbinary keyword argument "value" to the current array
             * item */
            PyDict_SetItemString(kwds, "value", cur_item);

            new_fp_object = PyObject_Call((PyObject *)&FpBinaryComplex_Type,
                                          fpbinarycomplex_args, kwds);

            if (new_fp_object)
            {
                PyList_SET_ITEM(result_list, i, new_fp_object);
            }
            else
            {
                return NULL;
            }
        }

        Py_DECREF(cur_item);
    }

    return result_list;
}

static bool
fpbinary_array_resize_nested(PyObject *array, PyObject *fpbinary_args,
                             PyObject *kwds)
{
    Py_ssize_t len = PySequence_Length(array);
    bool result = true;

    if (len < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Could not determine array length "
                                         "when resizing FpBinary in array.");
        return false;
    }

    for (int i = 0; i < len; i++)
    {
        PyObject *cur_item = PySequence_GetItem(array, i);

        if (PySequence_Check(cur_item))
        {
            result =
                fpbinary_array_resize_nested(cur_item, fpbinary_args, kwds);
        }
        else
        {
            PyObject *resized_obj = forward_call_with_args(
                cur_item, resize_method_name_str, fpbinary_args, kwds);
            ;
            result = (resized_obj != NULL);

            if (result)
            {
                Py_DECREF(resized_obj);
            }
        }

        Py_DECREF(cur_item);
    }

    return result;
}

/*
 * First argument must be an object that implements the __getitem__ method.
 * The rest of the arguments:
 * int_bits, frac_bits, signed, format_inst.
 *
 * We use the underlying FpBinary constructor format, so if format_inst is used,
 * it needs to be used as a keyword arg.
 *
 * Returns a list of FpBinary objects.
 */

FP_GLOBAL_Doc_STRVAR(
    FpBinary_FromArray_doc,
    "fpbinary_list_from_array(array, int_bits=1, frac_bits=0, signed=True, "
    "format_inst=None)\n"
    "--\n"
    "\n"
    "Converts the elements of array to a list of FpBinary objects using the "
    "format "
    "specified by int_bits/frac_bits or format_inst.\n"
    "If format_inst is used, it must be specified by keyword.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "array : Any object that implements __getitem__.\n"
    "\n"
    "int_bits, frac_bits, signed, format_inst : As per FpBinary constructor\n"
    "\n"
    "Returns\n"
    "----------\n"
    "list\n"
    "    Elements are FpBinary objects. Dimension of input array is "
    "maintained.\n");

PyObject *
FpBinary_FromArray(PyObject *self, PyObject *args, PyObject *kwds)
{
    Py_ssize_t args_len;
    PyObject *fpbinary_args = NULL;
    PyObject *temp_kwds = NULL;
    PyObject *result = NULL;

    if (!PyTuple_Check(args))
    {
        PyErr_SetString(
            PyExc_ValueError,
            "Unexpected parameter list when creating FpBinary from array.");
        return NULL;
    }

    args_len = PyTuple_Size(args);
    if (args_len < 1)
    {
        PyErr_SetString(PyExc_TypeError, "An array or list must be specified "
                                         "when creating FpBinary from array.");
        return NULL;
    }

    if (!PySequence_Check(PyTuple_GET_ITEM(args, 0)))
    {
        PyErr_SetString(PyExc_TypeError, "First argument must be an array or "
                                         "list when creating FpBinary from "
                                         "array.");
        return NULL;
    }

    if (args_len > 4)
    {
        PyErr_SetString(PyExc_ValueError,
                        "The only positional arguments allowed when when "
                        "creating FpBinary from array"
                        " are array, int_bits, frac_bits and signed.");
        return NULL;
    }

    fpbinary_args = PyTuple_GetSlice(args, 1, args_len);

    if (!kwds)
    {
        temp_kwds = PyDict_New();
        result = fpbinary_from_array_nested(PyTuple_GET_ITEM(args, 0),
                                            fpbinary_args, temp_kwds);
        Py_DECREF(temp_kwds);
    }
    else
    {
        result = fpbinary_from_array_nested(PyTuple_GET_ITEM(args, 0),
                                            fpbinary_args, kwds);
    }

    Py_DECREF(fpbinary_args);

    return result;
}

/*
 * First argument must be an object that implements the __getitem__ method.
 * The rest of the arguments:
 * int_bits, frac_bits, format_inst.
 *
 * We use the underlying FpBinaryComplex constructor format, so if format_inst
 * is used,
 * it needs to be used as a keyword arg.
 *
 * Returns a list of FpBinaryComplex objects.
 */

FP_GLOBAL_Doc_STRVAR(
    FpBinaryComplex_FromArray_doc,
    "fpbinarycomplex_list_from_array(array, int_bits=1, frac_bits=0, "
    "format_inst=None)\n"
    "--\n"
    "\n"
    "Converts the elements of array to a list of FpBinaryComplex objects using "
    "the format "
    "specified by int_bits/frac_bits or format_inst.\n"
    "If format_inst is used, it must be specified by keyword.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "array : Any object that implements __getitem__.\n"
    "\n"
    "int_bits, frac_bits, format_inst : As per FpBinaryComplex constructor\n"
    "\n"
    "Returns\n"
    "----------\n"
    "list\n"
    "    Elements are FpBinaryComplex objects. Dimension of input array is "
    "maintained.\n");

PyObject *
FpBinaryComplex_FromArray(PyObject *self, PyObject *args, PyObject *kwds)
{
    Py_ssize_t args_len;
    PyObject *fpbinarycomplex_args = NULL;
    PyObject *temp_kwds = NULL;
    PyObject *result = NULL;

    if (!PyTuple_Check(args))
    {
        PyErr_SetString(PyExc_ValueError, "Unexpected parameter list when "
                                          "creating FpBinaryComplex from "
                                          "array.");
        return NULL;
    }

    args_len = PyTuple_Size(args);
    if (args_len < 1)
    {
        PyErr_SetString(PyExc_TypeError, "An array or list must be specified "
                                         "when creating FpBinaryComplex from "
                                         "array.");
        return NULL;
    }

    if (!PySequence_Check(PyTuple_GET_ITEM(args, 0)))
    {
        PyErr_SetString(PyExc_TypeError, "First argument must be an array or "
                                         "list when creating FpBinaryComplex "
                                         "from array.");
        return NULL;
    }

    if (args_len > 3)
    {
        PyErr_SetString(PyExc_ValueError,
                        "The only positional arguments allowed when when "
                        "creating FpBinaryComplex from array"
                        " are array, int_bits and frac_bits.");
        return NULL;
    }

    fpbinarycomplex_args = PyTuple_GetSlice(args, 1, args_len);

    if (!kwds)
    {
        temp_kwds = PyDict_New();
        result = fpbinarycomplex_from_array_nested(
            PyTuple_GET_ITEM(args, 0), fpbinarycomplex_args, temp_kwds);
        Py_DECREF(temp_kwds);
    }
    else
    {
        result = fpbinarycomplex_from_array_nested(PyTuple_GET_ITEM(args, 0),
                                                   fpbinarycomplex_args, kwds);
    }

    Py_DECREF(fpbinarycomplex_args);

    return result;
}

/*
 * First argument must be an object that implements the __getitem__ method.
 * The rest of the arguments:
 * format, overflow_mode, round_mode.
 */

FP_GLOBAL_Doc_STRVAR(
    FpBinary_ArrayResize_doc,
    "array_resize(array, format, overflow_mode=0, round_mode=2)\n"
    "--\n"
    "\n"
    "Resizes the fixed point objects in array IN PLACE to the format described "
    "by format.\n"
    "See the documentation for FpBinary.resize for more information.\n"
    "\n"
    "Parameters\n"
    "----------\n"
    "array : Any object that implements __getitem__. Elements must be nested "
    "arrays or FpBinary or FpBinaryComplex objects.\n"
    "\n"
    "array, format, overflow_mode=0, round_mode : As per FpBinary.resize "
    "method.\n"
    "\n"
    "Returns\n"
    "----------\n"
    "None\n");

PyObject *
FpBinary_ArrayResize(PyObject *self, PyObject *args, PyObject *kwds)
{
    Py_ssize_t args_len;
    PyObject *fpbinary_args = NULL;

    if (!PyTuple_Check(args))
    {
        PyErr_SetString(
            PyExc_ValueError,
            "Unexpected parameter list when resizing array elements.");
        return NULL;
    }

    args_len = PyTuple_Size(args);
    if (args_len < 1)
    {
        PyErr_SetString(
            PyExc_TypeError,
            "An array or list must be specified when resizing array elements.");
        return NULL;
    }

    if (!PySequence_Check(PyTuple_GET_ITEM(args, 0)))
    {
        PyErr_SetString(PyExc_TypeError, "First argument must be an array or "
                                         "list when resizing array elements.");
        return NULL;
    }

    fpbinary_args = PyTuple_GetSlice(args, 1, args_len);
    if (fpbinary_array_resize_nested(PyTuple_GET_ITEM(args, 0), fpbinary_args,
                                     kwds))
    {
        Py_DECREF(fpbinary_args);
        Py_RETURN_NONE;
    }
    else
    {
        return NULL;
    }
}
