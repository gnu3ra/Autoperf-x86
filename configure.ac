#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AC_CONFIG_SRCDIR([build/test.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lhwloc':
AC_CHECK_LIB([hwloc], [main])
AC_SEARCH_LIBS([PMPI_Init],[mpich])
AC_SEARCH_LIBS([PAPI_start_counters], [papi])
AC_SEARCH_LIBS([uuid_generate_time], [libuuid])



# Checks for header files.
AC_CHECK_HEADERS([limits.h stdint.h stdlib.h string.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strcasecmp strchr strdup strtol sysinfo])

AC_CONFIG_FILES([makefile
                 src/lib/makefile])
AC_OUTPUT
