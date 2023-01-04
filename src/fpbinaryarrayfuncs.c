/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Functions to create or modify lists or arrays of fixed point objects.
 *
 *****************************************************************************/

#include "fpbinaryarrayfuncs.h"
#include "fpbinaryobject.h"

/*
 * Assumes kwds is NOT NULL.
 */
static PyObject* fpbinary_from_array_nested(PyObject* array, PyObject* fpbinary_args, PyObject* kwds)
{
    Py_ssize_t len = PySequence_Length(array);
    PyObject* result_list = NULL;

    if (len < 0)
    {
        PyErr_SetString(PyExc_TypeError, "Could not determine array length when creating FpBinary from array.");
        return NULL;
    }

    result_list = PyList_New(len);

    for (int i = 0; i < len; i++)
    {
        PyObject* cur_item = PySequence_GetItem(array, i);

        if (PySequence_Check(cur_item))
        {
            PyObject* nested_list =
                    fpbinary_from_array_nested(PySequence_GetItem(array, i), fpbinary_args, kwds);
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
            PyObject* new_fp_object = NULL;

            /* Set the fpbinary keyword argument "value" to the current array item */
            PyDict_SetItemString(kwds, "value", cur_item);

            new_fp_object = PyObject_Call((PyObject *)&FpBinary_Type, fpbinary_args, kwds);

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
 * First argument must be an object that implements the __getitem__ method.
 * The rest of the arguments:
 * int_bits, frac_bits, signed, format_inst.
 *
 * We use the underlying FpBinary constructor format, so if format_inst is used,
 * it needs to be used as a keyword arg.
 *
 * Returns a list of FpBinary objects.
 */
PyObject* FpBinary_FromArray(PyObject* self, PyObject* args, PyObject* kwds)
{
    Py_ssize_t args_len;
    PyObject* fpbinary_args = NULL;
    PyObject* temp_kwds = NULL;
    PyObject* result = NULL;

    if (!PyTuple_Check(args))
    {
        PyErr_SetString(PyExc_ValueError, "Unexpected parameter list when creating FpBinary from array.");
        return NULL;
    }

    args_len = PyTuple_Size(args);
    if (args_len < 1)
    {
        PyErr_SetString(PyExc_TypeError, "An array or list must be specified when creating FpBinary from array.");
        return NULL;
    }

    if (!PySequence_Check(PyTuple_GET_ITEM(args, 0)))
    {
        PyErr_SetString(PyExc_TypeError, "First argument must be an array or list when creating FpBinary from array.");
        return NULL;
    }

    if (args_len > 4)
    {
        PyErr_SetString(PyExc_ValueError, "The only positional arguments allowed when when creating FpBinary from array"
                " are array, int_bits, frac_bits and signed.");
        return NULL;
    }

    fpbinary_args = PyTuple_GetSlice(args, 1, args_len);

    if (!kwds)
    {
        temp_kwds = PyDict_New();
        result = fpbinary_from_array_nested(PyTuple_GET_ITEM(args, 0), fpbinary_args, temp_kwds);
        Py_DECREF(temp_kwds);
    }
    else
    {
        result = fpbinary_from_array_nested(PyTuple_GET_ITEM(args, 0), fpbinary_args, kwds);
    }

    Py_DECREF(fpbinary_args);

    return result;
}



