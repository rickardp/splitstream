import unittest
import os
try:
    from StringIO import StringIO
except ImportError:
    from io import BytesIO as StringIO
import tempfile
import splitstream

class UbJsonTests(unittest.TestCase):
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
            return list(splitstream.splitfile(f, "ubjson", bufsize=self._bufsize, startdepth=startdepth))
        finally:
            f.close()

    def def_SplitSingleUbJson(self):
        v = self._do_split(b"[T]")
        exp = [ b"[T]" ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitTwoUbJson(self):
        v = self._do_split(b"{C{C{}{C{C{}")
        exp = [ b"{C{C{}", b"{C{C{}" ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitPaddedUbJson(self):
        v = self._do_split(b" N N T {C{Si\x07}}}}}}}}   {C{C{}")
        exp = [ b"{C{Si\x07}}}}}}}}", b"{C{C{}" ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitUbJsonInt8(self):
        x = b"[C{Si\x01}]"
        v = self._do_split(b"".join([x, x]))
        exp = [ x, x ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitUbJsonInt16(self):
        x = b"{C{SI\x01\x00" + 0x0100 * b"}" + b"}"
        v = self._do_split(b"".join([x, x]))
        exp = [ x, x ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitUbJsonInt32(self):
        x = b"{C{Sl\x00\x00\x01\x00" + 0x000100 * b"}" + b"}"
        v = self._do_split(b"".join([x, x]))
        exp = [ x, x ]
        assert v == exp, "%r != %r" % (v, exp )

    def def_SplitUbJsonUInt8(self):
        x = b"{C{SU\xff" + 0xff * b"}" + b"}"
        v = self._do_split(b"".join([x, x]))
        exp = [ x, x ]
        assert v == exp, "%r != %r" % (v, exp )

    # TODO: Optimized collections are not yet implemented
    #def def_SplitUbJsonOptimizedList(self):
    #    x = b"[$i#i\x03]]}"
    #    v = self._do_split(b"".join([x, x]))
    #    exp = [ x, x ]
    #    assert v == exp, "%r != %r" % (v, exp )
        
    def __init__(self, *a, **kw):
        unittest.TestCase.__init__(self, *a, **kw)
        self._loadstr = None
        self._bufsize = 0
        
for m in dir(UbJsonTests):
    if m.startswith("def_"):
        func = getattr(UbJsonTests, m)
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
                    setattr(UbJsonTests, "test_%s_buf%04d_%s" % (m[4:], bufsize, mode), ff)
                addt(m, mode, bufsize, func)