LIB += testmanager
testmanager_SOURCES = testmanager.cpp integration.cpp testdb.cpp
testmanager_LDADD = -lbase
#mypanel_LDADD = -rdynamic
#WRAPPER += some_command_line_tool
#XMLLIST += some_xml
#XMLPATH = my_xml_path

VERSION = 5.0
LANGS = ru
MGR = testmanager
 
include ../isp.mk