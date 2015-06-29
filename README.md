# splitstream - Continuous object splitter

[![Travis CI status](https://travis-ci.org/evolvIQ/splitstream.svg)](https://travis-ci.org/evolvIQ/splitstream)
[![License](https://img.shields.io/github/license/evolvIQ/splitstream.svg)](https://github.com/evolvIQ/splitstream/blob/master/LICENSE)
[![PyPi version](https://img.shields.io/pypi/v/splitstream.svg)](https://pypi.python.org/pypi/splitstream/)
[![PyPi downloads](https://img.shields.io/pypi/dm/splitstream.svg)](https://pypi.python.org/pypi/splitstream/)
![Bindings](https://img.shields.io/badge/bindings-C%20%7C%20Python-lightgrey.svg)

This is a C library with Python bindings that will help you split continuous streams of non-delimited XML documents or [JSON](http://json.org) and [UBJSON](http://ubjson.org) objects/arrays. Instead of using regular expressions or other forms of string matching, the approach is to implement a basic tokenizer and element depth parser to detect where the document ends, without relying on the existence of specific elements, delimiters or length prefix.

The library is written in C and provides bindings for Python (supports Python 2.6+ and Python 3 as well as PyPy).

**Developing for OS X or iOS?** Take a look at [IQSerialization](https://github.com/evolvIQ/iqserialization), which provides Obj-C (and Swift) bindings for this library (and much more).

# Use cases

* **Parsing XML/JSON log files as they are written**.  In this case, the log file is usually not wellformed since the end tag for the root element is missing, or each log entry is its own root. This library handles both cases.
* **Parsing huge XML files as objects**. It may not be feasible to feed the entire file into a deserializer or DOM parser, and using an event based parser can add complexity to your code. By pre-processing with this library you can parse chunks of the stream instead.
* **Parsing objects from a raw TCP stream**. Parse the objects as they appear on the raw stream instead of wrapping in HTTP requests or your own protocol.

# Features

* Understands XML, JSON and UBJSON.
* Tokenizer will correctly handle complex documents (e.g. xml within comments or CDATA, escape sequences, processing instructions, etc).
* Comprehensive and growing test suite.
* Written in clean C with no dependencies except the standard C library.
* Possible to add your own tokenizers (in C) to add more object formats.
* Python bindings.
* Set initial parse depth to allow for e.g. parsing a log file with an "infinite" (unclosed) root element.
* Quite good performance and low memory footprint (constant with stream size), even from Python.
* Convenience API for working with `FILE*` pointers (C) and file objects (Python).

# Limitations

* **Will not actually parse the objects** (this is very much a design decision) - you need to feed the output from splitstream into your parser of choice.
* Only JSON arrays and objects are supported as root objects.
* Limited error handling. May not recover from malformed objects in many cases.
* Limited documentation (this page and the unit tests are a good start for now).
* UBJSON support is relatively untested.

# The C interface

## Tokenization context

Allocate a `SplitstreamState` on the stack or heap and initialize it:
```C
void SplitstreamInit(SplitstreamState* state);
void SplitstreamInitDepth(SplitstreamState* state, int startDepth);
```

The `SplitstreamInitDepth(...)` function also allows specifying a start depth that helps parsing subtrees of "infinite" XML documents, such as

    <?xml version="1.0" encoding="utf-8">
    <log>
      <logEntry id="1000" type="info">Hello</logEntry>
      <logEntry id="1001" type="info">Hello</logEntry>
      <logEntry id="1002" type="error">problem</logEntry>
      ...

When done tokenizing, free the context:

```C
void SplitstreamFree(SplitstreamState* state);
```
**Note:** This will free any buffers previously returned to the caller, so make sure not to use them after this function is called.

Once a context is specified, you need to drive the tokenizer

```C
SplitstreamDocument SplitstreamGetNextDocument(
   SplitstreamState* state, 
   size_t max, 
   const char* buf, 
   size_t len, 
   SplitstreamScanner scanner);
   
SplitstreamDocument SplitstreamGetNextDocumentFromFile(
   SplitstreamState* s, 
   char* buf, 
   size_t bufferSize, 
   size_t max, 
   FILE* file, 
   SplitstreamScanner scanner);
```

The `SplitstreamGetNextDocument` function lets you feed data from a memory buffer into the tokenizer. The `SplitstreamGetNextDocumentFromFile` is a helper function for the common case that you have a `FILE*` object.

The `state` parameter is a state context that was previously initialized using `SplitstreamInit` or `SplitstreamInitDepth`.

The `max` parameter is the maximum allowable document length. If the internal buffer exceeds this size, tokenization restarts at whatever the current position was (this may cause the next document to be invalid as well).

The `buf` parameter is a pointer to the input data (`SplitstreamGetNextDocument`) or a preallocated read buffer (`SplitstreamGetNextDocumentFromFile`). The `len` parameter is the number of bytes of valid data, `bufferSize` is similarly the size of the buffer.

The `file` parameter is a pointer to an open file or stream. The file needs to be readable, but not seekable.

The `scanner` parameter is one of `SplitstreamXMLScanner`, `SplitstreamJSONScanner` or `SplitstreamUBJSONScanner`, or your own tokenizer implementing the following prototype:

```C
typedef size_t (*SplitstreamScanner)(
   SplitstreamState* state, 
   const char* buf, 
   size_t len, 
   size_t* start);
```

### The tokenization pattern

**Important!** The C API is a low level interface to the tokenizer and you need to follow the following pattern to ensure that no document is missed:

```C
while(get_data(buf, &len)) {
   /* Feed new data and extract the first document, if any */
	doc = SplitstreamGetNextDocument(s, max, buf, len, scan);
	while(doc.buffer) {
	   handle_document(doc);
	   
	   /* Extract any documents remaining in the internal buffer */
	   doc = SplitstreamGetNextDocument(s, max, NULL, 0, scan);
	}
}
```

Similarly, for the `FILE*` version


```C
size_t len = 4096; /* Read buffer size */
char* buf = malloc(len);
while(!feof(f)) {
   /* Feed new data and extract the first document, if any */
	doc = SplitstreamGetNextDocumentFromFile(s, buf, len, 
	         max, file, scan);
	if(doc.buffer) {
	   handle_document(doc);
	}
}
do {
	/* Extract any documents remaining in the internal buffer */
	doc = SplitstreamGetNextDocumentFromFile(s, buf, len, 
	         max, NULL, scan);
	if(doc.buffer) {
	   handle_document(doc);
	}
} while(doc.buffer);
```

# The Python interface

## Installation

The easiest way to use splitstream is to create a requirements.txt for your project:

    echo 'splitstream>=1.0.2' >> requirements.txt

or install manually

    pip install splitstream

It is also very easy to clone this repo and build from source, see the *Building* section.

## Usage

There is only one function in the Python interface:

    splitfile(file, format[, callback[, startdepth
    	[, bufsize[, maxdocsize[, preamble]]]]])
    
The `file` argument is a file-like object (e.g. open file or `StringIO`, or anything with a `read([n])` method). 

`format` is either `"xml"`, `"json"` or `"ubjson"` and specifies the document type to split on.

`startdepth` helps parsing subtrees of "infinite" XML documents, such as

    <?xml version="1.0" encoding="utf-8">
    <log>
      <logEntry id="1000" type="info">Hello</logEntry>
      <logEntry id="1001" type="info">Hello</logEntry>
      <logEntry id="1002" type="error">problem</logEntry>
      ...

In this case, setting `startdepth` to 1 skips the `<log>` element and even allows parsing the `logEntry` elements before the document is finished (such as a current log file).

The `callback` argument is used to specify a callback function to be called with each document found in the stream. The `splitfile` function can operate in two modes:

* Callback mode (specifying a callback). The documents are passed to the callback.
* Generator mode (leaving `callback` unset or `None`). The documents are returned as a generator.

`bufsize` specifies the buffer size. It may make sense to increase this size when it is expected that the documents are large. Usually you should leave this as default.

`maxdocsize` specifies the maximum size for a document. Even if it is set, there is a maximum document size to prevent running out of memory in the case of oversized or malformed documents. If the internal buffer exceeds this size, tokenization restarts at whatever the current position was (this may cause the next document to be invalid as well).

`preamble` is an optional string that should be parsed before reading the file. By combining `preamble` with seeking the file, the header can be rewritten without filtering all subsequent reads. Another useful application is when reading the first few bytes to detect the file format (magic bytes) or when chaining stream splitters.

### Examples

```python
from splitstream import splitfile

with file("myfile.xmls") as f:
	for xml in splitfile(f, format="xml")):
   	    print xml
```

# Building


## The C library

Currently there is no build script to make a shared or static library for use with C or compatible languages. The setup script will only build the Python module, but it should be trivial to add the source file to your project with minimal overhead. 

The reason for this is that there are so many different build systems for C out there and there is no point choosing one or maintaining multiple build scripts. Adding the files to your Xcode, CMake or Visual Studio project is just a matter of adding the source and header files.

This library only depends on the standard C library.

## The Python module

The Python module uses the standard `distutils` build. To build, use:

    python setup.py build
    
or

    python setup.py test

to run the unit tests. To install, use

    python setup.py install