#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.50])
AC_INIT([demucc], [0.001], [https://github.com/autch/demucc])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([demucc.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([inttypes.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strtol])


AS_IF([test "x`basename $CC`" = "xemcc"], [
  AC_DEFINE([DEMUCC_NO_MAIN], 1, [Do not define main()])
])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
