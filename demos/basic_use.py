from fpbinary import FpBinary, OverflowEnum, RoundingEnum

# New fixed point number from float value
print(  FpBinary(int_bits=4, frac_bits=4, signed=True, value=2.5)  )

# New fixed point number from float value, format set by another instance
print(  FpBinary(format_inst=FpBinary(int_bits=4, frac_bits=4, signed=True, value=2.5),
                 signed=True, value=2.5)  )

# The format property is the (int_bits, frac_bits) information
print('format: {}'.format(FpBinary(4, 6).format))

# If you are dealing with massive numbers such that the float value doesn't have
# enough precision, you can define your value as a bit field (type int)
fp_massive = FpBinary(int_bits=128, frac_bits=128, signed=True, bit_field=(1 << 255) + 1)

# The default string rep uses floats, but the str_ex() method outputs a string that
# preserves precision
print(fp_massive)
print(fp_massive.str_ex())

print(fp_massive.__repr__())
# Basic math ops are supported, and overflow is guaranteed to NOT happen
#print( FpBinary(4, 4, value=2.5) + FpBinary(4, 4, value=2.5) ,

