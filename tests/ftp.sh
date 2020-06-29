#!/bin/sh
cd /tmp
ftp -n 127.0.0.1 9013 <<--END
    verbose on
    user anonymous a@b
    bin
    get testfile.txt
    bye
END
