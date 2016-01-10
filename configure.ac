# Use v2.61 for backwards compat with Ubuntu 12.04 LTS
AC_PREREQ([2.61])
AC_INIT([uftpd],[2.0-dev],[https://github.com/troglobit/uftpd/issues])
AM_INIT_AUTOMAKE([1.11 foreign])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([uftpd.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL

dnl - Configuration.
AC_HEADER_STDC
AC_CHECK_FUNCS(strstr getopt getsubopt)

dnl Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_OUTPUT
