AC_INIT([uftpd], [2.7-dev], [https://github.com/troglobit/uftpd/issues],, [http://troglobit.com/uftpd.html])
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([src/uftpd.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile])

AC_PROG_CC
AC_PROG_LN_S
AC_PROG_INSTALL

# Configuration.
AC_HEADER_STDC
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_FUNCS(strstr getopt getsubopt gettimeofday)

# Check for uint[8,16,32]_t
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

# Check for required libraries
PKG_CHECK_MODULES([uev],  [libuev >= 2.2.0])
PKG_CHECK_MODULES([lite], [libite >= 1.5.0])

AC_OUTPUT
