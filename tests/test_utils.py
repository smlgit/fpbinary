import sys, math


if sys.version_info[0] >= 3:
    from porting_v3_funcs import *


def get_small_type_size():
    """ Returns the number of bits the FpBinarySmall object should be able to support. """
    return int(math.log(sys.maxsize, 2)) + 1


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


if __name__ == '__main__':
    print('{} --> {}'.format(1.0, set_float_bit_precision(1.0, 4, 4, True)))
    print('{} --> {}'.format(1.0 / 3.0, set_float_bit_precision(1.0 / 3.0, 4, 4, True)))
    print('{} --> {}'.format(-7.75, set_float_bit_precision(-7.75, 4, 1, True)))
    print('{} --> {}'.format(21.0 / 2.0, set_float_bit_precision(21.0/2.0, 8, 0, True)))
    print('{} --> {}'.format(-21.0 / 2.0, set_float_bit_precision(-21.0 / 2.0, 8, 0, True)))


