SOURCE=../src
check_PROGRAMS =

if ENABLE_TESTS
check_PROGRAMS += test-config
TEST_COMMON_CPPHEADERS = tests.hpp
nodist_test_config_SOURCES = test-config.cpp \
	$(TEST_COMMON_CPPHEADERS)
test_config_LDADD = $(SOURCE)/libcbutils.a
test_config_CXXFLAGS=-DTEST_SRCDIR=\"@srcdir@\"
endif

TESTS = $(check_PROGRAMS)
