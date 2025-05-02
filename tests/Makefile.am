SOURCE=../src
check_PROGRAMS =
LDADD = $(SOURCE)/libcbutils.a

if ENABLE_TESTS
check_PROGRAMS += test-config test-interface test-metrics
TEST_COMMON_CPPHEADERS = tests.hpp
nodist_test_config_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-config.cpp
nodist_test_interface_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-interface.cpp
nodist_test_metrics_SOURCES = $(TEST_COMMON_CPPHEADERS) \
	test-metrics.cpp
endif

TESTS = $(check_PROGRAMS)
