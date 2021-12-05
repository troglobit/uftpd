EXTRA_DIST         = lib.sh ftp.sh tftp.sh maxfiles.sh
CLEANFILES         = *~ *.trs *.log

TEST_EXTENSIONS    = .sh
TESTS_ENVIRONMENT  = unshare -mrun

TESTS              = ftp.sh
TESTS             += tftp.sh
TESTS             += maxfiles.sh

.PRECIOUS: stop.log

