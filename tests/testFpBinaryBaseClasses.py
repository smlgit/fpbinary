#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SML, some tests adapted from RW Penney's Simple Fixed Point module

import sys, unittest, copy
import numpy as np
import scipy.signal as signal
import tests.test_utils as test_utils
from fpbinary import FpBinary, _FpBinarySmall, _FpBinaryLarge, OverflowEnum, RoundingEnum, FpBinaryOverflowException

if sys.version_info[0] >= 3:
    from tests.porting_v3_funcs import *


class AbstractTestHider(object):
    class BaseClassesTestAbstract(unittest.TestCase):
        def setUp(self):
            self.fp_zero = self.fp_binary_class(1, 0, signed=True, value=0.0)
            self.fp_one = self.fp_binary_class(2, 0, signed=True, value=1.0)
            self.fp_minus_one = self.fp_binary_class(2, 0, signed=True, value=-1.0)
            self.fp_two = self.fp_binary_class(3, 0, signed=True, value=2.0)

        def tearDown(self):
            pass

        # Required because base classes only support comparison operations between
        # their own kind.
        def assertEqualWithFloatCast(self, first, second, msg=None):
            super(AbstractTestHider.BaseClassesTestAbstract, self).assertEqual(float(first), float(second), msg)

        def assertAlmostEqual(self, first, second, places=7, msg=''):
            tol = 10.0 ** -places
            self.assertTrue(float(abs(first - second)) < tol,
                            '{} and {} differ by more than {} ({}) {}'.format(
                                first, second, tol, (first - second), msg))

        def testCreateWithOnlyValueParam(self):
            # Should create a fixed point type with smallest required int and frac bits
            fpNum = self.fp_binary_class(value=1.5)
            self.assertEqual(fpNum.format, (2, 1))

            fpNum = self.fp_binary_class(value=-8.0625)
            self.assertEqual(fpNum.format, (5, 4))

            fpNum = self.fp_binary_class(value=5)
            self.assertEqual(fpNum.format, (4, 0))

        def testCreateParamsWrong(self):
            # These parameter test cases should raise an exception
            params_test_cases = [
                # int_bits is float
                ([4.2, 3], {}),
                # frac_bits is float
                ([4, 3.2], {}),
                # signed is number
                ([4, 3], {'signed': 1}),
                # signed is text
                ([4, 3], {'signed': 'True'}),
                # value is text
                ([4, 3], {'signed': True, 'value': '45.6'}),
                # bit_field is float
                ([4, 3], {'signed': False, 'bit_field': 45.6}),
                # bit field is text
                ([4, 3], {'signed': True, 'bit_field': '20'}),
                # format_inst is a tuple
                ([4, 3], {'signed': True, 'format_inst': (2, 3)}),
            ]

            for test_case in params_test_cases:
                try:
                    fpNum = self.fp_binary_class(*test_case[0], **test_case[1])
                except TypeError:
                    pass
                except ValueError:
                    pass
                else:
                    self.fail('Failed on test case {}'.format(test_case))

        def testResizeParams(self):
            # These parameter test cases should raise an exception
            params_test_cases = [
                # no format tuple
                ((), {'overflow_mode': OverflowEnum.wrap, 'round_mode': RoundingEnum.direct_neg_inf}),
                # format tuple is not size 2
                ((4,), {'overflow_mode': OverflowEnum.wrap, 'round_mode': RoundingEnum.direct_neg_inf}),
                # format tuple is not size 2
                ((4, 5, 6), {'overflow_mode': OverflowEnum.sat, 'round_mode': RoundingEnum.near_pos_inf}),
            ]

            for test_case in params_test_cases:
                fpNum = self.fp_binary_class(5, 5, value=0.0)
                try:
                    fpNum.resize(*test_case[0], **test_case[1])
                except TypeError:
                    pass
                else:
                    self.fail('Failed on test case {}'.format(test_case))

        def testFormatProperty(self):
            fpNum = self.fp_binary_class(2, 5, value=1.5, signed=True)
            self.assertTrue(fpNum.format == (2, 5))

            fpNum = self.fp_binary_class(-200, 232, signed=True)
            self.assertTrue(fpNum.format == (-200, 232))

            fpNum = self.fp_binary_class(201, -190, signed=True)
            self.assertTrue(fpNum.format == (201, -190))

        def testBoolConditions(self):
            """Values used in boolean expressions should behave as true/false"""
            if self.fp_binary_class(2, 2, signed=True, value=0):
                self.fail()
            if self.fp_binary_class(2, 2, signed=True, value=1):
                pass
            else:
                self.fail()

        def testImmutable(self):
            """Arithmetic operations on object should not alter orignal value"""
            scale = 0.297
            for i in range(-8, 8):
                orig = self.fp_binary_class(4, 5, signed=True, value=i * scale)

                x = copy.copy(orig)
                x0 = x
                if x is x0:
                    pass
                else:
                    self.fail()

                x = copy.copy(orig)
                x0 = x
                x += self.fp_one
                self.assertEqualWithFloatCast(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x -= self.fp_one
                self.assertEqualWithFloatCast(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x *= self.fp_two
                self.assertEqualWithFloatCast(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x /= self.fp_two
                self.assertEqualWithFloatCast(orig, x0)
                if x is x0: self.fail()

        def testIntCasts(self):
            """Rounding on casting to int should match float-conversions"""
            for i in range(-40, 40):
                x = i / 8.0
                self.assertEqualWithFloatCast(int(x), int(self.fp_binary_class(4, 16, signed=True, value=x)))

        def testNegating(self):
            for i in range(-32, 32):
                x = i * 0.819
                fx = self.fp_binary_class(10, 16, signed=True, value=x)

                self.assertEqualWithFloatCast(0.0, (fx + (-fx)).resize(self.fp_zero.format))
                self.assertEqualWithFloatCast(0.0, (-fx + fx).resize(self.fp_zero.format))
                self.assertEqualWithFloatCast((self.fp_minus_one * fx).resize(fx.format), -fx)
                self.assertEqualWithFloatCast(0.0, ((self.fp_minus_one * fx).resize(fx.format) + (-fx) +
                                       (self.fp_two * (fx)).resize(fx.format)).resize(fx.format))

        def testAddition(self):
            """Addition operators should promote & commute"""
            scale = 0.125
            for x in range(-16, 16):
                fpx = self.fp_binary_class(8, 15, signed=True, value=x * scale)
                for y in range(-32, 32):
                    fpy = self.fp_binary_class(value=y * scale, format_inst=fpx)
                    fpa = self.fp_binary_class(value=(x + y) * scale, format_inst=fpx)

                    # compute various forms of a = (x + y):
                    self.assertEqualWithFloatCast(fpa, (fpx + fpy).resize(fpa.format))
                    self.assertEqualWithFloatCast(fpa, (fpy + fpx).resize(fpa.format))
                    self.assertEqualWithFloatCast((x + y) * scale, float(fpx + fpy))

                    tmp = fpx
                    tmp += fpy
                    self.assertEqualWithFloatCast(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b01.111 + b01.111 = b011.110
            fpx = self.fp_binary_class(2, 3, signed=True, value=1.875)
            addition = fpx + fpx
            self.assertEqualWithFloatCast(addition, 3.75)
            self.assertTrue(addition.format == (3, 3))

            # Check boundaries unsigned
            # b11.111 + b11.111 = b111.110
            fpx = self.fp_binary_class(2, 3, signed=False, value=3.875)
            addition = fpx + fpx
            self.assertEqualWithFloatCast(addition, 7.75)
            self.assertTrue(addition.format == (3, 3))

            # Check result format
            fpx = self.fp_binary_class(4, 5, signed=True, value=0.875)
            fpy = self.fp_binary_class(3, 6, signed=True, value=0.875)
            self.assertEqual((fpx + fpy).format, (5, 6))
            self.assertEqual((fpy + fpx).format, (5,6))


            """
            Various combinations of int_bits and frac_bits to test bit values.
            We work out what the minimum representable value is and use this as an increment
            to test adding from the minimum value to the max value.
            """
            for is_signed in iter([True, False]):
                format_test_cases = [(4, 4), (4, 0), (8, -3), (0, 4), (-3, 7)]
                for fmat in format_test_cases:
                    int_bits = fmat[0]
                    frac_bits = fmat[1]
                    msb_pos = int_bits - 1
                    lsb_pos = -frac_bits

                    min_val = -2.0**msb_pos if is_signed else 0.0
                    max_limit_val = 2.0**msb_pos if is_signed else 2.0**(msb_pos + 1)
                    inc = 2.0**lsb_pos

                    float_accum = min_val
                    fp_accum = self.fp_binary_class(int_bits, frac_bits, signed=is_signed, value=min_val)

                    # Test with a smaller format for variation
                    fp_inc = self.fp_binary_class(int_bits - 1, frac_bits, signed=is_signed, value=inc)

                    while float_accum < max_limit_val:
                        self.assertEqualWithFloatCast(fp_accum, float_accum)

                        fp_accum.resize((int_bits, frac_bits), overflow_mode=OverflowEnum.excep)

                        fp_accum += fp_inc
                        float_accum += inc

                        self.assertEqual(fp_accum.format, (int_bits + 1, frac_bits))




        def testSubtraction(self):
            """Subtraction operators should promote & anti-commute"""
            scale = 0.0625
            for x in range(-16, 16):
                fpx = self.fp_binary_class(8, 15, signed=True, value=x * scale)
                for y in range(-32, 32):
                    fpy = self.fp_binary_class(value=y * scale, format_inst=fpx)
                    fpa = self.fp_binary_class(value=(x - y) * scale, format_inst=fpx)

                    # compute various forms of a = (x - y):

                    self.assertEqualWithFloatCast(fpa, (fpx - fpy).resize(fpa.format))
                    self.assertEqualWithFloatCast(-fpa, (fpy - fpx).resize(fpa.format))
                    self.assertEqualWithFloatCast((x - y) * scale, float(fpx - fpy))

                    tmp = fpx
                    tmp -= fpy
                    self.assertEqualWithFloatCast(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b10.000 - b01.111 = b100.001
            fpx = self.fp_binary_class(2, 3, signed=True, value=-2.0)
            fpy = self.fp_binary_class(2, 3, signed=True, value=1.875)
            sub = fpx - fpy
            self.assertEqualWithFloatCast(sub, -3.875)
            self.assertTrue(sub.format == (3, 3))

            # Check boundaries unsigned
            # b00.000 - b11.111 = b100.001
            fpx = self.fp_binary_class(2, 3, signed=False, value=0.0)
            fpy = self.fp_binary_class(2, 3, signed=False, value=3.875)
            sub = fpx - fpy
            self.assertEqualWithFloatCast(sub, 4.125)
            self.assertTrue(sub.format == (3, 3))

            # Check result format
            fpx = self.fp_binary_class(4, 5, signed=True, value=0.875)
            fpy = self.fp_binary_class(3, 6, signed=True, value=0.875)
            self.assertEqual((fpx - fpy).format, (5, 6))
            self.assertEqual((fpy - fpx).format, (5, 6))


            """
            Various combinations of int_bits and frac_bits to test bit values.
            We work out what the minimum representable value is and use this as an increment
            to test subtraction from the max value to the minimum value.
            """

            for is_signed in iter([True, False]):
                format_test_cases = [(4, 4), (4, 0), (8, -3), (0, 4), (-3, 7)]
                for fmat in format_test_cases:
                    int_bits = fmat[0]
                    frac_bits = fmat[1]
                    msb_pos = int_bits - 1
                    lsb_pos = -frac_bits

                    min_val = -2.0 ** msb_pos if is_signed else 0.0
                    max_limit_val = 2.0 ** msb_pos if is_signed else 2.0 ** (msb_pos + 1)
                    inc = 2.0 ** lsb_pos

                    float_accum = max_limit_val - inc
                    fp_accum = self.fp_binary_class(int_bits, frac_bits, signed=is_signed, value=float_accum)

                    # Test with a smaller format for variation
                    fp_inc = self.fp_binary_class(int_bits - 1, frac_bits, signed=is_signed, value=inc)

                    while float_accum >= min_val:
                        self.assertEqualWithFloatCast(fp_accum, float_accum)

                        fp_accum.resize((int_bits, frac_bits), overflow_mode=OverflowEnum.excep)

                        fp_accum -= fp_inc
                        float_accum -= inc

                        self.assertEqual(fp_accum.format, (int_bits + 1, frac_bits))

        def testMultiplication(self):
            """Multiplication operators should promote & commute"""
            scale = 0.25
            scale2 = scale * scale
            for x in range(-16, 32):
                fpx = self.fp_binary_class(16, 16, signed=True, value=x * scale)
                for y in range(-32, 16):
                    fpy = self.fp_binary_class(value=y * scale, format_inst=fpx)
                    fpa = self.fp_binary_class(value=(x * y) * scale2, format_inst=fpx)
                    # compute various forms of a = (x * y):
                    self.assertEqualWithFloatCast(fpa, (fpx * fpy).resize(fpa.format))
                    self.assertEqualWithFloatCast(fpa, (fpy * fpx).resize(fpa.format))
                    self.assertEqualWithFloatCast((x * y) * scale2, float(fpx * fpy))

                    tmp = fpx
                    tmp *= fpy
                    self.assertEqualWithFloatCast(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b01.111 * b01.111
            fpx = self.fp_binary_class(2, 3, signed=True, value=1.875)
            mult = fpx * fpx
            self.assertEqualWithFloatCast(mult, 3.515625)
            self.assertTrue(mult.format == (4, 6))

            # Check boundaries unsigned
            # b11.111 * b11.111
            fpx = self.fp_binary_class(2, 3, signed=False, value=3.875)
            mult = fpx * fpx
            self.assertEqualWithFloatCast(mult, 15.015625)
            self.assertTrue(mult.format == (4, 6))

            # Check result format
            fpx = self.fp_binary_class(4, 5, signed=True, value=0.875)
            fpy = self.fp_binary_class(3, 6, signed=True, value=0.875)
            self.assertEqual((fpx * fpy).format, (7, 11))
            self.assertEqual((fpy * fpx).format, (7, 11))


            """
            Various combinations of int_bits and frac_bits to test bit values.
            """

            for is_signed in iter([True, False]):

                float_multiplier = -3.0 if is_signed else 3.0
                fp_multiplier = self.fp_binary_class(3, 0, signed=is_signed, value=float_multiplier)

                format_test_cases = [(8, 4), (8, 0), (10, -3), (0, 7), (-3, 9)]
                for fmat in format_test_cases:
                    int_bits = fmat[0]
                    frac_bits = fmat[1]
                    msb_pos = int_bits - 1
                    lsb_pos = -frac_bits

                    min_val = -2.0 ** msb_pos if is_signed else 0.0
                    max_limit_val = 2.0 ** msb_pos if is_signed else 2.0 ** (msb_pos + 1)
                    inc = 2.0 ** lsb_pos

                    float_accum = inc
                    fp_accum = self.fp_binary_class(int_bits, frac_bits, signed=is_signed, value=float_accum)

                    while float_accum >= min_val and float_accum < max_limit_val:
                        self.assertEqualWithFloatCast(fp_accum, float_accum)

                        fp_accum.resize((int_bits, frac_bits), overflow_mode=OverflowEnum.excep)

                        fp_accum *= fp_multiplier
                        float_accum *= float_multiplier

                        self.assertEqual(fp_accum.format, (int_bits + fp_multiplier.format[0],
                                                           frac_bits + fp_multiplier.format[1]))

        def testDivision(self):

            # We cycle through all number combinations for specified int/frac bits.
            # Accuracy is checked by using the test_utils.set_float_bit_precision
            # function to reduce the bits in the result of an actual float division.

            # Do for both signed and unsigned
            is_signed_test_cases = [False, True]

            for is_signed in is_signed_test_cases:

                num_int_bits = 4
                num_frac_bits = 3
                denom_int_bits = 5
                denom_frac_bits = 3

                num_min = -2.0 ** (num_int_bits - 1) if is_signed else 0.0
                num_max = -num_min - 1 if is_signed else 2.0 ** num_int_bits

                denom_min = -2.0 ** (denom_int_bits - 1) if is_signed else 0.0
                denom_max = -denom_min - 1 if is_signed else 2.0 ** denom_int_bits

                num_inc = 2.0 ** -num_frac_bits
                denom_inc = 2.0 ** -denom_frac_bits

                num_val = num_min

                while num_val < num_max:
                    fp_num = self.fp_binary_class(num_int_bits, num_frac_bits, signed=is_signed, value=num_val)
                    denom_val = denom_min
                    while denom_val < denom_max:
                        if denom_val != 0.0:
                            result_format = (num_int_bits + denom_frac_bits + 1 if is_signed else num_int_bits + denom_frac_bits,
                                             num_frac_bits + denom_int_bits)
                            fp_denom = self.fp_binary_class(denom_int_bits, denom_frac_bits, signed=is_signed,
                                                            value=denom_val)

                            self.assertEqualWithFloatCast(test_utils.set_float_bit_precision(num_val / denom_val,
                                                                                result_format[0],
                                                                                result_format[1],
                                                                                is_signed),
                                             fp_num / fp_denom)
                            self.assertAlmostEqual(float(fp_num / fp_denom), num_val / denom_val, places=2)
                            self.assertTrue((fp_num / fp_denom).format == result_format)

                        denom_val += denom_inc

                    num_val += num_inc

                # Check boundary of 64 bit implementation
                # Allow 2 bits for the denominator
                max_bits = test_utils.get_small_type_size() - 3 \
                    if is_signed else test_utils.get_small_type_size() - 2
                int_bits = int(max_bits / 2)
                frac_bits = max_bits - int_bits

                fp_num = self.fp_binary_class(int_bits, frac_bits, signed=is_signed,
                                              bit_field=((long(1) << max_bits) - 1))
                fp_denom = self.fp_binary_class(2, 0, signed=is_signed, value=1.0)

                self.assertEqual(fp_num / fp_denom, fp_num)
                self.assertEqual((fp_num / fp_denom).format,
                                 (int_bits + fp_denom.format[1] + 1 if is_signed else int_bits + fp_denom.format[1],
                                  frac_bits + fp_denom.format[0]))


                # Basic checks for negative int_bits and frac_bits
                fp_num = self.fp_binary_class(-2, 6, signed=is_signed, value=0.0625)
                fp_denom = self.fp_binary_class(-3, 8, signed=is_signed, value=0.0078125)
                fp_res = fp_num / fp_denom
                fp_check = self.fp_binary_class(7 if is_signed else 6, 3, signed=is_signed, value=8.0)
                self.assertEqual(fp_res, fp_check)
                self.assertEqual(fp_res.format, fp_check.format)

                fp_num = self.fp_binary_class(8, -2, signed=is_signed, value=32)
                fp_denom = self.fp_binary_class(2, 3, signed=is_signed, value=0.5)
                fp_res = fp_num / fp_denom
                fp_check = self.fp_binary_class(12 if is_signed else 11, 0, signed=is_signed, value=64)
                self.assertEqual(fp_res, fp_check)
                self.assertEqual(fp_res.format, fp_check.format)


                fp_num = self.fp_binary_class(2, 3, signed=is_signed, value=0.5)
                fp_denom = self.fp_binary_class(8, -2, signed=is_signed, value=32)
                fp_res = fp_num / fp_denom
                fp_check = self.fp_binary_class(1 if is_signed else 0, 11, signed=is_signed, value=0.015625)
                self.assertEqual(fp_res, fp_check)
                self.assertEqual(fp_res.format, fp_check.format)

                fp_num = self.fp_binary_class(9, -3, signed=is_signed, value=128)
                fp_denom = self.fp_binary_class(8, -2, signed=is_signed, value=32)
                fp_res = fp_num / fp_denom
                fp_check = self.fp_binary_class(8 if is_signed else 7, 5, signed=is_signed, value=4.0)
                self.assertEqual(fp_res, fp_check)
                self.assertEqual(fp_res.format, fp_check.format)

        def testBitShifts(self):
            """Check effects of left & right shift operators."""
            format_obj = self.fp_binary_class(32, 32, signed=True)

            self.assertEqualWithFloatCast(self.fp_binary_class(value=1, format_inst=format_obj) << long(2), 4)
            self.assertEqualWithFloatCast(self.fp_binary_class(value=3, format_inst=format_obj) << long(4), 48)
            self.assertEqualWithFloatCast(self.fp_binary_class(value=-7, format_inst=format_obj) << long(8), -7 * 256)

            self.assertEqualWithFloatCast(self.fp_binary_class(value=1, format_inst=format_obj) >> long(1), 0.5)
            self.assertEqualWithFloatCast(self.fp_binary_class(value=12, format_inst=format_obj) >> long(2), 3)
            self.assertEqualWithFloatCast(self.fp_binary_class(value=-71 * 1024, format_inst=format_obj) >> long(12), -17.75)

        def testIntRange(self):
            """ A floating point number is used to track the approximate value of a
                small increment operation. Fixed point numbers mirror the increment
                operation and are resized after each increment. At a certain point,
                the resize operation should raise an overflow exception. The try/except/else
                blocks verify whether an exception should or shouldn't be raised.
                Note that, in order to avoid erroneous fails, we need to used two
                increment representations - a rounded down and round up version. These
                are guaranteed (or should be...) to be less and larger than the floating
                point variable respectively. """
            int_length_list = [-2, -1, 0, 1, 2, 4, 6, 7, 8]
            for top in int_length_list:
                frac_bits = 16
                if (top < 0): frac_bits = frac_bits - top
                inc = 1.0 / 16.01
                format_obj = self.fp_binary_class(top, frac_bits, signed=True)

                pos_limit = 2 ** (top - 1)
                neg_limit = -2 ** (top - 1)

                # Fixed point increment values. We need a rounded down and a rounded
                # up version of the floating point inc number so that we can make sure
                # we are never in front or behind the floating point cnt variable.
                fpIncRoundedDown = self.fp_binary_class(max(int_length_list), frac_bits, signed=True,
                                                        value=inc)
                fpIncRoundedUp = self.fp_binary_class(max(int_length_list), frac_bits, signed=True, value=inc)

                cnt, xRoundedDown, xRoundedUp, yRoundedDown, yRoundedUp = 0, self.fp_zero, self.fp_zero, self.fp_zero, self.fp_zero
                while cnt < (pos_limit + 5):
                    cnt += inc

                    xRoundedDown += fpIncRoundedDown
                    xRoundedUp += fpIncRoundedUp
                    try:
                        xRoundedDown.resize(format_obj.format, overflow_mode=OverflowEnum.excep,
                                            round_mode=RoundingEnum.direct_neg_inf)
                    except FpBinaryOverflowException:
                        # print 'cnt: {0}  pos_limit: {1}  top: {2}   x: {3}'.format(cnt, pos_limit, top, float(x))
                        if cnt < pos_limit:
                            self.fail()

                    try:
                        xRoundedUp.resize(format_obj.format, overflow_mode=OverflowEnum.excep,
                                          round_mode=RoundingEnum.near_pos_inf)
                    except FpBinaryOverflowException:
                        pass
                    else:
                        # print 'cnt: {0}  pos_limit: {1}  top: {2}   x: {3}'.format(cnt, pos_limit, top, float(x))
                        if cnt >= pos_limit: self.fail()

                    yRoundedDown -= fpIncRoundedDown
                    yRoundedUp -= fpIncRoundedUp

                    try:
                        yRoundedDown.resize(format_obj.format, overflow_mode=OverflowEnum.excep,
                                            round_mode=RoundingEnum.direct_neg_inf)
                    except FpBinaryOverflowException:
                        if -cnt >= neg_limit: self.fail()

                    try:
                        yRoundedUp.resize(format_obj.format, overflow_mode=OverflowEnum.excep,
                                          round_mode=RoundingEnum.near_pos_inf)
                    except FpBinaryOverflowException:
                        pass
                    else:
                        if -cnt < neg_limit: self.fail()

        def testOverflowModes(self):
            # =======================================================================
            # Wrapping

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.875)
            fpNum.resize((3, 3), overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(fpNum, 3.875)

            # Losing MSB, positive to negative
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(fpNum, -0.25)

            # Losing MSB, positive to positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.75)
            fpNum.resize((3, 2), overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(fpNum, 2.75)

            # Neg int_bits, losing MSB, positive to negative
            fpNum = self.fp_binary_class(-3, 8, signed=True, value=0.0546875)
            fpNum.resize((-4, 7), overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(fpNum, -0.0078125)

            # Neg frac_bits, losing MSB, positive to negative
            fpNum = self.fp_binary_class(7, -3, signed=True, value=40.0)
            fpNum.resize((6, -3), overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(fpNum, -24.0)

            # =======================================================================
            # Saturation

            # Losing MSBs, no saturation required
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.25)
            fpNum.resize((3, 3), overflow_mode=OverflowEnum.sat)
            self.assertEqualWithFloatCast(fpNum, 3.25)

            # Losing MSB, positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.sat)
            self.assertEqualWithFloatCast(fpNum, 7.75)

            # Losing MSB, negative
            fpNum = self.fp_binary_class(4, 2, signed=True, value=-7.75)
            fpNum.resize((3, 2), overflow_mode=OverflowEnum.sat)
            self.assertEqualWithFloatCast(fpNum, -4.0)

            # Neg int_bits, losing MSB, positive
            fpNum = self.fp_binary_class(-1, 6, signed=True, value=0.15625)
            fpNum.resize((-2, 6), overflow_mode=OverflowEnum.sat)
            self.assertEqualWithFloatCast(fpNum, 0.109375)

            # Neg frac_bits, losing MSB, positive
            fpNum = self.fp_binary_class(5, -2, signed=True, value=8.0)
            fpNum.resize((4, -2), overflow_mode=OverflowEnum.sat)
            self.assertEqualWithFloatCast(fpNum, 4.0)

            # =======================================================================
            # Exception

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.875)
            try:
                fpNum.resize((3, 3), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                self.fail()

            # Losing MSB, positive to negative
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            try:
                fpNum.resize((4, 2), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, positive to positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.75)
            try:
                fpNum.resize((3, 2), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Neg int_bits, losing MSB, positive to positive
            fpNum = self.fp_binary_class(-1, 6, signed=True, value=0.15625)
            try:
                fpNum.resize((-2, 6), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Neg frac_bits, losing MSB, positive to positive
            fpNum = self.fp_binary_class(7, -3, signed=True, value=32.0)
            try:
                fpNum.resize((6, -3), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # =======================================================================
            # Left shifting

            # Losing MSBs, no sign change expected
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.25)
            fpNum <<= long(2)
            self.assertEqualWithFloatCast(fpNum, 13.0)

            # Losing MSB, positive to negative expected
            fpNum = self.fp_binary_class(5, 2, signed=True, value=5.5)
            fpNum <<= long(2)
            self.assertEqualWithFloatCast(fpNum, -10.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(5, 3, signed=True, value=-14.875)
            fpNum <<= long(3)
            self.assertEqualWithFloatCast(fpNum, 9.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(2, 2, signed=True, value=-1.5)
            fpNum <<= long(1)
            self.assertEqualWithFloatCast(fpNum, 1.0)

            # Neg int_bits, losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(-7, 11, signed=True, value=-0.00146484375)
            fpNum <<= long(2)
            self.assertEqualWithFloatCast(fpNum, 0.001953125)

            # Neg frac_bits, losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(6, -2, signed=True, value=-28.0)
            fpNum <<= long(1)
            self.assertEqualWithFloatCast(fpNum, 8.0)

        def testRoundingDirectNegativeInfinity(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.125)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 7), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -0.0234375)

            fpNum1 = self.fp_binary_class(7, -3, signed=True, value=-48.0)
            res = fpNum1.resize((7, -4), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -48.0)

            # =======================================================================
            # Change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.0)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -0.03125)

            fpNum1 = self.fp_binary_class(7, -2, signed=True, value=52.0)
            res = fpNum1.resize((7, -3), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 48.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -1.5)

            # =======================================================================
            # Max and min values frac resized

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=3.75)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 3.5)

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=-0.25)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -0.5)

            fpNum1 = self.fp_binary_class(2, 2, signed=False, value=3.75)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 3.5)

            # =======================================================================
            # Max/min values for native platform - check no overflow due to rounding
            # with saturation overflow mode.

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Tie break explicit testing

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 5.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-5.25)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -5.5)

            fpNum1 = self.fp_binary_class(4, 4, signed=False, value=5.125)
            res = fpNum1.resize((4, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 5.0)

        def testRoundingDirectTowardsZero(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.125)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 7), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -0.0234375)

            # =======================================================================
            # Change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.0)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -0.015625)

            fpNum1 = self.fp_binary_class(7, -2, signed=True, value=52.0)
            res = fpNum1.resize((7, -3), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 48.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -1.0)

            # =======================================================================
            # Max and min values frac resized

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=3.75)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 3.5)

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=-0.25)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 0.0)

            fpNum1 = self.fp_binary_class(2, 2, signed=False, value=3.75)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 3.5)

            # =======================================================================
            # Max/min values for native platform - check no overflow due to rounding
            # with saturation overflow mode.

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)


            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Tie break explicit testing

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 5.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-5.25)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -5.0)

            fpNum1 = self.fp_binary_class(4, 4, signed=False, value=5.125)
            res = fpNum1.resize((4, 2), round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 5.0)

        def testRoundingNearPositiveInfinity(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.125)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 7), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -0.0234375)

            fpNum1 = self.fp_binary_class(7, -3, signed=True, value=-48.0)
            res = fpNum1.resize((7, -4), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -48.0)

            # =======================================================================
            # Change expected after rounding
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.25)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -0.015625)

            fpNum1 = self.fp_binary_class(7, -2, signed=True, value=52.0)
            res = fpNum1.resize((7, -3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 56.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -1.0)

            # =======================================================================
            # Max and min values frac resized

            # -- The rounding will cause an overflow...
            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=3.75)
            res = fpNum1.resize((3, 1),
                                round_mode=RoundingEnum.near_pos_inf,
                                overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(res, -4.0)

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=-0.25)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 0.0)

            # -- The rounding will cause an overflow...
            fpNum1 = self.fp_binary_class(2, 2, signed=False, value=3.75)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 0.0)

            # =======================================================================
            # Max/min values for native platform - check no overflow due to rounding
            # with saturation overflow mode.

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Tie break explicit testing

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-5.25)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -5.0)

            fpNum1 = self.fp_binary_class(4, 4, signed=False, value=5.125)
            res = fpNum1.resize((4, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 5.25)

        def testRoundingNearEven(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.125)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 7), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -0.0234375)

            fpNum1 = self.fp_binary_class(7, -3, signed=True, value=-48.0)
            res = fpNum1.resize((7, -4), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -48.0)

            # =======================================================================
            # Change expected after rounding
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.0)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -0.03125)

            fpNum1 = self.fp_binary_class(7, -2, signed=True, value=52.0)
            res = fpNum1.resize((7, -3), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 48.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -1.0)

            # =======================================================================
            # Max and min values frac resized

            # -- The rounding will cause an overflow...
            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=3.75)
            res = fpNum1.resize((3, 1),
                                round_mode=RoundingEnum.near_even,
                                overflow_mode=OverflowEnum.wrap)
            self.assertEqualWithFloatCast(res, -4.0)

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=-0.25)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 0.0)

            # -- The rounding will cause an overflow...
            fpNum1 = self.fp_binary_class(2, 2, signed=False, value=3.75)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 0.0)

            # =======================================================================
            # Max/min values for native platform - check no overflow due to rounding
            # with saturation overflow mode.

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Tie break explicit testing

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=6.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-6.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.75)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 6.0)

            fpNum1 = self.fp_binary_class(4, 4, signed=True, value=5.25)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 5.0)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True, value=0.25)
            res = fpNum1.resize((0, 1), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 0.0)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True, value=0.375)
            res = fpNum1.resize((0, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -0.5)

            fpNum1 = self.fp_binary_class(4, 2, signed=False, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 6.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=False, value=6.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 6.0)

        def testRoundingNearZero(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.125)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 7), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -0.0234375)

            fpNum1 = self.fp_binary_class(7, -3, signed=True, value=-48.0)
            res = fpNum1.resize((7, -4), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -48.0)

            # =======================================================================
            # Change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.0)

            fpNum1 = self.fp_binary_class(-4, 8, signed=True, value=-0.0234375)
            res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -0.015625)

            fpNum1 = self.fp_binary_class(7, -2, signed=True, value=52.0)
            res = fpNum1.resize((7, -3), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 48.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -1.0)

            # =======================================================================
            # Max and min values frac resized

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=3.75)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 3.5)

            fpNum1 = self.fp_binary_class(3, 2, signed=True, value=-0.25)
            res = fpNum1.resize((3, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 0.0)

            fpNum1 = self.fp_binary_class(2, 2, signed=False, value=3.75)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 3.5)

            # =======================================================================
            # Max/min values for native platform - check no overflow due to rounding
            # with saturation overflow mode.

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size() - 1,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((0, test_utils.get_small_type_size() - 1),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)

            # After resizing, value should be max value for one less bits
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Tie break explicit testing

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=5.5)
            res = fpNum1.resize((4, 0), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 5.0)

            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=-5.25)
            res = fpNum1.resize((4, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -5.0)

            fpNum1 = self.fp_binary_class(4, 4, signed=False, value=5.125)
            res = fpNum1.resize((4, 2), round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 5.0)

        def testRoundingAndWrappingTupleResize(self):
            """ Tests resize when both int and frac bits change. """


            # =======================================================================
            # Rounding/Wrapping
            # Positive value
            # b0101.101 = 5.625
            #
            # Round up: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round down: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.5)
            # Round down via direct zero: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.5)
            # Round down via near zero: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.5)
            # Round down via near even: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.5)

            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.75)

            # =======================================================================
            # Rounding/Wrapping
            # Negative value start, positive end
            # b1101.101 = -2.375
            #
            # Round up: b1101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round down: b1101.101 = b01.10
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.5)
            # Round up: b1101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round up: b1101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round down to even: b1101.101 -> b01.10
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.5)

            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101.101 = b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping
            # Negative value start, negative/0.0 end
            # b1011.111 = -4.125
            #
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 0.0)
            # Round down: b1011.111 -> b11.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -0.25)
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_zero)
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 0.0)
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 0.0)

            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping, larger number
            # Negative value start, positive end
            # b1101101.101 = -18.375
            #
            # Round up: b1101101.101 -> b01.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round down: b1101101.101 = b01.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, 1.5)
            # Round up: b1101101.101 -> b01.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round up: b1101101.101 -> b01.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, 1.75)
            # Round up: b1101101.101 -> b01.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, 1.5)

            # Saturate: b1101101.101 -> b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping, larger number
            # Negative value start, negative end
            # b1101101.101 = -18.375
            #
            # Round up: b1101101.101 -> b101.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -2.25)
            # Round down: b1101101.101 = b101.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -2.5)
            # Round up: b1101101.101 -> b101.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -2.25)
            # Round up: b1101101.101 -> b101.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -2.25)
            # Round down: b1101101.101 -> b101.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -2.5)

            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, -4.0)
            # Saturate: b1101101.101 = b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqualWithFloatCast(res, -4.0)
            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_zero)
            self.assertEqualWithFloatCast(res, -4.0)
            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqualWithFloatCast(res, -4.0)
            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_even)
            self.assertEqualWithFloatCast(res, -4.0)

            # =======================================================================
            # Bit width max for native platform - removing int bits and adding frac
            # bits. This is an odd corner case that could produce odd results on
            # the _FpBinarySmall implementation.

            # Positive
            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 8,
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 6,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 6),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # A reduction in int bits should result in saturation to the largest possible
            # value for the new format.
            self.assertEqual(res, fpCheck)

            # Negative
            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 8,
                                          signed=True,
                                          bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 6,
                                           signed=True,
                                           bit_field=test_utils.get_min_signed_value_bit_field_for_arch())

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 6),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # A reduction in int bits should result in saturation to the smallest possible
            # value for the new format.
            self.assertEqual(res, fpCheck)

            # Unsigned
            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 8,
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 6,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch())

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 6),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # A reduction in int bits should result in saturation to the largest possible
            # value for the new format.
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Bit width max for native platform MINUS 1 - removing int bits and adding
            # frac bits.

            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 1 - 8,
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 1 - 6,
                                           signed=True,
                                           bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 1 - 6),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # A reduction in int bits should result in saturation to the largest possible
            # value for the new format.
            self.assertEqual(res, fpCheck)

            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 1 - 8,
                                          signed=False,
                                          bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 1 - 6,
                                           signed=False,
                                           bit_field=test_utils.get_max_unsigned_value_bit_field_for_arch() >> long(1))

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 1 - 6),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            # A reduction in int bits should result in saturation to the largest possible
            # value for the new format.
            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Bit width max for native platform - removing int bits and adding frac
            # bits, but no saturation required.

            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 8,
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch() >> long(2))

            fpCheck = self.fp_binary_class(6, test_utils.get_small_type_size() - 6,
                                           signed=True,
                                           bit_field=(test_utils.get_max_signed_value_bit_field_for_arch() >> long(
                                               2)) << long(2))

            res = fpNum1.resize((6, test_utils.get_small_type_size() - 6),
                                overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)

            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Bit width max for native platform - removing int bits and adding frac
            # bits, but no saturation required, large reduction/increase in int/frac
            # bits.

            # Signed
            fpNum1 = self.fp_binary_class(test_utils.get_small_type_size() - 2, 2,
                                          signed=True, value=0.25)

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                          signed=True, value=0.25)

            res = fpNum1.resize((0, test_utils.get_small_type_size()),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            self.assertEqual(res, fpCheck)

            # Unsigned
            fpNum1 = self.fp_binary_class(test_utils.get_small_type_size() - 1, 1,
                                          signed=False, value=0.5)

            fpCheck = self.fp_binary_class(0, test_utils.get_small_type_size(),
                                           signed=False, value=0.5)

            res = fpNum1.resize((0, test_utils.get_small_type_size()),
                                overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)

            self.assertEqual(res, fpCheck)

            # =======================================================================
            # Bit width max for native platform - removing int bits and adding frac
            # bits, exception raising required.

            fpNum1 = self.fp_binary_class(8, test_utils.get_small_type_size() - 8,
                                          signed=True,
                                          bit_field=test_utils.get_max_signed_value_bit_field_for_arch())

            try:
                fpNum1.resize((6, test_utils.get_small_type_size() - 6),
                              overflow_mode=OverflowEnum.excep,
                              round_mode=RoundingEnum.direct_neg_inf)
                self.fail()
            except FpBinaryOverflowException:
                pass

            # =======================================================================
            # Rounding/Wrapping
            # Increase int bits at same time reducing frac bits - check overflow
            # doesn't occur
            # Signed
            # b0111.10 = 7.5
            #
            # Round up: b0111. -> b1000 but with increase int bits -> b01000

            # OverflowEnum.wrap
            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=7.5)
            res = fpNum1.resize((5, 0), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 8.0)

            # OverflowEnum.sat
            fpNum1 = self.fp_binary_class(4, 2, signed=True, value=7.5)
            res = fpNum1.resize((5, 0), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 8.0)

            # =======================================================================
            # Rounding/Wrapping
            # Increase int bits at same time reducing frac bits - check overflow
            # doesn't occur
            # Unsigned
            # b111.10 = 7.5
            #
            # Round up: b111. -> b000. but with increase int bits -> b1000.

            # OverflowEnum.wrap
            fpNum1 = self.fp_binary_class(3, 2, signed=False, value=7.5)
            res = fpNum1.resize((4, 0), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 8.0)

            # OverflowEnum.sat
            fpNum1 = self.fp_binary_class(3, 2, signed=False, value=7.5)
            res = fpNum1.resize((4, 0), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqualWithFloatCast(res, 8.0)



        def testCompare(self):
            """ Verifies equals, greater than etc."""

            # Operand 1 starts from the minimum value, operand 2 starts from the max and we
            # increment/decrement and do comparison operations across the range allowed by
            # the operand with the "largest smallest" representable value.


            # NOTE: the (int_bits, frac_bits) must have some overlap between the two operands
            # because we use "lowest" value frac_bit param as the position of the smallest increment.
            # That is, BOTH operands must be able to represent this value.

            tests = \
                [
                    # Standard, no dramas
                    {'signed': True, 'op1_int_bits': 3, 'op1_frac_bits': 3, 'op2_int_bits': 3, 'op2_frac_bits': 3},
                    {'signed': False, 'op1_int_bits': 3, 'op1_frac_bits': 3, 'op2_int_bits': 3, 'op2_frac_bits': 3},

                    # Negative int bits inside positive int bit block
                    {'signed': True, 'op1_int_bits': -2, 'op1_frac_bits': 6, 'op2_int_bits': 1, 'op2_frac_bits': 7},
                    {'signed': False, 'op1_int_bits': 1, 'op1_frac_bits': 7, 'op2_int_bits': -2, 'op2_frac_bits': 6},

                    # Negative int bits overlaps positive int bit block
                    {'signed': True, 'op1_int_bits': -4, 'op1_frac_bits': 12, 'op2_int_bits': 1, 'op2_frac_bits': 7},
                    {'signed': False, 'op1_int_bits': 1, 'op1_frac_bits': 7, 'op2_int_bits': -4, 'op2_frac_bits': 12},

                    # Negative frac bits inside positive frac bit block
                    {'signed': True, 'op1_int_bits': 10, 'op1_frac_bits': 4, 'op2_int_bits': 6, 'op2_frac_bits': -2},
                    {'signed': False, 'op1_int_bits': 6, 'op1_frac_bits': -2, 'op2_int_bits': 10, 'op2_frac_bits': 4},

                    # Negative frac bits overlaps positive frac bit block
                    {'signed': True, 'op1_int_bits': 5, 'op1_frac_bits': 3, 'op2_int_bits': 9, 'op2_frac_bits': -3},
                    {'signed': False, 'op1_int_bits': 9, 'op1_frac_bits': -3, 'op2_int_bits': 5, 'op2_frac_bits': 3},
                ]

            for test_case in tests:
                least_frac_bits = min(test_case['op1_frac_bits'], test_case['op2_frac_bits'])
                min_int_bits = min(test_case['op1_int_bits'], test_case['op2_int_bits'])

                min_val = -2.0 ** (min_int_bits - 1) if test_case['signed'] else 0.0
                max_val = 2.0 ** (min_int_bits - 1) if test_case['signed'] else 2.0 ** min_int_bits
                inc = 2.0 ** -least_frac_bits

                op1_accum_float = min_val
                op2_accum_float = max_val - inc

                # Increment op1 and decrement op2 and compare

                while op1_accum_float < max_val and op2_accum_float > min_val:
                    op1_accum_fp = self.fp_binary_class(test_case['op1_int_bits'], test_case['op1_frac_bits'],
                                                        signed=test_case['signed'], value=op1_accum_float)
                    op2_accum_fp = self.fp_binary_class(test_case['op2_int_bits'], test_case['op2_frac_bits'],
                                                        signed=test_case['signed'], value=op2_accum_float)

                    self.assertEqual(op1_accum_fp == op2_accum_fp, op1_accum_float == op2_accum_float)
                    self.assertEqual(op1_accum_fp != op2_accum_fp, op1_accum_float != op2_accum_float)
                    self.assertEqual(op1_accum_fp < op2_accum_fp, op1_accum_float < op2_accum_float)
                    self.assertEqual(op1_accum_fp <= op2_accum_fp, op1_accum_float <= op2_accum_float)
                    self.assertEqual(op1_accum_fp > op2_accum_fp, op1_accum_float > op2_accum_float)
                    self.assertEqual(op1_accum_fp >= op2_accum_fp, op1_accum_float >= op2_accum_float)

                    op1_accum_float += inc
                    op2_accum_float -= inc

                # Make sure we definitely get some equals to check
                op1_accum_fp = self.fp_binary_class(test_case['op1_int_bits'], test_case['op1_frac_bits'],
                                                    signed=test_case['signed'], value=inc)
                op2_accum_fp = self.fp_binary_class(test_case['op2_int_bits'], test_case['op2_frac_bits'],
                                                    signed=test_case['signed'], value=inc)
                self.assertEqual(op1_accum_fp, op2_accum_fp)

                if test_case['signed']:
                    op1_accum_fp = self.fp_binary_class(test_case['op1_int_bits'], test_case['op1_frac_bits'],
                                                        signed=test_case['signed'], value=-inc)
                    op2_accum_fp = self.fp_binary_class(test_case['op2_int_bits'], test_case['op2_frac_bits'],
                                                        signed=test_case['signed'], value=-inc)
                    self.assertEqual(op1_accum_fp, op2_accum_fp)

            # Manual corner cases

            # Max small type length without overlap
            op1_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0, signed=True, value=1.0)
            op2_fp = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=True, value=0.25)
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)

            op1_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0, signed=False, value=1.0)
            op2_fp = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=False, value=0.5)
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)

            # Max small type length without overlap - negative int bits
            op1_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0, signed=True, value=1.0)
            # Really small number
            op2_fp = self.fp_binary_class(-test_utils.get_small_type_size(), 2*test_utils.get_small_type_size(),
                                          signed=True, bit_field=long(2))
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)

            op1_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0, signed=False, value=1.0)
            # Really small number
            op2_fp = self.fp_binary_class(-test_utils.get_small_type_size(), 2 * test_utils.get_small_type_size(),
                                          signed=False, bit_field=long(2))
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)


            # Max small type length without overlap - negative frac bits

            # Really big number
            op1_fp = self.fp_binary_class(test_utils.get_small_type_size() * 2,
                                          -test_utils.get_small_type_size(), signed=True, bit_field=long(12))
            op2_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0,
                                          signed=True, value=3.0)
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)

            # Really big number
            op1_fp = self.fp_binary_class(test_utils.get_small_type_size() * 2,
                                          -test_utils.get_small_type_size(), signed=False, bit_field=long(12))
            op2_fp = self.fp_binary_class(test_utils.get_small_type_size(), 0,
                                          signed=False, value=3.0)
            self.assertLess(op2_fp, op1_fp)
            self.assertLessEqual(op2_fp, op1_fp)
            self.assertNotEqual(op1_fp, op2_fp)
            self.assertGreater(op1_fp, op2_fp)
            self.assertGreaterEqual(op1_fp, op2_fp)
            self.assertFalse(op1_fp == op2_fp)


            # Check massively different formats are equal at zero

            op1_fp = self.fp_binary_class(-2000, 2016, signed=True, value=0.0)
            op2_fp = self.fp_binary_class(5000, -4990, signed=True, value=0.0)
            self.assertFalse(op2_fp < op1_fp)
            self.assertTrue(op2_fp <= op1_fp)
            self.assertFalse(op1_fp != op2_fp)
            self.assertFalse(op1_fp > op2_fp)
            self.assertTrue(op1_fp >= op2_fp)
            self.assertEqual(op1_fp, op2_fp)


        def testIntConversion(self):
            # =======================================================================
            # Signed to signed
            # b1001.10111 = -6.28125 - goes to -201 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-6.28125)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -201)

            # In conjunction with left shifting
            # b1000.10111 = -7.28125 - goes to b1111.00010 after >> 3
            # b1111.00010 = -30 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqualWithFloatCast((fpNum >> long(3)).bits_to_signed(), -30)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqualWithFloatCast((fpNum << long(2)).bits_to_signed(), 92)

            # Negative int_bits
            # b1.1111011 = -0.0390625 - goes to -5 as integer
            #       ^ - start of bits
            fpNum = self.fp_binary_class(-3, 7, signed=True, value=-0.0390625)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -5)

            # Negative frac_bits
            # b11011000 = -40.0 - goes to -5 as integer
            #      ^ - start of bits
            fpNum = self.fp_binary_class(8, -3, signed=True, value=-40.0)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -5)

            # =======================================================================
            # Signed to unsigned
            # =======================================================================
            # Signed to signed
            # b1001.10111 = -6.28125 - goes to 311 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-6.28125)
            self.assertEqualWithFloatCast(fpNum[:], 311)

            # In conjunction with left shifting
            # b1000.10111 = -7.28125 - goes to b1111.00010 after >> 3
            # b1111.00010 = 482 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqualWithFloatCast((fpNum >> long(3))[:], 482)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqualWithFloatCast((fpNum << long(2))[:], 92)

            # Negative int_bits
            # b1.1111011 = -0.0390625 - goes to 11 as integer
            #       ^ - start of bits
            fpNum = self.fp_binary_class(-3, 7, signed=True, value=-0.0390625)
            self.assertEqualWithFloatCast(fpNum[:], 11)

            # Negative frac_bits
            # b11011000 = -40.0 - goes to 54 as integer
            #       ^ - start of bits
            fpNum = self.fp_binary_class(8, -2, signed=True, value=-40.0)
            self.assertEqualWithFloatCast(fpNum[:], 54)

            # =======================================================================
            # Unsigned to signed
            # =======================================================================
            # Unsigned to signed
            # b1001.101 = 9.625 - goes to -51 as signed integer
            fpNum = self.fp_binary_class(4, 3, signed=False, value=9.625)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -51)
            # b0101.101 = 5.625 - goes to 45 as signed integer
            fpNum = self.fp_binary_class(4, 3, signed=False, value=5.625)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), 45)

            # Negative int_bits
            # b0.00001001011 = 0.03662109375 - goes to -53 as integer
            #        ^ - start of bits
            fpNum = self.fp_binary_class(-4, 11, signed=False, value=0.03662109375)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -53)

            # Negative frac_bits
            # b1001011000 = 600.0 - goes to -424 as integer
            #        ^ - start of bits
            fpNum = self.fp_binary_class(10, -3, signed=False, value=600.0)
            self.assertEqualWithFloatCast(fpNum.bits_to_signed(), -53)

        def testSequenceOps(self):
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.5)
            self.assertEqualWithFloatCast(fpNum[0], False)
            self.assertEqualWithFloatCast(fpNum[1], True)
            self.assertEqualWithFloatCast(fpNum[5], True)
            self.assertEqualWithFloatCast(fpNum[6], False)

            self.assertEqualWithFloatCast(long(fpNum[:]), 42)
            self.assertEqualWithFloatCast(long(fpNum[4:3]), 1)

            # Negative number
            fpNum = self.fp_binary_class(5, 2, signed=True, value=-8.75)
            self.assertEqualWithFloatCast(long(fpNum[6:0]), 93)

            # Negative int_bits
            fpNum = self.fp_binary_class(-3, 7, signed=True, value=-0.046875)
            self.assertEqualWithFloatCast(fpNum[0], False)
            self.assertEqualWithFloatCast(fpNum[1], True)
            self.assertEqualWithFloatCast(fpNum[2], False)
            self.assertEqualWithFloatCast(fpNum[3], True)

            self.assertEqualWithFloatCast(long(fpNum[:]), 10)
            self.assertEqualWithFloatCast(long(fpNum[2:1]), 1)

            # Negative frac_bits
            fpNum = self.fp_binary_class(1035, -1030, signed=True, bit_field=long(21))
            self.assertEqualWithFloatCast(fpNum[0], True)
            self.assertEqualWithFloatCast(fpNum[1], False)
            self.assertEqualWithFloatCast(fpNum[2], True)
            self.assertEqualWithFloatCast(fpNum[3], False)
            self.assertEqualWithFloatCast(fpNum[4], True)

            self.assertEqualWithFloatCast(long(fpNum[:]), 21)
            self.assertEqualWithFloatCast(long(fpNum[4:2]), 5)

            # Index error check
            try:
                fpNum[len(fpNum) + 1:0]
                self.fail()
            except:
                pass

            try:
                fpNum[0:-1]
                self.fail()
            except:
                pass

        def testStr(self):
            tests = \
                [
                    {'signed': True, 'int_bits': 4, 'frac_bits': 4},
                    {'signed': True, 'int_bits': -3, 'frac_bits': 7},
                    {'signed': False, 'int_bits': 4, 'frac_bits': 4},
                    {'signed': False, 'int_bits': -3, 'frac_bits': 7},

                    # Negative int_bits
                    {'signed': True, 'int_bits': -3, 'frac_bits': 8},
                    {'signed': False, 'int_bits': -3, 'frac_bits': 8},
                    {'signed': True, 'int_bits': -7, 'frac_bits': 12},
                    {'signed': False, 'int_bits': -7, 'frac_bits': 12},

                    # Negative frac bits
                    {'signed': True, 'int_bits': 8, 'frac_bits': -3},
                    {'signed': False, 'int_bits': 12, 'frac_bits': -6},
                    {'signed': True, 'int_bits': 8, 'frac_bits': -3},
                    {'signed': False, 'int_bits': 12, 'frac_bits': -6},
                ]

            for test_case in tests:
                min_val = -2.0 ** (test_case['int_bits'] - 1) if test_case['signed'] else 0.0
                max_val = 2.0 ** (test_case['int_bits'] - 1) if test_case['signed'] else 2.0 ** test_case['int_bits']
                inc = 2.0 ** -test_case['frac_bits']

                cur_val = min_val
                while cur_val < max_val:
                    fpNum = self.fp_binary_class(test_case['int_bits'], test_case['frac_bits'],
                                                 signed=test_case['signed'], value=cur_val)
                    self.assertTrue(str(fpNum) == str(cur_val), 'Test case: {}, cur_val: {}'.format(
                        test_case, cur_val
                    ))
                    cur_val += inc

        def testStrEx(self):
            tests = \
                [
                    {'signed': True, 'int_bits': 1, 'frac_bits': 0},
                    {'signed': False, 'int_bits': 1, 'frac_bits': 0},
                    {'signed': True, 'int_bits': 2, 'frac_bits': 0},
                    {'signed': False, 'int_bits': 2, 'frac_bits': 0},
                    {'signed': True, 'int_bits': 8, 'frac_bits': 8},
                    {'signed': False, 'int_bits': 8, 'frac_bits': 8},

                    # Negative int_bits
                    {'signed': True, 'int_bits': -3, 'frac_bits': 8},
                    {'signed': False, 'int_bits': -3, 'frac_bits': 8},
                    {'signed': True, 'int_bits': -7, 'frac_bits': 12},
                    {'signed': False, 'int_bits': -7, 'frac_bits': 12},

                    # Negative frac bits
                    {'signed': True, 'int_bits': 8, 'frac_bits': -3},
                    {'signed': False, 'int_bits': 12, 'frac_bits': -6},
                    {'signed': True, 'int_bits': 8, 'frac_bits': -3},
                    {'signed': False, 'int_bits': 12, 'frac_bits': -6},
                ]

            for test_case in tests:
                min_val = -2.0 ** (test_case['int_bits'] - 1) if test_case['signed'] else 0.0
                max_val = 2.0 ** (test_case['int_bits'] - 1) if test_case['signed'] else 2.0 ** test_case['int_bits']
                inc = 2.0 ** -test_case['frac_bits']

                cur_val = min_val
                while cur_val < max_val:
                    fpNum = self.fp_binary_class(test_case['int_bits'], test_case['frac_bits'],
                                                 signed=test_case['signed'], value=cur_val)
                    self.assertEqual(fpNum.str_ex(), str(cur_val))
                    cur_val += inc

            # Short numbers but with large negative int_bits

            # Via https://www.wolframalpha.com/input/?i=2.0**-131+%2B+2.0**-133
            # b010100 starting at bit 130
            fpNum = self.fp_binary_class(-129, 135, signed=True, bit_field=long(20))
            self.assertEqual(fpNum.str_ex(),
                            '0.0000000000000000000000000000000000000004591774807899560578002877098524397178979162331140966880893561352650067419745028018951416015625')

            # Via https://www.wolframalpha.com/input/?i=-2.0**-130+%2B+2.0**-132+%2B+2.0**-133
            # b101100 starting at bit 130
            fpNum = self.fp_binary_class(-129, 135, signed=True, bit_field=long(44))
            self.assertEqual(fpNum.str_ex(),
                             '-0.0000000000000000000000000000000000000004591774807899560578002877098524397178979162331140966880893561352650067419745028018951416015625')

            # Short numbers but with large negative frac_bits

            # Via https://www.wolframalpha.com/input/?i=2.0**138+%2B+2.0**137
            # b0011 starting at bit 140
            fpNum = self.fp_binary_class(141, -137, signed=True, bit_field=long(3))
            self.assertEqual(fpNum.str_ex(),
                             '522673715590561479879743397015195972796416.0')

            # Via https://www.wolframalpha.com/input/?i=-2.0**140+%2B+2.0**138+%2B+2.0**137
            # b1011 starting at bit 140
            fpNum = self.fp_binary_class(141, -137, signed=True, bit_field=long(11))
            self.assertEqual(fpNum.str_ex(),
                             '-871122859317602466466238995025326621327360.0')

        def testIndex(self):
            """When converting to binary sting, the assumption is that only the bits
               defined by the format are relevant, leading zeros are not shown and
               the bits are always considered "unsigned."""

            # b00101.0101
            fpNum = self.fp_binary_class(5, 4, signed=True, value=5.3125)
            self.assertTrue(bin(fpNum) == '0b1010101')

            # b11001.1010
            fpNum = self.fp_binary_class(5, 4, signed=True, value=-6.375)
            self.assertTrue(bin(fpNum) == '0b110011010')

            # b00101.0101
            fpNum = self.fp_binary_class(5, 4, signed=False, value=5.3125)
            self.assertTrue(bin(fpNum) == '0b1010101')

            # b11001.1010
            fpNum = self.fp_binary_class(5, 4, signed=False, value=25.625)
            self.assertTrue(bin(fpNum) == '0b110011010')

            # Negative int_bits
            # b1.1111011 = -0.0390625
            #        ^ - start of bits
            fpNum = self.fp_binary_class(-3, 7, signed=True, value=-0.0390625)
            self.assertTrue(bin(fpNum) == '0b1011')

            # Negative frac_bits
            # 1111000.0 = -8.0
            #     ^ - start of bits
            fpNum = self.fp_binary_class(7, -2, signed=True, value=-8.0)
            self.assertTrue(bin(fpNum) == '0b11110')

        def test_numpy_create(self):
            # 1D list init
            fp_list = [self.fp_binary_class(16, 16, signed=True, value=x / 100.0) for x in range(0, 10)]
            fp_ar = np.array(fp_list, dtype=object)

            for i, j in zip(fp_list, fp_ar):
                self.assertTrue(i == j)

            # 2D list init
            fp_list = [
                [self.fp_binary_class(16, 16, signed=True, value=x / 100.0) for x in range(-10, 10)],
                [self.fp_binary_class(16, 16, signed=False, value=x / 100.0) for x in range(0, 20)]
            ]

            fp_ar = np.array(fp_list, dtype=object)

            for row in range(0, len(fp_list)):
                for i, j in zip(fp_list[row], fp_ar[row]):
                    self.assertTrue(i == j)

            # 1D assign
            fp_list = [self.fp_binary_class(16, 16, signed=True, value=x / 100.0) for x in range(0, 10)]
            fp_ar = np.zeros((len(fp_list), ), dtype=object)
            fp_ar[:] = fp_list

            for i, j in zip(fp_list, fp_ar):
                self.assertTrue(i == j)

        def test_numpy_basic_math(self):
            base_fp_list = [self.fp_binary_class(8, 8, signed=True, value=1.0) for _ in range(-5, 4)]
            operand_list = [self.fp_binary_class(8, 8, signed=True, value=x * 0.125) for x in range(1, 10)]
            expected_add = [op1 + op2 for op1, op2 in zip(base_fp_list, operand_list)]
            expected_sub = [op1 - op2 for op1, op2 in zip(base_fp_list, operand_list)]
            expected_mult = [op1 * op2 for op1, op2 in zip(base_fp_list, operand_list)]
            expected_div = [op1 / op2 for op1, op2 in zip(base_fp_list, operand_list)]
            expected_abs = [abs(op1) for op1 in operand_list]
            min_max_list = [self.fp_binary_class(8, 8, signed=True, value=1.376),
                            self.fp_binary_class(8, 8, signed=True, value=-10.25)]
            expected_min = min_max_list[1]
            expected_max = min_max_list[0]

            np_base_ar = np.array([copy.copy(x) for x in base_fp_list], dtype=object)
            np_operand_ar = np.array([copy.copy(x) for x in operand_list], dtype=object)
            np_min_max_ar = np.array([copy.copy(x) for x in min_max_list], dtype=object)

            np_add = np_base_ar + np_operand_ar
            np_sub = np_base_ar - np_operand_ar
            np_mult = np_base_ar * np_operand_ar
            np_div = np_base_ar / np_operand_ar
            np_abs = abs(np_operand_ar)

            for i in range(0, len(expected_add)):
                self.assertEqual(expected_add[i], np_add[i])
                self.assertEqual(expected_add[i].format, np_add[i].format)

                self.assertEqual(expected_sub[i], np_sub[i])
                self.assertEqual(expected_sub[i].format, np_sub[i].format)

                self.assertEqual(expected_mult[i], np_mult[i])
                self.assertEqual(expected_mult[i].format, np_mult[i].format)

                self.assertEqual(expected_div[i], np_div[i])
                self.assertEqual(expected_div[i].format, np_div[i].format)

                self.assertEqual(expected_abs[i], np_abs[i])
                self.assertEqual(expected_abs[i].format, np_abs[i].format)

            self.assertEqual(expected_min, np.min(np_min_max_ar))
            self.assertEqual(expected_max, np.max(np_min_max_ar))


class FpBinarySmallTests(AbstractTestHider.BaseClassesTestAbstract):
    def setUp(self):
        self.fp_binary_class = _FpBinarySmall
        super(FpBinarySmallTests, self).setUp()


class FpBinaryLargeTests(AbstractTestHider.BaseClassesTestAbstract):
    def setUp(self):
        self.fp_binary_class = _FpBinaryLarge
        super(FpBinaryLargeTests, self).setUp()


class FpBinaryTests(AbstractTestHider.BaseClassesTestAbstract):
    def setUp(self):
        self.fp_binary_class = FpBinary
        super(FpBinaryTests, self).setUp()


if __name__ == "__main__":
    unittest.main()
