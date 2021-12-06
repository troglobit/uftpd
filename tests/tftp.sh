#!/bin/sh
#set -x

if [ x"${srcdir}" = x ]; then
    srcdir=.
fi
. ${srcdir}/lib.sh

get()
{
	tftp 127.0.0.1 -c get "$1"
	sleep 1
}

check_dep tftp
netstat -atnup

get testfile.txt
ls -la
[ -s testfile.txt ] && OK
FAIL

