SOURCEDIR = src
SUBDIRS = $(SOURCEDIR)
bin_PROGRAMS = controller segment proxy

LDADD = $(SOURCEDIR)/libcbutils.a
CONTROLLER_CPPSOURCES = $(SOURCEDIR)/cloudbus/controller/controller_connector.cpp \
    $(SOURCEDIR)/cloudbus/controller/controller_marshaller.cpp \
    $(SOURCEDIR)/main.cpp

CONTROLLER_CPPHEADERS = $(SOURCEDIR)/cloudbus/controller/controller_connector.hpp \
    $(SOURCEDIR)/cloudbus/controller/controller_marshaller.hpp \
    $(SOURCEDIR)/cloudbus/controller/controller_interfaces.hpp

SEGMENT_CPPSOURCES = $(SOURCEDIR)/cloudbus/segment/segment_connector.cpp \
    $(SOURCEDIR)/cloudbus/segment/segment_marshaller.cpp \
    $(SOURCEDIR)/main.cpp
    
SEGMENT_CPPHEADERS = $(SOURCEDIR)/cloudbus/segment/segment_connector.hpp \
    $(SOURCEDIR)/cloudbus/segment/segment_marshaller.hpp \
    $(SOURCEDIR)/cloudbus/segment/segment_interfaces.hpp

PROXY_CPPSOURCES = $(SOURCEDIR)/cloudbus/proxy/proxy_connector.cpp \
    $(SOURCEDIR)/cloudbus/proxy/proxy_marshaller.cpp \
    $(SOURCEDIR)/main.cpp
    
PROXY_CPPHEADERS = $(SOURCEDIR)/cloudbus/proxy/proxy_connector.hpp \
    $(SOURCEDIR)/cloudbus/proxy/proxy_marshaller.hpp \
    $(SOURCEDIR)/cloudbus/proxy/proxy_interfaces.hpp

controller_SOURCES = $(CONTROLLER_CPPSOURCES) $(CONTROLLER_CPPHEADERS)
controller_CXXFLAGS = -D COMPILE_CONTROLLER
segment_SOURCES = $(SEGMENT_CPPSOURCES) $(SEGMENT_CPPHEADERS)
segment_CXXFLAGS = -D COMPILE_SEGMENT
proxy_SOURCES = $(PROXY_CPPSOURCES) $(PROXY_CPPHEADERS)
proxy_CXXFLAGS = -D COMPILE_PROXY