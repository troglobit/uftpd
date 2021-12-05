#!/bin/sh

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

tftp -4 127.0.0.1 6969 -m binary -c get testfile.txt
