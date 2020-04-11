from fpbinary import FpBinary, OverflowEnum, RoundingEnum


# New fixed point number from float value
FpBinary(int_bits=4, frac_bits=4, signed=True, value=2.5)


# New fixed point number from float value, format set by another instance
FpBinary(format_inst=FpBinary(int_bits=4, frac_bits=4, signed=True, value=2.5),
               signed=True, value=2.5)


# The (int_bits, frac_bits) tuple can be accessed via the format property
FpBinary(4, 6).format


# If you are dealing with massive numbers such that the float value doesn't have
# enough precision, you can define your value as a bit field (type int)
fp_massive = FpBinary(int_bits=128, frac_bits=128, signed=True, bit_field=(1 << 255) + 1)

# The default string rep uses floats, but the str_ex() method outputs a string that
# preserves precision
fp_massive
fp_massive.str_ex()

# The format can have negative int_bits or negative frac_bits so long as the total
# bits is greater than 0. The meaning of a negative format value is that number of
# bits is removed from the other format part, but the extreme bit position remains
# the same. E.g.:
#
# a format of (-2, 10) gives 8 fractional bits, 0 integer bits and the fractional
# bit positions are 3 to 10 (inclusive).
FpBinary(int_bits=-2, frac_bits=10, signed=False, value=2.0**-10)

# This should saturate to the maximum representable value for an 8 bit unsigned
# with fractional bit positions 3 to 10:
FpBinary(int_bits=-2, frac_bits=10, signed=False, value=2.0**-1)

# Similarly, negative frac_bits produces an instance with only integer bit positions:
FpBinary(int_bits=10, frac_bits=-2, signed=False, value=2.0**9)

# Basic math ops are supported, and overflow is guaranteed to NOT happen
FpBinary(4, 4, value=2.5) + FpBinary(4, 4, value=2.5)
FpBinary(4, 4, value=2.5) - FpBinary(4, 4, value=2.5)
FpBinary(4, 4, value=2.5) * FpBinary(4, 4, value=2.5)
FpBinary(4, 4, value=2.5) / FpBinary(4, 4, value=2.5)

# Negative int/frac bits instances use the same rules of arithmetic, overflow, rounding
# and resultant format as ordinary formats.
add_res = FpBinary(-3, 8, value=0.03125) + FpBinary(9, -2, value=12.0)
add_res
add_res.format

mul_res = FpBinary(-3, 8, value=0.03125) * FpBinary(9, -2, value=12.0)
mul_res
mul_res.format


# Resizing numbers after operations can be done either by format tuple
# or taking the format from another FpBinary instance
mul_res = FpBinary(4, 4, value=2.5) * FpBinary(4, 4, value=2.5)
mul_res.resize((7, 8), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.near_pos_inf)
mul_res.resize(format=FpBinary(2, 4), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.near_pos_inf)


# FpBinary objects play well with other types of numbers - formats will be inferred from the number's value
FpBinary(4, 4, value=2.5) * 3.5
FpBinary(4, 4, value=2.5) * 3


# Unsigned data is also supported
num1 = FpBinary(4, 2, signed=False, value=3.75)
num2 = FpBinary(4, 2, signed=False, value=4.0)
num1 - num2


# FpBinary instances can be sliced and indexed (useful for things like NCOs.')
# The result of slicing is another FpBinary number whose value is the bits interpreted as an int.
# bin() is also useful.
num1 = FpBinary(4, 4, value=5.5)
num1, bin(num1), num1, num1[3:1], num1, bin(num1[3:1]), num1, bin(num1[:]), num1, num1[3]


# The bits_to_signed() method takes all the bits in a FpBinary and interprets them as a signed integer
num1 = FpBinary(4, 4, value=-3.5)
num1, bin(num1), num1.bits_to_signed()


# The __index__ method takes all the bits in a FpBinary and interprets them as an unsigned integer
num1, bin(num1), num1.__index__()
