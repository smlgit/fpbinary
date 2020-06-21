/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYCOMMON_H_
#define FPBINARYCOMMON_H_

#include "Python.h"
#include <stdbool.h>

/* defines for compatability between v2 and v3 */
#if PY_MAJOR_VERSION >= 3

#define nb_nonzero nb_bool
#define nb_long nb_int

#endif

#ifndef Py_TPFLAGS_CHECKTYPES
#define Py_TPFLAGS_CHECKTYPES 0
#endif

#define FP_INT_TYPE long long
#define FP_INT_NUM_BITS (sizeof(FP_INT_TYPE) * 8)
#define FP_INT_ALL_BITS_MASK (~((FP_INT_TYPE)0))

#define FP_UINT_TYPE unsigned long long
#define FP_UINT_NUM_BITS (sizeof(FP_INT_TYPE) * 8)
#define FP_UINT_MAX_SIGN_BIT (((FP_UINT_TYPE)1) << (FP_UINT_NUM_BITS - 1))
#define FP_UINT_ALL_BITS_MASK (~((FP_UINT_TYPE)0))
#define FP_UINT_MAX_VAL FP_UINT_ALL_BITS_MASK

/* Not sure if this is already done somewhere - couldn't find it...
 * I've chosen to call "abstract" methods directly (rather than using the
 * PyObject_CallMethodObjArgs function) for speed. */
#define FP_METHOD(ob, method_name) Py_TYPE(ob)->method_name
#define FP_NUM_METHOD(ob, method_name) Py_TYPE(ob)->tp_as_number->method_name
#define FP_SQ_METHOD(ob, method_name) Py_TYPE(ob)->tp_as_sequence->method_name
#define FP_MP_METHOD(ob, method_name) Py_TYPE(ob)->tp_as_mapping->method_name
#define FP_NUM_METHOD_PRESENT(ob, method_name)                                 \
    (ob && (Py_TYPE(ob)->tp_as_number && FP_NUM_METHOD(ob, method_name)))

#define xstr(s) str(s)
#define str(s) #s

/*
 * Will carry out method on op1 and steal the original reference to op1.
 */
#define FP_NUM_UNI_OP_INPLACE(op1, method)                                     \
    do                                                                         \
    {                                                                          \
        PyObject *tmp = op1;                                                   \
        op1 = FP_NUM_METHOD(tmp, method)(tmp);                                 \
        Py_XDECREF(tmp);                                                       \
    } while (0)

/*
 * Will carry out method on op1 and op2 and steal the original reference
 * to op1.
 */
#define FP_NUM_BIN_OP_INPLACE(op1, op2, method)                                \
    do                                                                         \
    {                                                                          \
        PyObject *tmp = op1;                                                   \
        op1 = FP_NUM_METHOD(tmp, method)(tmp, op2);                            \
        Py_XDECREF(tmp);                                                       \
    } while (0)

#define FP_GLOBAL_Doc_VAR(name) char name[]
#define FP_GLOBAL_Doc_STRVAR(name, str) FP_GLOBAL_Doc_VAR(name) = PyDoc_STR(str)

/* Packaging up code that changes the point of a PyObject field */
#define FP_ASSIGN_PY_FIELD(obj, value, field)                                  \
    do                                                                         \
    {                                                                          \
        PyObject *tmp = obj->field;                                            \
        Py_XINCREF(value);                                                     \
        obj->field = value;                                                    \
        Py_XDECREF(tmp);                                                       \
    } while (0)

#define FP_BASE_PYOBJ(ob) ((PyObject *)ob)
#define PYOBJ_FP_BASE(ob) ((fpbinary_base_t *)ob)
#define FP_BASE_METHOD(ob, method_name)                                        \
    PYOBJ_FP_BASE(ob)->private_iface->method_name

/* Similarly, not sure if this is defined in python 2, so adding my own to make
 * porting to 3 easier.
 */

#define FPBINARY_RETURN_NOT_IMPLEMENTED                                        \
    return Py_INCREF(Py_NotImplemented), Py_NotImplemented

typedef enum {
    ROUNDING_NEAR_POS_INF = 1,
    ROUNDING_DIRECT_NEG_INF = 2,
    ROUNDING_NEAR_ZERO = 3,
    ROUNDING_DIRECT_ZERO = 4,
    ROUNDING_NEAR_EVEN = 5,
} fp_round_mode_t;

typedef enum {
    OVERFLOW_WRAP = 0,
    OVERFLOW_SAT = 1,
    OVERFLOW_EXCEP = 2,
} fp_overflow_mode_t;

/* Pseudo polymorphism to speed up calling functions that are in the "methods"
 * struct or not exposed to python users at all.
 */
typedef struct
{
    FP_UINT_TYPE (*get_total_bits)(PyObject *);
    bool (*is_signed)(PyObject *);
    PyObject *(*resize)(PyObject *self, PyObject *, PyObject *);
    PyObject *(*str_ex)(PyObject *self);
    PyObject *(*to_signed)(PyObject *obj, PyObject *args);
    PyObject *(*bits_to_signed)(PyObject *, PyObject *);
    PyObject *(*copy)(PyObject *, PyObject *);
    PyObject *(*fp_getformat)(PyObject *, void *);
    PyObject *(*fp_from_double)(double, FP_INT_TYPE, FP_INT_TYPE, bool,
                                fp_overflow_mode_t, fp_round_mode_t);
    PyObject *(*fp_from_bits_pylong)(PyObject *, FP_INT_TYPE, FP_INT_TYPE,
                                     bool);

    PyObject *(*getitem)(PyObject *, PyObject *);

    bool (*build_pickle_dict)(PyObject *self, PyObject *dict);

} fpbinary_private_iface_t;

typedef struct
{
    PyObject_HEAD fpbinary_private_iface_t *private_iface;
} fpbinary_base_t;

extern PyObject *FpBinaryOverflowException;
extern PyObject *py_zero;
extern PyObject *py_one;
extern PyObject *py_minus_one;

/* For pickling base objects */
extern PyObject *fp_small_type_id;
extern PyObject *fp_large_type_id;

FP_UINT_TYPE fp_uint_lshift(FP_UINT_TYPE value, FP_UINT_TYPE num_shifts);
FP_UINT_TYPE fp_uint_rshift(FP_UINT_TYPE value, FP_UINT_TYPE num_shifts);

/* Required for compatibility between v2 and v3 */
bool FpBinary_IntCheck(PyObject *ob);
PyObject *FpBinary_EnsureIsPyLong(PyObject *ob);
PyObject *FpBinary_TryConvertToPyInt(PyObject *ob);
int FpBinary_TpCompare(PyObject *op1, PyObject *op2);

void FpBinaryCommon_InitModule(void);

bool fp_binary_new_params_parse(PyObject *args, PyObject *kwds, long *int_bits,
                                long *frac_bits, bool *is_signed, double *value,
                                PyObject **bit_field,
                                PyObject **format_instance);
bool fp_binary_subscript_get_item_index(PyObject *item, Py_ssize_t *index);
bool fp_binary_subscript_get_item_start_stop(PyObject *item, Py_ssize_t *start,
                                             Py_ssize_t *stop,
                                             Py_ssize_t assumed_length);
PyObject *calc_scaled_val_bits(PyObject *obj, FP_UINT_TYPE frac_bits);
void calc_double_to_fp_params(double input_value, double *scaled_value,
                              FP_UINT_TYPE *int_bits, FP_UINT_TYPE *frac_bits);
void calc_pyint_to_fp_params(PyObject *input_value, PyObject **scaled_value,
                             FP_UINT_TYPE *int_bits);
PyObject *fp_uint_as_pylong(FP_UINT_TYPE value);
PyObject *fp_int_as_pylong(FP_INT_TYPE value);
FP_UINT_TYPE pylong_as_fp_uint(PyObject *val);
FP_INT_TYPE pylong_as_fp_int(PyObject *val);
void build_scaled_bits_from_pyfloat(PyObject *value, PyObject *frac_bits,
                                    fp_round_mode_t round_mode,
                                    PyObject **output_obj);
bool extract_fp_format_from_tuple(PyObject *format_tuple_param,
                                  PyObject **int_bits, PyObject **frac_bits);
bool check_new_method_input_types(PyObject *py_is_signed, PyObject *bit_field);
PyObject *scaled_long_to_float_str(PyObject *scaled_value, PyObject *int_bits,
                                   PyObject *frac_bits);

/*
 * Macro to check if the PyObject obj is of a type that FpBinary should be able
 * to do arithmetic operations with.
 */
#define check_supported_builtin(obj) ((PyFloat_Check(obj) || PyLong_Check(obj) || FpBinary_IntCheck(obj)))

#endif /* FPBINARYCOMMON_H_ */
