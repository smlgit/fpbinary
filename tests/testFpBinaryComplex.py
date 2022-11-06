#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SML, some tests adapted from RW Penney's Simple Fixed Point module

import sys, unittest, copy, pickle, os, time
import numpy as np
import scipy.signal as signal
import tests.test_utils as test_utils
from fpbinary import FpBinary, FpBinaryComplex, OverflowEnum, RoundingEnum, FpBinaryOverflowException

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

def fp_binary_complex_add(real1, imag1, real2, imag2):
    return real1 + real2, imag1 + imag2

def fp_binary_complex_sub(real1, imag1, real2, imag2):
    return real1 - real2, imag1 - imag2

def fp_binary_complex_mult(real1, imag1, real2, imag2):
    return real1 * real2 - imag1 * imag2, real1 * imag2 + real2 * imag1

def fp_binary_complex_div(real1, imag1, real2, imag2):
    conj_mult_real, conj_mult_imag = fp_binary_complex_mult(real1, imag1, real2, -imag2)
    denom_energy = real2 * real2 + imag2 * imag2
    return conj_mult_real / denom_energy, conj_mult_imag / denom_energy

def fp_binary_complex_expected_str_ex_from_fp_binary(fp_binary_real, fp_binary_imag):
    if fp_binary_imag < 0.0:
        return '(' + fp_binary_real.str_ex() + fp_binary_imag.str_ex() + 'j)'
    else:
        return '(' + fp_binary_real.str_ex() + '+' + fp_binary_imag.str_ex() + 'j)'

def set_fp_binary_instances_to_best_format(op1, op2):
    """
    Makes op1 and op2 have the largest required format such that they are equal.
    """
    op1.resize((max(op1.format[0], op2.format[0]),
                max(op1.format[1], op2.format[1])))
    op2.resize(op1)

class WrapperClassesTestAbstract(unittest.TestCase):
    def tearDown(self):
        remove_pickle_file()

    def assertAlmostEqual(self, first, second, places=7, delta=None):
        """Overload TestCase.assertAlmostEqual() to avoid use of round()"""
        diff = float(abs(first - second))

        if delta is None:
            tol = 10.0 ** -places
            self.assertTrue(diff < tol,
                            '{} and {} differ by more than {} ({})'.format(
                                first, second, tol, diff))
        else:
            self.assertTrue(diff < delta,
                            '{} and {} differ by more than {} ({})'.format(
                                first, second, delta, diff))

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
        return str(long(int_value)) + ('%.128f' % frac_value).lstrip('-').lstrip('0').rstrip('0')

    def testBasicMath(self):
        """
        Add, Sub, Mult, Div operations between FpBinaryComplex types.
        Also checks between an FpBinaryComplex type and complex, float, int and FpBinary types."""

        total_bits = 4
        frac_bits = 2
        int_bits = total_bits - frac_bits

        increment = 1 / 2**frac_bits
        min_val = -2**(int_bits - 1)
        max_val = 2**(int_bits - 1) - 1

        real1 = min_val
        while real1 <= max_val:
            imag1 = min_val
            while imag1 <= max_val:
                real2 = min_val
                while real2 <= max_val:
                    imag2 = min_val
                    while imag2 <= max_val:
                        fp_num_complex1 = FpBinaryComplex(int_bits, frac_bits, value=complex(real1, imag1))
                        fp_num_complex2 = FpBinaryComplex(int_bits, frac_bits, value=complex(real2, imag2))
                        add_result = fp_num_complex1 + fp_num_complex2
                        sub_result = fp_num_complex1 - fp_num_complex2
                        mult_result = fp_num_complex1 * fp_num_complex2

                        fp_num_real1 = FpBinary(int_bits, frac_bits, value=real1)
                        fp_num_imag1 = FpBinary(int_bits, frac_bits, value=imag1)
                        fp_num_real2 = FpBinary(int_bits, frac_bits, value=real2)
                        fp_num_imag2 = FpBinary(int_bits, frac_bits, value=imag2)

                        expected_add_real, expected_add_imag = fp_binary_complex_add(fp_num_real1, fp_num_imag1, fp_num_real2, fp_num_imag2)
                        expected_sub_real, expected_sub_imag = fp_binary_complex_sub(fp_num_real1, fp_num_imag1, fp_num_real2, fp_num_imag2)
                        expected_mult_real, expected_mult_imag = fp_binary_complex_mult(fp_num_real1, fp_num_imag1, fp_num_real2, fp_num_imag2)

                        # With other types
                        #
                        # Op2 is float
                        add_result_fp_comp_with_float = fp_num_complex1 + real2
                        sub_result_fp_comp_with_float = fp_num_complex1 - real2
                        mult_result_fp_comp_with_float = fp_num_complex1 * real2


                        expected_add_fp_comp_with_float_real = fp_num_real1 + real2
                        expected_add_fp_comp_with_float_imag = (fp_num_imag1 + 0.0).resize(expected_add_fp_comp_with_float_real)
                        expected_sub_fp_comp_with_float_real = fp_num_real1 - real2
                        expected_sub_fp_comp_with_float_imag = (fp_num_imag1 - 0.0).resize(expected_sub_fp_comp_with_float_real)
                        expected_mult_fp_comp_with_float_real = fp_num_real1 * real2
                        expected_mult_fp_comp_with_float_imag = fp_num_imag1 * real2
                        # Add extra bit to account for the add in a complex multiply
                        expected_mult_fp_comp_with_float_real.resize(
                            (expected_mult_fp_comp_with_float_real.format[0] + 1,
                             expected_mult_fp_comp_with_float_real.format[1]))
                        expected_mult_fp_comp_with_float_imag.resize(
                            (expected_mult_fp_comp_with_float_imag.format[0] + 1,
                             expected_mult_fp_comp_with_float_imag.format[1]))

                        # Op1 is float
                        add_result_float_with_fp_comp = real1 + fp_num_complex2
                        sub_result_float_with_fp_comp = real1 - fp_num_complex2
                        mult_result_float_with_fp_comp = real1 * fp_num_complex2

                        expected_add_float_with_fp_comp_real = real1 + fp_num_real2
                        expected_add_float_with_fp_comp_imag = (0.0 + fp_num_imag2).resize(
                            expected_add_float_with_fp_comp_real)
                        expected_sub_float_with_fp_comp_real = real1 - fp_num_real2
                        expected_sub_float_with_fp_comp_imag = (0.0 - fp_num_imag2).resize(
                            expected_sub_float_with_fp_comp_real)
                        expected_mult_float_with_fp_comp_real = real1 * fp_num_real2
                        expected_mult_float_with_fp_comp_imag = real1 * fp_num_imag2
                        # Add extra bit to account for the add in a complex multiply
                        expected_mult_float_with_fp_comp_real.resize(
                            (expected_mult_float_with_fp_comp_real.format[0] + 1,
                             expected_mult_float_with_fp_comp_real.format[1]))
                        expected_mult_float_with_fp_comp_imag.resize(
                            (expected_mult_float_with_fp_comp_imag.format[0] + 1,
                             expected_mult_float_with_fp_comp_imag.format[1]))

                        # Op2 is complex
                        fp_num_auto_real1 = FpBinary(value=real1)
                        fp_num_auto_imag1 = FpBinary(value=imag1)
                        set_fp_binary_instances_to_best_format(fp_num_auto_real1,
                                                               fp_num_auto_imag1)
                        fp_num_auto_real2 = FpBinary(value=real2)
                        fp_num_auto_imag2 = FpBinary(value=imag2)
                        set_fp_binary_instances_to_best_format(fp_num_auto_real2,
                                                               fp_num_auto_imag2)

                        add_result_fp_comp_with_comp = fp_num_complex1 + complex(real2, imag2)
                        sub_result_fp_comp_with_comp = fp_num_complex1 - complex(real2, imag2)
                        mult_result_fp_comp_with_comp = fp_num_complex1 * complex(real2, imag2)

                        expected_add_fp_comp_with_comp_real = fp_num_real1 + real2
                        expected_add_fp_comp_with_comp_imag = fp_num_imag1 + imag2
                        set_fp_binary_instances_to_best_format(expected_add_fp_comp_with_comp_real,
                                                               expected_add_fp_comp_with_comp_imag)
                        expected_sub_fp_comp_with_comp_real = fp_num_real1 - real2
                        expected_sub_fp_comp_with_comp_imag = fp_num_imag1 - imag2
                        set_fp_binary_instances_to_best_format(expected_sub_fp_comp_with_comp_real,
                                                               expected_sub_fp_comp_with_comp_imag)
                        expected_mult_fp_comp_with_comp_real, expected_mult_fp_comp_with_comp_imag = fp_binary_complex_mult(
                            fp_num_real1, fp_num_imag1, fp_num_auto_real2, fp_num_auto_imag2
                        )

                        # Op1 is complex
                        add_result_comp_with_fp_comp = complex(real1, imag1) + fp_num_complex2
                        sub_result_comp_with_fp_comp = complex(real1, imag1) - fp_num_complex2
                        mult_result_comp_with_fp_comp = complex(real1, imag1) * fp_num_complex2

                        expected_add_comp_with_fp_comp_real = real1 + fp_num_real2
                        expected_add_comp_with_fp_comp_imag = imag1 + fp_num_imag2
                        set_fp_binary_instances_to_best_format(expected_add_comp_with_fp_comp_real,
                                                               expected_add_comp_with_fp_comp_imag)
                        expected_sub_comp_with_fp_comp_real = real1 - fp_num_real2
                        expected_sub_comp_with_fp_comp_imag = imag1 - fp_num_imag2
                        set_fp_binary_instances_to_best_format(expected_sub_comp_with_fp_comp_real,
                                                               expected_sub_comp_with_fp_comp_imag)
                        expected_mult_comp_with_fp_comp_real, expected_mult_comp_with_fp_comp_imag = fp_binary_complex_mult(
                            fp_num_auto_real1, fp_num_auto_imag1, fp_num_real2, fp_num_imag2,
                        )

                        # Basic checks for interaction with int.
                        # Just checking no exception is thrown...
                        # Op2 is an int
                        add_result_fp_comp_with_int = fp_num_complex1 + long(6)
                        sub_result_fp_comp_with_int = fp_num_complex1 - long(6)
                        mult_result_fp_comp_with_int = fp_num_complex1 * long(-5)
                        div_result_fp_comp_with_int = fp_num_complex1 / long(3)

                        # Op1 is an int
                        add_result_fp_comp_with_int = long(4) + fp_num_complex2
                        sub_result_fp_comp_with_int = long(-26) - fp_num_complex2
                        mult_result_fp_comp_with_int = long(567) * fp_num_complex2
                        if fp_num_complex2 != 0.0:
                            div_result_fp_comp_with_int = long(-7) / fp_num_complex2


                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(add_result.real, expected_add_real))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(add_result.imag, expected_add_imag))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(sub_result.real, expected_sub_real))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(sub_result.imag, expected_sub_imag))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(mult_result.real, expected_mult_real))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(add_result_fp_comp_with_float.real, expected_add_fp_comp_with_float_real))
                        self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(add_result_fp_comp_with_float.imag, expected_add_fp_comp_with_float_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_fp_comp_with_float.real,
                                                                             expected_sub_fp_comp_with_float_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_fp_comp_with_float.imag,
                                                                             expected_sub_fp_comp_with_float_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_fp_comp_with_float.real,
                                                                             expected_mult_fp_comp_with_float_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_fp_comp_with_float.imag,
                                                                             expected_mult_fp_comp_with_float_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_float_with_fp_comp.real,
                                                                             expected_add_float_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_float_with_fp_comp.imag,
                                                                             expected_add_float_with_fp_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_float_with_fp_comp.real,
                                                                             expected_sub_float_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_float_with_fp_comp.imag,
                                                                             expected_sub_float_with_fp_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_float_with_fp_comp.real,
                                                                             expected_mult_float_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_float_with_fp_comp.imag,
                                                                             expected_mult_float_with_fp_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_fp_comp_with_comp.real,
                                                                             expected_add_fp_comp_with_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_fp_comp_with_comp.imag,
                                                                             expected_add_fp_comp_with_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_fp_comp_with_comp.real,
                                                                             expected_sub_fp_comp_with_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_fp_comp_with_comp.imag,
                                                                             expected_sub_fp_comp_with_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_fp_comp_with_comp.real,
                                                                             expected_mult_fp_comp_with_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_fp_comp_with_comp.imag,
                                                                             expected_mult_fp_comp_with_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_comp_with_fp_comp.real,
                                                                             expected_add_comp_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(add_result_comp_with_fp_comp.imag,
                                                                             expected_add_comp_with_fp_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_comp_with_fp_comp.real,
                                                                             expected_sub_comp_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(sub_result_comp_with_fp_comp.imag,
                                                                             expected_sub_comp_with_fp_comp_imag))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_comp_with_fp_comp.real,
                                                                             expected_mult_comp_with_fp_comp_real))
                        self.assertTrue(
                            test_utils.fp_binary_instances_are_totally_equal(mult_result_comp_with_fp_comp.imag,
                                                                             expected_mult_comp_with_fp_comp_imag))


                        if fp_num_complex2 != 0.0:
                            div_result = fp_num_complex1 / fp_num_complex2
                            expected_div_real, expected_div_imag = fp_binary_complex_div(fp_num_real1, fp_num_imag1,
                                                                                         fp_num_real2, fp_num_imag2)
                            self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(div_result.real,
                                                                                             expected_div_real))
                            self.assertTrue(test_utils.fp_binary_instances_are_totally_equal(div_result.imag,
                                                                                             expected_div_imag))

                        imag2 += increment

                    real2 += increment

                imag1 += increment

            real1 += increment



    def testBitShifts(self):
        """Check effects of left & right shift operators."""

        # Small size
        format_fp = FpBinaryComplex(int(test_utils.get_small_type_size() / 2),
                                    int(test_utils.get_small_type_size() / 2))

        self.assertEqual(FpBinaryComplex(value=complex(1, -7), format_inst=format_fp) << 2, complex(4, -28))
        self.assertEqual(FpBinaryComplex(value=complex(1,-71 * 64) , format_inst=format_fp) >> 1, complex(0.5, -71 * 32))


        # Large size
        format_fp = FpBinaryComplex(test_utils.get_small_type_size(), test_utils.get_small_type_size())

        self.assertEqual(FpBinaryComplex(value=complex(1, -7), format_inst=format_fp) << 2, complex(4, -28))
        self.assertEqual(FpBinaryComplex(value=complex(1,-71 * 64) , format_inst=format_fp) >> 1, complex(0.5, -71 * 32))

        # Negative int_bits
        # Small size
        format_fp = FpBinaryComplex(-3, int(test_utils.get_small_type_size()) + 3)
        self.assertEqual(FpBinaryComplex(value=complex(0.0322265625, 0.0322265625), format_inst=format_fp) << 1,
                         complex(-0.060546875, -0.060546875))

        # Large size
        format_fp = FpBinaryComplex(-5, test_utils.get_small_type_size() + 10)
        self.assertEqual(FpBinaryComplex(value=complex(0.0068359375, 0.0068359375), format_inst=format_fp) << 2,
                         complex(-0.00390625, -0.00390625))

        # Negative frac_bits

        # Small size
        format_fp = FpBinaryComplex(int(test_utils.get_small_type_size()) + 10, -10)
        self.assertEqual(FpBinaryComplex(value=complex(1024.0, 1024.0), format_inst=format_fp) << 1,
                         complex(2048.0, 2048.0))

        # Large size
        format_fp = FpBinaryComplex(test_utils.get_small_type_size() + 10, -6)
        self.assertEqual(FpBinaryComplex(value=complex(192.0, 192.0), format_inst=format_fp) << 2,
                         complex(768.0, 768.0))


    def testOverflowModes(self):
        # =======================================================================
        # Wrapping

        # Losing MSBs, no wrapping required
        fpNum = FpBinaryComplex(6, 3, value=complex(3.875, -1.25))
        fpNum.resize((3, 3), overflow_mode=OverflowEnum.wrap)
        self.assertEqual(fpNum, complex(3.875, -1.25))

        # Losing MSB
        fpNum = FpBinaryComplex(5, 2, value=complex(15.75, -13.25))
        fpNum.resize((4, 2), overflow_mode=OverflowEnum.wrap)
        self.assertEqual(fpNum, complex(-0.25, 2.75))


        # =======================================================================
        # Saturation

        # Losing MSBs, no saturation required
        fpNum = FpBinaryComplex(6, 3, value=complex(3.25, -0.5))
        fpNum.resize((3, 3), overflow_mode=OverflowEnum.sat)
        self.assertEqual(fpNum, complex(3.25, -0.5))

        # Losing MSB, positive
        fpNum = FpBinaryComplex(5, 2, value=complex(15.75, -15.75))
        fpNum.resize((4, 2), overflow_mode=OverflowEnum.sat)
        self.assertEqual(fpNum, complex(7.75, -8.0))


        # =======================================================================
        # Exception

        # Losing MSBs, no wrapping required
        fpNum = FpBinaryComplex(6, 3, value=complex(-2.0, 3.875))
        try:
            fpNum.resize((3, 3), overflow_mode=OverflowEnum.excep)
        except FpBinaryOverflowException:
            self.fail()

        # Losing MSB

        fpNum = FpBinaryComplex(5, 2, value=15.75)
        try:
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.excep)
        except FpBinaryOverflowException:
            pass
        else:
            self.fail()

        fpNum = FpBinaryComplex(5, 2, value=-13.25j)
        try:
            fpNum.resize((4, 2), overflow_mode=OverflowEnum.excep)
        except FpBinaryOverflowException:
            pass
        else:
            self.fail()


    def testRoundingModes(self):
        # Direct Negative Infinity
        fpNum1 = FpBinaryComplex(2, 4, value=1.125)
        res = fpNum1.resize((2, 2), round_mode=RoundingEnum.direct_neg_inf)
        self.assertEqual(res, 1.0)

        fpNum1 = FpBinaryComplex(-4, 8, value=-0.0234375j)
        res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.direct_neg_inf)
        self.assertEqual(res, -0.03125j)

        # Direct towards zero
        fpNum1 = FpBinaryComplex(2, 4, value=1.125j)
        res = fpNum1.resize((2, 2), round_mode=RoundingEnum.direct_zero)
        self.assertEqual(res, 1.0j)

        fpNum1 = FpBinaryComplex(-4, 8, value=-0.0234375)
        res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.direct_zero)
        self.assertEqual(res, -0.015625)

        # Near positive infinity
        fpNum1 = FpBinaryComplex(2, 4, value=1.125)
        res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_pos_inf)
        self.assertEqual(res, 1.25)

        fpNum1 = FpBinaryComplex(-4, 8, value=-0.0234375j)
        res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_pos_inf)
        self.assertEqual(res, -0.015625j)

        # Near even
        fpNum1 = FpBinaryComplex(2, 4, value=1.125j)
        res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_even)
        self.assertEqual(res, 1.0j)

        fpNum1 = FpBinaryComplex(-4, 8, value=-0.0234375j)
        res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_even)
        self.assertEqual(res, -0.03125j)

        # Near zero
        fpNum1 = FpBinaryComplex(2, 4, value=1.125j)
        res = fpNum1.resize((2, 2), round_mode=RoundingEnum.near_zero)
        self.assertEqual(res, 1.0j)

        fpNum1 = FpBinaryComplex(-4, 8, value=-0.0234375)
        res = fpNum1.resize((-4, 6), round_mode=RoundingEnum.near_zero)
        self.assertEqual(res, -0.015625)


    def testStr(self):
        self.assertEqual(str(FpBinaryComplex(16, 16, value=23.125+54.5j)), str(23.125 + 54.5j))
        self.assertEqual(str(FpBinaryComplex(8, 6, value=-23.125 - 54.5j)), str(-23.125 - 54.5j))
        self.assertEqual(str(FpBinaryComplex(8, 6, value=23.125 - 54.5j)), str(23.125 - 54.5j))
        self.assertEqual(str(FpBinaryComplex(8, 6, value=-23.125 + 54.5j)), str(-23.125 + 54.5j))

    def testStrEx(self):
        self.assertEqual(FpBinaryComplex(16, 16, value=23.335345+54.2345353j).str_ex(),
                        fp_binary_complex_expected_str_ex_from_fp_binary(
                            FpBinary(16, 16, value=23.335345), FpBinary(16, 16, value=54.2345353)
                        ))
        self.assertEqual(FpBinaryComplex(17, 12, value=0.078955 - 54685.3334j).str_ex(),
                         fp_binary_complex_expected_str_ex_from_fp_binary(
                             FpBinary(17, 12, value=0.078955), FpBinary(17, 12, value=-54685.3334)
                         ))
        self.assertEqual(FpBinaryComplex(17, 12, value=-12.07574747478955 - 54685.0025j).str_ex(),
                         fp_binary_complex_expected_str_ex_from_fp_binary(
                             FpBinary(17, 12, value=-12.07574747478955), FpBinary(17, 12, value=-54685.0025)
                         ))

    def test_numpy_basic_math(self):
        base_fp_list = [FpBinaryComplex(17, 16, value=x) for x in range(-5, 4)]
        operand_list = [FpBinaryComplex(16, 16, value=x * 0.125) for x in range(1, 10)]
        expected_add = [op1 + op2 for op1, op2 in zip(base_fp_list, operand_list)]
        expected_sub = [op1 - op2 for op1, op2 in zip(base_fp_list, operand_list)]
        expected_mult = [op1 * op2 for op1, op2 in zip(base_fp_list, operand_list)]
        expected_div = [op1 / op2 for op1, op2 in zip(base_fp_list, operand_list)]
        expected_abs = [abs(op1) for op1 in operand_list]

        np_base_ar = np.array([copy.copy(x) for x in base_fp_list], dtype=object)
        np_operand_ar = np.array([copy.copy(x) for x in operand_list], dtype=object)

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

    #
    # def test_numpy_resize_vectorized(self):
    #     operand_list = [FpBinaryComplex(64, 64, value=x * 0.125) for x in range(-10, 10)]
    #     np_resize_func = np.vectorize(FpBinaryComplex.resize, excluded=[1])
    #
    #     expected = [copy.copy(x).resize((12, 1)) for x in operand_list]
    #
    #     for i in range(0, len(expected)):
    #         np_resized = np_resize_func(np.array(operand_list, dtype=object), (12,1))
    #         self.assertEqual(expected[i], np_resized[i])
    #         self.assertEqual(expected[i].format, np_resized[i].format)
    #
    # def test_numpy_convolve(self):
    #     coeffs_fp_list = [FpBinaryComplex(8, 8, value=x) for x in range(-5, 4)]
    #     input_fp_list = [FpBinaryComplex(8, 8, value=x * 0.125) for x in range(1, 10)]
    #
    #     coeffs_float_list = [float(x) for x in coeffs_fp_list]
    #     input_float_list = [float(x) for x in input_fp_list]
    #
    #     result_fp = np.convolve(np.array(coeffs_fp_list), np.array(input_fp_list))
    #     result_float = np.convolve(coeffs_float_list, input_float_list)
    #
    #     for i in range(0, len(result_fp)):
    #         self.assertEqual(float(result_fp[i]), result_float[i])
    #
    # def test_numpy_lfilter(self):
    #     b_fp_list = np.array([FpBinaryComplex(8, 8, value=x) for x in range(-5, 4)])
    #     a_fp_list = [FpBinaryComplex(8, 8, value=1.0)]
    #     input_fp_list = np.array([FpBinaryComplex(8, 8, value=x * 0.125) for x in range(1, 10)])
    #
    #     b_float_list = [float(x) for x in b_fp_list]
    #     a_float_list = [float(x) for x in a_fp_list]
    #     input_float_list = [float(x) for x in input_fp_list]
    #
    #     result_fp = signal.lfilter(b_fp_list, a_fp_list, input_fp_list)
    #     result_float = signal.lfilter(b_float_list, a_float_list, input_float_list)
    #
    #     for i in range(0, len(result_float)):
    #         self.assertEqual(result_fp[i], result_float[i])
    #         self.assertEqual(type(result_fp[i]), FpBinaryComplex)
    #
    # def test_numpy_lfilter_ic(self):
    #     b_fp_list = np.array([FpBinaryComplex(8, 8, value=x) for x in range(-5, 4)])
    #     a_fp_list = [FpBinaryComplex(8, 8, value=1.0)]
    #     input_fp_list = np.array([FpBinaryComplex(8, 8, value=x * 0.125) for x in range(1, 10)])
    #     initial_input_fp_list = np.array([FpBinaryComplex(8, 8, value=-3.5),
    #                                       FpBinaryComplex(8, 8, value=0.0625)])
    #     initial_y_fp_list = np.array([FpBinaryComplex(8, 8, value=0.125)])
    #
    #     b_float_list = [float(x) for x in b_fp_list]
    #     a_float_list = [float(x) for x in a_fp_list]
    #     input_float_list = [float(x) for x in input_fp_list]
    #     initial_input_float_list = [float(x) for x in initial_input_fp_list]
    #     initial_y_float_list = [float(x) for x in initial_y_fp_list]
    #
    #     result_fp, zf_fp = signal.lfilter(b_fp_list, a_fp_list, input_fp_list,
    #                                zi=signal.lfiltic(b_fp_list, a_fp_list, initial_y_fp_list, initial_input_fp_list))
    #     result_float, zf_float = signal.lfilter(b_float_list, a_float_list, input_float_list,
    #                                   zi=signal.lfiltic(b_float_list, a_float_list, initial_y_float_list, initial_input_float_list))
    #
    #     for i in range(0, len(result_fp)):
    #         self.assertEqual(result_fp[i], result_float[i])
    #         self.assertEqual(type(result_fp[i]), FpBinaryComplex)
    #
    #     for i in range(0, len(zf_float)):
    #         self.assertEqual(zf_fp[i], zf_float[i])
    #         self.assertEqual(type(zf_fp[i]), FpBinaryComplex)
    #
    # def testPickle(self):
    #     fp_list = [
    #         FpBinaryComplex(8, 8, value=0.01234),
    #         FpBinaryComplex(8, 8, value=-3.01234),
    #         FpBinaryComplex(8, 8, signed=False, value=0.01234),
    #         FpBinaryComplex(test_utils.get_small_type_size() - 2, 2, value=56.789),
    #         FpBinaryComplex(test_utils.get_small_type_size() - 2, 3, value=56.789),
    #         FpBinaryComplex(test_utils.get_small_type_size(),
    #                              test_utils.get_small_type_size(),
    #                              bit_field=(1 << (test_utils.get_small_type_size() + 5)) + 23),
    #         FpBinaryComplex(test_utils.get_small_type_size(),
    #                              test_utils.get_small_type_size(), signed=False,
    #                              bit_field=(1 << (test_utils.get_small_type_size() * 2)) - 1),
    #     ]
    #
    #
    #     for pickle_lib in pickle_libs:
    #
    #         unpickled = None
    #
    #         # Test saving of individual objects
    #         for test_case in fp_list:
    #             with open(pickle_test_file_name, 'wb') as f:
    #                 pickle_lib.dump(test_case, f, pickle_lib.HIGHEST_PROTOCOL)
    #
    #             with open(pickle_test_file_name, 'rb') as f:
    #                 unpickled = pickle_lib.load(f)
    #                 self.assertTrue(
    #                     test_utils.fp_binary_instances_are_totally_equal(test_case, unpickled))
    #
    #             # Test that the unpickled object is usable
    #             self.assertEqual(test_case + 1.0, unpickled + 1.0)
    #             self.assertEqual(test_case * 2.0, unpickled * 2.0)
    #
    #         # With append
    #         remove_pickle_file()
    #
    #         for test_case in fp_list:
    #             with open(pickle_test_file_name, 'ab') as f:
    #                 pickle_lib.dump(test_case, f, pickle_lib.HIGHEST_PROTOCOL)
    #
    #         unpickled = []
    #         with open(pickle_test_file_name, 'rb') as f:
    #             while True:
    #                 try:
    #                     unpickled.append(pickle_lib.load(f))
    #                 except:
    #                     break
    #
    #         for expected, loaded in zip(fp_list, unpickled):
    #             self.assertTrue(
    #                 test_utils.fp_binary_instances_are_totally_equal(expected, loaded))
    #
    #             # Test that the unpickled object is usable
    #             self.assertEqual(expected << 2, loaded << 2)
    #             self.assertEqual(expected >> 3, loaded >> 3)
    #
    #
    #         # Test saving of list of objects
    #
    #         with open(pickle_test_file_name, 'wb') as f:
    #             pickle_lib.dump(fp_list, f, pickle_lib.HIGHEST_PROTOCOL)
    #
    #         with open(pickle_test_file_name, 'rb') as f:
    #             unpickled = pickle_lib.load(f)
    #
    #         for expected, loaded in zip(fp_list, unpickled):
    #             self.assertTrue(
    #                 test_utils.fp_binary_instances_are_totally_equal(expected, loaded))



if __name__ == "__main__":
    unittest.main()
