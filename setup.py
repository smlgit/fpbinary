import os
from setuptools import setup, Extension

# Version information
def get_version_number():
    with open('VERSION', 'r') as f:
        for line in f:
            return line.split('.')

    return ()

version_tuple = get_version_number()

if len(version_tuple) < 3:
    raise SystemError("Couldn't find the module version number!")

version = '{}.{}.{}'.format(version_tuple[0], version_tuple[1], version_tuple[2])

# Can specify alpha_build_num to show build in release files.
# Can specify private_build_num to only show build number in python __build_num_ variable.
build_num = 'none'
if 'alpha_build_num' in os.environ:
    build_num = os.environ['alpha_build_num']
    version += 'a{}'.format(build_num)
elif 'private_build_num' in os.environ:
    build_num = os.environ['private_build_num']

fpbinary_module = Extension('fpbinary',
                            define_macros=[('MAJOR_VERSION', version_tuple[0]),
                                           ('MINOR_VERSION', version_tuple[1]),
                                           ('MICRO_VERSION', version_tuple[2]),
                                           ('BUILD_VERSION', build_num),
                                           ('VERSION_STRING', version)],
                            sources=['src/fpbinarymodule.c',
                                     'src/fpbinaryglobaldoc.c',
                                     'src/fpbinarycommon.c',
                                     'src/fpbinarysmall.c',
                                     'src/fpbinarylarge.c',
                                     'src/fpbinaryobject.c',
                                     'src/fpbinaryswitchable.c',
                                     'src/fpbinaryenums.c'])


setup(name='fpbinary',
      version=version,
      description='Provides binary fixed point functionality.',
      author_email='smlgit@protonmail.com',
      ext_modules=[fpbinary_module])
