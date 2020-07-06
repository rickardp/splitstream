#!/usr/bin/env python
# -*- coding: utf-8 -*
import os
from setuptools import setup, Extension

ROOT_DIR = os.path.dirname(__file__)
SOURCE_DIR = os.path.join(ROOT_DIR)

test_requirements = []
requirements = []

setup(
    name="splitstream",
    url="https://github.com/rickardp/splitstream",
    author="Rickard Lyrenius",
    author_email="rickard@evolviq.com",
    version="1.2.3",
    description="Splitting of (XML, JSON) objects from a continuous stream",
    ext_modules=[Extension('splitstream', ['src/python/splitstream_py.c', 'src/splitstream.c', 'src/splitstream_xml.c', 'src/splitstream_json.c', 'src/splitstream_ubjson.c', 'src/mempool.c'])],
    headers=['src/splitstream.h', 'src/splitstream_private.h'],
    install_requires=requirements + test_requirements,
    zip_safe=False,
    test_suite='test',
    classifiers=[
        "License :: OSI Approved :: Apache Software License", 
        "Topic :: Software Development :: Libraries :: Python Modules",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.2",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
        "Intended Audience :: Developers"]
)
