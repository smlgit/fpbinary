#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SM), some tests adapted from RW Penney's Simple Fixed Point module

import sys, unittest, copy
from fpbinary import FpBinary, _FpBinarySmall, _FpBinaryLarge, OverflowEnum, RoundingEnum, FpBinaryOverflowException


if sys.version_info[0] >= 3:
    from porting_v3_funcs import *


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
        def assertEqual(self, first, second, msg=None):
            super(AbstractTestHider.BaseClassesTestAbstract, self).assertEqual(float(first), float(second), msg)

        def assertAlmostEqual(self, first, second, places=7):
            """Overload TestCase.assertAlmostEqual() to avoid use of round()"""
            tol = 10.0 ** -places
            self.assertTrue(float(abs(first - second)) < tol,
                            '{} and {} differ by more than {} ({})'.format(
                                first, second, tol, (first - second)))

        def testCreateParams(self):
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
                ((4,5,6), {'overflow_mode': OverflowEnum.sat, 'round_mode': RoundingEnum.near_pos_inf}),
                ]

            for test_case in params_test_cases:
                fpNum = self.fp_binary_class(5,5, value=0.0)
                try:
                    fpNum.resize(*test_case[0], **test_case[1])
                except TypeError:
                    pass
                else:
                    self.fail('Failed on test case {}'.format(test_case))

        def testFormatProperty(self):
            fpNum = self.fp_binary_class(2, 5, value=1.5, signed=True)
            self.assertTrue(fpNum.format == (2, 5))

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
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x -= self.fp_one
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x *= self.fp_two
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x /= self.fp_two
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

        def testIntCasts(self):
            """Rounding on casting to int should match float-conversions"""
            for i in range(-40,40):
                x = i / 8.0
                self.assertEqual(int(x), int(self.fp_binary_class(4, 16, signed=True, value=x)))


        def testNegating(self):
            for i in range(-32, 32):
                x = i * 0.819
                fx = self.fp_binary_class(10, 16, signed=True, value=x)

                self.assertEqual(0.0, (fx + (-fx)).resize(self.fp_zero.format))
                self.assertEqual(0.0, (-fx + fx).resize(self.fp_zero.format))
                self.assertEqual((self.fp_minus_one * fx).resize(fx.format), -fx)
                self.assertEqual(0.0, ((self.fp_minus_one * fx).resize(fx.format) + (-fx) +
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
                    self.assertEqual(fpa, (fpx + fpy).resize(fpa.format))
                    self.assertEqual(fpa, (fpy + fpx).resize(fpa.format))
                    self.assertEqual((x + y) * scale, float(fpx + fpy))

                    tmp = fpx
                    tmp += fpy
                    self.assertEqual(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b01.111 + b01.111 = b011.110
            fpx = self.fp_binary_class(2, 3, signed=True, value=1.875)
            addition = fpx + fpx
            self.assertEqual(addition, 3.75)
            self.assertTrue(addition.format == (3,3))

            # Check boundaries unsigned
            # b11.111 + b11.111 = b111.110
            fpx = self.fp_binary_class(2, 3, signed=False, value=3.875)
            addition = fpx + fpx
            self.assertEqual(addition, 7.75)
            self.assertTrue(addition.format == (3, 3))

        def testSubtraction(self):
            """Subtraction operators should promote & anti-commute"""
            scale = 0.0625
            for x in range(-16, 16):
                fpx = self.fp_binary_class(8, 15, signed=True, value=x * scale)
                for y in range(-32, 32):
                    fpy = self.fp_binary_class(value=y * scale, format_inst=fpx)
                    fpa = self.fp_binary_class(value=(x - y) * scale, format_inst=fpx)

                    # compute various forms of a = (x - y):

                    self.assertEqual(fpa, (fpx - fpy).resize(fpa.format))
                    self.assertEqual(-fpa, (fpy - fpx).resize(fpa.format))
                    self.assertEqual((x - y) * scale, float(fpx - fpy))

                    tmp = fpx
                    tmp -= fpy
                    self.assertEqual(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b10.000 - b01.111 = b100.001
            fpx = self.fp_binary_class(2, 3, signed=True, value=-2.0)
            fpy = self.fp_binary_class(2, 3, signed=True, value=1.875)
            sub = fpx - fpy
            self.assertEqual(sub, -3.875)
            self.assertTrue(sub.format == (3, 3))

            # Check boundaries unsigned
            # b00.000 - b11.111 = b100.001
            fpx = self.fp_binary_class(2, 3, signed=False, value=0.0)
            fpy = self.fp_binary_class(2, 3, signed=False, value=3.875)
            sub = fpx - fpy
            self.assertEqual(sub, 4.125)
            self.assertTrue(sub.format == (3, 3))


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
                    self.assertEqual(fpa, (fpx * fpy).resize(fpa.format))
                    self.assertEqual(fpa, (fpy * fpx).resize(fpa.format))
                    self.assertEqual((x * y) * scale2, float(fpx * fpy))

                    tmp = fpx
                    tmp *= fpy
                    self.assertEqual(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b01.111 * b01.111
            fpx = self.fp_binary_class(2, 3, signed=True, value=1.875)
            mult = fpx * fpx
            self.assertEqual(mult, 3.515625)
            self.assertTrue(mult.format == (4, 6))

            # Check boundaries unsigned
            # b11.111 * b11.111
            fpx = self.fp_binary_class(2, 3, signed=False, value=3.875)
            mult = fpx * fpx
            self.assertEqual(mult, 15.015625)
            self.assertTrue(mult.format == (4, 6))


        def testDivision(self):
            """Division operators should promote & inverse-commute"""
            format_obj = self.fp_binary_class(16, 16, signed=True)
            scale = 0.125
            scale2 = scale * scale
            for a in range(-32, 32):
                if a == 0: continue
                fpa = self.fp_binary_class(signed=True, value=a * scale, format_inst=format_obj)
                for y in range(-16, 16):
                    if y == 0: continue
                    fpy = self.fp_binary_class(signed=True, value=y * scale, format_inst=format_obj)
                    fpx = self.fp_binary_class(signed=True, value=(y * a) * scale2, format_inst=format_obj)

                    # compute various forms of a = (x / y):
                    self.assertAlmostEqual(fpa, (fpx / fpy).resize(fpa.format))
                    self.assertAlmostEqual((self.fp_one / fpa).resize(fpa.format), (fpy / fpx).resize(fpa.format))
                    self.assertAlmostEqual((a * scale), float(fpx / fpy))

                    tmp = fpx
                    tmp /= fpy
                    self.assertAlmostEqual(fpa, tmp.resize(fpa.format))

            # Check boundaries
            # b01.111 / b01.111 = b0001.000000
            fpx = self.fp_binary_class(2, 3, signed=True, value=1.875)
            div = fpx / fpx
            self.assertEqual(div, 1.0)
            self.assertTrue(div.format == (4, 6))

            # Check boundaries
            # b11.111 / b11.111 = b0001.000000
            fpx = self.fp_binary_class(2, 3, signed=False, value=3.875)
            div = fpx / fpx
            self.assertEqual(div, 1.0)
            self.assertTrue(div.format == (4, 6))


        def testBitShifts(self):
            """Check effects of left & right shift operators."""
            format_obj = self.fp_binary_class(32, 32, signed=True)

            self.assertEqual(self.fp_binary_class(value= 1, format_inst=format_obj) << long(2), 4)
            self.assertEqual(self.fp_binary_class(value= 3, format_inst=format_obj) << long(4), 48)
            self.assertEqual(self.fp_binary_class(value= -7, format_inst=format_obj) << long(8), -7 * 256)

            self.assertEqual(self.fp_binary_class(value= 1, format_inst=format_obj) >> long(1), 0.5)
            self.assertEqual(self.fp_binary_class(value= 12, format_inst=format_obj) >> long(2), 3)
            self.assertEqual(self.fp_binary_class(value= -71 * 1024, format_inst=format_obj) >> long(12), -17.75)

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
            int_length_list = [1, 2, 4, 6, 7, 8]
            for top in int_length_list:
                frac_bits = 16
                inc = 1.0/16.01
                format_obj = self.fp_binary_class(top, frac_bits, signed=True)

                pos_limit = 2**(top - 1)
                neg_limit = -2**(top - 1)

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
                    try:  xRoundedDown.resize(format_obj.format, overflow_mode=OverflowEnum.excep, round_mode=RoundingEnum.direct_neg_inf)
                    except FpBinaryOverflowException:
                        #print 'cnt: {0}  pos_limit: {1}  top: {2}   x: {3}'.format(cnt, pos_limit, top, float(x))
                        if cnt < pos_limit:
                            self.fail()

                    try:  xRoundedUp.resize(format_obj.format, overflow_mode=OverflowEnum.excep, round_mode=RoundingEnum.near_pos_inf)
                    except FpBinaryOverflowException:
                        pass
                    else:
                        #print 'cnt: {0}  pos_limit: {1}  top: {2}   x: {3}'.format(cnt, pos_limit, top, float(x))
                        if cnt >= pos_limit: self.fail()

                    yRoundedDown -= fpIncRoundedDown
                    yRoundedUp -= fpIncRoundedUp

                    try: yRoundedDown.resize(format_obj.format, overflow_mode=OverflowEnum.excep, round_mode=RoundingEnum.direct_neg_inf)
                    except FpBinaryOverflowException:
                        if -cnt >= neg_limit: self.fail()

                    try:  yRoundedUp.resize(format_obj.format, overflow_mode=OverflowEnum.excep, round_mode=RoundingEnum.near_pos_inf)
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
            self.assertEqual(fpNum, 3.875)

            # Losing MSB, positive to negative
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, -0.25)

            # Losing MSB, positive to positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.75)
            fpNum.resize((3, 2), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, 2.75)

            # =======================================================================
            # Saturation

            # Losing MSBs, no saturation required
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.25)
            fpNum.resize((3, 3), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, 3.25)

            # Losing MSB, positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, 7.75)

            # Losing MSB, negative
            fpNum = self.fp_binary_class(4, 2, signed=True, value=-7.75)
            fpNum.resize((3, 2), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, -4.0)

            # =======================================================================
            # Exception

            #Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.875)
            try: fpNum.resize((3, 3), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                self.fail()

            # Losing MSB, positive to negative
            fpNum = self.fp_binary_class(5, 2, signed=True, value=15.75)
            try: fpNum.resize((4, 2), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, positive to positive
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.75)
            try: fpNum.resize((3, 2), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # =======================================================================
            # Left shifting

            # Losing MSBs, no sign change expected
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.25)
            fpNum <<= long(2)
            self.assertEqual(fpNum, 13.0)

            # Losing MSB, positive to negative expected
            fpNum = self.fp_binary_class(5, 2, signed=True, value=5.5)
            fpNum <<= long(2)
            self.assertEqual(fpNum, -10.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(5, 3, signed=True, value=-14.875)
            fpNum <<= long(3)
            self.assertEqual(fpNum, 9.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(2, 2, signed=True, value=-1.5)
            fpNum <<= long(1)
            self.assertEqual(fpNum, 1.0)

        def testRoundingModes(self):
            # =======================================================================
            # No change expected after rounding

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.125)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.125)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 3), round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.125)

            # =======================================================================
            # Change expected after rounding
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.25)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.0)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=1.125)
            res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.0)

            # =======================================================================
            # Change expected after rounding, crossing frac/int boundary
            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -1.0)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -1.5)

            fpNum1 = self.fp_binary_class(2, 4, signed=True, value=-1.1875)
            res = fpNum1.resize((2, 1), round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -1.0)

        def testRoundingAndWrappingTupleResize(self):
            # =======================================================================
            # Rounding/Wrapping
            # Positive value
            # b0101.101 = 5.625
            #
            # Round up: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.75)
            # Round down: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.5)
            # Round down via near zero: 01.10 = 1.5
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.5)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.75)
            # Saturate: b0101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=5.625)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.75)

            # =======================================================================
            # Rounding/Wrapping
            # Negative value start, positive end
            # b1101.101 = -2.375
            #
            # Round up: b1101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.75)
            # Round down: b1101.101 = b01.10
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.5)
            # Round up: b1101.101 -> b01.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.75)
            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1101.101 = b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1101.101 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-2.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping
            # Negative value start, negative/0.0 end
            # b1011.111 = -4.125
            #
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 0.0)
            # Round down: b1011.111 -> b11.11
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -0.25)
            # Round up: b1011.111 -> b00.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 0.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1011.111 -> b10.00
            fpNum1 = self.fp_binary_class(4, 3, signed=True, value=-4.125)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping, larger number
            # Negative value start, positive end
            # b1101101.101 = -18.375
            #
            # Round up: b1101101.101 -> b01.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 1.75)
            # Round down: b1101101.101 = b01.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 1.5)
            # Round up: b1101101.101 -> b01.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, 1.75)
            # Saturate: b1101101.101 -> b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -2.0)
            # Saturate: b1101101.101 = b10.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((2, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -2.0)

            # =======================================================================
            # Rounding/Wrapping, larger number
            # Negative value start, negative end
            # b1101101.101 = -18.375
            #
            # Round up: b1101101.101 -> b101.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -2.25)
            # Round down: b1101101.101 = b101.10
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -2.5)
            # Round up: b1101101.101 -> b101.11
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.wrap,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -2.25)
            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, -4.0)
            # Saturate: b1101101.101 = b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, -4.0)
            # Saturate: b1101101.101 -> b100.00
            fpNum1 = self.fp_binary_class(7, 3, signed=True, value=-18.375)
            res = fpNum1.resize((3, 2), overflow_mode=OverflowEnum.sat,
                                round_mode=RoundingEnum.near_zero)
            self.assertEqual(res, -4.0)

        def testIntConversion(self):
            # =======================================================================
            # Signed to signed
            # b1001.10111 = -6.28125 - goes to -201 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-6.28125)
            self.assertEqual(fpNum.bits_to_signed(), -201)

            # In conjunction with left shifting
            # b1000.10111 = -7.28125 - goes to b1111.00010 after >> 3
            # b1111.00010 = -30 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum >> long(3)).bits_to_signed(), -30)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum << long(2)).bits_to_signed(), 92)

            # =======================================================================
            # Signed to unsigned
            # =======================================================================
            # Signed to signed
            # b1001.10111 = -6.28125 - goes to 311 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-6.28125)
            self.assertEqual(fpNum[:], 311)

            # In conjunction with left shifting
            # b1000.10111 = -7.28125 - goes to b1111.00010 after >> 3
            # b1111.00010 = 482 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum >> long(3))[:], 482)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum << long(2))[:], 92)

            # =======================================================================
            # Unsigned to signed
            # =======================================================================
            # Unsigned to signed
            # b1001.101 = 9.625 - goes to -51 as signed integer
            fpNum = self.fp_binary_class(4, 3, signed=False, value=9.625)
            self.assertEqual(fpNum.bits_to_signed(), -51)
            # b0101.101 = 5.625 - goes to 45 as signed integer
            fpNum = self.fp_binary_class(4, 3, signed=False, value=5.625)
            self.assertEqual(fpNum.bits_to_signed(), 45)

        def testSequenceOps(self):
            fpNum = self.fp_binary_class(5, 2, signed=True, value=10.5)
            self.assertEqual(fpNum[0], False)
            self.assertEqual(fpNum[1], True)
            self.assertEqual(fpNum[5], True)
            self.assertEqual(fpNum[6], False)

            self.assertEqual(int(fpNum[:]), 42)
            self.assertEqual(int(fpNum[4:3]), 1)

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

            # Negative number
            fpNum = self.fp_binary_class(5, 2, signed=True, value=-8.75)
            self.assertEqual(int(fpNum[6:0]), 93)

        def testStr(self):
            tests = \
                [
                    {'signed': True, 'int_bits': 4, 'frac_bits': 4},
                    {'signed': False, 'int_bits': 4, 'frac_bits': 4},
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
            tests =\
                [
                    {'signed': True, 'int_bits': 1, 'frac_bits': 0},
                    {'signed': False, 'int_bits': 1, 'frac_bits': 0},
                    {'signed': True, 'int_bits': 2, 'frac_bits': 0},
                    {'signed': False, 'int_bits': 2, 'frac_bits': 0},
                    {'signed': True, 'int_bits': 8, 'frac_bits': 8},
                    {'signed': False, 'int_bits': 8, 'frac_bits': 8},
                ]

            for test_case in tests:
                min_val = -2.0**(test_case['int_bits'] - 1) if test_case['signed'] else 0.0
                max_val = 2.0**(test_case['int_bits'] - 1) if test_case['signed'] else 2.0**test_case['int_bits']
                inc = 2.0**-test_case['frac_bits']

                cur_val = min_val
                while cur_val < max_val:
                    fpNum = self.fp_binary_class(test_case['int_bits'], test_case['frac_bits'],
                                                 signed=test_case['signed'], value=cur_val)
                    self.assertTrue(fpNum.str_ex() == str(cur_val), 'Test case: {}, cur_val: {}'.format(
                        test_case, cur_val
                    ))
                    cur_val += inc
                    #print(fpNum.__getitem__(slice(0,1)))


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

