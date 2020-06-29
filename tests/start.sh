#!/bin/bash
cd ../tests
cp /etc/passwd testfile.txt

../src/uftpd -n -o ftp=9013,tftp=6969 -l none . &
echo $! >/tmp/uftpd.pid

sleep 1
