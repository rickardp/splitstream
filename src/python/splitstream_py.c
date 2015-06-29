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
#include <bytesobject.h>
#include "../splitstream.h"
const static int SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT = 8;
const static int SPLITSTREAM_STATE_FLAG_FILE_EOF = 16;
 
static PyObject* splitfile(PyObject* self, PyObject* args, PyObject* kwargs);
static int call_callback(SplitstreamDocument* doc, PyObject* callback);
static PyObject* as_python_object(SplitstreamDocument* doc);
static int splitfile_pure_once(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, SplitstreamDocument* doc);

typedef struct {
	PyObject_HEAD
	PyObject* read, *callback;
	SplitstreamScanner scanner;
	SplitstreamState state;
	int eof, fileeof, preambleDoc;
	FILE* f;
	long bufsize, max;
	char* preamble;
	char* buf;
} Generator;

static Generator* splitstream_generator_new(PyTypeObject *type, PyObject *args, PyObject *kwargs);
static void splitstream_generator_dealloc(Generator* state);
static PyObject* splitstream_generator_next(Generator *state);

/*
 Module definition
 */
 
static PyMethodDef methods[] = {
    {"splitfile", (PyCFunction)splitfile, METH_VARARGS | METH_KEYWORDS, "Split a file object.\n\nsplitfile(file, format[, callback]) -> Split the file, optionally specifying a callback that will be called with each object.\n\nIf callback is not specified, the function instead returns a list of the string chunks.\n\nOptional keyword arguments:\n  bufsize - Size of read buffer"},
    {NULL, NULL, 0, NULL}
};

#define MODULE_NAME "splitstream"
#define MODULE_DESC "Splitting of (XML, JSON) objects from a continuous stream"
 
#if PY_MAJOR_VERSION >= 3
PyObject* PyInit_splitstream(void) 
{
	static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        MODULE_NAME,
        MODULE_DESC,
        0,
        methods,
        NULL,
        NULL,
        NULL,
        NULL
	};
	return PyModule_Create(&moduledef);
}
#else
PyMODINIT_FUNC
initsplitstream(void)
{
    (void) Py_InitModule3(MODULE_NAME, methods, MODULE_DESC);
}
#endif

/*
 'splitfile' Entry point
 */
static PyObject* splitfile(PyObject* self, PyObject* args, PyObject* kwargs)
{
    PyObject* file, *ret = Py_None;
    PyObject* file_read = NULL, *file_fileno = NULL, *noargs = NULL;
    const char* fmt = NULL;
    const char* preamble = NULL;
    PyObject* callback = NULL;
    long bufsize = 0, max = 0, startDepth = 0;
    int fileno = -1;
    SplitstreamScanner scanner;
    Generator* g;
    static int gt = 0;
    static PyTypeObject gentype = {
    	PyVarObject_HEAD_INIT(&PyType_Type, 0)
    	"splitstream.( generator )",
    	sizeof(Generator)
    };
    if(!gt) {
    	gentype.tp_dealloc = (destructor)splitstream_generator_dealloc;
	    gentype.tp_flags = Py_TPFLAGS_DEFAULT;
    	gentype.tp_iter = PyObject_SelfIter;
	    gentype.tp_iternext = (iternextfunc)splitstream_generator_next;
    	gentype.tp_alloc = PyType_GenericAlloc;
	    gentype.tp_new = (newfunc)splitstream_generator_new;
	    if(PyType_Ready(&gentype) < 0)
	    	return NULL;
	    Py_INCREF(&gentype);
    	gt = 1;
    }
    
    static char* kwarg_list[] = {"file", "format", "callback", "startdepth", "bufsize", "maxdocsize", "preamble", NULL};
 
 
	noargs = PyTuple_Pack(0);
	if(!noargs) return NULL;
	#if PY_MAJOR_VERSION >= 3
	#define FMT "Os|Oiiiy"
	#else
	#define FMT "Os|Oiiis"
	#endif
	
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, FMT, kwarg_list, &file, &fmt, &callback, &startDepth, &bufsize, &max, &preamble))
        return NULL;
    
    #undef FMT
    
    if(preamble && !preamble[0]) preamble = NULL;
    
    if(!file || file == Py_None) {
    	PyErr_SetString(PyExc_TypeError, "file argument not set"); 
    	return NULL;
    }
    Py_INCREF(file);
    Py_XINCREF(callback);
    
    do {
    	file_read = PyObject_GetAttrString(file, "read");
	    if(!file_read) { ret = NULL; break; }
    
    	file_fileno = PyObject_GetAttrString(file, "fileno");
    	if(file_fileno) {
	    	PyObject* fn = PyObject_Call(file_fileno, noargs, NULL);
	    	#if PY_MAJOR_VERSION >= 3
	    	if(fn) {
	    	#else
	    	if(!fn) { ret = NULL; break; }
	    	#endif
	    	
    		fileno = (int)PyLong_AsLong(fn);
    		if(fileno < 0) {
    			if(!PyErr_Occurred()) {
    				PyErr_Format(PyExc_ValueError, "Invalid fileno %d.", fileno); 
    			}
    			ret = NULL; break;
    		}
    		#if PY_MAJOR_VERSION >= 3
    		} else PyErr_Clear();
    		#endif
    	} else PyErr_Clear();
    
	    if(!strcmp(fmt, "xml")) {
    		scanner = SplitstreamXMLScanner;
	    } else if(!strcmp(fmt, "json")) {
    		scanner = SplitstreamJSONScanner;
	    } else if(!strcmp(fmt, "ubjson")) {
    		scanner = SplitstreamUBJSONScanner;
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
    
    	g = splitstream_generator_new(&gentype, PyTuple_Pack(0), NULL);
	    if(!g) { ret = NULL; break; }
	    
	    if(fileno >= 0) {
			g->f = fdopen(fileno, "r");
			if(!g->f) {
		    	Py_DECREF((PyObject*)g);
		    	PyErr_SetString(PyExc_IOError, "Unable to open file handle for reading."); 
				ret = NULL; break;
			}
	    }
	    g->read = file_read; Py_XINCREF(file_read);
	    g->scanner = scanner;
	    g->callback = callback; Py_XINCREF(callback);
	    g->bufsize = bufsize;
	    g->max = max;
	    if(preamble) g->preamble = strdup(preamble);
	    SplitstreamInitDepth(&g->state, (int)startDepth);
	    
	    if(!callback) {
	    	ret = (PyObject*)g;
	    } else {
	    	while(!g->eof) {
	    		splitstream_generator_next(g);
	    	}
	    	Py_DECREF((PyObject*)g);
	    }
	} while(0);
	
    Py_XDECREF(file_fileno);
    Py_XDECREF(file_read);
    Py_XDECREF(file);
    Py_XDECREF(callback);
    Py_XDECREF(noargs);
    
    return ret;
}

/**
*** Generator object
**/

static Generator* splitstream_generator_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	Generator* state = (Generator *)type->tp_alloc(type, 0);
	if (!state) return NULL;
	
	state->read = state->callback = NULL;
	state->eof = state->fileeof = 0;
	state->f = NULL;
	state->buf = NULL;
	memset(&state->state, 0, sizeof(state->state));

    return state;
}

static void splitstream_generator_dealloc(Generator* state)
{
	Py_XDECREF(state->read); state->read = NULL;
	Py_XDECREF(state->callback); state->callback = NULL;
	SplitstreamFree(&state->state);
	if(state->buf) free(state->buf);
	if(state->preamble) free(state->preamble);
	state->buf = NULL;
	Py_TYPE(state)->tp_free(state);
}

static PyObject* handle_doc(Generator *state, SplitstreamDocument* doc) {
	if(state->callback) {
		if(call_callback(doc, state->callback) < 0)
			return NULL;
		return Py_None;
	} else {
		return as_python_object(doc);
	}
}

static PyObject* splitstream_generator_next(Generator *state)
{
    SplitstreamDocument doc;
    doc.buffer = NULL;
	if(!state->scanner) {
		PyErr_SetString(PyExc_ValueError, "Invalid generator.");
		return NULL;
	}
	if(state->eof) {
		return NULL;
	}
	PyObject* readargs = state->f ? NULL : Py_BuildValue("(i)", state->bufsize);
	PyObject* ret = NULL;
	do {
	
		if(state->preamble) {
			doc = SplitstreamGetNextDocument(&state->state, state->max, state->preamble, strlen(state->preamble), state->scanner);
			free(state->preamble);
			state->preamble = NULL;
			if(doc.buffer) {
				state->preambleDoc = 1;
				ret = handle_doc(state, &doc);
				break;
			}
		}

		while(state->preambleDoc) {
			state->preambleDoc = 0;
			doc = SplitstreamGetNextDocument(&state->state, state->max, NULL, 0, state->scanner);
			if(doc.buffer) {
				state->preambleDoc = 1;
				ret = handle_doc(state, &doc);
				break;
			}
		}
		if(doc.buffer) break;
    
		if(state->f) {
			if(!state->buf) state->buf = malloc(state->bufsize);
			if(!state->buf) {
				PyErr_SetString(PyExc_MemoryError, "Unable to allocate buffer."); 
				ret = NULL; break;
			}
			while(!(state->state.flags & SPLITSTREAM_STATE_FLAG_FILE_EOF)) {
				doc = SplitstreamGetNextDocumentFromFile(&state->state, state->buf, state->bufsize, state->max, state->f, state->scanner);
				if(doc.buffer) {
					ret = handle_doc(state, &doc);
					break;
				}
			}
			if(doc.buffer) break;
			doc = SplitstreamGetNextDocumentFromFile(&state->state, state->buf, state->bufsize, state->max, state->f, state->scanner);
			if(doc.buffer) {
				ret = handle_doc(state, &doc);
				break;
			} else {
				state->eof = 1;
			}
		} else {
			while(!state->fileeof) {
				int eof = splitfile_pure_once(&state->state, state->read, readargs, state->max, state->scanner, &doc);
				if(eof < 0) { ret = NULL; break; }
				state->fileeof = eof;
				if(doc.buffer) {
					ret = handle_doc(state, &doc);
					break;
				}
			}
			if(doc.buffer) break;
			state->eof = splitfile_pure_once(&state->state, NULL, NULL, state->max, state->scanner, &doc);
			if(doc.buffer) {
				ret = handle_doc(state, &doc);
				break;
			}
		}
	} while (0);
	Py_XDECREF(readargs);
	SplitstreamDocumentFree(&state->state, &doc);
	return ret;
}

/**
*** Helpers
**/

static int splitfile_pure_once(SplitstreamState* s, PyObject* read, PyObject* readargs, long max, SplitstreamScanner scanner, SplitstreamDocument* doc)
{
	int eof = 1;
    if(s->flags & SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT) {
        *doc = SplitstreamGetNextDocument(s, max, NULL, 0, scanner);
        if(doc->buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
            return 0;
        }
    }

    while(read) {
	    Py_ssize_t len;
	    char* buf;
        PyObject* data = PyObject_Call(read, readargs, NULL);
        if(!data) return -1;
        
        if(PyBytes_AsStringAndSize(data, &buf, &len) < 0) return -1;
        eof = len == 0;

        *doc = SplitstreamGetNextDocument(s, max, buf, len, scanner);
        Py_DECREF(data);
        if(doc->buffer) {
            s->flags |= SPLITSTREAM_STATE_FLAG_DID_RETURN_DOCUMENT;
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
	
		val = PyBytes_FromStringAndSize(doc->buffer, doc->length);
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

static PyObject* as_python_object(SplitstreamDocument* doc)
{
	if(doc->buffer) {
		return PyBytes_FromStringAndSize(doc->buffer, doc->length);
	} else {
		return Py_None;
	}
}
