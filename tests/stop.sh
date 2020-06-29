#!/bin/bash
cd ../tests
rm -f testfile.txt

if [ -e /tmp/uftpd.pid ]; then
    PID=`cat /tmp/uftpd.pid`
    echo "Stopping uftpd PID $PID"
    kill -9 $PID
fi

rm -f /tmp/uftpd.pid
rm -f /tmp/testfile.txt
