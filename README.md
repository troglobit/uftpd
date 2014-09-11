uftpd -- the no nonsense (T)FTP server
======================================
[![Build Status](https://travis-ci.org/troglobit/uftpd.png?branch=master)](https://travis-ci.org/troglobit/uftpd)[![Coverity Scan Status](https://scan.coverity.com/projects/2947/badge.svg)](https://scan.coverity.com/projects/2947)

uftpd is a simple FTP/TFTP daemon that serves files, that's it.  It
allows symlinks and a group writable FTP home directory -- it does not
even need to be configured!  If there is no ftp user it defaults to the
"standard" `/srv/ftp` directory.

By default it uses the host system's ftp user's home directory to serve
files from, and it finds the FTP and TFTP ports from `/etc/services`,
so it's very UNIX like.

Features:

  * FTP and/or TFTP support
  * No configuration file
  * Listens to ftp/tcp and tftp/udp found in /etc/services, by default
  * Serves files from the ftp user's $HOME, found in /etc/passwd
  * Possible to use symlinks outside of the home directory (INSECURE)
  * Use group writable home directory (INSECURE)
  * Run from inetd, or as a standalone daemon

Patches are most welcome! :)

Start uftpd by simply calling `sudo ./uftpd` after building it with
make.  That will start uftpd as a TFTP server.  To enable both FTP and
TFTP you need to call `sudo ./uftpd -f -t`, both `-f` and `-t` can be
given an alternative port as extra argument for either service.  There
is no way to configure the TFTP/FTP home directory though, just change
the `ftp` user's `/etc/passwd` entry for that.

It is however recommended to run uftpd from the Internet super server,
inetd.  Use the following two lines for `/etc/inetd.conf`:

    ftp		stream	tcp	nowait	root	/usr/sbin/tcpd	/usr/sbin/uftpd -i -f
    tftp	dgram	udp	wait	root	/usr/sbin/tcpd	/usr/sbin/uftpd -i -t

Remember to reload inetd after adding these lines.  Alternatively you
can build and install the .deb package to install uftpd, it will take
care of installing and setting up inetd for you.  Use `make package`

Originally based on [FtpServer](https://github.com/xu-wang11/FtpServer)
by [Xu Wang](mailto:wangxu.93@icloud.com), uftpd is a complete rewrite
with TFTP support.

uftpd is maintained by [Joachim Nilsson](mailto:troglobit@gmail.com) at
GitHub https://github.com/troglobit/uftpd

