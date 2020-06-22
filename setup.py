import os
from setuptools import setup, Extension

# Version information
def get_version_number():
    with open('VERSION', 'r') as f:
        for line in f:
            return line.split('.')

    return ()

# alpha build information
def get_alpha_str():
    if os.path.exists('ALPHA'):
        with open('ALPHA', 'r') as f:
            for line in f:
                return line.strip()

    return None

version_tuple = get_version_number()

if len(version_tuple) < 3:
    raise SystemError("Couldn't find the module version number!")

version = '{}.{}.{}'.format(version_tuple[0], version_tuple[1], version_tuple[2])
alpha_str = get_alpha_str()

if alpha_str is not None:
    version += alpha_str

fpbinary_module = Extension('fpbinary',
                            define_macros=[('MAJOR_VERSION', version_tuple[0]),
                                           ('MINOR_VERSION', version_tuple[1]),
                                           ('MICRO_VERSION', version_tuple[2]),
                                           ('BUILD_VERSION', 'none'),
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
