#!/usr/bin/python
# Unit-tests for FpBinary Python module
# SML, some tests adapted from RW Penney's Simple Fixed Point module

import sys, unittest, copy, pickle, os, time
import numpy as np
import scipy.signal as signal
import tests.test_utils as test_utils
from fpbinary import FpBinary, FpBinaryComplex, OverflowEnum, RoundingEnum, FpBinaryOverflowException
from fpbinary import fpbinarycomplex_list_from_array, array_resize

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

class FpBinaryComplexTest(unittest.TestCase):
    def setUp(self):
        self.fp_zero = FpBinaryComplex(1, 0, value=0.0)
        self.fp_one = FpBinaryComplex(2, 0, value=1.0)
        self.fp_minus_one = FpBinaryComplex(2, 0, value=-1.0)
        self.fp_two = FpBinaryComplex(3, 0, value=2.0)
        
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

    def testCreate(self):
        # On value only
        expected_real = FpBinary(value=-6.5634765625)
        expected_imag = FpBinary(value=0.06965625)
        set_fp_binary_instances_to_best_format(expected_real, expected_imag)
        fp_complex_num = FpBinaryComplex(value=-6.5634765625+0.06965625j)
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.real, expected_real))
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.imag, expected_imag))

        # On value with bits explicit
        expected_real = FpBinary(16, 16, value=-6.54)
        expected_imag = FpBinary(16, 16, value=0.00156)
        fp_complex_num = FpBinaryComplex(16, 16, value=-6.54 + 0.00156j)
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.real, expected_real))
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.imag, expected_imag))

        # On fp value only
        expected_real = FpBinary(value=-6.54)
        expected_imag = FpBinary(value=0.00156)
        fp_complex_num = FpBinaryComplex(real_fp_binary=expected_real,
                                         imag_fp_binary=expected_imag)
        set_fp_binary_instances_to_best_format(expected_real, expected_imag)
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.real, expected_real))
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.imag, expected_imag))

        # On bit_fields
        expected_real = FpBinary(value=-6.54)
        expected_imag = FpBinary(value=0.00156)
        set_fp_binary_instances_to_best_format(expected_real, expected_imag)
        fp_complex_num = FpBinaryComplex(real_bit_field=expected_real.bits_to_signed(),
                                         imag_bit_field=expected_imag.bits_to_signed(),
                                         format_inst=expected_real)
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.real, expected_real))
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(fp_complex_num.imag, expected_imag))

        # Check castable to float objects work
        FpBinaryComplex(value=np.complex64(-6.5634765625 + 0.06965625j))
        FpBinaryComplex(value=np.float64(-6.5634765625) + np.float64(0.06965625)*1j)
        FpBinaryComplex(value=np.float64(-6.5634765625))

    def testCreateParamsWrong(self):
        # These parameter test cases should raise an exception
        params_test_cases = [
            # int_bits is float
            ([4.2, 3], {}),
            # frac_bits is float
            ([4, 3.2], {}),
            # int_bits but no frac_bits
            ([], {'int_bits': 3}),
            # frac_bits but no frac_bits
            ([], {'frac_bits': 3}),
            # real_fp_binary but no imag
            ([5, 5], {'real_fp_binary': FpBinary(4, 4, value=2.0)}),
            # imag_fp_binary but no real
            ([5, 5], {'imag_fp_binary': FpBinary(4, 4, value=2.0)}),
        ]

        for test_case in params_test_cases:
            try:
                fpNum = FpBinaryComplex(*test_case[0], **test_case[1])
            except TypeError:
                pass
            except ValueError:
                pass
            else:
                self.fail('Failed on test case {}'.format(test_case))

    def testFormatProperty(self):
        fpNum = FpBinaryComplex(2, 5, value=1.5+8.9j)
        self.assertTrue(fpNum.format == (2, 5))

        fpNum = FpBinaryComplex(-200, 232)
        self.assertTrue(fpNum.format == (-200, 232))

        fpNum = FpBinaryComplex(201, -190)
        self.assertTrue(fpNum.format == (201, -190))

    def testBoolConditions(self):
        """Values used in boolean expressions should behave as true/false"""
        if FpBinaryComplex(2, 2, value=0):
            self.fail()
        if FpBinaryComplex(2, 2, value=0.0+0.0j):
            self.fail()
        if FpBinaryComplex(2, 2, value=1):
            pass
        else:
            self.fail()

    def testComplexCasts(self):
        # Small type
        for i in range(-40, 40):
            x = i / 8.0 - i * 1.0j / 8.0
            self.assertEqual(x, complex(FpBinaryComplex(4, 16, value=x)))

        # Large type
        # Float comparison should be ok as long as the value is small enough
        for i in range(-40, 40):
            x = i / 8.0 - i * 1.0j / 8.0
            self.assertEqual(x, complex(FpBinaryComplex(test_utils.get_small_type_size(), 16,
                                                        value=x)))

    def testImmutable(self):
        """Arithmetic operations on object should not alter orignal value"""
        scale_real = 0.297
        scale_imag = -0.111
        for i in range(-8, 8):
            orig = FpBinaryComplex(4, 5, value=i * scale_real + i * scale_imag * 1.0j)

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

    def testBasicMath(self):
        """
        Add, Sub, Mult, Div operations between FpBinaryComplex types.
        Also checks between an FpBinaryComplex type and complex, float, int and FpBinary types."""

        total_bits = 4
        frac_bits = 2
        int_bits = total_bits - frac_bits

        increment = 1.0 / 2**frac_bits
        min_val = -2.0**(int_bits - 1)
        max_val = 2.0**(int_bits - 1) - 1

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

    def testPower(self):
        # When the first operand is FpBinary, only support squaring.
        self.assertEqual(FpBinaryComplex(value=4.5 - 0.65j) ** long(2),
                         FpBinaryComplex(value=4.5 - 0.65j) * FpBinaryComplex(value=4.5 - 0.65j))
        self.assertEqual(FpBinaryComplex(value=-4.5 + 0.65j) ** 2.0,
                         FpBinaryComplex(value=-4.5 + 0.65j) * FpBinaryComplex(value=-4.5 + 0.65j))
        self.assertEqual(
            FpBinaryComplex(value=-4.5 + 0.65j) ** FpBinaryComplex(value=2.0 + 0.0j),
            FpBinaryComplex(value=-4.5 + 0.65j) * FpBinaryComplex(value=-4.5 + 0.65j))
        try:
            FpBinaryComplex(value=-4.5 + 0.65j) ** 2.125
        except TypeError:
            pass
        except Exception as e:
            self.fail(e)

        # When the first operand is a native type, should work as normal if the imaginary part is zero
        self.assertEqual(0.11111 ** FpBinaryComplex(16, 16, value=0.0625),
                         0.11111 ** 0.0625)
        self.assertEqual(0.11111 ** FpBinaryComplex(16, 16, value=-0.0625 + 0.0j),
                         0.11111 ** -0.0625)
        try:
            0.11111 ** FpBinaryComplex(16, 16, value=-0.0625 + 0.125j)
        except TypeError:
            pass
        except Exception as e:
            self.fail(e)

        # Interoperation with FpBinary
        self.assertEqual(FpBinary(value=4.875, signed=True) ** FpBinaryComplex(16, 16, value=2.0),
                         FpBinary(value=4.875, signed=True) * FpBinary(value=4.875, signed=True))
        self.assertEqual(FpBinaryComplex(value=4.875 - 0.0006j) ** FpBinary(3, 2, value=2.0, signed=True),
                         FpBinaryComplex(value=4.875 - 0.0006j) * FpBinaryComplex(value=4.875 - 0.0006j))


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

    def testResizeTotalBitsLTZero(self):
        # These parameter test cases should raise an exception
        params_test_cases = [

            # total bits is less than 1
            (4, -5),
            (-3, 3),
            (-7, 6),
            (-3, 3),
            (-7, 6),
            (3, -3),
            (7, -8),
            (3, -78),
            (7, -11),
        ]

        for test_case in params_test_cases:
            try:
                fpNum = FpBinaryComplex(5, 6, value=6.5 - 3.125j)
                fpNum.resize(test_case)
            except ValueError:
                pass
            else:
                self.fail('Failed on test case {}'.format(test_case))

    def testConjugate(self):
        self.assertEqual(FpBinaryComplex(5, 6, value=6.5 - 3.125j).conjugate(),
                         FpBinaryComplex(5, 6, value=6.5 + 3.125j))
        self.assertEqual(FpBinaryComplex(5, 6, value=6.5 + 3.125j).conjugate(),
                         FpBinaryComplex(5, 6, value=6.5 - 3.125j))

        # Check format is as expected
        fp_comp_num = FpBinaryComplex(5, 6, value=6.5 - 3.125j)
        expected_conj = FpBinaryComplex(6, 6, value=6.5 + 3.125j)
        conj = fp_comp_num.conjugate()
        self.assertTrue(test_utils.fp_binary_complex_fields_equal(
            expected_conj, conj
        ))

        # Check zero imag
        self.assertEqual(FpBinaryComplex(100, 101, value=6.5 - 0.0j).conjugate(),
                         FpBinaryComplex(5, 6, value=6.5 + 0.0j))
        self.assertEqual(FpBinaryComplex(100, 101, value=6.5 + 0.0j).conjugate(),
                         FpBinaryComplex(5, 6, value=6.5 + 0.0j))

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
        op1_fp_list = [FpBinaryComplex(17, 16, value=complex(x[0], x[1])) for x in np.linspace([-100.0, 1.0], [100.0, 2.0], 128)]
        op2_fp_list = [FpBinaryComplex(17, 16, value=complex(x[0], x[1])) for x in
                       np.linspace([100.0, -1.0], [10000.0, 2.0], 128)]
        expected_add = [op1 + op2 for op1, op2 in zip(op1_fp_list, op2_fp_list)]
        expected_sub = [op1 - op2 for op1, op2 in zip(op1_fp_list, op2_fp_list)]
        expected_mult = [op1 * op2 for op1, op2 in zip(op1_fp_list, op2_fp_list)]
        expected_div = [op1 / op2 for op1, op2 in zip(op1_fp_list, op2_fp_list)]
        expected_abs = [abs(op1) for op1 in op2_fp_list]

        np_op1_ar = np.array([copy.copy(x) for x in op1_fp_list], dtype=object)
        np_op2_ar = np.array([copy.copy(x) for x in op2_fp_list], dtype=object)

        np_add = np_op1_ar + np_op2_ar
        np_sub = np_op1_ar - np_op2_ar
        np_mult = np_op1_ar * np_op2_ar
        np_div = np_op1_ar / np_op2_ar
        np_abs = abs(np_op2_ar)

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

    def test_numpy_astype_complex(self):
        complex_array = np.array([(1 >> 12) * -i*1.0 + (1 >> 5) * i*1.0j for i in range(16)])
        fp_complex_array = np.array([FpBinaryComplex(16, 16, value=x) for x in complex_array], dtype=object)
        converted_array = fp_complex_array.astype(complex)
        self.assertTrue((converted_array == complex_array).all())
        self.assertEqual(type(complex_array[0]), type(converted_array[0]))

    def test_numpy_resize_vectorized(self):
        operand_list = [FpBinaryComplex(64, 64, value=complex(x[0], x[1]))
                        for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)]
        np_resize_func = np.vectorize(FpBinaryComplex.resize, excluded=[1])

        expected = [copy.copy(x).resize((12, 1)) for x in operand_list]

        for i in range(0, len(expected)):
            np_resized = np_resize_func(np.array(operand_list, dtype=object), (12,1))
            self.assertEqual(expected[i], np_resized[i])
            self.assertEqual(expected[i].format, np_resized[i].format)

    def test_create_from_list(self):
        operand_list = [complex(x[0], x[1]) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)]
        expected_list = [FpBinaryComplex(8, 6, value=x) for x in operand_list]

        # Explicit int and frac bits format
        actual_list = fpbinarycomplex_list_from_array(operand_list, 8, 6)

        for i in range(len(expected_list)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_list[i], actual_list[i]
            ))

        # Use of FpBinary instance for format
        actual_list = fpbinarycomplex_list_from_array(operand_list, format_inst=expected_list[0])

        for i in range(len(expected_list)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_list[i], actual_list[i]
            ))

        # Multi dimensional list
        operand_list = [
            [complex(x[0], x[1]) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)],
            [complex(x[0], x[1]) for x in np.linspace([-100.0, 1.0], [1000.0, 2.0], 16)]
        ]
        expected_list = [
            [FpBinaryComplex(8, 6, value=x) for x in operand_list[0]],
            [FpBinaryComplex(8, 6, value=x) for x in operand_list[1]]
        ]
        actual_list = fpbinarycomplex_list_from_array(operand_list, 8, 6)

        for row in range(2):
            for i in range(len(expected_list[row])):
                self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                    expected_list[row][i], actual_list[row][i]))

    def test_list_resize(self):
        operand_list = [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)]
        expected_list = np.array(
            [x.resize((2, 1), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf)
             for x in copy.deepcopy(operand_list)],
            dtype=object)
        array_resize(operand_list, (2, 1), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf)

        for i in range(len(expected_list)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_list[i], operand_list[i]
            ))

        # Multi dimensional list
        operand_list = [
            [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)],
            [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([-100.0, 1.0], [1000.0, 2.0], 16)]
        ]

        expected_list = [[], []]

        for row in range(len(operand_list)):
            for i in range(len(operand_list[row])):
                expected_list[row].append(copy.copy(operand_list[row][i]).resize(
                    (1, 5), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf))

        array_resize(operand_list, (1, 5), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf)

        for row in range(len(expected_list)):
            for i in range(len(expected_list[row])):
                self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                    expected_list[row][i], operand_list[row][i]
                ))

    def test_numpy_create_from_array(self):
        operand_array = [complex(x[0], x[1]) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)]
        expected_list = [FpBinaryComplex(16, 16, value=x) for x in operand_array]
        expected_numpy_array = np.array(expected_list, dtype=object)

        # Explicit int and frac bits format
        actual_list = fpbinarycomplex_list_from_array(operand_array, 16, 16)
        actual_numpy_array = np.array(actual_list, dtype=object)

        for i in range(len(expected_list)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_list[i], actual_list[i]
            ))
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_numpy_array[i], actual_numpy_array[i]
            ))

        # Use of FpBinary instance for format
        actual_list = fpbinarycomplex_list_from_array(operand_array, format_inst=expected_list[0])
        actual_numpy_array = np.array(actual_list, dtype=object)

        for i in range(len(expected_list)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_list[i], actual_list[i]
            ))
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_numpy_array[i], actual_numpy_array[i]
            ))

        # Multi dimensional ndarray
        operand_array = np.array([
            [complex(x[0], x[1]) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)],
            [complex(x[0], x[1]) for x in np.linspace([-100.0, 1.0], [1000.0, 2.0], 16)]
        ])
        expected_numpy_array = np.zeros(operand_array.shape, dtype=object)
        expected_numpy_array[0,] = np.array(
            [FpBinaryComplex(16, 16, value=x) for x in operand_array[0,]])
        expected_numpy_array[1,] = np.array(
            [FpBinaryComplex(16, 16, value=x) for x in operand_array[1,]])
        actual_numpy_array = np.array(fpbinarycomplex_list_from_array(operand_array, 16, 16))

        for i in range(len(expected_numpy_array.flat)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_numpy_array.flat[i], actual_numpy_array.flat[i]
            ))

    def test_numpy_array_resize(self):
        operand_array = np.array(
            [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)])
        expected_numpy_array = np.array(
            [x.resize((1, 5), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf) for x in
             copy.deepcopy(operand_array)],
            dtype=object)
        array_resize(operand_array, (1, 5), overflow_mode=OverflowEnum.sat, round_mode=RoundingEnum.direct_neg_inf)

        for i in range(len(expected_numpy_array)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_numpy_array[i], operand_array[i]
            ))

        # Multi dimensional ndarray
        operand_array = np.array([
            [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([100.0, -1.0], [10000.0, 2.0], 16)],
            [FpBinaryComplex(16, 16, value=complex(x[0], x[1])) for x in np.linspace([-100.0, 1.0], [1000.0, 2.0], 16)]
        ])

        expected_numpy_array = np.zeros(operand_array.shape, dtype=object)
        expected_numpy_array[0,] = np.array(
            [x.resize((1, 5), overflow_mode=OverflowEnum.wrap, round_mode=RoundingEnum.near_pos_inf)
             for x in copy.deepcopy(operand_array[0,])], dtype=object)
        expected_numpy_array[1,] = np.array(
            [x.resize((1, 5), overflow_mode=OverflowEnum.wrap, round_mode=RoundingEnum.near_pos_inf)
             for x in copy.deepcopy(operand_array[1,])], dtype=object)
        array_resize(operand_array, (1, 5), overflow_mode=OverflowEnum.wrap, round_mode=RoundingEnum.near_pos_inf)

        for i in range(len(expected_numpy_array.flat)):
            self.assertTrue(test_utils.fp_binary_complex_fields_equal(
                expected_numpy_array.flat[i], operand_array.flat[i]
            ))

    def test_numpy_convolve(self):
        coeffs_fp_list = [FpBinaryComplex(8, 8, value=complex(x[0], x[1]))
                          for x in np.linspace([1.0, -1.0], [3.0, 1.0], 16, endpoint=False)]
        input_fp_list = [FpBinaryComplex(8, 8, value=complex(x[0], x[1]))
                          for x in np.linspace([-1.0, 1.0], [1.0, 3.0], 16, endpoint=False)]

        coeffs_float_list = [complex(float(x.real), float(x.imag)) for x in coeffs_fp_list]
        input_float_list = [complex(float(x.real), float(x.imag)) for x in input_fp_list]

        result_fp = np.convolve(np.array(coeffs_fp_list), np.array(input_fp_list))
        result_float = np.convolve(coeffs_float_list, input_float_list)

        for i in range(0, len(result_fp)):
            self.assertEqual(result_fp[i], result_float[i])

    def test_numpy_lfilter(self):
        b_fp_list = [FpBinaryComplex(8, 8, value=complex(x[0], x[1]))
                          for x in np.linspace([1.0, -1.0], [3.0, 1.0], 16, endpoint=False)]
        input_fp_list = [FpBinaryComplex(8, 8, value=complex(x[0], x[1]))
                          for x in np.linspace([-1.0, 1.0], [1.0, 3.0], 16, endpoint=False)]
        a_fp_list = [FpBinaryComplex(8, 8, value=1.0+0.0j)]

        b_float_list = [complex(float(x.real), float(x.imag)) for x in b_fp_list]
        a_float_list = [complex(float(x.real), float(x.imag)) for x in a_fp_list]
        input_float_list = [complex(float(x.real), float(x.imag)) for x in input_fp_list]

        result_fp = signal.lfilter(b_fp_list, a_fp_list, input_fp_list)
        result_float = signal.lfilter(b_float_list, a_float_list, input_float_list)

        for i in range(0, len(result_float)):
            self.assertEqual(result_fp[i], result_float[i])
            self.assertEqual(type(result_fp[i]), FpBinaryComplex)

    def test_numpy_lfilter_ic(self):
        b_fp_list = [FpBinaryComplex(8, 8, value=complex(x[0], x[1]))
                     for x in np.linspace([1.0, -1.0], [3.0, 1.0], 16, endpoint=False)]
        input_fp_list = [FpBinaryComplex(9, 9, value=complex(x[0], x[1]))
                         for x in np.linspace([-1.0, 1.0], [1.0, 3.0], 16, endpoint=False)]
        a_fp_list = [FpBinaryComplex(8, 8, value=1.0 + 0.0j)]
        initial_input_fp_list = np.array([FpBinaryComplex(8, 8, value=complex(-3.5, 0.5)),
                                          FpBinaryComplex(8, 8, value=0.0625-0.875j)])
        initial_y_fp_list = np.array([FpBinaryComplex(8, 8, value=0.125+5.5j)])
        b_float_list = [complex(float(x.real), float(x.imag)) for x in b_fp_list]
        a_float_list = [complex(float(x.real), float(x.imag)) for x in a_fp_list]
        input_float_list = [complex(float(x.real), float(x.imag)) for x in input_fp_list]
        initial_input_float_list = [complex(float(x.real), float(x.imag)) for x in initial_input_fp_list]
        initial_y_float_list = [complex(float(x.real), float(x.imag)) for x in initial_y_fp_list]

        result_fp, zf_fp = signal.lfilter(b_fp_list, a_fp_list, input_fp_list,
                                   zi=signal.lfiltic(b_fp_list, a_fp_list, initial_y_fp_list, initial_input_fp_list))
        result_float, zf_float = signal.lfilter(b_float_list, a_float_list, input_float_list,
                                      zi=signal.lfiltic(b_float_list, a_float_list, initial_y_float_list, initial_input_float_list))

        for i in range(0, len(result_fp)):
            self.assertEqual(result_fp[i], result_float[i])
            self.assertEqual(type(result_fp[i]), FpBinaryComplex)

        for i in range(0, len(zf_float)):
            self.assertEqual(zf_fp[i], zf_float[i])
            self.assertEqual(type(zf_fp[i]), FpBinaryComplex)

    def testPickle(self):
        fp_list = [
            FpBinaryComplex(8, 8, value=0.01234+1.57j),
            FpBinaryComplex(8, 8, value=-3.01234+8.999j),
            FpBinaryComplex(test_utils.get_small_type_size() - 2, 2, value=56.789-0.5j),
            FpBinaryComplex(test_utils.get_small_type_size() - 2, 3, value=56.789-0.5j),
            FpBinaryComplex(test_utils.get_small_type_size(),
                                 test_utils.get_small_type_size(),
                                 real_bit_field=(1 << (test_utils.get_small_type_size() + 5)) + 23,
                            imag_bit_field=(1 << (test_utils.get_small_type_size() + 5)) + 21),
            FpBinaryComplex(test_utils.get_small_type_size(),
                                 test_utils.get_small_type_size(),
                                 real_bit_field=(1 << (test_utils.get_small_type_size() * 2)) - 1,
                            imag_bit_field=(1 << (test_utils.get_small_type_size() * 2)) - 1),
        ]

        for pickle_lib in pickle_libs:

            # Test saving of individual objects
            for test_case in fp_list:
                with open(pickle_test_file_name, 'wb') as f:
                    pickle_lib.dump(test_case, f, pickle_lib.HIGHEST_PROTOCOL)

                with open(pickle_test_file_name, 'rb') as f:
                    unpickled = pickle_lib.load(f)
                    self.assertTrue(
                        test_utils.fp_binary_complex_fields_equal(test_case, unpickled))

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
                    test_utils.fp_binary_complex_fields_equal(expected, loaded))

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
                    test_utils.fp_binary_complex_fields_equal(expected, loaded))



if __name__ == "__main__":
    unittest.main()
