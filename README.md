uftpd -- the no nonsense (T)FTP server
======================================
[![Build Status](https://travis-ci.org/troglobit/uftpd.png?branch=master)](https://travis-ci.org/troglobit/uftpd)

uftpd is not for everyone ... it's for developers, home users, and
really _not_ for Internet use!  It's a very simple daemon that likely
works right out of the box for most, if not all, use-cases.  If it
doesn't it's likely a bug!

Features:

  * FTP and/or TFTP support
  * No configuration file
  * Listens to ftp/tcp and tftp/udp found in /etc/services, by default
  * Serves files from the ftp user's $HOME, found in /etc/passwd
  * Possible to use symlinks outside of the home directory (INSECURE)
  * Use group writable home directory (INSECURE)
  * Run from inetd, or as a standalone daemon

Patches are most welcome! :)

It is recommended to run uftpd from the Internet super server, inetd.
Use the following two lines for `/etc/inetd.conf`:

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

