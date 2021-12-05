#!/bin/sh

# shellcheck source=/dev/null
. "$(dirname "$0")/lib.sh"

get()
{
    ftp -n 127.0.0.1 9013 <<-END
	verbose on
    	user anonymous a@b
	bin
	get testfile.txt
	bye
	END
}

get
