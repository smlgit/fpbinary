# `fpbinary`

## Overview

fpbinary is a binary fixed point library for Python. It is written as an extension module for the CPython implementation of Python.

fpbinary was created with FAST simulation of math-intensive systems destined for digital hardware (e.g. signal processing) in mind. While Python has great support for signal processing functions, there is no offical fixed point library. Implementaions written completely in Python tend to be frustratingly slow, so fpbinary is an attempt to make fixed point simulation of large, complex hardware systems in Python viable.

## Installation

Installation is currently only via source download and build. You require an installation of git, python >= 2.7.12 and a C99 compliant compiler.

Clone the fpbinary repository and run the setup script: 


```bash
git clone https://github.com/smlgit/fpbinary.git

cd fpbinary
python setup install
```

The library has been tested (via pyenv) on versions 2.7.12, 3.5.2 and 3.6.5.

## Use

fpbinary provides two main objects - `FpBinary` and `FpBinarySwitchable`. The best way to learn how they work is to read the help documentation:

```python
from fpbinary import FpBinary, FpBinarySwitchable
help(FpBinary)
help(FpBinarySwitchable)
```

There are also some useful [demos](./demos).
 
 Below is a very brief introduction to the objects.
 
 ## Object Introduction

#### `FpBinary`

This object represents a real number with a specified number of integer and fractional bits. View the documentation via:

Some basic usage:

```python
>>> fp_num = FpBinary(int_bits=4, frac_bits=4, signed=True, value=2.5)
>>> fp_num
2.5
>>> fp_num.format
(4, 4)
>>> fp_num * 2.0
5.0
>>> fp_num.resize((1,4))
0.5

```

#### `FpBinarySwitchable`

This object is intended to be used in simulation code where the user wants to switch between fixed and floating point math operation. It allows a simulation to be coded with fixed point method calls (like resize()) but to be run in floating point mode at the flick of a constructor switch:

```python
def dsp_sim(fp_mode):
    num1 = FpBinarySwitchable(fp_mode=fp_mode, fp_value=FpBinary(8, 8, value=6.7), float_value=6.7)
    num2 = FpBinary(16, 16, value=0.005)
    
    num3 = (num1 * num2).resize((8, 8), overflow_mode=OverflowEnum.wrap,
                                        rounding_mode=RoundingEnum.direct_neg_inf)
    
    # Do other stuff...
    
    return num3
    
```

`FpBinarySwitchable` also provides the `value` property. This can be set to fixed or floating point objects (depending on the mode) and the min and max values over the lifetime of the object are tracked. This gives the designer an indication of the required fixed point format of the various data points in their design:

```python

inp = FpBinarySwitchable(fp_mode=fp_mode, fp_value=FpBinary(8, 8, value=0.0), float_value=0.0)
scaled = FpBinarySwitchable(fp_mode=fp_mode, fp_value=FpBinary(16, 16, value=0.0), float_value=0.0)

def some_dsp_next_sample(sample):
    inp.value = sample.resize(format_inst=inp)
    scaled.value = inp * scale_factor
    
    # ....
    return val
    
def run(fp_mode):
    # call some_dsp_next_sample a whole heap
    
    return inp.min_value, inp.max_value, scaled.min_value, scaled.max_value
```







