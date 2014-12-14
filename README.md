uftpd -- the no nonsense (T)FTP server
======================================
[![Build Status](https://travis-ci.org/troglobit/uftpd.png?branch=master)](https://travis-ci.org/troglobit/uftpd)
[![Coverity Scan Status](https://scan.coverity.com/projects/2947/badge.svg)](https://scan.coverity.com/projects/2947)

uftpd is a simple TFTP/FTP daemon that serves files -- that's it!

uftpd is a UNIX daemon in the true sense, it has no config file of its
own, instead it uses the host system's ftp user's home directory, found
in `/etc/passwd`, to serve files from, using the FTP and TFTP ports from
`/etc/services`.  If there is no ftp user in the system, uftpd defaults
to use `/srv/ftp`, if FTP or TFTP services are not listed, it defaults
to `21/tcp` and `69/udp`, respectively.

Features:

  * FTP and/or TFTP support
  * No configuration file
  * Run from inetd, or as a standalone daemon
  * Listens to ftp/tcp and tftp/udp found in /etc/services, by default
  * Serves files from the ftp user's $HOME, found in /etc/passwd
  * Privilege separation, drops root privileges before serving files
  * Possible to use symlinks outside of the home directory (INSECURE,
    but user friendly)
  * Possible to use group writable home directory (INSECURE, but again
    user friendly)

Patches are most welcome! :)


Caveat
------

uftpd is not made for secure installations, it's made for home users and
developers.  That will always be the primary goal of uftpd.  As such it
allows symlinks outside the FTP home as well as a group writable FTP
home directory -- user-friendly features that can easily cause security
breaches, but also very useful for people who do not care and just wants
their FTP server to work, dammit!

Seriously, the uftpd developers do not advise you to ignore any security
aspect of your installation.  If security is a concern for you, consider
using a different server.

That being said, a lot of care has been taken to lock down and secure
uftpd by default.  So if you refain from symlinking stuff from your home
directory and carefully setup more strict permissions on your FTP home,
then uftpd is likely as secrure as any other TFTP/FTP server.


Running
-------

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


Credits & Contact
-----------------

Originally based on [FtpServer](https://github.com/xu-wang11/FtpServer)
by [Xu Wang](mailto:wangxu.93@icloud.com), uftpd is a complete rewrite
with TFTP support.

uftpd is maintained by [Joachim Nilsson](mailto:troglobit@gmail.com) at
GitHub https://github.com/troglobit/uftpd

