#!/bin/sh

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

#max=`ulimit -n`
max=1040

# check beyond max to verify uftpd doesn't leak descriptors
max=$((max + 20))

get()
{
    ftp -n 127.0.0.1 9013 <<-END
    	user anonymous a@b
        get testfile.txt
	bye
	END
}

i=1
while [ $i -lt $max ]; do
    get
    rm testfile.txt
    i=$((i + 1))
done
