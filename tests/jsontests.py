import unittest
import os
try:
    from StringIO import StringIO
except ImportError:
    from io import BytesIO as StringIO
import tempfile
import splitstream

class JsonTests(unittest.TestCase):
    def _stringio(self, string):
        class C(StringIO):
            def read(self, n):
                return StringIO.read(self, n)
        if bytes != type(string):
            b = bytes(string, 'utf-8')
        else:
            b = string
        return C(b)
    
    def _tempfile(self, string):
        f = tempfile.TemporaryFile()
        if bytes != type(string):
            b = bytes(string, 'utf-8')
        else:
            b = string
        f.write(b)
        f.seek(0)
        return f
    
    def _do_split(self, string, startdepth=0):
        f = self._loadstr(string)
        try:
            return list(splitstream.splitfile(f, "json", bufsize=self._bufsize, startdepth=startdepth))
        finally:
            f.close()
        
    DATA_JSON = b"{\"a\":[true,2,\"3\",[4,1.0,-1,-1.0],[],{}]}"

    def def_SplitSingleJson(self):
        v = self._do_split(self.DATA_JSON)
        assert v == [ self.DATA_JSON ], "%r != %r" % (v, [ self.DATA_JSON ] )
        
    def def_SplitTwoJson(self):
        v = self._do_split(b"{\"a\":3}{\"b\":3}")
        exp = [ b"{\"a\":3}", b"{\"b\":3}" ]
        assert v == exp, "%r != %r" % (v, exp)

    def def_SplitTwoJsonDocumentsWithEscaped(self):
        v = self._do_split(b"{\"a}\":3}{\"b\\\"}\":3}")
        exp = [ b"{\"a}\":3}", b"{\"b\\\"}\":3}" ]
        assert v == exp, "%r != %r" % (v, exp)
        
    def def_SplitTwoJsonDocumentsWithWhitespace(self):
        v = self._do_split(b"  {\"a\":3}  \t{\"b\":3}")
        exp = [ b"{\"a\":3}", b"{\"b\":3}" ]
        assert v == exp, "%r != %r" % (v, exp)
        
    def __init__(self, *a, **kw):
        unittest.TestCase.__init__(self, *a, **kw)
        self._loadstr = None
        self._bufsize = 0
        
for m in dir(JsonTests):
    if m.startswith("def_"):
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