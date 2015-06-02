import unittest
import os
from StringIO import StringIO
import tempfile
import splitstream

class XmlTests(unittest.TestCase):
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
    
    DATA_XMLRPC = "<params><param><value><struct><member><name>a</name><value><int>3</int></value></member><member><name>x</name><value><array><data><value><int>1</int></value><value><double>3.14</double></value><value><boolean>1</boolean></value><value><nil/></value></data></array></value></member><member><name>b</name><value><nil/></value></member><member><name>c</name><value><dateTime.iso8601>20121001T12:34:00</dateTime.iso8601></value></member></struct></value></param></params>"
    
    def _do_split(self, string, startdepth=0):
        f = self._loadstr(string)
        return splitstream.splitfile(f, "xml", bufsize=self._bufsize, startdepth=startdepth)

    def def_SplitSingleXmlRpc(self):
        v = self._do_split(self.DATA_XMLRPC)
        assert v == [ self.DATA_XMLRPC ]
        
    def def_SplitTwoSimpleXml(self):
        v = self._do_split("<root></root><root2/>")
        assert v == [ "<root></root>","<root2/>" ]

    def def_SplitTwoXmlRpc(self):
        v = self._do_split(self.DATA_XMLRPC + self.DATA_XMLRPC)
        assert v == [ self.DATA_XMLRPC, self.DATA_XMLRPC ]
        
    def def_SplitHugeXmlRpc(self):
        v = self._do_split(''.join([ self.DATA_XMLRPC ] * (1<<18)))
        assert len(v) == 1<<18
        assert v[0] ==  self.DATA_XMLRPC 
        assert v[1] ==  self.DATA_XMLRPC 
        assert v[-1] ==  self.DATA_XMLRPC 
        assert v[-2] ==  self.DATA_XMLRPC 
        
    def def_TwoXmlDocumentsWithLeadingComments(self):
        v = self._do_split("<!-- First document begins here--><root></root><!-- Second document begins here --><root2/>")
        assert v == [ "<!-- First document begins here--><root></root>", "<!-- Second document begins here --><root2/>" ]
        
    def def_TwoXmlDocumentsWithWhitespace(self):
        v = self._do_split("  <root></root>\r\n<root2/>")
        assert v == [ "<root></root>", "<root2/>" ]
        
    def def_XmlDocumentsWithStartDepth(self):
        v = self._do_split("  <logfile>  <logent val=\"x\"></logent>\r\n<logent val=\"y\"></logent><logent val=\"z\"></logent>", startdepth=1)
        assert v == [ "<logent val=\"x\"></logent>",
                      "<logent val=\"y\"></logent>",
                      "<logent val=\"z\"></logent>" ]
                      
    def def_TwoXmlDocumentsWithDoctype(self):
        v = self._do_split("<!doctype misc><root></root>\r\n<!doctype misc><root2/>")
        assert v == [ "<!doctype misc><root></root>", "<!doctype misc><root2/>" ]
                      
    def def_TwoXmlDocumentsWithDoctypeElement(self):
        v = self._do_split("<root/><!DOCTYPE doc SYSTEM \"001.ent\" [\n<!ELEMENT doc EMPTY>/n]>\n<doc></doc>")
        assert v == [ "<root/>", "<!DOCTYPE doc SYSTEM \"001.ent\" [\n<!ELEMENT doc EMPTY>/n]>\n<doc></doc>" ]
                      
    def def_TwoXmlDocumentsWithCdata(self):
        v = self._do_split("<root><![CDATA[ <root></root> ]]></root>\r\n<root2><![CDATA[ >> \" ]]></root2>")
        assert v == [ "<root><![CDATA[ <root></root> ]]></root>", "<root2><![CDATA[ >> \" ]]></root2>" ]
                      
    def def_TwoXmlDocumentsWithAngleBracketComments(self):
        v = self._do_split("<root><!-- Weird <> comment --></root><root2/>")
        assert v == [ "<root><!-- Weird <> comment --></root>", "<root2/>" ]
        
        
    def __init__(self, *a, **kw):
        unittest.TestCase.__init__(self, *a, **kw)
        self._loadstr = None
        self._bufsize = 0
        
for m in dir(XmlTests):
    if m.startswith("def_"):
        print m
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