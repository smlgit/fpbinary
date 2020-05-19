#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SML, some tests adapted from RW Penney's Simple Fixed Point module

import sys, unittest, copy, pickle, os
import tests.test_utils as test_utils
from fpbinary import FpBinary, OverflowEnum, RoundingEnum, FpBinaryOverflowException

if sys.version_info[0] >= 3:
    from tests.porting_v3_funcs import *


pickle_test_file_name = 'pickle_test.data'
pickle_libs = [pickle]
try:
    import cPickle
    pickle_libs.append(cPickle)
except:
    pass

def remove_pickle_file():
    if os.path.exists(pickle_test_file_name):
        os.remove(pickle_test_file_name)

class AbstractTestHider(object):
    class WrapperClassesTestAbstract(unittest.TestCase):
        def tearDown(self):
            remove_pickle_file()

        def assertAlmostEqual(self, first, second, places=7):
            """Overload TestCase.assertAlmostEqual() to avoid use of round()"""
            tol = 10.0 ** -places
            self.assertTrue(float(abs(first - second)) < tol,
                            '{} and {} differ by more than {} ({})'.format(
                                first, second, tol, (first - second)))

        def _build_long_float_str(self, int_value, frac_value):
            """
            Creates a string representation of a float by breaking up the int and frac
            parts. This allows us to effectively double the native float types for testing
            with the large fixed point objects.

            NOTE!!! If the frac value is negative, the minus sign will be removed. Beware
            when inputting negative values with this function because simply adding the
            int and frac components might not behave as you expect.
            """

            # 128 decimal places should be enough...
            return str(int(int_value)) + ('%.128f' % frac_value).lstrip('-').lstrip('0').rstrip('0')

        def testCreateParams(self):
            """ Checking error is raised on create for wrapper classes. Use for code that
                only checks at the wrapper level.
            """

            # These parameter test cases should raise an exception
            params_test_cases = [

                # total bits is less than 1
                ([-3, 3], {'signed': True}),
                ([-7, 6], {'signed': True}),
                ([-3, 3], {'signed': False}),
                ([-7, 6], {'signed': False}),
                ([3, -3], {'signed': True}),
                ([7, -8], {'signed': True}),
                ([3, -78], {'signed': False}),
                ([7, -11], {'signed': False}),
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
                x += 1
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x -= 1
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x *= 2
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

                x = copy.copy(orig)
                x0 = x
                x /= 2
                self.assertEqual(orig, x0)
                if x is x0: self.fail()

        def testIntCasts(self):
            """Rounding on casting to int should match float-conversions"""

            # Small type
            for i in range(-40, 40):
                x = i / 8.0
                self.assertEqual(int(x), int(self.fp_binary_class(4, 16, signed=True, value=x)))

            # Large type
            # Float comparison should be ok as long as the value is small enough
            for i in range(-40, 40):
                x = i / 8.0
                self.assertEqual(int(x), int(self.fp_binary_class(test_utils.get_small_type_size(), 16,
                                                                  signed=True, value=x)))

        def testNegating(self):
            # Small type
            for i in range(-32, 32):
                x = i * 0.819
                fx = self.fp_binary_class(10, 16, signed=True, value=x)

                self.assertEqual(0.0, (fx + (-fx)))
                self.assertEqual(0.0, (-fx + fx))
                self.assertEqual((-1 * fx), -fx)
                self.assertEqual(0.0, ((-1 * fx) + (-fx) + (2 * (fx))))

            # Large type
            for i in range(-32, 32):
                x = i * 0.819
                fx = self.fp_binary_class(test_utils.get_small_type_size(), 16, signed=True, value=x)

                self.assertEqual(0.0, (fx + (-fx)))
                self.assertEqual(0.0, (-fx + fx))
                self.assertEqual((-1 * fx), -fx)
                self.assertEqual(0.0, (-1 * fx) + (-fx) + (2 * (fx)))

        def testAddition(self):
            """Addition operations between different types"""

            bits_small = int(test_utils.get_small_type_size() / 4)
            bits_large = test_utils.get_small_type_size()

            scale = 0.125

            for x in range(-16, 16):
                fpx_small = self.fp_binary_class(bits_small, bits_small, signed=True, value=x * scale)
                fpx_large = self.fp_binary_class(bits_large, bits_large, signed=True, value=x * scale)

                for y in range(-32, 32):
                    fpy_small = self.fp_binary_class(value=y * scale, format_inst=fpx_small)
                    fpa_small = self.fp_binary_class(value=(x + y) * scale, format_inst=fpx_small)
                    fpy_large = self.fp_binary_class(value=y * scale, format_inst=fpx_large)
                    fpa_large = self.fp_binary_class(value=(x + y) * scale, format_inst=fpx_large)

                    # compute various forms of a = (x + y):
                    self.assertEqual(fpa_small, (fpx_small + fpy_small).resize(fpa_small.format))
                    self.assertEqual(fpa_small, (fpy_small + fpx_small).resize(fpa_small.format))
                    self.assertEqual((x + y) * scale, float(fpx_small + fpy_small))

                    self.assertEqual(fpa_large, (fpx_large + fpy_large).resize(fpa_large.format))
                    self.assertEqual(fpa_large, (fpy_large + fpx_large).resize(fpa_large.format))
                    self.assertEqual((x + y) * scale, float(fpx_large + fpy_large))

                    self.assertEqual(fpa_small, (fpx_small + fpy_large).resize(fpa_small.format))
                    self.assertEqual(fpa_small, (fpy_large + fpx_small).resize(fpa_small.format))
                    self.assertEqual(fpa_large, (fpx_small + fpy_large).resize(fpa_large.format))
                    self.assertEqual(fpa_large, (fpy_large + fpx_small).resize(fpa_large.format))

                    tmp = fpx_large
                    tmp += fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) + fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))
                    tmp = fpx_small + float(y * scale)
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) + fpy_large
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))
                    tmp = fpx_large + float(y * scale)
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    self.assertEqual(x * scale + 4, float(fpx_small + 4))
                    self.assertEqual(x * scale + 4, float(fpx_large + 4))
                    self.assertEqual(x * scale + 4, float(4 + fpx_small))
                    self.assertEqual(x * scale + 4, float(4 + fpx_large))

        def testSubtract(self):
            """Subtraction operations between different types"""

            bits_small = int(test_utils.get_small_type_size() / 4)
            bits_large = test_utils.get_small_type_size()

            scale = 0.125

            for x in range(-16, 16):
                fpx_small = self.fp_binary_class(bits_small, bits_small, signed=True, value=x * scale)
                fpx_large = self.fp_binary_class(bits_large, bits_large, signed=True, value=x * scale)

                for y in range(-32, 32):
                    fpy_small = self.fp_binary_class(value=y * scale, format_inst=fpx_small)
                    fpa_small = self.fp_binary_class(value=(x - y) * scale, format_inst=fpx_small)
                    fpy_large = self.fp_binary_class(value=y * scale, format_inst=fpx_large)
                    fpa_large = self.fp_binary_class(value=(x - y) * scale, format_inst=fpx_large)

                    # compute various forms of a = (x - y):
                    self.assertEqual(fpa_small, (fpx_small - fpy_small).resize(fpa_small.format))
                    self.assertEqual(-fpa_small, (fpy_small - fpx_small).resize(fpa_small.format))
                    self.assertEqual((x - y) * scale, float(fpx_small - fpy_small))

                    self.assertEqual(fpa_large, (fpx_large - fpy_large).resize(fpa_large.format))
                    self.assertEqual(-fpa_large, (fpy_large - fpx_large).resize(fpa_large.format))
                    self.assertEqual((x - y) * scale, float(fpx_large - fpy_large))

                    self.assertEqual(fpa_small, (fpx_small - fpy_large).resize(fpa_small.format))
                    self.assertEqual(-fpa_small, (fpy_large - fpx_small).resize(fpa_small.format))
                    self.assertEqual(fpa_large, (fpx_small - fpy_large).resize(fpa_large.format))
                    self.assertEqual(-fpa_large, (fpy_large - fpx_small).resize(fpa_large.format))

                    tmp = fpx_large
                    tmp -= fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) - fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))
                    tmp = fpx_small - float(y * scale)
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) - fpy_large
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))
                    tmp = fpx_large - float(y * scale)
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    self.assertEqual(x * scale - 4, float(fpx_small - 4))
                    self.assertEqual(x * scale - 4, float(fpx_large - 4))
                    self.assertEqual(4 - x * scale, float(4 - fpx_small))
                    self.assertEqual(4 - x * scale, float(4 - fpx_large))

        def testMultiplication(self):
            """Subtraction operations between different types"""

            bits_small = int(test_utils.get_small_type_size() / 4)
            bits_large = test_utils.get_small_type_size()

            scale = 0.25
            scale2 = scale * scale

            for x in range(-16, 32):
                fpx_small = self.fp_binary_class(bits_small, bits_small, signed=True, value=x * scale)
                fpx_large = self.fp_binary_class(bits_large, bits_large, signed=True, value=x * scale)

                for y in range(-32, 16):
                    fpy_small = self.fp_binary_class(value=y * scale, format_inst=fpx_small)
                    fpy_large = self.fp_binary_class(value=y * scale, format_inst=fpx_large)
                    fpa_small = self.fp_binary_class(value=(x * y) * scale2, format_inst=fpx_small)
                    fpa_large = self.fp_binary_class(value=(x * y) * scale2, format_inst=fpx_large)

                    # compute various forms of a = (x * y):
                    self.assertEqual(fpa_small, (fpx_small * fpy_small).resize(fpa_small.format))
                    self.assertEqual(fpa_small, (fpy_small * fpx_small).resize(fpa_small.format))
                    self.assertEqual((x * y) * scale2, float(fpx_small * fpy_small))

                    self.assertEqual(fpa_large, (fpx_large * fpy_large).resize(fpa_large.format))
                    self.assertEqual(fpa_large, (fpy_large * fpx_large).resize(fpa_large.format))
                    self.assertEqual((x * y) * scale2, float(fpx_large * fpy_large))

                    self.assertEqual(fpa_small, (fpx_small * fpy_large).resize(fpa_small.format))
                    self.assertEqual(fpa_small, (fpy_large * fpx_small).resize(fpa_small.format))
                    self.assertEqual(fpa_large, (fpx_large * fpy_small).resize(fpa_large.format))
                    self.assertEqual(fpa_large, (fpy_small * fpx_large).resize(fpa_large.format))

                    tmp = fpx_small
                    tmp *= fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = fpx_large
                    tmp *= fpy_large
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    tmp = fpx_small
                    tmp *= fpy_large
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    tmp = fpx_large
                    tmp *= fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) * fpy_small
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = fpx_small * float(y * scale)
                    self.assertEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(x * scale) * fpy_large
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    tmp = fpx_large * float(y * scale)
                    self.assertEqual(fpa_large, tmp.resize(fpa_large.format))

                    self.assertEqual(x * scale * 5, float(fpx_small * 5))
                    self.assertEqual(x * scale * 5, float(fpx_large * 5))
                    self.assertEqual(x * scale * 5, float(5 * fpx_small))
                    self.assertEqual(x * scale * 5, float(5 * fpx_large))

        def testDivision(self):
            """Subtraction operations between different types"""

            almost_equal_places_small = 4
            bits_small = int(test_utils.get_small_type_size() / 4)
            bits_large = test_utils.get_small_type_size()

            scale = 0.125
            scale2 = scale * scale
            for a in range(-32, 32):
                if a == 0: continue
                fpa_small = self.fp_binary_class(bits_small, bits_large, signed=True, value=a * scale)
                fpa_large = self.fp_binary_class(bits_large, bits_large, signed=True, value=a * scale)

                for y in range(-16, 16):
                    if y == 0: continue
                    fpy_small = self.fp_binary_class(signed=True, value=y * scale, format_inst=fpa_small)
                    fpy_large = self.fp_binary_class(signed=True, value=y * scale, format_inst=fpa_large)
                    fpx_small = self.fp_binary_class(signed=True, value=(y * a) * scale2, format_inst=fpa_small)
                    fpx_large = self.fp_binary_class(signed=True, value=(y * a) * scale2, format_inst=fpa_large)

                    # compute various forms of a = (x / y):
                    self.assertAlmostEqual(fpa_small, (fpx_small / fpy_small).resize(fpa_small.format), places=almost_equal_places_small)
                    self.assertAlmostEqual((1 / fpa_small).resize(fpa_small.format),
                                           (fpy_small / fpx_small).resize(fpa_small.format), places=almost_equal_places_small)
                    self.assertAlmostEqual((a * scale), float(fpx_small / fpy_small), places=almost_equal_places_small)

                    self.assertAlmostEqual(fpa_large, (fpx_large / fpy_large).resize(fpa_large.format))
                    self.assertAlmostEqual((1 / fpa_large).resize(fpa_large.format),
                                           (fpy_large / fpx_large).resize(fpa_large.format))
                    self.assertAlmostEqual((a * scale), float(fpx_large / fpy_large))

                    self.assertAlmostEqual(fpa_small, (fpx_small / fpy_large).resize(fpa_small.format), places=almost_equal_places_small)
                    self.assertAlmostEqual((1 / fpa_small).resize(fpa_small.format),
                                           (fpy_small / fpx_large).resize(fpa_small.format), places=almost_equal_places_small)
                    self.assertAlmostEqual((a * scale), float(fpx_small / fpy_large), places=almost_equal_places_small)

                    self.assertAlmostEqual(fpa_large, (fpx_large / fpy_small).resize(fpa_large.format))
                    self.assertAlmostEqual((1 / fpa_large).resize(fpa_large.format),
                                           (fpy_large / fpx_small).resize(fpa_large.format))
                    self.assertAlmostEqual((a * scale), float(fpx_large / fpy_small))

                    tmp = fpx_small
                    tmp /= fpy_small
                    self.assertAlmostEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = fpx_large
                    tmp /= fpy_large
                    self.assertAlmostEqual(fpa_large, tmp.resize(fpa_large.format))

                    tmp = float(a * y * scale2) / fpy_small
                    self.assertAlmostEqual(fpa_small, tmp.resize(fpa_small.format))
                    tmp = fpx_small / float(y * scale)
                    self.assertAlmostEqual(fpa_small, tmp.resize(fpa_small.format))

                    tmp = float(a * y * scale2) / fpy_large
                    self.assertAlmostEqual(fpa_large, tmp.resize(fpa_large.format))
                    tmp = fpx_large / float(y * scale)
                    self.assertAlmostEqual(fpa_large, tmp.resize(fpa_large.format))

                    self.assertEqual(y * scale / 5, float(fpy_small / 5))
                    self.assertEqual(y * scale / 5, float(fpy_large / 5))

        def testBitShifts(self):
            """Check effects of left & right shift operators."""

            # Small size
            format_fp = self.fp_binary_class(int(test_utils.get_small_type_size() / 2),
                                             int(test_utils.get_small_type_size() / 2),
                                             signed=True)

            self.assertEqual(self.fp_binary_class(value=1, format_inst=format_fp) << 2, 4)
            self.assertEqual(self.fp_binary_class(value=3, format_inst=format_fp) << 4, 48)
            self.assertEqual(self.fp_binary_class(value=-7, format_inst=format_fp) << 8, -7 * 256)

            self.assertEqual(self.fp_binary_class(value=1, format_inst=format_fp) >> 1, 0.5)
            self.assertEqual(self.fp_binary_class(value=12, format_inst=format_fp) >> 2, 3)
            self.assertEqual(self.fp_binary_class(value=-71 * 1024, format_inst=format_fp) >> 12, -17.75)

            # Large size
            format_fp = self.fp_binary_class(test_utils.get_small_type_size(), test_utils.get_small_type_size(),
                                             signed=True)

            self.assertEqual(self.fp_binary_class(value=1, format_inst=format_fp) << 2, 4)
            self.assertEqual(self.fp_binary_class(value=3, format_inst=format_fp) << 4, 48)
            self.assertEqual(self.fp_binary_class(value=-7, format_inst=format_fp) << 8, -7 * 256)

            self.assertEqual(self.fp_binary_class(value=1, format_inst=format_fp) >> 1, 0.5)
            self.assertEqual(self.fp_binary_class(value=12, format_inst=format_fp) >> 2, 3)
            self.assertEqual(self.fp_binary_class(value=-71 * 1024, format_inst=format_fp) >> 12, -17.75)


            # Negative int_bits

            # Small size
            format_fp = self.fp_binary_class(-3, int(test_utils.get_small_type_size()) + 3, signed=True)
            self.assertEqual(self.fp_binary_class(value=0.0322265625, format_inst=format_fp) << 1, -0.060546875)

            # Large size
            format_fp = self.fp_binary_class(-5, test_utils.get_small_type_size() + 10, signed=True)
            self.assertEqual(self.fp_binary_class(value=0.0068359375, format_inst=format_fp) << 2, -0.00390625)

            # Negative frac_bits

            # Small size
            format_fp = self.fp_binary_class(int(test_utils.get_small_type_size()) + 10, -10, signed=True)
            self.assertEqual(self.fp_binary_class(value=1024.0, format_inst=format_fp) << 1, 2048.0)

            # Large size
            format_fp = self.fp_binary_class(test_utils.get_small_type_size() + 10, -6, signed=True)
            self.assertEqual(self.fp_binary_class(value=192.0, format_inst=format_fp) << 2, 768.0)

        def testOverflowModeOnCreate(self):
            # Overflow mode on create is saturation. Verify values that have magnitudes that
            # are too large are saturated correctly. A pass requires no exception raised and
            # we also use the bit_field to construct an FpBinary instance to compare against.
            # This should be safe because the bit_field method has no concept of overflow.

            # =======================================================================
            # Signed

            # We set our input value to the largest possible INTEGER value for the platform.
            # Having any fractional bits means this value is too large to represent and
            # saturation must be performed (saturate to the largest/smallest possible value
            # given the int_bits, frac_bits format)

            native_max_int_val = (1 << (test_utils.get_small_type_size() - 1)) - 1
            native_min_int_val = -(native_max_int_val + 1)

            for int_bits in range(0, 2):
                for frac_bits in range(1, test_utils.get_small_type_size() + 2):
                    total_bits = int_bits + frac_bits
                    if total_bits > 1:
                        max_val_bit_field = (1 << (total_bits - 1)) - 1
                    else:
                        max_val_bit_field = 0

                    min_val_bit_field = (1 << (total_bits - 1))


                    fpNum = self.fp_binary_class(int_bits, frac_bits, signed=True, value=native_max_int_val)
                    expected = self.fp_binary_class(int_bits, frac_bits, signed=True,
                                                    bit_field=long(max_val_bit_field))
                    self.assertAlmostEqual(fpNum, expected)

                    fpNum = self.fp_binary_class(int_bits, frac_bits, signed=True, value=native_min_int_val)
                    expected = self.fp_binary_class(int_bits, frac_bits, signed=True,
                                                    bit_field=long(min_val_bit_field))
                    self.assertAlmostEqual(fpNum, expected)

            # =======================================================================
            # Unsigned

            native_max_int_val = (1 << test_utils.get_small_type_size()) - 1

            for int_bits in range(0, 2):
                for frac_bits in range(1, test_utils.get_small_type_size() + 2):
                    total_bits = int_bits + frac_bits
                    max_val_bit_field = (1 << total_bits) - 1
                    min_val_bit_field = 0

                    fpNum = self.fp_binary_class(int_bits, frac_bits, signed=False, value=native_max_int_val)
                    expected = self.fp_binary_class(int_bits, frac_bits, signed=False,
                                                    bit_field=long(max_val_bit_field))
                    self.assertAlmostEqual(fpNum, expected)

                    fpNum = self.fp_binary_class(int_bits, frac_bits, signed=False, value=-1.0)
                    expected = self.fp_binary_class(int_bits, frac_bits, signed=False,
                                                    bit_field=long(min_val_bit_field))
                    self.assertAlmostEqual(fpNum, expected)


        def testZeroIntBitsOnCreate(self):
            # Checks the corner case where there are 0 int bits and we are using the native
            # bit length number of frac bits. This is a specific test because of the use of
            # native unsigned ints for arithmetic and the need to scale by the number of
            # frac bits (which was originally implemented using shifts. E.g. value * (1 << 64), which
            # wouldn't work.

            # =======================================================================
            # Verify the specified user value is preserved when no rounding or overflow
            # is required.

            # =======================================================================
            # Signed

            value_cases = [0.3125, -0.25]

            for value in value_cases:
                # Cross the system word length boundary
                for frac_bits in range(test_utils.get_small_type_size() - 1, test_utils.get_small_type_size() + 2):
                    fpNum = self.fp_binary_class(0, frac_bits, signed=True, value=value)
                    self.assertEqual(fpNum, value)

            # =======================================================================
            # Unsigned

            value_cases = [0.3125, 0.125]

            for value in value_cases:
                # Cross the system word length boundary
                for frac_bits in range(test_utils.get_small_type_size() - 1,
                                       test_utils.get_small_type_size() + 2):
                    fpNum = self.fp_binary_class(0, frac_bits, signed=False, value=value)
                    self.assertEqual(fpNum, value)

        def testZeroIntBitsBasicMath(self):
            # Basic arithmetic to sanity check the corner case where there are 0 int bits and we
            # are using the native bit length number of frac bits.

            # =======================================================================
            # Signed

            fp_num1 = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=True, value=0.25)
            fp_num2 = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=True, value=-0.25)

            self.assertEqual(fp_num1 + fp_num2, 0.0)
            self.assertEqual(fp_num2 - fp_num1, -0.5)
            self.assertEqual(fp_num1 * fp_num2, -0.0625)

            # =======================================================================
            # Unsigned

            fp_num1 = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=False, value=0.25)
            fp_num2 = self.fp_binary_class(0, test_utils.get_small_type_size(), signed=False, value=0.125)

            self.assertEqual(fp_num1 + fp_num2, 0.375)
            self.assertEqual(fp_num1 - fp_num2, 0.125)
            self.assertEqual(fp_num1 * fp_num2, 0.03125)


        def testOverflowModesSmall(self):
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

            # =======================================================================
            # Left shifting (uses mask mode of overflow/rounding)

            # Losing MSBs, no sign change expected
            fpNum = self.fp_binary_class(6, 3, signed=True, value=3.25)
            fpNum <<= 2
            self.assertEqual(fpNum, 13.0)

            # Losing MSB, positive to negative expected
            fpNum = self.fp_binary_class(5, 2, signed=True, value=5.5)
            fpNum <<= 2
            self.assertEqual(fpNum, -10.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(5, 3, signed=True, value=-14.875)
            fpNum <<= 3
            self.assertEqual(fpNum, 9.0)

            # Losing MSB, negative to positive expected
            fpNum = self.fp_binary_class(2, 2, signed=True, value=-1.5)
            fpNum <<= 1
            self.assertEqual(fpNum, 1.0)

        def testOverflowModesSignedLarge(self):
            # Rough estimate of float accuracy
            max_total_float_bits = int(test_utils.get_small_type_size() / 2)
            # Adding 1 for the sign bit
            large_int_bits = test_utils.get_small_type_size() + 8 + 1
            frac_bits = 3
            high_order_bits_pos = 0x7A << test_utils.get_small_type_size()

            low_order_value = 13.125
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)
            value_bit_field_neg = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits) - \
                                  (1 << (large_int_bits + frac_bits - 1))
            bit_field_top_nibble_mask = (1 << (large_int_bits + frac_bits - 5)) - 1

            # =======================================================================
            # Wrapping

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         value=low_order_value)
            fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, low_order_value)

            # Losing MSB, positive to negative, low order bits
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, -2.875)

            # Losing MSB, positive to positive, low order bits
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((2, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, 1.125)

            # Losing MSB, positive to negative, high order bits
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            # Resize to cut the top nibble + sign off: 010101010---01101.001
            fpNum.resize((large_int_bits - 5, frac_bits), overflow_mode=OverflowEnum.wrap)
            # use [:] to create unsigned long to represent large fp value
            self.assertEqual(int(fpNum[:]), value_bit_field_pos & bit_field_top_nibble_mask)

            # =======================================================================
            # Saturation

            # Losing MSBs, no saturation required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         value=low_order_value)
            fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, low_order_value)

            # Losing MSB, positive to positive, low order bits
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, 7.875)

            # Losing MSB, positive to negative, low order bits
            # Start: 111---101111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            fpNum.resize((2, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, -2.0)

            # =======================================================================
            # Exception

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         value=low_order_value)
            try:
                fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                self.fail()

            # Losing MSB, wrap would be positive to negative
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            try:
                fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, wrap would be positive to positive
            # Start: 001111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            try:
                fpNum.resize((5, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, negative bit field, wrap would be negative to positive
            # Start: 111--101111010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            try:
                fpNum.resize((large_int_bits - 1, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

        def testOverflowModesUnsignedLarge(self):
            # Rough estimate of float accuracy
            max_total_float_bits = int(test_utils.get_small_type_size() / 2)
            large_int_bits = test_utils.get_small_type_size() + 8
            frac_bits = 3
            high_order_bits_pos = 0xAA << test_utils.get_small_type_size()

            low_order_value = 13.125
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)
            bit_field_top_nibble_mask = (1 << (large_int_bits + frac_bits - 4)) - 1

            # =======================================================================
            # Wrapping

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         value=low_order_value)
            fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, low_order_value)

            # Losing MSB, to top bit set to 1, low order bits
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, 13.125)

            # Losing MSB, to top bit set to 0, low order bits
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((2, frac_bits), overflow_mode=OverflowEnum.wrap)
            self.assertEqual(fpNum, 1.125)

            # Losing MSB, to top bit set to 1, high order bits
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            # Resize to cut the top nibble + sign off: 10101010---01101.001
            fpNum.resize((large_int_bits - 4, frac_bits), overflow_mode=OverflowEnum.wrap)
            # use [:] to create unsigned long to represent large fp value
            self.assertEqual(int(fpNum[:]), value_bit_field_pos & bit_field_top_nibble_mask)

            # =======================================================================
            # Saturation

            # Losing MSBs, no saturation required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         value=low_order_value)
            fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, low_order_value)

            # Losing MSB, to top bit set to 1, low order bits
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)

            fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, 15.875)

            # Losing MSB, to top bit set to 0, low order bits
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((2, frac_bits), overflow_mode=OverflowEnum.sat)
            self.assertEqual(fpNum, 3.875)

            # =======================================================================
            # Exception

            # Losing MSBs, no wrapping required
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         value=low_order_value)
            try:
                fpNum.resize((max_total_float_bits, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                self.fail()

            # Losing MSBs, low order bits only
            # Start: 01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         value=low_order_value)
            try:
                fpNum.resize((2, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, wrap would be positive to negative
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            try:
                fpNum.resize((4, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

            # Losing MSB, wrap would be positive to positive
            # Start: 10101010---01101.001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            try:
                fpNum.resize((5, frac_bits), overflow_mode=OverflowEnum.excep)
            except FpBinaryOverflowException:
                pass
            else:
                self.fail()

        def testRoundingModesSignedLarge(self):
            # Adding 1 for the sign bit
            large_int_bits = test_utils.get_small_type_size() + 8 + 1
            frac_bits = 4
            high_order_bits_pos = 0x7A << test_utils.get_small_type_size()

            low_order_value = 13.875
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)
            value_bit_field_neg = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits) - \
                                  (1 << (large_int_bits + frac_bits - 1))

            fpNum_high_order_pos_only = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                                             bit_field=(high_order_bits_pos << frac_bits))
            fpNum_high_order_neg_only = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                                             bit_field=value_bit_field_neg -
                                                                       int(low_order_value * 2 ** frac_bits))

            # =======================================================================
            # No change expected after rounding, low order bits only
            # Start: 000---01101.001
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=True, value=13.125)
            res = fpNum1.resize((large_int_bits, 3), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.125)

            # =======================================================================
            # Change expected after rounding, low order bits only
            # Start: 000---01101.001
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=True, value=13.125)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 13.25)

            # Start: 000---01101.111
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=True, value=13.875)
            res = fpNum1.resize((large_int_bits, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 14.0)

            # Start: 000---01101.011
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=True, value=13.375)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.25)

            # Start: 000---01101.111
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=True, value=13.875)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.75)

            # =======================================================================
            # Change expected after rounding, with high order bits

            # Start: 001111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 14.0)

            # Start: 001111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.875)

            # Start: 001111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.5)

            # Start: 001111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 0), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.0)

            # Start: 111---101111010---01101.1110 (i.e. negative)
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            fpNum.resize((large_int_bits, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_neg_only, 14.0)

            # Start: 111---101111010---01101.1110 (i.e. negative)
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            fpNum.resize((large_int_bits, 3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_neg_only, 13.875)

            # Start: 111---101111010---01101.1110 (i.e. negative)
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            fpNum.resize((large_int_bits, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_neg_only, 13.5)

            # Start: 111---101111010---01101.1110 (i.e. negative)
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            fpNum.resize((large_int_bits, 0), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_neg_only, 13.0)

        def testRoundingModesUnsignedLarge(self):
            # Adding 1 for the sign bit
            large_int_bits = test_utils.get_small_type_size() + 8
            frac_bits = 4
            high_order_bits_pos = 0xFA << test_utils.get_small_type_size()

            low_order_value = 13.875
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)

            fpNum_high_order_pos_only = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                                             bit_field=(high_order_bits_pos << frac_bits))

            # =======================================================================
            # No change expected after rounding, low order bits only
            # Start: 000---01101.001
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=False, value=13.125)
            res = fpNum1.resize((large_int_bits, 3), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.125)

            # =======================================================================
            # Change expected after rounding, low order bits only
            # Start: 000---01101.001
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=False, value=13.125)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 13.25)

            # Start: 000---01101.111
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=False, value=13.875)
            res = fpNum1.resize((large_int_bits, 1), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(res, 14.0)

            # Start: 000---01101.011
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=False, value=13.375)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.25)

            # Start: 000---01101.111
            fpNum1 = self.fp_binary_class(large_int_bits, 4, signed=False, value=13.875)
            res = fpNum1.resize((large_int_bits, 2), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(res, 13.75)

            # =======================================================================
            # Change expected after rounding, with high order bits

            # Start: 11111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 2), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 14.0)

            # Start: 11111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 3), round_mode=RoundingEnum.near_pos_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.875)

            # Start: 11111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 1), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.5)

            # Start: 11111010---01101.1110
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            fpNum.resize((large_int_bits, 0), round_mode=RoundingEnum.direct_neg_inf)
            self.assertEqual(fpNum - fpNum_high_order_pos_only, 13.0)

        def testIntConversionSmall(self):
            # =======================================================================
            # Signed to signed
            # b1001.10111 = -6.28125 - goes to -201 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-6.28125)
            self.assertEqual(fpNum.bits_to_signed(), -201)

            # In conjunction with left shifting
            # b1000.10111 = -7.28125 - goes to b1111.00010 after >> 3
            # b1111.00010 = -30 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum >> 3).bits_to_signed(), -30)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum << 2).bits_to_signed(), 92)

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
            self.assertEqual((fpNum >> 3)[:], 482)
            # In conjunction with right shifting
            # b1000.10111 = -7.28125 - goes to b0010.11100 after << 2
            # b0010.11100 = 220 as unsigned integer
            fpNum = self.fp_binary_class(4, 5, signed=True, value=-7.28125)
            self.assertEqual((fpNum << 2)[:], 92)

        def testIntConversionLarge(self):
            # Adding 1 for the sign bit
            large_int_bits = test_utils.get_small_type_size() + 8 + 1
            frac_bits = 4
            total_bits_mask = (1 << (large_int_bits + frac_bits)) - 1
            high_order_bits_pos = 0x7A << test_utils.get_small_type_size()

            low_order_value = 13.0625
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)
            value_bit_field_neg = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits) - \
                                  (1 << (large_int_bits + frac_bits - 1))

            # =======================================================================
            # Signed to signed (interpret bits as signed int values)

            # b101111010---1101.1001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual(fpNum.bits_to_signed(), value_bit_field_neg)
            self.assertNotEqual(fpNum, fpNum.bits_to_signed())

            # b001111010---1101.1001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_pos)
            self.assertEqual(fpNum.bits_to_signed(), value_bit_field_pos)
            self.assertNotEqual(fpNum, fpNum.bits_to_signed())

            # In conjunction with left shifting
            # b101111010---1101.1001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual((fpNum >> 3).bits_to_signed(), value_bit_field_neg >> 3)
            self.assertNotEqual(fpNum, (fpNum >> 3).bits_to_signed())

            # In conjunction with right shifting
            # b101111010---1101.1001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual((fpNum << 15).bits_to_signed(),
                             int(low_order_value * 2.0 ** frac_bits) << 15)

            # =======================================================================
            # Signed to unsigned (interpret bits as unsigned int values)
            # =======================================================================
            # Signed to signed
            # # b101111010---1101.1001
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual(fpNum[:], value_bit_field_neg & total_bits_mask)

            # In conjunction with left shifting
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual((fpNum >> 3)[:],
                             (value_bit_field_neg >> 3) & total_bits_mask)

            # In conjunction with right shifting
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                         bit_field=value_bit_field_neg)
            self.assertEqual((fpNum << 4)[:],
                             (value_bit_field_neg << 4) & total_bits_mask)

            # =======================================================================
            # Unsigned to signed (interpret bits as signed int values)
            # =======================================================================

            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_neg)
            self.assertEqual(fpNum.bits_to_signed(), value_bit_field_neg)

            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            self.assertEqual(fpNum.bits_to_signed(), value_bit_field_pos)

            # =======================================================================
            # Unsigned to unsigned (interpret bits as unsigned int values)
            # =======================================================================
            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_neg)
            self.assertEqual(fpNum[:], value_bit_field_neg & total_bits_mask)

            fpNum = self.fp_binary_class(large_int_bits, frac_bits, signed=False,
                                         bit_field=value_bit_field_pos)
            self.assertEqual(fpNum[:], value_bit_field_pos)

        def testIsSignedConversionSmall(self):
            # Test unsigned conversions don't lose information and resulting
            # math operation is signed.

            int_bits = 5
            frac_bits = 3
            max_unsigned_mag = 2.0 ** int_bits
            max_signed_mag = 2.0 ** (int_bits - 1)
            inc = 2.0 ** (-frac_bits)

            unsigned_val = 0

            while unsigned_val < max_unsigned_mag:
                signed_val = -max_signed_mag
                while signed_val < max_signed_mag:
                    unsigned_num = self.fp_binary_class(int_bits, frac_bits, signed=False, value=unsigned_val)
                    signed_num = self.fp_binary_class(int_bits, frac_bits, signed=True, value=signed_val)
                    add_result = unsigned_num + signed_num
                    sub_result = unsigned_num - signed_num
                    mult_result = unsigned_num * signed_num

                    self.assertEqual(add_result, unsigned_val + signed_val)
                    self.assertTrue(add_result.is_signed)
                    self.assertEqual(sub_result, unsigned_val - signed_val)
                    self.assertTrue(sub_result.is_signed)
                    self.assertEqual(mult_result, unsigned_val * signed_val)
                    self.assertTrue(mult_result.is_signed)

                    signed_val += inc
                unsigned_val += inc

        def testIsSignedConversionLarge(self):
            # Test unsigned conversions don't lose information and resulting
            # math operation is signed.

            int_bits = test_utils.get_small_type_size() + 16
            frac_bits = 4
            max_unsigned_bit_field = (1 << (int_bits + frac_bits)) - 1
            max_signed_bit_field = (1 << (int_bits + frac_bits - 1)) - 1

            unsigned_bit_field = long(1)
            while unsigned_bit_field < max_unsigned_bit_field:
                # Start at max magnitude negative value and get closer to zero, then
                # wrap to max postive value

                signed_additive = long(1)
                signed_bit_field = -(1 << (int_bits + frac_bits - 1))
                while signed_bit_field < max_signed_bit_field:
                    unsigned_num = self.fp_binary_class(int_bits, frac_bits, signed=False,
                                                        bit_field=unsigned_bit_field)
                    signed_num = self.fp_binary_class(int_bits, frac_bits, signed=True,
                                                      bit_field=signed_bit_field)
                    add_result = unsigned_num + signed_num
                    sub_result = unsigned_num - signed_num
                    mult_result = unsigned_num * signed_num

                    self.assertEqual(add_result.bits_to_signed(), unsigned_bit_field + signed_bit_field)
                    self.assertTrue(add_result.is_signed)
                    self.assertEqual(sub_result.bits_to_signed(), unsigned_bit_field - signed_bit_field)
                    self.assertTrue(sub_result.is_signed)
                    self.assertEqual(mult_result.bits_to_signed(), unsigned_bit_field * signed_bit_field)
                    self.assertTrue(mult_result.is_signed)

                    signed_additive = (signed_additive << 1) + 1
                    signed_bit_field = -(1 << (int_bits + frac_bits - 1)) + signed_additive
                unsigned_bit_field = (unsigned_bit_field << 1) + 1

        def testSequenceOpsSmall(self):
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

        def testSequenceOpsLarge(self):
            large_int_bits = test_utils.get_small_type_size() + 8 + 1
            frac_bits = 4
            total_bits = large_int_bits + frac_bits
            total_bits_mask = (1 << (large_int_bits + frac_bits)) - 1
            high_order_bits_pos = 0x7A << test_utils.get_small_type_size()

            low_order_value = 13.875
            value_bit_field_pos = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits)
            value_bit_field_neg = (high_order_bits_pos << frac_bits) + int(low_order_value * 2 ** frac_bits) - \
                                  (1 << (large_int_bits + frac_bits - 1))

            # # b001111010---1101.1110
            fpNum_positive = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                                  bit_field=value_bit_field_pos)

            self.assertEqual(fpNum_positive[0], False)
            self.assertEqual(fpNum_positive[1], True)
            self.assertEqual(fpNum_positive[total_bits - 1], False)
            self.assertEqual(fpNum_positive[total_bits - 3], True)

            self.assertEqual(int(fpNum_positive[:]), value_bit_field_pos)
            self.assertEqual(int(fpNum_positive[total_bits - 1:total_bits - 3]), 1)

            # Index error check
            try:
                fpNum[len(fpNum_positive) + 1:0]
                self.fail()
            except:
                pass

            try:
                fpNum_positive[0:-1]
                self.fail()
            except:
                pass

            # Negative number
            # # b101111010---1101.1110
            fpNum_negative = self.fp_binary_class(large_int_bits, frac_bits, signed=True,
                                                  bit_field=value_bit_field_neg)
            self.assertEqual(fpNum_negative[0], False)
            self.assertEqual(fpNum_negative[1], True)
            self.assertEqual(fpNum_negative[total_bits - 1], True)
            self.assertEqual(fpNum_negative[total_bits - 3], True)

            self.assertEqual(int(fpNum_negative[:]), value_bit_field_neg & total_bits_mask)
            self.assertEqual(int(fpNum_negative[total_bits - 1:total_bits - 3]), 5)

        def testStrEx(self):
            # Rough estimate of machine precision
            prec_bits = int(test_utils.get_small_type_size() * 0.65)

            # Smallest magnitude negative signed
            val_frac = -2.0 ** -prec_bits
            fpNum = self.fp_binary_class(prec_bits + 1, prec_bits, signed=True, value=val_frac)
            # Have to make the string look positive because the _build_long_float_str only
            # sees sign on the int input...
            self.assertTrue(fpNum.str_ex().lstrip('-') == self._build_long_float_str(0.0, val_frac))

            # Max value signed
            val_int = 2.0 ** prec_bits - 1
            val_frac = sum([2.0 ** -i for i in range(1, prec_bits + 1)])
            fpNum = self.fp_binary_class(prec_bits + 1, prec_bits, signed=True, value=val_int)
            fpNum = fpNum + val_frac
            self.assertTrue(fpNum.str_ex() == self._build_long_float_str(val_int, val_frac))

            # Max value unsigned
            val_int = 2.0 ** prec_bits - 1
            val_frac = sum([2.0 ** -i for i in range(1, prec_bits + 1)])
            fpNum = self.fp_binary_class(prec_bits, prec_bits, signed=False, value=val_int)
            fpNum = fpNum + val_frac
            self.assertTrue(fpNum.str_ex() == self._build_long_float_str(val_int, val_frac))

            # Using bitfields - 2^33 + 2^-33 = 8589934592.000000000116415321826934814453125
            # via https://www.wolframalpha.com/input/?i=2.0%5E33+%2B+2%5E-33
            fpNum = self.fp_binary_class(35, 33, signed=True, bit_field=((1 << 33) << 33) + 1)
            self.assertTrue(fpNum.str_ex() == '8589934592.000000000116415321826934814453125')

            # Using bitfields - 2.0^33 + (2.0^34 - 1) / 2.0^34 = 8589934592.9999999999417923390865325927734375
            # via https://www.wolframalpha.com/input/?i=2.0%5E33+%2B+(2.0%5E34+-+1)+%2F+2.0%5E34
            fpNum = self.fp_binary_class(35, 34, signed=True,
                                         bit_field=((1 << 33) << 34) + ((1 << 34) - 1))
            self.assertTrue(fpNum.str_ex() == '8589934592.9999999999417923390865325927734375')

        def testPickle(self):
            fp_list = [
                self.fp_binary_class(8, 8, signed=True, value=0.01234),
                self.fp_binary_class(8, 8, signed=True, value=-3.01234),
                self.fp_binary_class(8, 8, signed=False, value=0.01234),
                self.fp_binary_class(test_utils.get_small_type_size() - 2, 2, signed=True, value=56.789),
                self.fp_binary_class(test_utils.get_small_type_size() - 2, 3, signed=True, value=56.789),
                self.fp_binary_class(test_utils.get_small_type_size(),
                                     test_utils.get_small_type_size(), signed=True,
                                     bit_field=(1 << (test_utils.get_small_type_size() + 5)) + 23),
                self.fp_binary_class(test_utils.get_small_type_size(),
                                     test_utils.get_small_type_size(), signed=False,
                                     bit_field=(1 << (test_utils.get_small_type_size() * 2)) - 1),
            ]


            for pickle_lib in pickle_libs:

                unpickled = None

                # Test saving of individual objects
                for test_case in fp_list:
                    with open(pickle_test_file_name, 'wb') as f:
                        pickle_lib.dump(test_case, f, pickle_lib.HIGHEST_PROTOCOL)

                    with open(pickle_test_file_name, 'rb') as f:
                        unpickled = pickle_lib.load(f)
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(test_case, unpickled))

                    # Test that the unpickled object is usable
                    self.assertEqual(test_case + 1.0, unpickled + 1.0)
                    self.assertEqual(test_case * 2.0, unpickled * 2.0)

                # With append
                remove_pickle_file()

                for test_case in fp_list:
                    with open(pickle_test_file_name, 'ab') as f:
                        pickle_lib.dump(test_case, f, pickle_lib.HIGHEST_PROTOCOL)

                unpickled = []
                with open(pickle_test_file_name, 'rb') as f:
                    while True:
                        try:
                            unpickled.append(pickle_lib.load(f))
                        except:
                            break

                for expected, loaded in zip(fp_list, unpickled):
                    self.assertTrue(
                        test_utils.fp_binary_instances_are_totally_equal(expected, loaded))

                    # Test that the unpickled object is usable
                    self.assertEqual(expected << 2, loaded << 2)
                    self.assertEqual(expected >> 3, loaded >> 3)


                # Test saving of list of objects

                with open(pickle_test_file_name, 'wb') as f:
                    pickle_lib.dump(fp_list, f, pickle_lib.HIGHEST_PROTOCOL)

                with open(pickle_test_file_name, 'rb') as f:
                    unpickled = pickle_lib.load(f)

                for expected, loaded in zip(fp_list, unpickled):
                    self.assertTrue(
                        test_utils.fp_binary_instances_are_totally_equal(expected, loaded))


class FpBinaryTests(AbstractTestHider.WrapperClassesTestAbstract):
    def setUp(self):
        self.fp_binary_class = FpBinary
        super(FpBinaryTests, self).setUp()


if __name__ == "__main__":
    unittest.main()
