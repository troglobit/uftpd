#!/bin/sh

# Test name, used everywhere as /tmp/uftpd/$NM/foo
NM=$(basename "$0" .sh)
DIR=/tmp/uftpd/$NM
CDIR=/tmp/uftpd/${NM}-client

# Print heading for test phases
print()
{
	printf "\e[7m>> %-76s\e[0m\n" "$1"
}

dprint()
{
	printf "\e[2m%-76s\e[0m\n" "$1"
}

SKIP()
{
	print "TEST: SKIP"
	[ $# -gt 0 ] && echo "$*"
	exit 77
}

FAIL()
{
	print "TEST: FAIL"
	[ $# -gt 0 ] && echo "$*"
	exit 99
}

OK()
{
	print "TEST: OK"
	[ $# -gt 0 ] && echo "$*"
	exit 0
}

# shellcheck disable=SC2068
check_dep()
{
    if [ -n "$2" ]; then
	if ! $@; then
	    SKIP "$* is not supported on this system."
	fi
    elif ! command -v "$1" >/dev/null; then
	SKIP "Cannot find $1, skipping test."
    fi
}

# Stop all lingering collectors and other tools
kill_pids()
{
	# shellcheck disable=SC2162
	if [ -f "$DIR/PIDs" ]; then
		while read ln; do kill "$ln" 2>/dev/null; done < "$DIR/PIDs"
		rm "$DIR/PIDs"
	fi
}

teardown()
{
	kill_pids
	sleep 1

	[ -d "${DIR}"  ] && rm -rf "${DIR}"
	[ -d "${CDIR}" ] && rm -rf "${CDIR}"
}

signal()
{
	echo
	if [ "$1" != "EXIT" ]; then
		print "Got signal, cleaning up"
	fi
	teardown
}

# props to https://stackoverflow.com/a/2183063/1708249
# shellcheck disable=SC2064
trapit()
{
	func="$1" ; shift
	for sig ; do
		trap "$func $sig" "$sig"
	done
}

setup()
{
	bindir=$(pwd)/../src
	ls -l $bindir
	ip link set lo up
	sleep 1
	cp /etc/passwd "${DIR}/testfile.txt"
	"${bindir}/uftpd" -l debug "$DIR" -p "$DIR/pid" >"$DIR/log"
	cd "${CDIR}" || exit 1
	return 0
}

# Runs once when including lib.sh
mkdir -p "${DIR}"
mkdir -p "${CDIR}"
touch "$DIR/PIDs"

# Call signal() on signals or on exit
trapit signal INT TERM QUIT EXIT

# Basic setup for all tests
setup