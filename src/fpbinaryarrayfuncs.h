/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

#ifndef FPBINARYARRAYFUNCS_H_
#define FPBINARYARRAYFUNCS_H_

#include "fpbinarycommon.h"

extern FP_GLOBAL_Doc_VAR(FpBinary_FromArray_doc);
extern FP_GLOBAL_Doc_VAR(FpBinaryComplex_FromArray_doc);
extern FP_GLOBAL_Doc_VAR(FpBinary_ArrayResize_doc);

PyObject* FpBinary_FromArray(PyObject* self, PyObject* args, PyObject* kwds);
PyObject* FpBinaryComplex_FromArray(PyObject* self, PyObject* args, PyObject* kwds);
PyObject* FpBinary_ArrayResize(PyObject* self, PyObject* args, PyObject* kwds);

#endif /* FPBINARYARRAYFUNCS_H_ */
