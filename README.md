# splitstream - Continuous object splitter

This is a C library with Python bindings that will help you split continuous streams of non-delimited XML documents or JSON objects/arrays. Instead of using regular expressions or other forms of string matching, this library tries to implement a basic tokenizer and element depth parser to detect where the document ends, without relying on the existence of specific elements, delimiters or length prefix.

**Developing for OS X or iOS?** Take a look at [IQSerialization](https://github.com/evolvIQ/iqserialization), which provides Obj-C bindings for this library (and much more).

# Use cases

* **Parsing XML/JSON log files as they are written. ** In this case, the log file is usually not wellformed since the end tag for the root element is missing, or each log entry is its own root. This library handles both cases.
* **Parsing huge XML files as objects. ** It may not be feasible to feed the entire file into a deserializer or DOM parser, and using an event based parser is not always desired. By pre-processing with this library you can parse chunks of the stream instead.
* **Parsing objects from a raw TCP stream. ** Parse the objects as they appear on the raw stream instead of wrapping in HTTP requests or your own protocol.

# Features

* Understands XML and JSON.
* Tokenizer will correctly handle complex documents (e.g. xml within comments or CDATA, escape sequences, processing instructions, etc).
* Comprehensive and growing test suite.
* Written in clean C with no dependencies except the standard C library.
* Possible to add your own tokenizers (in C) to add more object formats.
* Python bindings.
* Set initial parse depth to allow for e.g. parsing a log file with an "infinite" (unclosed) root element.
* Quite good performance and low memory footprint (constant with stream size), even from Python.
* Convenience API for working with FILE* pointers (C) and file objects (Python).

# Limitations

* **Will not actually parse the objects** (this is very much a design decision) - you need to feed the output from splitstream into your parser of choice.
* Only JSON arrays and objects are supported as root objects.
* Limited error handling. May not recover from malformed objects in many cases.
* Limited documentation (the unit tests are a good start for now).

# Building

To build, use 

    python setup.py build
    
or

    python setup.py test

to run the unit tests. To install, use

    python setup.py install

Currently there is no build script to make a shared or static library for use with C or compatible languages. The setup script will only build the Python module, but it should be trivial to add the source file to your project with minimal overhead. This library only depends on the standard C library.
