import sys, math, pickle, os, struct
from fpbinary import FpBinary, FpBinarySwitchable, FpBinaryComplex


if sys.version_info[0] >= 3:
    from tests.porting_v3_funcs import *


def get_small_type_size():
    """ Returns the number of bits the FpBinarySmall object should be able to support.
        This is based on the assumption that FpBinary uses the long long type . """
    return 8 * struct.calcsize("q")

def get_interpreter_arch_size():
    """ Returns the number of bits the python environment was compiled for.
        Note that this could be 32 on a 64 bit machine if running python in 32 bit. """
    return 8 * struct.calcsize("P")

def get_max_signed_value_bit_field(num_bits):
    return (long(1) << (num_bits - 1)) - 1

def get_min_signed_value_bit_field(num_bits):
    return (long(1) << (num_bits - 1))

def get_max_unsigned_value_bit_field(num_bits):
    return (long(1) << num_bits) - 1

def get_max_signed_value_bit_field_for_arch():
    return get_max_signed_value_bit_field(get_small_type_size())

def get_min_signed_value_bit_field_for_arch():
    return get_min_signed_value_bit_field(get_small_type_size())

def get_max_unsigned_value_bit_field_for_arch():
    return get_max_unsigned_value_bit_field(get_small_type_size())

def convert_float_to_bit_field(value, int_bits, frac_bits):
    mant, exp = math.frexp(value)

    # Need to operate on magnitude bits and then do the 2's complement
    # to get the appropriate truncation behavior for negative numbers.
    value_mag = abs(mant)

    scaled_value = long(value_mag * 2.0 ** (exp + frac_bits))

    # Now convert back to negative representation if needed
    if value < 0.0:
        scaled_value = (~scaled_value) + 1

    # Make sure only desired bits are used (note that this will convert
    # the number back to a positive long integer - its a bit field.
    return scaled_value & ((long(1) << (int_bits + frac_bits)) - 1)


def set_float_bit_precision(value, int_bits, frac_bits, is_signed):
    """
    Modifies value to the precision defined by int_bits and frac_bits.

    :param value: input float
    :param int_bits: number of integer bits to restrict to
    :param frac_bits: number of fractional bits to restrict to
    :param is_signed: Determines how many int bits can actually be used for magnitude
    :return: float - the input restricted to (int_bits + frac_bits) bits
    """

    bit_field = convert_float_to_bit_field(value, int_bits, frac_bits)

    # Convert to an actual negative number if required
    if is_signed and value < 0.0:
        bit_field -= (long(1) << (int_bits + frac_bits))

    # And convert back to float
    return bit_field / 2.0**frac_bits


def fp_binary_fields_equal(op1, op2):
    """
    Returns true if the FpBinary fields of op1 and op2 are equal (including value)
    """
    return (op1.format == op2.format and op1.is_signed == op2.is_signed and op1 == op2)

def fp_binary_complex_fields_equal(op1, op2):
    """
    Returns true if the FpBinaryComplex fields of op1 and op2 are equal (including value)
    """
    return (op1.format == op2.format and op1.is_signed == op2.is_signed and op1 == op2)

def fp_binary_instances_are_totally_equal(op1, op2):
    """
    The point of this is to check instances of FpBinary/Switchable/Complex are equal
    in value and other properties. If python numeric objects are passed, only value is checked.
    Returns True if the properties of the two objects are the same.
    """

    if isinstance(op1, FpBinary) or isinstance(op2, FpBinary):
        return (isinstance(op1, FpBinary) and isinstance(op2, FpBinary) and op1.format == op2.format and
                op1.is_signed == op2.is_signed and op1 == op2)
    elif isinstance(op1, FpBinarySwitchable) or isinstance(op2, FpBinarySwitchable):
        return (isinstance(op1, FpBinarySwitchable) and isinstance(op2, FpBinarySwitchable) and
                op1.fp_mode == op2.fp_mode and op1.min_value == op2.min_value and op1.max_value == op2.max_value and
                fp_binary_instances_are_totally_equal(op1.value, op2.value))
    elif isinstance(op1, FpBinaryComplex) or isinstance(op2, FpBinaryComplex):
        return (isinstance(op1, FpBinaryComplex) and isinstance(op2, FpBinaryComplex) and op1.format == op2.format and
                op1.is_signed == op2.is_signed and op1 == op2)


    return op1 == op2


# ================================================================================
# Generating and getting back pickled data from multiple versions.
# This includes FpBinary and FpBinarySwitchable instances
# ================================================================================

pickle_static_file_prefix = 'pickletest'
pickle_static_file_dir = 'data'
pickle_static_data = [
    FpBinary(8, 8, signed=True, value=0.01234),
    FpBinary(8, 8, signed=True, value=-3.01234),
    FpBinary(8, 8, signed=False, value=0.01234),

    FpBinary(64 - 2, 2, signed=True, value=56.789),
    FpBinary(64 - 2, 2, signed=False, value=56.789),

    FpBinarySwitchable(fp_mode=False, fp_value=FpBinary(16, 16, signed=True, value=5.875)),
    FpBinarySwitchable(fp_mode=False, float_value=-45.6),


    # All ones, small size
    FpBinary(64 - 2, 2, signed=True, bit_field=(1 << 64) - 1),
    FpBinary(64 - 2, 2, signed=False, bit_field=(1 << 64) - 1),

    FpBinary(64 - 2, 3, signed=True, value=56436.25),
    FpBinary(64 - 2, 3, signed=False, value=56436.25),

    FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(64 - 2, 2, signed=True)),

    # All ones, large size
    FpBinary(64 - 2, 3, signed=True, bit_field=(1 << (64 + 1)) - 1),
    FpBinary(64 - 2, 3, signed=False, bit_field=(1 << (64 + 1)) - 1),

    FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(64, 64, signed=False,
                         bit_field=(1 << (64 * 2)) - 1)),


    FpBinary(64, 64, signed=True, bit_field=(1 << (64 + 5)) + 23),
    FpBinary(64, 64, signed=False, bit_field=(1 << (64 * 2)) - 1),

    FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=5.875)),
    FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(64 - 2, 3, signed=True)),
    FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(64, 64, signed=True,
                    bit_field=(1 << (64 + 5)) + 23)),

]

def gen_static_pickle_files():
    """
    File name format: pickle_test_v[python_version]_p[pickle protocol]_[os_name]_[word_len].data
    """

    this_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(this_dir, pickle_static_file_dir)

    for protocol in range(2, pickle.HIGHEST_PROTOCOL + 1):
        fname = '{}_v{}_{}_{}_p{}_{}_{}.data'.format(
            pickle_static_file_prefix,
            sys.version_info.major, sys.version_info.minor, sys.version_info.micro,
            protocol, sys.platform, get_interpreter_arch_size()
        )

        with open(os.path.join(data_dir, fname), 'wb') as f:
            pickle.dump(pickle_static_data, f, protocol)

def get_static_pickle_file_paths():
    result = []

    this_dir = os.path.dirname(os.path.abspath(__file__))
    data_dir = os.path.join(this_dir, pickle_static_file_dir)

    for f in os.listdir(data_dir):
        if pickle_static_file_prefix in f:
            file_protocol = int(f.split('_')[4].split('.')[0].strip('p'))
            if file_protocol <= pickle.HIGHEST_PROTOCOL:
                result.append(os.path.join(data_dir, f))

    return result


if __name__ == '__main__':
    # Generate pickling data
    gen_static_pickle_files()
    print(get_static_pickle_file_paths())


