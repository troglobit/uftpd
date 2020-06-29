#!/bin/sh
cd /tmp
tftp -4 127.0.0.1 9014 -m binary -c get testfile.txt
