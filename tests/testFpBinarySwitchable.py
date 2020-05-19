#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SML

import sys, unittest, random, copy, os, pickle
import tests.test_utils as test_utils
from fpbinary import FpBinary, FpBinarySwitchable, OverflowEnum, RoundingEnum


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

class FpBianrySwitchableTests(unittest.TestCase):
    def assertAlmostEqual(self, first, second, places=7):
        """Overload TestCase.assertAlmostEqual() to avoid use of round()"""
        tol = 10.0 ** -places
        self.assertTrue(float(abs(first - second)) < tol,
                        '{} and {} differ by more than {} ({})'.format(
                            first, second, tol, (first - second)))

    def testConstructorCombinations(self):
        # Fp mode, no values set or not FpBinary type
        with self.assertRaises(TypeError):
            fpNum = FpBinarySwitchable(fp_mode=True)
        with self.assertRaises(TypeError):
            fpNum = FpBinarySwitchable(fp_mode=True, fp_value=object())

        # Non-Fp mode, no values set or not float convertable value

        # No value ok, defaults to 0.0
        fpNum = FpBinarySwitchable(fp_mode=False)

        with self.assertRaises(TypeError):
            fpNum = FpBinarySwitchable(fp_mode=False, float_value='sfasef')

    def testBasicMathFpMode(self):
        # Negate
        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=12.125))
        self.assertEqual(-fpNum, -12.125)
        self.assertTrue((-fpNum).fp_mode)
        self.assertTrue(isinstance((-fpNum).value, FpBinary))

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-7, 20, signed=True, value=0.0001220703125))
        self.assertEqual(-fpNum, -0.0001220703125)
        self.assertTrue((-fpNum).fp_mode)
        self.assertTrue(isinstance((-fpNum).value, FpBinary))

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, -8, signed=True, value=1024.0))
        self.assertEqual(-fpNum, -1024.0)
        self.assertTrue((-fpNum).fp_mode)
        self.assertTrue(isinstance((-fpNum).value, FpBinary))

        # Add
        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.25))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=-0.25))
        self.assertEqual(fpNum1 + fpNum2, 15.0)
        self.assertTrue((fpNum1 + fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.03125))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=-0.03125))
        self.assertEqual(fpNum1 + fpNum2, 15.0)
        self.assertTrue((fpNum1 + fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.0))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, -3, signed=True, value=8.0))
        self.assertEqual(fpNum1 + fpNum2, 23.0)
        self.assertTrue((fpNum1 + fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        # Subtract
        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.25))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=-0.25))
        self.assertEqual(fpNum1 - fpNum2, 15.5)
        self.assertTrue((fpNum1 - fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 - fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 - fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.03125))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=-0.03125))
        self.assertEqual(fpNum1 - fpNum2, 15.0625)
        self.assertTrue((fpNum1 - fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 - fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 - fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.0))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, -3, signed=True, value=8.0))
        self.assertEqual(fpNum1 - fpNum2, 7.0)
        self.assertTrue((fpNum1 - fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        # Multiply
        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(100, 16, signed=True, value=0.0625))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=2.0))
        self.assertEqual(fpNum1 * fpNum2, 0.125)
        self.assertTrue((fpNum1 * fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 * fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 * fpNum2, FpBinarySwitchable))


        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=-0.03125))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=6.25))
        self.assertEqual(fpNum1 * fpNum2, -0.1953125)
        self.assertTrue((fpNum1 * fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 * fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 * fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=2.5))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(12, -3, signed=True, value=-72.0))
        self.assertEqual(fpNum1 * fpNum2, -180.0)
        self.assertTrue((fpNum1 * fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        # Divide
        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(5, 5, signed=True, value=-3.0))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(5, 5, signed=True, value=2.0))
        self.assertEqual(fpNum1 / fpNum2, -1.5)
        self.assertTrue((fpNum1 / fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 / fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 / fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=0.03125))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=-0.03125))
        self.assertEqual(fpNum1 / fpNum2, -1.0)
        self.assertTrue((fpNum1 / fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 / fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 / fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=80.0))
        fpNum2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, -3, signed=True, value=-40.0))
        self.assertEqual(fpNum1 / fpNum2, -2.0)
        self.assertTrue((fpNum1 / fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, FpBinary))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        # ABS
        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=-12.125))
        self.assertEqual(abs(fpNum), 12.125)
        self.assertTrue(abs(fpNum).fp_mode)
        self.assertTrue(isinstance(abs(fpNum).value, FpBinary))
        self.assertTrue(isinstance(abs(fpNum1), FpBinarySwitchable))

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=12.125))
        self.assertEqual(abs(fpNum), 12.125)

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=-0.0234375))
        self.assertEqual(abs(fpNum), 0.0234375)
        self.assertTrue(abs(fpNum).fp_mode)
        self.assertTrue(isinstance(abs(fpNum).value, FpBinary))
        self.assertTrue(isinstance(abs(fpNum1), FpBinarySwitchable))

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(-3, 19, signed=True, value=0.0234375))
        self.assertEqual(abs(fpNum), 0.0234375)

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, -8, signed=True, value=-256.0))
        self.assertEqual(abs(fpNum), 256.0)
        self.assertTrue(abs(fpNum).fp_mode)
        self.assertTrue(isinstance(abs(fpNum).value, FpBinary))
        self.assertTrue(isinstance(abs(fpNum1), FpBinarySwitchable))

        fpNum = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, -8, signed=True, value=256.0))
        self.assertEqual(abs(fpNum), 256.0)

    def testBasicMathDoubleMode(self):
        fpNum = FpBinarySwitchable(fp_mode=False, float_value=12.125)
        self.assertEqual(-fpNum, -12.125)
        self.assertFalse((-fpNum).fp_mode)
        self.assertTrue(isinstance((-fpNum).value, float))

        fpNum1 = FpBinarySwitchable(fp_mode=False, float_value=15.25)
        fpNum2 = FpBinarySwitchable(fp_mode=False, float_value=-0.25)
        self.assertEqual(fpNum1 + fpNum2, 15.0)
        self.assertFalse((fpNum1 + fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 + fpNum2).value, float))
        self.assertTrue(isinstance(fpNum1 + fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=False, float_value=15.25)
        fpNum2 = FpBinarySwitchable(fp_mode=False, float_value=-0.25)
        self.assertEqual(fpNum1 - fpNum2, 15.5)
        self.assertFalse((fpNum1 - fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 - fpNum2).value, float))
        self.assertTrue(isinstance(fpNum1 - fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=False, float_value=0.0625)
        fpNum2 = FpBinarySwitchable(fp_mode=False, float_value=2.0)
        self.assertEqual(fpNum1 * fpNum2, 0.125)
        self.assertFalse((fpNum1 * fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 * fpNum2).value, float))
        self.assertTrue(isinstance(fpNum1 * fpNum2, FpBinarySwitchable))

        fpNum1 = FpBinarySwitchable(fp_mode=False, float_value=-3.0)
        fpNum2 = FpBinarySwitchable(fp_mode=False, float_value=2.0)
        self.assertEqual(fpNum1 / fpNum2, -1.5)
        self.assertFalse((fpNum1 / fpNum2).fp_mode)
        self.assertTrue(isinstance((fpNum1 / fpNum2).value, float))
        self.assertTrue(isinstance(fpNum1 / fpNum2, FpBinarySwitchable))

        fpNum = FpBinarySwitchable(fp_mode=False, float_value=-12.125)
        self.assertEqual(abs(fpNum), 12.125)
        self.assertFalse(abs(fpNum).fp_mode)
        self.assertTrue(isinstance(abs(fpNum).value, float))
        self.assertTrue(isinstance(abs(fpNum1), FpBinarySwitchable))

        fpNum = FpBinarySwitchable(fp_mode=False, float_value=12.125)
        self.assertEqual(abs(fpNum), 12.125)

    def testMixedTypeMathFpMode(self):
        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.25))
        fp_bin = FpBinary(10, 8, signed=True, value=-0.25)
        self.assertEqual(switchable + fp_bin, 15.0)
        self.assertTrue(isinstance(switchable + fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin + switchable, 15.0)
        self.assertTrue((fp_bin + switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin + switchable).value, FpBinary))
        self.assertTrue(isinstance(fp_bin + switchable, FpBinarySwitchable))
        self.assertEqual(switchable + -0.25, 15.0)
        self.assertTrue(isinstance(switchable + -0.25, FpBinarySwitchable))
        self.assertEqual(-0.25 + switchable, 15.0)
        self.assertTrue((-0.25 + switchable).fp_mode)
        self.assertTrue(isinstance((-0.25 + switchable).value, FpBinary))
        self.assertTrue(isinstance(-0.25 + switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=15.25))
        fp_bin = FpBinary(10, 8, signed=True, value=20.5)
        self.assertEqual(switchable - fp_bin, -5.25)
        self.assertTrue(isinstance(switchable - fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin - switchable, 5.25)
        self.assertTrue((fp_bin - switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin - switchable).value, FpBinary))
        self.assertTrue(isinstance(fp_bin - switchable, FpBinarySwitchable))
        self.assertEqual(switchable - 20.5, -5.25)
        self.assertTrue(isinstance(switchable - 20.5, FpBinarySwitchable))
        self.assertEqual(20.5 - switchable, 5.25)
        self.assertTrue((20.5 - switchable).fp_mode)
        self.assertTrue(isinstance((20.5 - switchable).value, FpBinary))
        self.assertTrue(isinstance(20.5 - switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=-3.125))
        fp_bin = FpBinary(10, 8, signed=True, value=-2.0)
        self.assertEqual(switchable * fp_bin, 6.25)
        self.assertTrue(isinstance(switchable * fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin * switchable, 6.25)
        self.assertTrue((fp_bin * switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin * switchable).value, FpBinary))
        self.assertTrue(isinstance(fp_bin * switchable, FpBinarySwitchable))
        self.assertEqual(switchable * -2.0, 6.25)
        self.assertTrue(isinstance(switchable * -2.0, FpBinarySwitchable))
        self.assertEqual(-2.0 * switchable, 6.25)
        self.assertTrue((-2.0 * switchable).fp_mode)
        self.assertTrue(isinstance((-2.0 * switchable).value, FpBinary))
        self.assertTrue(isinstance(-2.0 * switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 16, signed=True, value=3.0))
        fp_bin = FpBinary(10, 8, signed=True, value=1.5)
        self.assertEqual(switchable / fp_bin, 2.0)
        self.assertTrue(isinstance(switchable / fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin / switchable, 0.5)
        self.assertTrue((fp_bin / switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin / switchable).value, FpBinary))
        self.assertTrue(isinstance(fp_bin / switchable, FpBinarySwitchable))
        self.assertEqual(switchable / 1.5, 2.0)
        self.assertTrue(isinstance(switchable / 1.5, FpBinarySwitchable))
        self.assertEqual(1.5 / switchable, 0.5)
        self.assertTrue((1.5 / switchable).fp_mode)
        self.assertTrue(isinstance((1.5 / switchable).value, FpBinary))
        self.assertTrue(isinstance(1.5 / switchable, FpBinarySwitchable))

    def testMixedTypeMathDoubleMode(self):
        switchable = FpBinarySwitchable(fp_mode=False, float_value=15.25)
        fp_bin = FpBinary(10, 8, signed=True, value=-0.25)
        self.assertEqual(switchable + fp_bin, 15.0)
        self.assertTrue(isinstance(switchable + fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin + switchable, 15.0)
        self.assertFalse((fp_bin + switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin + switchable).value, float))
        self.assertTrue(isinstance(fp_bin + switchable, FpBinarySwitchable))
        self.assertEqual(switchable + -0.25, 15.0)
        self.assertTrue(isinstance(switchable + -0.25, FpBinarySwitchable))
        self.assertEqual(-0.25 + switchable, 15.0)
        self.assertFalse((-0.25 + switchable).fp_mode)
        self.assertTrue(isinstance((-0.25 + switchable).value, float))
        self.assertTrue(isinstance(-0.25 + switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=False, float_value=15.25)
        fp_bin = FpBinary(10, 8, signed=True, value=20.5)
        self.assertEqual(switchable - fp_bin, -5.25)
        self.assertTrue(isinstance(switchable - fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin - switchable, 5.25)
        self.assertFalse((fp_bin - switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin - switchable).value, float))
        self.assertTrue(isinstance(fp_bin - switchable, FpBinarySwitchable))
        self.assertEqual(switchable - 20.5, -5.25)
        self.assertTrue(isinstance(switchable - 20.5, FpBinarySwitchable))
        self.assertEqual(20.5 - switchable, 5.25)
        self.assertFalse((20.5 - switchable).fp_mode)
        self.assertTrue(isinstance((20.5 - switchable).value, float))
        self.assertTrue(isinstance(20.5 - switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=False, float_value=-3.125)
        fp_bin = FpBinary(10, 8, signed=True, value=-2.0)
        self.assertEqual(switchable * fp_bin, 6.25)
        self.assertTrue(isinstance(switchable * fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin * switchable, 6.25)
        self.assertFalse((fp_bin * switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin * switchable).value, float))
        self.assertTrue(isinstance(fp_bin * switchable, FpBinarySwitchable))
        self.assertEqual(switchable * -2.0, 6.25)
        self.assertTrue(isinstance(switchable * -2.0, FpBinarySwitchable))
        self.assertEqual(-2.0 * switchable, 6.25)
        self.assertFalse((-2.0 * switchable).fp_mode)
        self.assertTrue(isinstance((-2.0 * switchable).value, float))
        self.assertTrue(isinstance(-2.0 * switchable, FpBinarySwitchable))

        switchable = FpBinarySwitchable(fp_mode=False, float_value=3.0)
        fp_bin = FpBinary(10, 8, signed=True, value=1.5)
        self.assertEqual(switchable / fp_bin, 2.0)
        self.assertTrue(isinstance(switchable / fp_bin, FpBinarySwitchable))
        self.assertEqual(fp_bin / switchable, 0.5)
        self.assertFalse((fp_bin / switchable).fp_mode)
        self.assertTrue(isinstance((fp_bin / switchable).value, float))
        self.assertTrue(isinstance(fp_bin / switchable, FpBinarySwitchable))
        self.assertEqual(switchable / 1.5, 2.0)
        self.assertTrue(isinstance(switchable / 1.5, FpBinarySwitchable))
        self.assertEqual(1.5 / switchable, 0.5)
        self.assertFalse((1.5 / switchable).fp_mode)
        self.assertTrue(isinstance((1.5 / switchable).value, float))
        self.assertTrue(isinstance(1.5 / switchable, FpBinarySwitchable))

    def testDoubleVsFpModeBasic(self):
        switchable_double = FpBinarySwitchable(fp_mode=False, float_value=1.0 / 3.0)
        switchable_fp = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 3, signed=True, value=(1.0 / 3.0)))
        self.assertNotEqual(switchable_double, switchable_fp)
        self.assertEqual(switchable_double, 1.0 / 3.0)
        self.assertEqual(switchable_fp, 0.375)

    def testDoubleVsFpModeMath(self):
        # fixed point mode takes precedence
        double_num = FpBinarySwitchable(fp_mode=False, float_value=1.5)
        fp_num = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(10, 3, signed=True, value=3.0))

        self.assertEqual(double_num + fp_num, 4.5)
        self.assertTrue((double_num + fp_num).fp_mode)
        self.assertTrue(isinstance((double_num + fp_num).value, FpBinary))

        self.assertEqual(fp_num + double_num, 4.5)
        self.assertTrue((fp_num + double_num).fp_mode)
        self.assertTrue(isinstance((fp_num + double_num).value, FpBinary))

        self.assertEqual(double_num - fp_num, -1.5)
        self.assertTrue((double_num - fp_num).fp_mode)
        self.assertTrue(isinstance((double_num - fp_num).value, FpBinary))

        self.assertEqual(fp_num - double_num, 1.5)
        self.assertTrue((fp_num - double_num).fp_mode)
        self.assertTrue(isinstance((fp_num - double_num).value, FpBinary))

        self.assertEqual(double_num * fp_num, 4.5)
        self.assertTrue((double_num * fp_num).fp_mode)
        self.assertTrue(isinstance((double_num * fp_num).value, FpBinary))

        self.assertEqual(fp_num * double_num, 4.5)
        self.assertTrue((fp_num * double_num).fp_mode)
        self.assertTrue(isinstance((fp_num * double_num).value, FpBinary))

        self.assertEqual(double_num / fp_num, 0.5)
        self.assertTrue((double_num / fp_num).fp_mode)
        self.assertTrue(isinstance((double_num / fp_num).value, FpBinary))

        self.assertEqual(fp_num / double_num, 2.0)
        self.assertTrue((fp_num / double_num).fp_mode)
        self.assertTrue(isinstance((fp_num * double_num).value, FpBinary))

    def testResizeFpMode(self):
        # Wrap b01.0101 -> b01.01
        fp_num = FpBinarySwitchable(True,
                                    fp_value=FpBinary(10, 10, signed=True, value=1.3125),
                                    float_value=4.5)
        self.assertEqual(fp_num.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                                       round_mode=RoundingEnum.direct_neg_inf), 1.25)
        self.assertEqual(fp_num.format, (2, 2))

        # Round Up b01.0101 -> b01.011
        fp_num = FpBinarySwitchable(True,
                                    fp_value=FpBinary(10, 10, signed=True, value=1.3125),
                                    float_value=4.5)
        self.assertEqual(fp_num.resize((2, 3), overflow_mode=OverflowEnum.wrap,
                                       round_mode=RoundingEnum.near_pos_inf), 1.375)
        self.assertEqual(fp_num.format, (2, 3))

        # Check negative format
        fp_num = FpBinarySwitchable(True,
                                    fp_value=FpBinary(-2, 10, signed=True, value=-0.0625),
                                    float_value=-0.0625)
        self.assertEqual(fp_num.resize((-3, 8), overflow_mode=OverflowEnum.wrap,
                                       round_mode=RoundingEnum.near_pos_inf), -0.0625)
        self.assertEqual(fp_num.format, (-3, 8))

        fp_num = FpBinarySwitchable(True,
                                    fp_value=FpBinary(11, -3, signed=True, value=24.0),
                                    float_value=24.0)
        self.assertEqual(fp_num.resize((10, -4), overflow_mode=OverflowEnum.wrap,
                                       round_mode=RoundingEnum.near_pos_inf), 32.0)
        self.assertEqual(fp_num.format, (10, -4))

        # Check no exception with floating point mode
        fp_num = FpBinarySwitchable(False, float_value=54.000987)
        fp_num.resize((2, 2), overflow_mode=OverflowEnum.wrap,
                      round_mode=RoundingEnum.direct_neg_inf)

    def testCopy(self):
        fp_num = FpBinarySwitchable(True, fp_value=FpBinary(10, 10, signed=True, value=1.3125))
        fp_num.value = fp_num - 1.0
        fp_num.value = fp_num + 10.0
        fp_copy = copy.copy(fp_num)
        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(fp_num, fp_copy))
        self.assertFalse(fp_num is fp_copy)

        fp_num = FpBinarySwitchable(False, float_value=1.3125)
        # fp_num.value = fp_num - 1.0
        # fp_num.value = fp_num + 10.0
        fp_copy = copy.copy(fp_num)
        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(fp_num, fp_copy))
        self.assertFalse(fp_num is fp_copy)

    def testFormatProperty(self):
        fp_num = FpBinarySwitchable(True, fp_value=FpBinary(8, 8, signed=True, value=-56.89))
        self.assertEqual(fp_num.format, (8, 8))

        fp_num = FpBinarySwitchable(True, fp_value=FpBinary(-2, 8, signed=True, value=-0.0625))
        self.assertEqual(fp_num.format, (-2, 8))

        fp_num = FpBinarySwitchable(True, fp_value=FpBinary(10, -4, signed=True, value=32.0))
        self.assertEqual(fp_num.format, (10, -4))

        # Check floating point mode doesn't raise an exception
        fp_num = FpBinarySwitchable(False, float_value=1.3125)
        the_format = fp_num.format

    def testValueSet(self):
        # Exception when in fp mode and set to object that isn't instance of
        # FpBinary or FpBinarySwitchable
        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=67.3453))
        with self.assertRaises(TypeError):
            switchable.value = object()
        with self.assertRaises(TypeError):
            switchable.value = 1.0
        with self.assertRaises(TypeError):
            switchable.value = 56

        # Exception when not in fp mode and set to object that can't be cast
        # to a float
        switchable = FpBinarySwitchable(fp_mode=False, float_value=-45367.12345)
        with self.assertRaises(TypeError):
            switchable.value = object()
        with self.assertRaises(TypeError):
            switchable.value = 'test'

        # Valid types when in fp mode: FpBinary and FpBinarySwitchable
        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=-0.00123))
        switchable.value = FpBinary(16, 16, signed=False, value=0.125)
        self.assertEqual(switchable, 0.125)
        self.assertEqual(switchable.value, 0.125)
        switchable.value = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=-0.0625))
        self.assertEqual(switchable, -0.0625)
        self.assertEqual(switchable.value, -0.0625)

        # Valid types when not in fp mode: float castable
        switchable = FpBinarySwitchable(fp_mode=False, float_value=0.0001234)
        switchable.value = -678.987
        self.assertEqual(switchable, -678.987)
        self.assertEqual(switchable.value, -678.987)
        switchable.value = 45
        self.assertEqual(switchable, 45)
        self.assertEqual(switchable.value, 45)

    def testMinMaxTracking(self):
        values = [random.uniform(-10000.0, 10000) for i in range(0, 1000)]
        min_val = min(values)
        max_val = max(values)

        switchable_double = FpBinarySwitchable(fp_mode=False, float_value=min_val + 0.1)
        for val in values:
            switchable_double.value = val

        self.assertEqual(switchable_double.min_value, min_val)
        self.assertEqual(switchable_double.max_value, max_val)

    def testBitShifts(self):
        # fp mode
        switchable = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=5.875))
        self.assertEqual(switchable << long(2), 23.5)
        self.assertEqual(switchable >> long(3), 0.734375)

        # double mode (shifts are mult/div by 2
        switchable = FpBinarySwitchable(fp_mode=False, float_value=5.875)
        self.assertEqual(switchable << long(2), 23.5)
        self.assertEqual(switchable >> long(3), 0.734375)

    def testPickle(self):

        # Instances with min and max values set
        fp1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=5.875),
                                 float_value=5.875)
        fp1.value = FpBinary(16, 16, signed=True, value=-34.5)
        fp1.value = FpBinary(16, 16, signed=True, value=34.5)

        fp2 = FpBinarySwitchable(fp_mode=False, fp_value=FpBinary(16, 16, signed=True, value=5.875),
                                 float_value=5.875)
        fp2.value = FpBinary(16, 16, signed=True, value=-34.5)
        fp2.value = FpBinary(16, 16, signed=True, value=34.5)

        fp3 = FpBinarySwitchable(fp_mode=False, fp_value=FpBinary(16, 16, signed=True, value=5.875),
                                 float_value=5.875)
        fp3.value = -56.98
        fp3.value = 26

        fp_list = [
            FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(16, 16, signed=True, value=5.875)),
            FpBinarySwitchable(fp_mode=False, fp_value=FpBinary(16, 16, signed=True, value=5.875)),
            FpBinarySwitchable(fp_mode=False, float_value=-45.6),



            # Extreme sized FpBinary instances
            FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(test_utils.get_small_type_size() - 2, 2, signed=True)),
            FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(test_utils.get_small_type_size() - 2, 3, signed=True)),
            FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(test_utils.get_small_type_size(),
                                 test_utils.get_small_type_size(), signed=True,
                                 bit_field=(1 << (test_utils.get_small_type_size() + 5)) + 23)),
            FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(test_utils.get_small_type_size(),
                                 test_utils.get_small_type_size(), signed=False,
                                 bit_field=(1 << (test_utils.get_small_type_size() * 2)) - 1)),

            fp1,
            fp2,
            fp3,
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

            # Test saving of list of objects

            with open(pickle_test_file_name, 'wb') as f:
                pickle_lib.dump(fp_list, f, pickle_lib.HIGHEST_PROTOCOL)

            with open(pickle_test_file_name, 'rb') as f:
                unpickled = pickle_lib.load(f)

            for expected, loaded in zip(fp_list, unpickled):
                self.assertTrue(
                    test_utils.fp_binary_instances_are_totally_equal(expected, loaded))

                # Test that the unpickled object is usable
                self.assertEqual(expected - 2, loaded - 2)
                self.assertEqual(expected * 3, loaded * 3)

    def testPickleAcrossVersions(self):
        """
        This tests data saved in multiple files, each one from a different python version/pickle protocol.
        It includes both FpBinary and FpBinarySwitchable instances.
        :return:
        """
        for pickle_lib in pickle_libs:
            for fname in test_utils.get_static_pickle_file_paths():
                with open(fname, 'rb') as f:
                    unpickled = pickle_lib.load(f)

                for expected, loaded in zip(test_utils.pickle_static_data, unpickled):
                    self.assertTrue(
                        test_utils.fp_binary_instances_are_totally_equal(expected, loaded))


if __name__ == "__main__":
    unittest.main()
