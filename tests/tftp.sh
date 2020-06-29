#!/bin/sh
set -x
cd /tmp
tftp -4 127.0.0.1 6969 -m binary -c get testfile.txt
