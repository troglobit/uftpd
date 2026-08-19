#!/bin/sh
# TESTS_ENVIRONMENT wrapper.  Every test runs in its own mount, user, and
# network namespace so the suite needs no root and can run in parallel.
#
# Where that is unavailable -- a container without newuidmap, a Debian
# buildd, Ubuntu's AppArmor clamp on unprivileged user namespaces -- the
# tests would otherwise die with a bare exit 127 from unshare(1), which
# automake reports as a hard failure.  That breaks `make package`, since
# dh_auto_test runs the suite during the package build.
#
# We cannot exit 77 (automake's SKIP) from here: TESTS_ENVIRONMENT wraps
# the log driver, not the test, so the driver would never run and never
# record the result.  Instead tell lib.sh to skip, and let each test
# report it from inside the driver where the exit status is understood.

if unshare -mrun --map-auto true 2>/dev/null; then
	exec unshare -mrun --map-auto "$@"
fi

UFTPD_NO_USERNS=1
export UFTPD_NO_USERNS

exec "$@"
