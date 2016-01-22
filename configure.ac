# Use v2.61 for backwards compat with Ubuntu 12.04 LTS
AC_PREREQ([2.61])
AC_INIT([uftpd],[2.0],[https://github.com/troglobit/uftpd/issues])
AM_INIT_AUTOMAKE([1.11 foreign no-dist-gzip dist-xz])
AM_SILENT_RULES([yes])

AC_CONFIG_SRCDIR([uftpd.c])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile libite/Makefile libuev/Makefile libuev/examples/Makefile])

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

# Needed for the libraries
AM_PROG_AR
LT_INIT([disable-shared static])
AM_CONDITIONAL([ENABLE_EXAMPLES], [test "$enable_examples" = yes])

# Check if user has downloaded a GitHub ZIP file and attempt to fix it.
AC_MSG_CHECKING([status of GIT submodules])
if test ! -e libite/lite.h; then
    AC_MSG_RESULT([Missing!])
    AC_MSG_CHECKING([if we can update all submodules])
    run=`git submodule update --init`
    if test $? -ne 0; then
       AC_MSG_RESULT([Failed!  No Internet access?])
       exit 1
    fi
    AC_MSG_RESULT([Yes, all fixed!])
else
    AC_MSG_RESULT([OK])
fi

AC_OUTPUT
