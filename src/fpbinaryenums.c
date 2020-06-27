/******************************************************************************
 * Licensed under GNU General Public License 2.0 - see LICENSE
 *****************************************************************************/

/******************************************************************************
 *
 * Basic objects to make the use of enumerated values easier pre python3.
 * For each enum type, a PyTypeObject is defined which has an int field for
 * each possible value. The fields are exposed to python users as get
 * properties.
 *
 *****************************************************************************/

#include "fpbinaryenums.h"
#include "structmember.h"

/*
 *
 * Overflow enumeration class.
 *
 */

PyDoc_STRVAR(fpbinaryoverflow_enum_doc,
        "Provides static fields for overflow modes.\n"
        "\n"
        "Attributes\n"
        "----------\n"
        "wrap : int\n"
        "    This is essentially the truncation of any int bits that are being removed (usually via a resize() call). \n"
        "    For signed types, this mayresult in a positive number becoming negative and vice versa.\n"
        "\n"
        "sat : int\n"
        "    If an overflow occurs, the value is railed to the min or max value of the new bit format.\n"
        "\n"
        "excep : int\n"
        "    If an overflow occurs, an FpBinaryOverflowException is raised.\n");
PyTypeObject OverflowEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.OverflowEnum",
    .tp_doc = fpbinaryoverflow_enum_doc,
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
};

/*
 *
 * Rounding enumeration class.
 *
 */

PyDoc_STRVAR(fpbinaryrounding_enum_doc,
        "Provides static fields for rounding modes.\n"
        "The enums will generally be of the 'direct' or 'near' types. \n"
        "'near' implies that a rule is applied if the value is exactly "
        "halfway between the representable value. \n"
        "'direct' implies that no consideration is given to these halfway situations. \n"
        "\n"
        "Attributes\n"
        "----------\n"
        "near_pos_inf : int\n"
        "    The value is rounded towards the nearest value representable by the new format. \n"
        "    Ties (i.e. X.5) are rounded towards positive infinity.\n"
        "    The IEEE 754 standard does not have an equivalent, but this is common in "
        "general arithmetic that many call 'rounding up'.\n"
        "    Examples: *5.5 and 5.6 both go to 6.0 (assuming resizing to zero fract_bits). \n"
        "    -5.25 goes to -5.0, -5.375 goes to -5.5 (assuming resizing to one fract_bit).* \n"
        "\n"
        "direct_neg_inf : int\n"
        "    The value is rounded in the negative direction to the nearest "
        "    value representable by the new format. \n"
        "    This is a clean truncate of bits without any other processing. It is "
        "    often called 'flooring'. \n"
        "    This is the mode the VHDL fixed point library applies when using the 'truncate' mode. \n"
        "    The IEEE 754 standard calls this 'Round toward -infinity'. \n"
        "    Examples: *5.5 and 5.6 both go to 5.0 (assuming resizing to zero fract_bits). \n"
        "    -5.25 and -5.375 both go to -5.5 (assuming resizing to one fract_bit).* \n"
        "\n"
        "near_zero : int\n"
        "    The value is rounded towards the nearest value representable by the new format. \n"
        "    Ties (i.e. X.5) are rounded towards zero. \n"
        "    The IEEE 754 standard does not have an equivalent, but python uses this mode when converting floats to ints. \n"
        "    Examples: *5.5 goes to 5.0, 5.6 goes to 6.0 (assuming resizing to zero fract_bits).\n"
        "    -5.25 goes to -5.0, -5.375 goes to -5.5 (assuming resizing to one fract_bit).* \n"
        "\n"
        "direct_zero : int\n"
        "    The value is rounded in the direction towards zero to the nearest value representable by the new format. \n"
        "    The IEEE 754 standard calls this 'Round toward 0' or 'truncation'. \n"
        "    Examples: *5.5 and 5.6 both go to 5.0 (assuming resizing to zero fract_bits). \n"
        "    -5.25 and -5.375 both go to -5.0 (assuming resizing to one fract_bit).* \n"
        "\n"
        "near_even : int\n"
        "    The value is rounded towards the nearest value representable by the new format. \n"
        "    Ties (i.e. X.5) are rounded towards the 'even' representation. This means that, after rounding a tie, the lsb is zero. \n"
        "    The IEEE 754 standard calls this 'Round to nearest, ties to even'. \n"
        "    This is also the mode the VHDL fixed point library applies when using the 'round' mode. \n"
        "    Examples: *5.5 and 6.5 both go to 6.0 (assuming resizing to zero fract_bits). \n"
        "    -5.5 and -6.5 both go to -6.0 (assuming resizing to zero fract_bits). \n"
        "    5.75 goes to 6.0, 5.25 goes to 5.0 (assuming resizing to one fract_bit).* \n");
PyTypeObject RoundingEnumType = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "fpbinary.RoundingEnum",
    .tp_doc = fpbinaryrounding_enum_doc,
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
};

void fpbinaryenums_InitModule(void)
{
    /* Set type object attribute dicts with our fields for both enums */

    PyDict_SetItemString(OverflowEnumType.tp_dict, "wrap", PyLong_FromLong((long)OVERFLOW_WRAP));
    PyDict_SetItemString(OverflowEnumType.tp_dict, "sat", PyLong_FromLong((long)OVERFLOW_SAT));
    PyDict_SetItemString(OverflowEnumType.tp_dict, "excep", PyLong_FromLong((long)OVERFLOW_EXCEP));

    PyDict_SetItemString(RoundingEnumType.tp_dict, "near_pos_inf", PyLong_FromLong((long)ROUNDING_NEAR_POS_INF));
    PyDict_SetItemString(RoundingEnumType.tp_dict, "direct_neg_inf", PyLong_FromLong((long)ROUNDING_DIRECT_NEG_INF));
    PyDict_SetItemString(RoundingEnumType.tp_dict, "near_zero", PyLong_FromLong((long)ROUNDING_NEAR_ZERO));
    PyDict_SetItemString(RoundingEnumType.tp_dict, "direct_zero", PyLong_FromLong((long)ROUNDING_DIRECT_ZERO));
    PyDict_SetItemString(RoundingEnumType.tp_dict, "near_even", PyLong_FromLong((long)ROUNDING_NEAR_EVEN));
}
