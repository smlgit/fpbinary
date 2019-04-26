import os.path
import glob
import cProfile
import pstats
from argparse import ArgumentParser
from fpbinary import FpBinary, FpBinarySwitchable, _FpBinarySmall, _FpBinaryLarge, OverflowEnum, RoundingEnum

NUM_OBJECT_CREATIONS = 100000
NUM_MULTIPLIES = 100000
NUM_ADDS = 100000
NUM_RESIZES = 100000


def build_create_params_float(int_bits, frac_bits, val):
    return (val,)


def build_create_params_fpbinary(int_bits, frac_bits, val):
    return (int_bits, frac_bits, True, val)


def build_create_params_fpswitchable_fpmode(int_bits, frac_bits, val):
    return (True,
            FpBinary(int_bits=int_bits, frac_bits=frac_bits, signed=True, value=val),
            0.0)


def build_create_params_fpswitchable_non_fpmode(int_bits, frac_bits, val):
    return (False,
            FpBinary(int_bits=int_bits, frac_bits=frac_bits, signed=True, value=val),
            val)


def test_create(num_of_creates, class_type, build_param_func, int_bits, frac_bits, value):
    params = build_param_func(int_bits, frac_bits, value)
    for i in range(0, num_of_creates):
        res = class_type(*params)


def test_multiply(num_of_multiplies, class_type, build_param_func,
                  int_bits, frac_bits, val1, val2):
    num1 = class_type(*build_param_func(int_bits, frac_bits, val1))
    num2 = class_type(*build_param_func(int_bits, frac_bits, val2))

    for i in range(0, num_of_multiplies):
        res = num1 * num2


def test_add(num_of_adds, class_type, build_param_func,
             int_bits, frac_bits, val1, val2):
    num1 = class_type(*build_param_func(int_bits, frac_bits, val1))
    num2 = class_type(*build_param_func(int_bits, frac_bits, val2))

    for i in range(0, num_of_adds):
        res = num1 + num2


def test_resize(num_of_resizes, class_type, build_param_func,
                start_int_bits, start_frac_bits, resize_int_bits,
                resize_frac_bits, start_val, overflow_mode, round_mode):
    num1 = class_type(*build_param_func(start_int_bits, start_frac_bits, start_val))

    if hasattr(num1, 'resize') == False:
        return

    resize_tup = (resize_int_bits, resize_frac_bits)

    for i in range(0, num_of_resizes):
        num1.resize(resize_tup, overflow_mode=overflow_mode, round_mode=round_mode)


types_table = {
    'fpbinary': {'class': FpBinary, 'build_params_func': build_create_params_fpbinary},
    'fpbinaryswitchable_fpmode': {'class': FpBinarySwitchable,
                                  'build_params_func': build_create_params_fpswitchable_fpmode},
    'fpbinaryswitchable_non_fpmode': {'class': FpBinarySwitchable,
                                      'build_params_func': build_create_params_fpswitchable_non_fpmode},
    'fpbinarysmall': {'class': _FpBinarySmall, 'build_params_func': build_create_params_fpbinary},
    'fpbinarylarge': {'class': _FpBinaryLarge, 'build_params_func': build_create_params_fpbinary},
    'float': {'class': float, 'build_params_func': build_create_params_float},
}


def run_type_test(type_name):

    # Create test
    test_create(NUM_OBJECT_CREATIONS, types_table[type_name]['class'],
                types_table[type_name]['build_params_func'],
                int_bits=8, frac_bits=8, value=66.666)

    # Multiply test
    test_multiply(NUM_MULTIPLIES, types_table[type_name]['class'],
                   types_table[type_name]['build_params_func'],
                   int_bits = 3, frac_bits = 6, val1 = 1.23, val2 = 45.7)

    # Add test
    test_add(NUM_ADDS, types_table[type_name]['class'],
             types_table[type_name]['build_params_func'],
             int_bits=3, frac_bits=6, val1=-3.3445322, val2=0.035)

    # Resize test
    test_resize(NUM_RESIZES, types_table[type_name]['class'],
                types_table[type_name]['build_params_func'],
                start_int_bits=16, start_frac_bits=16, resize_int_bits=8,
                resize_frac_bits=8, start_val=-23343.3445322, overflow_mode=OverflowEnum.wrap,
                round_mode=RoundingEnum.near_pos_inf)


def main(types_list, base_filename=''):
    for type_name in types_list:
        filename = '{}_{}'.format(base_filename, type_name) if base_filename else None

        cProfile.run('run_type_test(\'{}\')'.format(type_name), filename=filename, sort='tottime')

        if filename:
            p = pstats.Stats(filename)
            p.strip_dirs().sort_stats('tottime').print_stats()


if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('-t', '--type', type=str, help='One of {}, or \'all\'.'.format(types_table.keys()))
    parser.add_argument('--base-fname', type=str, default=None)
    parser.add_argument('--print-data', action='store_true')

    args = parser.parse_args()

    if args.print_data and args.base_fname:
        for filename in glob.glob('{}*'.format(args.base_fname)):
            p = pstats.Stats(filename)
            p.strip_dirs().sort_stats('tottime').print_stats()
    elif args.type != '':
        if args.type == 'all':
            types_list = types_table.keys()
        else:
            types_list = [args.type]

        main(types_list=types_list, base_filename=args.base_fname)
