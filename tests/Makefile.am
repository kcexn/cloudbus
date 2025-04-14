TESTS = test-config
SOURCE=../src

check_PROGRAMS = test-config
TEST_COMMON_CPPHEADERS = tests.hpp
test_config_SOURCES = test-config.cpp \
    data/config.ini \
    $(TEST_COMMON_CPPHEADERS)
test_config_LDADD = $(SOURCE)/libcbutils.a
test_config_CXXFLAGS=-DTEST_SRCDIR=\"@srcdir@\"