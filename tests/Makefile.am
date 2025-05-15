SOURCE=../src
check_PROGRAMS =
LDADD = $(SOURCE)/libcbutils.a

if ENABLE_TESTS
check_PROGRAMS += test-config \
    test-interface \
    test-metrics \
    test-messages \
    test-logging \
    test-connector
TEST_COMMON_CPPHEADERS = tests.hpp
nodist_test_config_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-config.cpp
nodist_test_interface_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-interface.cpp
nodist_test_metrics_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-metrics.cpp
nodist_test_messages_SOURCES = $(TEST_COMMON_CPPHEADERS) \
    test-messages.cpp
nodist_test_logging_SOURCES = $(TEST_COMMON_CPPHEADERS) \
    test-logging.cpp
nodist_test_connector_SOURCES = $(TEST_COMMON_CPPHEADERS) \
    test-connector.cpp    
endif

TESTS = $(check_PROGRAMS)
