from distutils.core import setup, Extension

fpbinary_module = Extension('fpbinary',
                            define_macros=[('MAJOR_VERSION', '1'),
                                           ('MINOR_VERSION', '0')],
                            sources=['src/fpbinarymodule.c',
                                     'src/fpbinaryglobaldoc.c',
                                     'src/fpbinarycommon.c',
                                     'src/fpbinarysmall.c',
                                     'src/fpbinarylarge.c',
                                     'src/fpbinaryobject.c',
                                     'src/fpbinaryswitchable.c',
                                     'src/fpbinaryenums.c'])

setup(name='FpBinary',
      version='1.0',
      description='Provides binary fixed point functionality.',
      author_email='smlgit@protonmail.com',
      ext_modules=[fpbinary_module])
