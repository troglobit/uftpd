EXTRA_DIST         = README.md lib.sh unshare.sh ftp.sh tftp.sh oack.sh dupack.sh lockstep.sh rollover.sh wrq.sh zombies.sh ipv6.sh mlst.sh maxfiles.sh
CLEANFILES         = *~ *.trs *.log

TEST_EXTENSIONS    = .sh
TESTS_ENVIRONMENT  = $(srcdir)/unshare.sh

# Slowest tests first so they start early when 'make -j check' has fewer
# job slots than tests (e.g. on CI), minimizing total wall-clock time.
TESTS              = maxfiles.sh
TESTS             += rollover.sh
TESTS             += ftp.sh
TESTS             += tftp.sh
TESTS             += oack.sh
TESTS             += dupack.sh
TESTS             += lockstep.sh
TESTS             += wrq.sh
TESTS             += zombies.sh
TESTS             += ipv6.sh
TESTS             += mlst.sh
