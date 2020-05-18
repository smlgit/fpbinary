import sys, math, pickle, os
from fpbinary import FpBinary


if sys.version_info[0] >= 3:
    from tests.porting_v3_funcs import *


def get_small_type_size():
    """ Returns the number of bits the FpBinarySmall object should be able to support. """
    return int(math.log(sys.maxsize, 2)) + 1

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

    scaled_value = int(value_mag * 2.0 ** (exp + frac_bits))

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

def fp_binary_instances_are_totally_equal(op1, op2):
    """
    Returns True if the value, format and signed propeties of the instances are equal.
    """
    if op1 == op2 and op1.format == op2.format and op1.is_signed == op2.is_signed:
        return True

    return False


# ================================================================================
# Generating and getting back pickled data from multiple versions
# ================================================================================

pickle_static_file_prefix = 'pickletest'
pickle_static_file_dir = 'data'
pickle_static_data = [
    FpBinary(8, 8, signed=True, value=0.01234),
    FpBinary(8, 8, signed=True, value=-3.01234),
    FpBinary(8, 8, signed=False, value=0.01234),

    FpBinary(get_small_type_size() - 2, 2, signed=True, value=56.789),
    FpBinary(get_small_type_size() - 2, 2, signed=False, value=56.789),

    # All ones, small size
    FpBinary(get_small_type_size() - 2, 2, signed=True,
             bit_field=(1 << get_small_type_size()) - 1),
    FpBinary(get_small_type_size() - 2, 2, signed=False,
             bit_field=(1 << get_small_type_size()) - 1),

    FpBinary(get_small_type_size() - 2, 3, signed=True, value=56436.25),
    FpBinary(get_small_type_size() - 2, 3, signed=False, value=56436.25),

    # All ones, large size
    FpBinary(get_small_type_size() - 2, 3, signed=True,
             bit_field=(1 << (get_small_type_size() + 1)) - 1),
    FpBinary(get_small_type_size() - 2, 3, signed=False,
             bit_field=(1 << (get_small_type_size() + 1)) - 1),

    FpBinary(get_small_type_size(),
             get_small_type_size(), signed=True,
             bit_field=(1 << (get_small_type_size() + 5)) + 23),
    FpBinary(get_small_type_size(),
             get_small_type_size(), signed=False,
             bit_field=(1 << (get_small_type_size() * 2)) - 1),
]

def gen_static_pickle_files():
    """
    File name format: pickle_test_v[python_version]_p[pickle protocol].data
    """

    for protocol in range(2, pickle.HIGHEST_PROTOCOL + 1):
        fname = '{}_v{}_{}_{}_p{}.data'.format(
            pickle_static_file_prefix,
            sys.version_info.major, sys.version_info.minor, sys.version_info.micro,
            protocol
        )

        with open(os.path.join(pickle_static_file_dir, fname), 'wb') as f:
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


