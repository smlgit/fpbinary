import psutil
import time
import gc
import fpbinary
import numpy as np


def create_float_obj(quantity=1000000):
    l = [0.001 * j for j in range(quantity)]


def create_fpbinarycomplex_value_param_obj(quantity=1000000):
    l = [fpbinary.FpBinaryComplex(8, 8, value=1.05 + 0.25j) for j in range(quantity)]


def create_fpbinarycomplex_obj(quantity=1000000):
    l = [fpbinary.FpBinaryComplex(8, 8) for j in range(quantity)]


def create_fpbinarycomplex_numpy_obj(quantity=1000000):
    ar = np.array([fpbinary.FpBinaryComplex(8, 8) for j in range(quantity)])


def create_fpbinaryswitchable_obj_float_mode(quantity=1000000):
    l = [fpbinary.FpBinarySwitchable(False, float_value=quantity / 0.3) for j in range(quantity)]


def create_fpbinaryswitchable_obj_float_mode_npfloat(quantity=1000000):
    l = [fpbinary.FpBinarySwitchable(False, float_value=np.float64(quantity / 0.3)) for j in range(quantity)]


def create_fpbinaryswitchable_obj_fp_mode(quantity=1000000):
    l = [fpbinary.FpBinarySwitchable(False,
                                     fp_value=fpbinary.FpBinary(8, 8, True, quantity / 0.3),
                                     float_value=quantity / 0.3)
         for j in range(quantity)]


def resize_fpbinary_obj(quantity=1000000):
    l = [fpbinary.FpBinary(8, 8) for j in range(quantity)]
    for obj in l:
        obj.resize((4, 4))


def resize_fpbinarycomplex_obj(quantity=1000000):
    l = [fpbinary.FpBinaryComplex(8, 8) for j in range(quantity)]
    for obj in l:
        obj.resize((4, 4))


def basic_arith_fpbinary(quantity):
    ops_1 = [fpbinary.FpBinary(8, 8, value=0.1*j) for j in range(1, quantity + 1)]
    ops_2 = [fpbinary.FpBinary(8, 8, value=-0.5*j) for j in range(1, quantity + 1)]

    for i in range(len(ops_1)):
        add = ops_1[i] + ops_2[i]
        sub = ops_1[i] - ops_2[i]
        mult = ops_1[i] * ops_2[i]
        divide = ops_1[i] / ops_2[i]
        neg = -ops_1[i]
        ab = abs(ops_1[i])
        po = ops_1[i] ** 2
        ls = ops_1[i] << 3
        rs = ops_1[i] >> 3


def basic_arith_fpbinarycomplex(quantity):
    ops_1 = [fpbinary.FpBinaryComplex(8, 8, value=0.1*j - 0.1*j*1.0j) for j in range(1, quantity + 1)]
    ops_2 = [fpbinary.FpBinaryComplex(8, 8, value=-0.5*j + 0.2*j*1.0j) for j in range(1, quantity + 1)]

    for i in range(len(ops_1)):
        add = ops_1[i] + ops_2[i]
        sub = ops_1[i] - ops_2[i]
        mult = ops_1[i] * ops_2[i]
        divide = ops_1[i] / ops_2[i]
        neg = -ops_1[i]
        ab = abs(ops_1[i])
        po = ops_1[i]**2
        ls = ops_1[i] << 3
        rs = ops_1[i] >> 3


def fpbinarycomplex_conjugate(quantity):
    ops_1 = [fpbinary.FpBinaryComplex(8, 8, value=0.1*j - 0.1*j*1.0j) for j in range(1, quantity + 1)]

    for i in range(len(ops_1)):
        conj = ops_1[i].conjugate()


def fpbinary_array_ops(quantity):
    float_l = [0.001 * j for j in range(quantity)]

    fpbinary_l = fpbinary.fpbinary_list_from_array(float_l, 8, 8)
    fpbinary.array_resize(fpbinary_l, (4, 6))


def fpbinarycomplex_array_ops(quantity):
    complex_l = [0.001 * j - 0.02j * j for j in range(quantity)]

    fpbinarycomplex_l = fpbinary.fpbinarycomplex_list_from_array(complex_l, 8, 8)
    fpbinary.array_resize(fpbinarycomplex_l, (4, 6))


funcs = {
    'fpbinarycomplex': create_fpbinarycomplex_obj,
    'fpbinarycomplex_float_param': create_fpbinarycomplex_value_param_obj,
    'fpbinarycomplex_numpy': create_fpbinarycomplex_value_param_obj,
    'fpbinaryswitchable_float_mode': create_fpbinaryswitchable_obj_float_mode,
    'fpbinaryswitchable_float_mode_npfloat': create_fpbinaryswitchable_obj_float_mode_npfloat,
    'fpbinaryswitchable_fp_mode': create_fpbinaryswitchable_obj_fp_mode,
    'fpbinary_resize': resize_fpbinary_obj,
    'fpbinarycomplex_resize': resize_fpbinarycomplex_obj,
    'fpbinary_basic_arith': basic_arith_fpbinary,
    'fpbinarycomplex_basic_arith': basic_arith_fpbinarycomplex,
    'fpbinarycomplex_conjugate': fpbinarycomplex_conjugate,
    'fpbinary_array_ops': fpbinary_array_ops,
    'fpbinarycomplex_array_ops': fpbinarycomplex_array_ops,
}


def fpbinary_func_test(create_func_name, iterations=16, objects_per_iteration=1000000):

    proc = psutil.Process()

    start_mem = None

    for i in range(iterations):

        funcs[create_func_name](objects_per_iteration)

        if start_mem is None:
            start_mem = proc.memory_info().rss

    gc.collect()
    time.sleep(1)
    mem_increase = proc.memory_info().rss - start_mem

    if mem_increase > 0.5 * start_mem:
        raise MemoryError('Memory increased by {} \%'.format(mem_increase / start_mem * 100))


def run_all_leak_funcs(iterations=16, objects_per_iteration=100000):
    for func_name in funcs.keys():
        print(func_name + '...')
        fpbinary_func_test(func_name, iterations, objects_per_iteration)

    print('No memory leaks detected.')


def main():
    run_all_leak_funcs()


if __name__ == '__main__':
    main()
