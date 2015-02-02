No Nonsense FTP/TFTP Server
===========================
[![Travis Status]][Travis] [![Coverity Status]][Coverity Scan]

uftpd is a true UNIX FTP/TFTP daemon, it serves files, and nothing else.

If the host system has an `ftp` user, its `$HOME` is used to serve
files.  The FTP and TFTP ports are read from `/etc/services`.  If
anything is missing uftpd falls back to a set of sane defaults.

It just works.


Features
--------

* FTP and/or TFTP
* No configuration file
* Runs from standard UNIX inetd, or standalone
* Listens to `ftp/tcp` and `tftp/udp` found in `/etc/services`
* Serves files from the ftp user's `$HOME`, found in `/etc/passwd`
* Privilege separation, drops root privileges before serving files
* Possible to use symlinks outside of the home directory (INSECURE,
  but user friendly)
* Possible to use group writable home directory (INSECURE, but again
  user friendly)


Caveat
------

uftpd is not made for secure installations, it's made for home users and
developers.  This will always be the primary goal of uftpd.  As such it
allows symlinks outside the FTP home as well as a group writable FTP
home directory -- user-friendly features that can easily cause security
breaches, but also very useful for people who do not care and just wants
their FTP server to work.

Seriously, we do not advise you to ignore any security aspect of your
installation.  If security is a concern for you, consider using a
different server.

That being said, a lot of care has been taken to lock down and secure
uftpd by default.  So if you refrain from symlinking stuff from your
home directory and carefully setup more strict permissions on your FTP
home, then uftpd is likely as secrure as any other TFTP/FTP server.


Running
-------

Start uftpd by simply calling `sudo ./uftpd` after building it with
make.  That will start uftpd as a TFTP server.  To enable both FTP and
TFTP you need to call `sudo ./uftpd -f -t`, both `-f` and `-t` can be
given an alternative port as extra argument for either service.  There
are more command line options, e.g., for overriding the TFTP/FTP home
directory.  See the man page or the <kdb>`uftpd --help`</kdb> output.

It is however recommended to run uftpd from the Internet super server,
inetd.  Use the following two lines for `/etc/inetd.conf`:

    ftp		stream	tcp	nowait	root	/usr/sbin/tcpd	/usr/sbin/uftpd -i -f
    tftp	dgram	udp	wait	root	/usr/sbin/tcpd	/usr/sbin/uftpd -i -t

Remember to activate your changes to inetd by reloading the service or
sending SIGHUP.  Alternatively, you can build and install the [.deb]
package to install uftpd, it will take care of installing and setting up
inetd for you.  Use `make package` to build your own .deb file.


Origin & References
-------------------

Originally based on [FtpServer] by [Xu Wang], uftpd is a complete
rewrite with TFTP support.

uftpd is maintained by [Joachim Nilsson] at [GitHub].

[.deb]:            http://ftp.troglobit.com/uftpd/uftpd_1.7-1_amd64.deb
[Joachim Nilsson]: http://troglobit.com
[Xu Wang]:         https://github.com/xu-wang11/
[FtpServer]:       https://github.com/xu-wang11/FtpServer
[GitHub]:          https://github.com/troglobit/uftpd
[Travis]:          https://travis-ci.org/troglobit/uftpd
[Travis Status]:   https://travis-ci.org/troglobit/uftpd.png?branch=master
[Coverity Scan]:   https://scan.coverity.com/projects/2947
[Coverity Status]: https://scan.coverity.com/projects/2947/badge.svg
