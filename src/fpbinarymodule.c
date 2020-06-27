/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Packaging up of publicly accessible objects into the fpbinary Python module.
 *
 *****************************************************************************/

#include "fpbinaryenums.h"
#include "fpbinarylarge.h"
#include "fpbinaryobject.h"
#include "fpbinarysmall.h"
#include "fpbinaryswitchable.h"
#include "fpbinaryversion.h"

#define FPBINARY_MOD_NAME "fpbinary"
#define FPBINARY_MOD_DOC "Fixed point binary module."

PyObject *FpBinaryOverflowException;
static PyObject *FpBinaryVersionString;

static PyMethodDef fpbinarymod_methods[] = {
    {NULL}, /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
static PyModuleDef fpbinarymoduledef = {
    PyModuleDef_HEAD_INIT,
    .m_name = FPBINARY_MOD_NAME,
    .m_doc = FPBINARY_MOD_DOC,
    .m_methods = fpbinarymod_methods,
    .m_size = -1,
};

#define INITERROR return NULL
PyMODINIT_FUNC
PyInit_fpbinary(void)
#else

#define INITERROR return
#ifndef PyMODINIT_FUNC /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initfpbinary(void)
#endif
{
    PyObject *m;

    if (PyType_Ready(&FpBinary_SmallType) < 0)
        INITERROR;

    if (PyType_Ready(&FpBinary_LargeType) < 0)
        INITERROR;

    if (PyType_Ready(&FpBinary_Type) < 0)
        INITERROR;

    if (PyType_Ready(&FpBinarySwitchable_Type) < 0)
        INITERROR;

    if (PyType_Ready(&OverflowEnumType) < 0)
    {
        INITERROR;
    }

    if (PyType_Ready(&RoundingEnumType) < 0)
    {
        INITERROR;
    }

    FpBinaryCommon_InitModule();

#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&fpbinarymoduledef);
#else
    m = Py_InitModule3("fpbinary", fpbinarymod_methods,
                       "Fixed point binary module.");
#endif

    Py_INCREF(&FpBinary_SmallType);
    PyModule_AddObject(m, "_FpBinarySmall", (PyObject *)&FpBinary_SmallType);

    Py_INCREF(&FpBinary_LargeType);
    PyModule_AddObject(m, "_FpBinaryLarge", (PyObject *)&FpBinary_LargeType);
    FpBinaryLarge_InitModule();

    Py_INCREF(&FpBinary_Type);
    PyModule_AddObject(m, "FpBinary", (PyObject *)&FpBinary_Type);

    FpBinarySwitchable_InitModule();
    Py_INCREF(&FpBinarySwitchable_Type);
    PyModule_AddObject(m, "FpBinarySwitchable",
                       (PyObject *)&FpBinarySwitchable_Type);

    /* Create enum instances */
    Py_INCREF(&OverflowEnumType);
    PyModule_AddObject(m, "OverflowEnum", (PyObject *)&OverflowEnumType);

    Py_INCREF(&RoundingEnumType);
    PyModule_AddObject(m, "RoundingEnum", (PyObject *)&RoundingEnumType);

    fpbinaryenums_InitModule();

    FpBinaryOverflowException =
        PyErr_NewException("fpbinary.FpBinaryOverflowException", NULL, NULL);
    PyModule_AddObject(m, "FpBinaryOverflowException",
                       FpBinaryOverflowException);

/* Doing this to ensure the version string is the default type on
 * each version of python.
 */
#if PY_MAJOR_VERSION >= 3
    FpBinaryVersionString = PyUnicode_FromString(FPBINARY_VERSION_STR);
#else
    FpBinaryVersionString = PyString_FromString(FPBINARY_VERSION_STR);
#endif
    PyModule_AddObject(m, "__version__", FpBinaryVersionString);

#if PY_MAJOR_VERSION >= 3
    return m;
#endif
}
