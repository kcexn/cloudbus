SOURCE=../src
check_PROGRAMS =

if ENABLE_TESTS
check_PROGRAMS += tests
TEST_COMMON_CPPHEADERS = tests.hpp
nodist_tests_SOURCES = tests.cpp \
    test-config.cpp \
    test_config.hpp \
	$(TEST_COMMON_CPPHEADERS)
tests_LDADD = $(SOURCE)/libcbutils.a
tests_CXXFLAGS=-DTEST_SRCDIR=\"@srcdir@\"
endif

TESTS = $(check_PROGRAMS)
