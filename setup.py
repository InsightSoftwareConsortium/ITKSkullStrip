# -*- coding: utf-8 -*-
from __future__ import print_function
from os import sys

try:
    from skbuild import setup
except ImportError:
    print('scikit-build is required to build from source.', file=sys.stderr)
    print('Please run:', file=sys.stderr)
    print('', file=sys.stderr)
    print('  python -m pip install scikit-build')
    sys.exit(1)

setup(
    name='itk-skullstripping',
    version='0.1.0',
    author='Stefan Bauer',
    author_email='stefan.bauer@istb.unibe.ch',
    packages=['itk'],
    package_dir={'itk': 'itk'},
    download_url=r'https://github.com/InsightSoftwareConsortium/ITKSkullStrip',
    description=r'Automatic skull-stripping for neuroimage analysis',
    long_description='ITKSkullStrip provides a class to perform automatic'
                     'skull-stripping for neuroimage analysis.\n'
                     'Please refer to:'
                     'Bauer S., Fejes T., Reyes M.,'
                     '“Skull-Stripping Filter for ITK”, '
                     'Insight Journal, http://hdl.handle.net/10380/3353, 2012.',
    classifiers=[
        "License :: OSI Approved :: Apache Software License",
        "Programming Language :: Python",
        "Programming Language :: C++",
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "Intended Audience :: Education",
        "Intended Audience :: Healthcare Industry",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering",
        "Topic :: Scientific/Engineering :: Medical Science Apps.",
        "Topic :: Scientific/Engineering :: Information Analysis",
        "Topic :: Software Development :: Libraries",
        "Operating System :: Android",
        "Operating System :: Microsoft :: Windows",
        "Operating System :: POSIX",
        "Operating System :: Unix",
        "Operating System :: MacOS"
        ],
    license='Apache',
    keywords='ITK InsightToolkit neuroimaging neuroimaging-analysis',
    url=r'https://github.com/InsightSoftwareConsortium/ITKSkullStrip',
    install_requires=[
        r'itk>=5.2.0.post2'
    ]
    )
