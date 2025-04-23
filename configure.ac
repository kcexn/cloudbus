AC_PREREQ([2.71])
AC_INIT([cloudbus], [0.1.0], [kevin.exton@pm.me])
AC_ARG_ENABLE([tests],
	[AS_HELP_STRING([--enable-tests],
		[enable development tests (default=no)])],
	[enable_tests="$enableval"],
	[enable_tests=no])
AM_CONDITIONAL([ENABLE_TESTS],
	[test "x$enable_tests" = "xyes"])
AM_SILENT_RULES([yes])
AC_SEARCH_LIBS(
	[ares_version],
	[cares],
	[],
	[AC_MSG_ERROR([Version >= 1.18.1 of libcares and its development headers must be installed to build cloudbus.])]
)
AC_SEARCH_LIBS(
	[pcre2_compile_8],
	[pcre2-8],
	[],
	[AC_MSG_ERROR([libpcre2 and its development headers must be installed to build cloudbus.])]
)
AC_CHECK_HEADERS(
	[ares.h],
	[],
	[AC_MSG_ERROR([development headers for libcares version >= 1.81.1 must be installed to build cloudbus.])]
)
CPPFLAGS="$CPPFLAGS -DPCRE2_CODE_UNIT_WIDTH=8"
AC_CHECK_HEADERS(
	[pcre2.h],
	[],
	[AC_MSG_ERROR([development headers for libcares version >= 1.81.1 must be installed to build cloudbus.])]
)
: ${CXXFLAGS=""}
AC_PROG_CXX
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign])
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
