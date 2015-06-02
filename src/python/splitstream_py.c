/*
 *   splitstream_py.c
 *   splitstream - Stream object splitter (Python API)
 *
 *   Copyright © 2015 Rickard Lyrenius
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include <Python.h>
#include "../splitstream.h"
const static int SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT = 8;
 
static int call_callback(SplitstreamDocument* doc, PyObject* callback);
static int splitfile_pure_once(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, PyObject* callback);
static int splitfile_pure_core(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, PyObject* callback);

/*
 At its simplest, splitstream requires an object with a read([integer]) method.
 */
static int splitfile_pure(PyObject* read, long max, long bufsize, SplitstreamScanner scanner, 
	int startDepth, PyObject* callback)
{
	int ret;
	PyObject* readargs = Py_BuildValue("(i)", bufsize);
	SplitstreamState state;
	SplitstreamInitDepth(&state, startDepth);
	ret = splitfile_pure_core(&state, read, readargs, max, scanner, callback);
	SplitstreamFree(&state);
	Py_DECREF(readargs);
	return ret;
}

/*
 Optimized version - when a file object contains a file handle, we don't have to ask Python
 for each new data block since we can read it directly using fread.
 */
static int splitfile_fileno(int fileno, long max, long bufsize, SplitstreamScanner scanner,
	int startDepth, PyObject* callback)
{
	FILE* f;
	char* buf = malloc(bufsize);
	if(!buf) {
    	PyErr_SetString(PyExc_MemoryError, "Unable to allocate buffer."); 
		return -1;
	}
	f = fdopen(fileno, "r");
	if(!f) {
		free(buf);
    	PyErr_SetString(PyExc_IOError, "Unable to open file handle for reading."); 
		return -1;
	}
	SplitstreamDocument doc;
	SplitstreamState state;
	SplitstreamInitDepth(&state, startDepth);
    while(!feof(f)) {
    	doc = SplitstreamGetNextDocumentFromFile(&state, buf, bufsize, max, f, scanner);
    	if(doc.buffer) {
            if(call_callback(&doc, callback) < 0) {
				SplitstreamFree(&state);
	            return -1;
	        }
    	}
    }
    do {
    	doc = SplitstreamGetNextDocumentFromFile(&state, buf, bufsize, max, f, scanner);
    	if(doc.buffer) {
            if(call_callback(&doc, callback) < 0) {
				SplitstreamFree(&state);
	            return -1;
	        }
    	}
    } while (doc.buffer);
    free(buf);
	SplitstreamFree(&state);
	return 0;
}

/*
 Entry point
 */
static PyObject* splitfile(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* file, *ret = Py_None;
    PyObject* file_read = NULL, *file_fileno = NULL, *noargs = NULL;
    const char* fmt = NULL;
    PyObject* callback = NULL;
    long bufsize = 0, max = 0, startDepth = 0;
    int fileno = -1;
    SplitstreamScanner scanner;
    static char* kwarg_list[] = {"file", "format", "callback", "startdepth", "bufsize", "maxdocsize", NULL};
 
 
	noargs = PyTuple_Pack(0);
	if(!noargs) return NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "Os|Oiii", kwarg_list, &file, &fmt, &callback, &startDepth, &bufsize, &max))
        return NULL;
    
    if(!file || file == Py_None) {
    	PyErr_SetString(PyExc_TypeError, "file argument not set"); 
    	return NULL;
    }
    Py_INCREF(file);
    Py_XINCREF(callback);
    
    do {
    	file_read = PyObject_GetAttr(file, PyString_FromString("read"));
	    if(!file_read) { ret = NULL; break; }
    
    	file_fileno = PyObject_GetAttr(file, PyString_FromString("fileno"));
    	if(file_fileno) {
	    	PyObject* fn = PyObject_Call(file_fileno, noargs, NULL);
	    	if(!fn) { ret = NULL; break; }
	    	
    		fileno = (int)PyInt_AsLong(fn);
    		if(fileno < 0) {
    			if(!PyErr_Occurred()) {
    				PyErr_Format(PyExc_ValueError, "Invalid fileno %d.", fileno); 
    			}
    			ret = NULL; break;
    		}
    	} else PyErr_Clear();
    
	    if(!strcmp(fmt, "xml")) {
    		scanner = SplitstreamXMLScanner;
	    } else if(!strcmp(fmt, "json")) {
    		scanner = SplitstreamJSONScanner;
	    } else {
    		PyErr_SetString(PyExc_ValueError, "Invalid object format name specified"); 
		    ret = NULL; break;
	    }
	    if(bufsize <= 0) bufsize = 1024;
    	if(bufsize > 1024*1024*100) {
	    	PyErr_Format(PyExc_ValueError, "Buffer size %ld out of range.", bufsize); 
		    ret = NULL; break;
    	}
	    if(max <= 0) max = 100*1024*1024;
    	if(max > 1<<30) {
    		PyErr_Format(PyExc_ValueError, "Max document size %ld out of range.", max); 
		    ret = NULL; break;
    	}
    
	    if(!callback) {
    		ret = PyList_New(0);
    		callback = PyObject_GetAttr(ret, PyString_FromString("append"));
    		if(!callback) { Py_DECREF(ret); ret = NULL; break; }
	    }
    
    	if(fileno >= 0) {
		    if(splitfile_fileno(fileno, max, bufsize, scanner, (int)startDepth, callback) < 0) {
			    Py_DECREF(ret);
	    		ret = NULL; break;
	    	}
    	} else {
		    if(splitfile_pure(file_read, max, bufsize, scanner, (int)startDepth, callback) < 0) {
			    Py_DECREF(ret);
	    		ret = NULL; break;
	    	}
	    }
	} while(0);
	
    Py_XDECREF(file_fileno);
    Py_XDECREF(file_read);
    Py_XDECREF(file);
    Py_XDECREF(callback);
    Py_XDECREF(noargs);
    
    return ret;
}
 
static PyMethodDef methods[] = {
    {"splitfile", (PyCFunction)splitfile, METH_VARARGS | METH_KEYWORDS, "Split a file object.\n\nsplitfile(file, format[, callback]) -> Split the file, optionally specifying a callback that will be called with each object.\n\nIf callback is not specified, the function instead returns a list of the string chunks.\n\nOptional keyword arguments:\n  bufsize - Size of read buffer"},
    {NULL, NULL, 0, NULL}
};
 
PyMODINIT_FUNC
initsplitstream(void)
{
    (void) Py_InitModule3("splitstream", methods, "Splitting of (XML, JSON) objects from a continuous stream");
}

/**
*** Helpers
**/

static int splitfile_pure_core(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, PyObject* callback)
{
    int eof = 0;
    while(!eof) {
		eof = splitfile_pure_once(s, read, readargs, max, scanner, callback);
		if(eof < 0) return -1;
    }
    do {
		eof = splitfile_pure_once(s, NULL, NULL, max, scanner, callback);
		if(eof < 0) return -1;
    } while (!eof);
    return 0;
}

static int splitfile_pure_once(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, PyObject* callback)
{
	int eof = 1;
    if(s->flags & SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT) {
        SplitstreamDocument doc = SplitstreamGetNextDocument(s, max, NULL, 0, scanner);
        if(doc.buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
            if(call_callback(&doc, callback) < 0)
	            return -1;
            return 0;
        }
    }

    while(read) {
	    long len;
	    char* buf;
        PyObject* data = PyObject_Call(read, readargs, NULL);
        if(!data) return -1;
        
        if(PyString_AsStringAndSize(data, &buf, &len) < 0) return -1;
        eof = len == 0;

        SplitstreamDocument doc = SplitstreamGetNextDocument(s, max, buf, len, scanner);
        Py_DECREF(data);
        if(doc.buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
            if(call_callback(&doc, callback) < 0)
	            return -1;
            return eof;
        }
        if(eof) break;
    }
    s->flags &= ~SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;

    return eof;
}

static int call_callback(SplitstreamDocument* doc, PyObject* callback)
{
	PyObject* ret = NULL;
	if(doc->buffer) {
		PyObject* val, *vals;
	
		val = PyString_FromStringAndSize(doc->buffer, doc->length);
		if(!val) return -1;
		vals = PyTuple_Pack(1, val);
		if(!vals) {
			Py_DECREF(val);
			return -1;
		}
		
		ret = PyObject_Call(callback, vals, NULL);
		
		Py_DECREF(vals);
		Py_DECREF(val);
		if(ret) {
			Py_DECREF(ret);
			return 0;
		}
	
    } else PyErr_SetString(PyExc_ValueError, "Invalid object"); 
	return -1;
}