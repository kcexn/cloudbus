
ARFLAGS = cr
noinst_LIBRARIES = libcbutils.a

COMMON_CPPSOURCES = config/config.cpp \
    connector/connectors.cpp \
    formats/xmsg.cpp \
    interfaces/interfaces.cpp \
    io/sockbuf.cpp \
    io/poller.cpp \
    messages/messages.cpp \
    node/node.cpp

COMMON_CPPHEADERS = config/config.hpp \
    connector/connectors.hpp \
    formats/xmsg.hpp \
    interfaces/interfaces.hpp \
    io/io.hpp \
    io/buffers.hpp \
    io/streams.hpp \
    marshallers/marshallers.hpp \
    messages/messages.hpp \
    node/node.hpp \
    config.hpp \
    connectors.hpp \
    formats.hpp \
    interfaces.hpp \
    io.hpp \
    marshallers.hpp \
    messages.hpp \
    node.hpp
    
libcbutils_a_SOURCES = $(COMMON_CPPSOURCES) $(COMMON_CPPHEADERS)
