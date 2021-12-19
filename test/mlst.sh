#!/bin/sh
#set -x

if [ x"${srcdir}" = x ]; then
    srcdir=.
fi
. ${srcdir}/lib.sh

cmd()
{
	tnftp anonymous@127.0.0.1 <<-EOF
		cd foo
		ls
		$*
		EOF
}

check_dep tnftp

cmd mlst bar |grep -q "perm=lepc;type=dir; bar" || FAIL "missing bar"
cmd mlst baz |grep -q "perm=rw;size=0;type=file; baz"  || FAIL "missing baz"

OK
