from distutils.core import setup, Extension

# Version information
def get_version_number(num_type):
    if num_type == 'minor':
        token = 'FPBINARY_MINOR_VER'
    else:
        token = 'FPBINARY_MAJOR_VER'


    with open('src/fpbinaryversion.h', 'r') as f:
        for line in f:
            if token in line:
                _, _, version = line.split()
                return version

    return None

maj_version = get_version_number('major')
min_version = get_version_number('minor')

if maj_version == None or min_version == None:
    raise SystemError("Couldn't find the module version number!")




fpbinary_module = Extension('fpbinary',
                            define_macros=[('MAJOR_VERSION', maj_version),
                                           ('MINOR_VERSION', min_version)],
                            sources=['src/fpbinarymodule.c',
                                     'src/fpbinaryglobaldoc.c',
                                     'src/fpbinarycommon.c',
                                     'src/fpbinarysmall.c',
                                     'src/fpbinarylarge.c',
                                     'src/fpbinaryobject.c',
                                     'src/fpbinaryswitchable.c',
                                     'src/fpbinaryenums.c'])

setup(name='FpBinary',
      version='{}.{}'.format(maj_version, min_version),
      description='Provides binary fixed point functionality.',
      author_email='smlgit@protonmail.com',
      ext_modules=[fpbinary_module])
