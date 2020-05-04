

from setuptools import setup, Extension
import sys
import setuptools
import unittest
def sib_test():
    test_loader = unittest.TestLoader()
    test_suite = test_loader.discover('test', pattern='*_tests.py')
    return test_suite


__version__ = '0.1'
COMPILE_FLAGS = "-fPIC -std=c++11 -Wall -O3 -g -fopenmp"



class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __str__(self):
        import pybind11
        return pybind11.get_include()


ext_modules = [
    Extension(
        '_sib',
        ["bp.cpp",
        #"bp.h",
        #"cavity.h",
        "params.cpp",
        #"params.h",
        "pysib.cpp"
        ],
        include_dirs=[
            # Path to pybind11 headers
            get_pybind_include(),
        ],
        extra_compile_args=[*COMPILE_FLAGS.split(" ")],
        extra_link_args=["-lgomp", "-lm"],
        #language='c++'
    ),
]






setup(
    name="sib",
    version=__version__,
    author="Sybil Team",
    description="""Belief Propagation for inference in epidemics""",
    py_modules=[
        "sib"
    ],
    ext_modules=ext_modules,
    setup_requires=['pybind11>=2.5.0'],
    test_suite="setup.sib_test",
)