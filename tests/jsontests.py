import unittest
import os
from StringIO import StringIO
import tempfile
import splitstream

class JsonTests(unittest.TestCase):
    def _stringio(self, string):
        class C(StringIO):
            def read(self, n):
                return StringIO.read(self, n)
        return C(string)
    
    def _tempfile(self, string):
        f = tempfile.TemporaryFile()
        f.write(string)
        f.seek(0)
        return f
    
    def _do_split(self, string, startdepth=0):
        f = self._loadstr(string)
        return splitstream.splitfile(f, "json", bufsize=self._bufsize, startdepth=startdepth)
        
    DATA_JSON = "{\"a\":[true,2,\"3\",[4,1.0,-1,-1.0],[],{}]}"

    def def_SplitSingleJson(self):
        v = self._do_split(self.DATA_JSON)
        assert v == [ self.DATA_JSON ]
        
    def def_SplitTwoJson(self):
        v = self._do_split("{\"a\":3}{\"b\":3}")
        assert v == [ "{\"a\":3}","{\"b\":3}" ]

    def def_SplitTwoJsonDocumentsWithEscaped(self):
        v = self._do_split("{\"a}\":3}{\"b\\\"}\":3}")
        assert v == [ "{\"a}\":3}", "{\"b\\\"}\":3}" ]
        
    def def_SplitTwoJsonDocumentsWithWhitespace(self):
        v = self._do_split("  {\"a\":3}  \t{\"b\":3}")
        assert v == [ "{\"a\":3}","{\"b\":3}" ]
        
    def __init__(self, *a, **kw):
        unittest.TestCase.__init__(self, *a, **kw)
        self._loadstr = None
        self._bufsize = 0
        
for m in dir(JsonTests):
    if m.startswith("def_"):
        print m
        func = getattr(JsonTests, m)
        for mode in ["str", "file"]:
            for bufsize in [1, 2, 7, 4096]:
                if "SplitHuge" in m and bufsize != 4096: continue
                def addt(m, mode, bufsize, func):
                    def ff(self):
                        if mode == "str":
                            self._loadstr = self._stringio
                        else:
                            self._loadstr = self._tempfile
                        self._bufsize = bufsize
                        return func(self)
                    setattr(JsonTests, "test_%s_buf%04d_%s" % (m[4:], bufsize, mode), ff)
                addt(m, mode, bufsize, func)