import math
from collections import deque
from fpbinary import FpBinary, OverflowEnum, RoundingEnum

# ======================================
# Basic FIR Demo
data_path_len = 32

# Just a delay filter
filter_len = 11
filter_coeffs = int(filter_len / 2) * [FpBinary(2, 30, value=0.0)] + \
                [FpBinary(2, 30, value=1.0)] + \
                int(filter_len / 2) * [FpBinary(2, 30, value=0.0)]

# 32 bit data
delay_line = deque(filter_len * [FpBinary(2, 30, value=0.0)], filter_len)

def fir_next_sample(sample):
    # Allow for hardware specs
    guard_bits = int(math.log(filter_len, 2)) + 1
    adder_bits = 48
    adder_in_format = (2 + guard_bits, adder_bits - 2 - guard_bits)
    output_format = (2 + guard_bits, 32 - 2 - guard_bits)

    # Ensure data is correct format
    delay_line.appendleft(sample.resize(format=delay_line[0], overflow_mode=OverflowEnum.wrap))

    accum = 0.0
    for tap, coeff in zip(delay_line, filter_coeffs):
        accum += (tap * coeff).resize(adder_in_format, round_mode=RoundingEnum.direct_neg_inf)

    return accum.resize(output_format)


if __name__ == '__main__':
    for i in range(-10, 10):
        next_sample = fir_next_sample(FpBinary(int_bits=2, frac_bits=30, value=i / 10.0))
        print(next_sample, next_sample.format)





