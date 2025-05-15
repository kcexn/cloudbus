ARFLAGS = cr
AM_CXXFLAGS = -O3
noinst_LIBRARIES = libcbutils.a

COMMON_CPPSOURCES = config/config.cpp \
    connector/connector_timerqueue.cpp \
	connector/connectors.cpp \
	formats/xmsg.cpp \
	interfaces/interfaces.cpp \
	io/sockbuf.cpp \
	io/poller.cpp \
	messages/messages.cpp \
	node/node.cpp \
	manager/manager.cpp \
	dns/dns.cpp \
    metrics/metrics.cpp \
    options/options.cpp \
    logging/logging.cpp

COMMON_CPPHEADERS = config/config.hpp \
    connector/connector_timerqueue.hpp \
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
	node.hpp \
	manager/manager.hpp \
	manager.hpp \
	dns/dns.hpp \
	dns/dns_poll.hpp \
	dns.hpp \
	metrics/metrics.hpp \
	metrics.hpp \
    options/options.hpp \
    options.hpp \
    logging/logging.hpp \
    logging.hpp
    
libcbutils_a_SOURCES = $(COMMON_CPPSOURCES) $(COMMON_CPPHEADERS)
