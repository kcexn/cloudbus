AC_PREREQ([2.71])
AC_INIT([cloudbus], [0.0.4], [kevin.exton@pm.me])
AC_ARG_ENABLE([tests],
	[AS_HELP_STRING([--enable-tests],
		[enable development tests (default=no)])],
	[enable_tests="$enableval"],
	[enable_tests=no])
AM_CONDITIONAL([ENABLE_TESTS],
	[test "x$enable_tests" = "xyes"])
AC_SEARCH_LIBS(
	[ares_version], [cares],
	[],
	[AC_MSG_ERROR([Version >= 1.18.1 of libcares and its development headers must be installed to build cloudbus.])]
)
AC_CHECK_HEADERS(
	[ares.h], [],
	[AC_MSG_ERROR([development headeres for libcares version >= 1.81.1 must be installed to build cloudbus.])]
)
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign])
: ${CXXFLAGS=""} AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
AC_CONFIG_FILES([
    Makefile
    src/Makefile
    tests/Makefile
    conf/systemd/controller.service
    conf/systemd/segment.service
])
AC_OUTPUT
