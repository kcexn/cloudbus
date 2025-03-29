AC_PREREQ([2.71])
AC_INIT([cloudbus], [0.0.2], [kevin.exton@pm.me])
AC_CHECK_LIB(cares, ares_version)
AM_INIT_AUTOMAKE([-Wall subdir-objects foreign])
: ${CXXFLAGS=""}
AC_PROG_CXX
AM_PROG_AR
AC_PROG_RANLIB
AC_CONFIG_FILES([
    Makefile
    src/Makefile
])
AC_OUTPUT
