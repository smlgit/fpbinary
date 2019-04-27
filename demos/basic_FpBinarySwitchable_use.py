import random
from fpbinary import FpBinary, FpBinarySwitchable, OverflowEnum, RoundingEnum


fp_mode = True


print('An instance can be created with fixed point and float values: {}'.format(
      FpBinarySwitchable(fp_mode=fp_mode, fp_value=FpBinary(8, 8, value=6.7), float_value=6.7)
      ))


print('FpBinarySwitchable looks like FpBinary:')
num1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=6.7), float_value=6.7)
num2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=7.3), float_value=6.7)
print('{} + {} == {}   {} * {} = {}'.format(num1, num2, num1 + num2, num1, num2, num1 * num2))


print('FpBinarySwitchable plays well with FpBinary:')
num1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=6.7), float_value=6.7)
num2 = FpBinary(8, 8, value=7.3)
print('{} + {} == {}   {} * {} = {} (type: {})'.format(
    num1, num2, num1 + num2, num1, num2, num1 * num2, type(num1 * num2)))


print('FpBinarySwitchable is so called because you can switch operation between fixed and floating point, '
      'simply by flicking a constructor variable. Operations that aren\'t supported by normal numberical '
      'objects (like resize) can still be called in floating point mode without an exception:')

for i in range(0, 2):
    num1 = FpBinarySwitchable(fp_mode=fp_mode, fp_value=FpBinary(8, 8, value=6.7), float_value=6.7)
    print('fp_mode: {}  Resized: {}'.format(fp_mode, num1.resize((2, 7))))
    fp_mode = not fp_mode


print('FpBinarySwitchable can also be used to track min and max values while in floating point mode by '
      'using the value property:')

num1 = FpBinarySwitchable(fp_mode=False, fp_value=FpBinary(8, 8, value=0.0), float_value=0.0)
for i in range(0, 5):
    num1.value = FpBinary(8, 8, value=random.uniform(-1.0, 1.0))
    print('    Value: {}'.format(num1))

print('min value: {}  max value: {}'.format(num1.min_value, num1.max_value))


print('The value property can be set to other FpBinarySwitchable instances too:')
num1 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=0.0), float_value=0.0)
num2 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=5.0), float_value=5.0)
num3 = FpBinarySwitchable(fp_mode=True, fp_value=FpBinary(8, 8, value=0.0), float_value=0.0)
num3.value = num1 + num2
print('{} + {} = {}'.format(type(num1), type(num2), type(num3)))
