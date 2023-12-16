NULL =

SPEC_XML_FILES =			\
	cafe-screensaver.xml		\
	dbus-interface.xml		\
	$(NULL)

if DOCBOOK_DOCS_ENABLED

htmldocdir = $(DOCDIR)
htmldoc_DATA = cafe-screensaver.html

cafe-screensaver.html : dbus-interface.xml
	$(XMLTO) html-nochunks -m config.xsl cafe-screensaver.xml

endif # DOCBOOK_DOCS_ENABLED

EXTRA_DIST =				\
	cafe-screensaver.xml.in	\
	config.xsl			\
	docbook.css			\
	$(SPEC_XML_FILES)
	$(NULL)

clean-local:
	rm -f *~
	rm -f cafe-screensaver.html
