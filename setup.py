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
    url="https://github.com/evolvIQ/splitstream",
    author="Rickard Lyrenius",
    author_email="rickard@evolviq.com",
    version='1.0.0',
    description="Splitting of (XML, JSON) objects from a continuous stream",
    ext_modules=[Extension('splitstream', ['src/python/splitstream_py.c', 'src/splitstream.c', 'src/splitstream_xml.c', 'src/splitstream_json.c'])],
    install_requires=requirements + test_requirements,
    zip_safe=False,
    test_suite='tests'
)