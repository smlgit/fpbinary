/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Doc strings that can be used by more than one object are defined here.
 *
 *****************************************************************************/

#include "fpbinaryglobaldoc.h"

FP_GLOBAL_Doc_STRVAR(
    resize_doc,
    "resize(format, overflow_mode=fpbinary.OverflowEnum.wrap, "
    "round_mode=fpbinary.RoundingEnum.direct_neg_inf)\n"
    "--\n"
    "\n"
    "Resizes the fixed point object IN PLACE to the format described by "
    "format.\n"
    "Parameters\n"
    "----------\n"
    "format : length 2 tuple or instance of this object.\n"
    "    If tuple, will be resized to format[0] int bits and format[1] frac "
    "bits.\n"
    "    If instance of this type, the format will be taken from format.\n"
    "\n"
    "overflow_mode: Value from OverflowEnum\n"
    "    Specifies how to deal with overflows when int bits are reduced.\n"
    "\n"
    "round_mode: Value from RoundingEnum\n"
    "    Specifies how to deal with rounding when frac bits are reduced.\n"
    "\n"
    "Returns\n"
    "----------\n"
    "self : PyObject *\n");

FP_GLOBAL_Doc_STRVAR(
    str_ex_doc,
    "str_ex(self)\n"
    "--\n"
    "\n"
    "Returns a str displaying the full precision value of the fixed point "
    "object.\n"
    "This is useful when the fixed point value has more bits than native "
    "floating \n"
    "point types can handle. Note that scientific notation is not used.\n"
    "Returns\n"
    "----------\n"
    "result : str or unicode\n");

FP_GLOBAL_Doc_STRVAR(
    bits_to_signed_doc,
    "bits_to_signed(self)\n"
    "--\n"
    "\n"
    "Interprets the bits of the fixed point binary object as a 2's complement "
    "signed\n"
    "integer and returns an int. If self is an unsigned object, the MSB, as "
    "defined by\n"
    "the int_bits and frac_bits values, will be considered a sign bit.\n"
    "Returns\n"
    "----------\n"
    "result : int\n");

FP_GLOBAL_Doc_STRVAR(copy_doc,
                     "__copy__(self)\n"
                     "--\n"
                     "\n"
                     "Performs a copy of self and returns a new object. If "
                     "self is a wrapper object, the\n"
                     "embedded fixed point object will also be copied.\n"
                     "Returns\n"
                     "----------\n"
                     "result : type(self)\n");

FP_GLOBAL_Doc_STRVAR(format_doc,
                     "tuple : (int_bits, frac_bits) tuple (read only).");

FP_GLOBAL_Doc_STRVAR(
    is_signed_doc,
    "bool : True if the fixed point object is signed (read only).");
