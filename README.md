# `fpbinary`

## Table of Contents
1. [Introduction](#intro)
1. [Features](#features)
1. [Installation](#install)
1. [Use](#use)
1. [Development](#dev)
1. [Enhancements](#enhancements)

<a name="intro"/>

## Introduction

fpbinary is a binary fixed point library for Python. It is written as an extension module for the CPython implementation of Python.

fpbinary was created with **_fast_** simulation of math-intensive systems destined for digital hardware (e.g. signal processing) in mind. While Python has great support for signal processing functions, there is no offical fixed point library. Implementaions written completely in Python tend to be frustratingly slow, so fpbinary is an attempt to make fixed point simulation of large, complex hardware systems in Python viable.


<a name="features"/>

## Features
- Arbitrary precision representation of real numbers (including a `str_ex()` method for string display of high precision numbers)
- Definable integer and fractional bit formats
- Fixed point basic math operations
- Bitwise/index/slice operations
- Gracefully plays with int and float Python types
- Switch between fixed and floating point math without changing code
- Tracking of min/max values for prototyping
- Follows the VHDL fixed point library conventions (relatively) closely
- Objects are picklable (only pickle protocols >= 2 are supported)
- The fpbinary objects **ARE NOT** subclassable at present

<a name="install"/>

## Installation

Installation is currently only via source download and build. You require an installation of git, python >= 2.7.12 and a C99 compliant compiler.

>**Installing on Windows**: fpbinary is developed using Linux, but it does get tested using Windows 10. Windows testing currently uses the Python-recommended Visual C++ compiler which is specified for each python version at [Windows Compilers](https://wiki.python.org/moin/WindowsCompilers). fpbinary testing on Windows currently uses [Microsoft Build Tools for Visual Studio 2019](https://www.visualstudio.com/downloads/#build-tools-for-visual-studio-2019).


The easiest way is to use pip:

```bash
pip install git+https://github.com/smlgit/fpbinary.git
```

This will download the source from git and rund the setup script for you.

Alternatively, you can clone the fpbinary repository and run the setup script: 


```bash
git clone https://github.com/smlgit/fpbinary.git

cd fpbinary
python setup install
```

The library has been tested on:
- Linux 16.04 LTS: python versions 2.7.12, 3.5.2, 3.6.5, 3.7.5 and 3.8.2 .
- Windows 10: VS Build Tools 2019 v16.6 (compiler version 19.26), python versions 3.6.5, 3.7.5 and 3.8.0 .

<a name="use"/>

## Use

fpbinary provides two main objects - `FpBinary` and `FpBinarySwitchable`. The best way to learn how they work is to read the help documentation:

```python
from fpbinary import FpBinary, FpBinarySwitchable
help(FpBinary)
help(FpBinarySwitchable)
```

There are also some useful [demos](./demos).
 
 Below is a very brief introduction to the objects.
 
### Objects

#### `FpBinary`

This object represents a real number with a specified number of integer and fractional bits.

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

<a name="dev"/>

## Development
fpbinary was designed from the point of view of a frustrated FPGA designer. Speed and useability for FPGA/hardware engineers drove the implementation decisions.

### Architecture
The two main objects are `FpBinary` and `FpBinarySwitchable`.

#### `FpBinary`
Is a wrapper that is composed of an instance of one of two "base" types:
- `_FpBinarySmall`: this object uses native c types for the underlying value representation. This makes operations as fast as possible. However, use of this object is limited by the machine bit width.
- `_FpBinaryLarge`: this object uses Python integer objects (`PyLong`) for the value representation. This allows arbitrary length data at the expense of slower operation (and messier c code...).

The purpose of `FpBinary` is to work out whether the faster object can be used for a representation or operation result and select between the two base types accordingly. It also must make sure the operands of binary/ternary operations are cast to the base type before forwarding them on.

This architecture does make the code and maintenance more complicated and it is questionable whether it is worth having the small object at all. Basic profiling does suggest that `FpBinary` is faster than `_FpBinaryLarge` on its own (for numbers < 64 bits), but the difference isn't that big (and is mostly in the creation of objects rather than the math ops).

#### `FpBinarySwitchable`
The point of this object is to allow a designer to write their simulation code assuming fixed point operation (i.e. with fixed point operations like the `resize()`) method, but to be able to force floating point math with the flick of a switch. Not only is the normal workflow to try out a design using floating point math first, it is also incredibly handy to be able to switch back and forth through the entire project lifecycle.

`FpBinarySwitchable` is composed of a `FpBinary` instance and a native c `double` variable. Which variable is actually used when an operation is invoked on the instance is dictated by the `fp_mode`, which is defined at constructor time. The `FpBinarySwitchable` code is essentially tasked with casting the other operand to the right type (fixed or float) and then forwarding on the underlying operation to the right object.

`FpBinarySwitchable` also implements a `value` property that can be used to set the composition instances. This makes it easy for the designer to write simulation code with apparently mutable data points. The advantage of this is that minimum and maximum values can be tracked during the lifetime of the object - Matlab implements a similar feature for its fixed point variables and it allows the user to get an idea for the required format of each data point. `FpBinarySwitchable` implements this functionality with simple logic in the property setter method. Note that this is only done when in floating point mode.

`FpBinarySwitchable` is designed to "look" like an `FpBinary` object, at least when it makes sense to flick the operation to float mode. So I have implemented `resize()` operations (no change in float mode) and shifting operations (mult/div by powers of 2) as well as the math operations. But index/slice and bitwise operations have **not** been implemented.

### Coding Notes
- Direct calls to object methods (like the tp_as_number methods) was favoured over the c api PyNumber abstract methods where possible. This was done for speed.
- Similarly, a private interface was created for `_FpBinarySmall` and `_FpBinaryLarge` to implement so `FpBinary` could access them without going through the abstract call functions (that use string methods for lookup). This provided some type of polymorphism via the `fpbinary_base_t` type placed at the top of the base's object definitions.

<a name="enhancements"/>

## Enhancements
- [ ] Possibly jettison the base class architecure and use `_FpBinaryLarge` as the main object.
- [ ] Add global contexts that allows the user to define "hardware" specifications so inputs and outputs to math operations can be resized automatically (i.e. without the need for explicit resizing code). 
- [ ] Add more advanced operations like log, exp, sin/cos/tan. I have stopped short of doing these thus far because a user may rather simulate the actual hardware implementation (e.g. a lookup table would likely be used for sin). Having said that, a convienient fpbinary method should give the same result.
- [ ] Add complex number versions of the two main classes.
- [ ] Allow `FpBinary` and `FpBinarySwitchable` to be subclassable. Would require some basic changes to (mostly) `FpBinarySwitchable` to use the abstract methods from the Python Numeric/Sequence interfaces rather than direct accessing via the type memory. Might reduce speed slightly. 





