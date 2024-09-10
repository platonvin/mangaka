from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules=[
    Extension("combine",["combine.py"], 
    # extra_compile_args=["-O3", "-march=native"]
    ),
    Extension("compress",["compress.py"], 
    # extra_compile_args=["-O3", "-march=native"]
    ),
    Extension("scale",["scale.py"], 
    # extra_compile_args=["-O3", "-march=native"]
    ),
]

setup(
  name = 'MyProject',
  cmdclass = {'build_ext': build_ext},
  ext_modules = ext_modules,
)