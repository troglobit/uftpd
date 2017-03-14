AC_INIT([uftpd],[2.2],[https://github.com/troglobit/uftpd/issues])
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([uftpd.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL

# Configuration.
AC_HEADER_STDC
AC_CHECK_FUNCS(strstr getopt getsubopt)

# Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 1.4.0])
PKG_CHECK_MODULES([lite], [libite >= 1.5.0])

AC_OUTPUT
