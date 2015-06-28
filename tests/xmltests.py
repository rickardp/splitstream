import unittest
import os
try:
    from StringIO import StringIO
except ImportError:
    from io import BytesIO as StringIO
import tempfile
import splitstream

class XmlTests(unittest.TestCase):
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
    
    DATA_XMLRPC = b"<params><param><value><struct><member><name>a</name><value><int>3</int></value></member><member><name>x</name><value><array><data><value><int>1</int></value><value><double>3.14</double></value><value><boolean>1</boolean></value><value><nil/></value></data></array></value></member><member><name>b</name><value><nil/></value></member><member><name>c</name><value><dateTime.iso8601>20121001T12:34:00</dateTime.iso8601></value></member></struct></value></param></params>"
    
    def _do_split(self, string, **kw):
        f = self._loadstr(string)
        try:
            return list(splitstream.splitfile(f, "xml", bufsize=self._bufsize, **kw))
        finally:
            f.close()

    def def_SplitSingleXmlRpc(self):
        v = self._do_split(self.DATA_XMLRPC)
        assert v == [ self.DATA_XMLRPC ]

    def def_SplitWithPreamble(self):
        v = self._do_split(self.DATA_XMLRPC[1:], preamble=self.DATA_XMLRPC[0])
        assert v == [ self.DATA_XMLRPC ]

    def def_SplitWithLargePreamble(self):
        v = self._do_split(self.DATA_XMLRPC, preamble=self.DATA_XMLRPC + self.DATA_XMLRPC)
        assert len(v) == 3, "%d != 3" % len(v)
        assert v == [ self.DATA_XMLRPC, self.DATA_XMLRPC, self.DATA_XMLRPC ], "%r != %r" % (v, [ self.DATA_XMLRPC, self.DATA_XMLRPC, self.DATA_XMLRPC ])
        
    def def_SplitTwoSimpleXml(self):
        v = self._do_split(b"<root></root><root2/>")
        assert v == [ b"<root></root>",b"<root2/>" ]

    def def_SplitTwoXmlRpc(self):
        v = self._do_split(self.DATA_XMLRPC + self.DATA_XMLRPC)
        assert v == [ self.DATA_XMLRPC, self.DATA_XMLRPC ]
        
    def def_SplitHugeXmlRpc(self):
        v = self._do_split(b''.join([ self.DATA_XMLRPC ] * (1<<18)))
        assert len(v) == 1<<18, "Length %d != %d" % (len(v), 1<<18)
        assert v[0] ==  self.DATA_XMLRPC 
        assert v[1] ==  self.DATA_XMLRPC 
        assert v[-1] ==  self.DATA_XMLRPC 
        assert v[-2] ==  self.DATA_XMLRPC 
        
    def def_SplitHugeOneXmlRpc(self):
        vv = b''.join([ "<blob>" ] + [ self.DATA_XMLRPC ] * (1<<17) + [ "</blob>" ])
        v = self._do_split(vv)
        assert len(v) == 1, "Length %d != %d" % (len(v), 1)
        assert v[0] == vv
        
    def def_TwoXmlDocumentsWithLeadingComments(self):
        v = self._do_split(b"<!-- First document begins here--><root></root><!-- Second document begins here --><root2/>")
        assert v == [ b"<!-- First document begins here--><root></root>", b"<!-- Second document begins here --><root2/>" ]
        
    def def_TwoXmlDocumentsWithWhitespace(self):
        v = self._do_split(b"  <root></root>\r\n<root2/>")
        assert v == [ b"<root></root>", b"<root2/>" ]
        
    def def_XmlDocumentsWithStartDepth(self):
        v = self._do_split(b"  <logfile>  <logent val=\"x\"></logent>\r\n<logent val=\"y\"></logent><logent val=\"z\"></logent>", startdepth=1)
        exp = [ b"<logent val=\"x\"></logent>",
                      b"<logent val=\"y\"></logent>",
                      b"<logent val=\"z\"></logent>" ]
        assert v == exp, "%r != %r" % (v, exp)
                      
    def def_TwoXmlDocumentsWithDoctype(self):
        v = self._do_split(b"<!doctype misc><root></root>\r\n<!doctype misc><root2/>")
        assert v == [ b"<!doctype misc><root></root>", b"<!doctype misc><root2/>" ]
                      
    def def_TwoXmlDocumentsWithDoctypeElement(self):
        v = self._do_split(b"<root/><!DOCTYPE doc SYSTEM \"001.ent\" [\n<!ELEMENT doc EMPTY>/n]>\n<doc></doc>")
        assert v == [ b"<root/>", b"<!DOCTYPE doc SYSTEM \"001.ent\" [\n<!ELEMENT doc EMPTY>/n]>\n<doc></doc>" ]
                      
    def def_TwoXmlDocumentsWithCdata(self):
        v = self._do_split(b"<root><![CDATA[ <root></root> ]]></root>\r\n<root2><![CDATA[ >> \" ]]></root2>")
        assert v == [ b"<root><![CDATA[ <root></root> ]]></root>", b"<root2><![CDATA[ >> \" ]]></root2>" ]
                      
    def def_TwoXmlDocumentsWithAngleBracketComments(self):
        v = self._do_split(b"<root><!-- Weird <> comment --></root><root2/>")
        assert v == [ b"<root><!-- Weird <> comment --></root>", b"<root2/>" ]
        
        
    def __init__(self, *a, **kw):
        unittest.TestCase.__init__(self, *a, **kw)
        self._loadstr = None
        self._bufsize = 0
        
for m in dir(XmlTests):
    if m.startswith("def_"):
        func = getattr(XmlTests, m)
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
                    setattr(XmlTests, "test_%s_buf%04d_%s" % (m[4:], bufsize, mode), ff)
                addt(m, mode, bufsize, func)