No Nonsense FTP/TFTP Server
===========================
[![License Badge][]][License] [![Travis Status][]][Travis] [![Coverity Status][]][Coverity Scan]

uftpd is a UNIX daemon with sane built-in defaults.  It just works.


Features
--------

* FTP and/or TFTP
* No complex configuration file
* Runs from standard UNIX inetd, or standalone
* Uses `ftp` user's `$HOME`, from `/etc/passwd`, or custom path
* Uses `ftp/tcp` and `tftp/udp` from `/etc/services`, or custom ports
* Privilege separation, drops root privileges having bound to ports
* Possible to use symlinks outside of the FTP home directory
* Possible to have group writable FTP home directory


Caveat
------

uftpd is primarily not targetted at secure installations, it is targeted
at home users and developers in need of a simple FTP/TFTP server.  uftpd
allows symlinks to outside the FTP home, as well as a group writable FTP
home directory &mdash; user-friendly features that potentially can cause
security breaches, but also very useful for people who just want their
FTP server to work.

*Seriously*, we do not advise you to ignore any security aspect of your
installation.  If security is a concern for you, consider using another
FTP/TFTP server!

That being said, a lot of care has been taken to lock down and secure
uftpd by default.  So, if you refrain from symlinking stuff from your
home directory and take care to set up strict permissions, then uftpd is
likely as secure as any other FTP/TFTP server.


Download
--------

uftpd depend on two other projects to build from source, [libuEv][] and
[lite][].  See their respective README's for details, there should be no
real surprises, both use the familiar configure, make, make install.

See below if you want to contribute.


Building
--------

uftpd, as well as its dependencys, can be built as `.deb` packages on
Debian or Ubuntu based distributions.  Simply download each source
component and run

    ./autogen.sh      <--- Only needed if using GIT sources
    ./configure
    make package

If you are using a different Linux or UNIX distribution, check the
output from `./configure --help`, followed by `make all install`.
For instance, building on [Alpine Linux](https://alpinelinux.org/):

    PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure \
	    --prefix=/usr --localstatedir=/var --sysconfdir=/etc

Provided the library dependencies were installed in `/usr/local/`.  This
`PKG_CONFIG_LIBDIR` trick may be needed on other GNU/Linux, or UNIX,
distributions as well.


Running
-------

To start uftpd in the background as an FTP/TFTP server, simply start the
daemon, possibly using `sudo`:

    $ uftpd

To change port on either FTP or TFTP, use

    $ uftpd -o ftp=PORT,tftp=PORT

Set `PORT` to zero (0) to disable either service.  Use `sudo`, or set
`CAP_NET_BIND_SERVICE` capabilities, on `uftpd` to allow regular users
to start `uftpd` on privileged (standard) ports, i.e. `< 1024`:

    $ sudo setcap cap_net_bind_service+ep uftpd

### Running from inetd ###

To run uftpd from the Internet super server, inetd¹, use the following
two lines in `/etc/inetd.conf`, notice how `in.ftpd` and `in.tftpd` are
symlinks to the `uftpd` binary:

    ftp     stream  tcp nowait  root    /usr/sbin/in.ftpd
    tftp    dgram   udp wait    root    /usr/sbin/in.tftpd

Remember to activate your changes to inetd by reloading the service or
sending `SIGHUP` to it.  Another inetd server may use different syntax.
Like the inetd that comes built-in to [Finit][], in `/etc/finit.conf`:

    inetd ftp/tcp   nowait /usr/sbin/in.ftpd  -- The uftpd FTP server
    inetd tftp/udp    wait /usr/sbin/in.tfptd -- The uftpd TFTP server

____
¹ Recommended inetd: [openbsd-inetd](apt:openbsd-inetd)


Origin & References
-------------------

uftpd was originally based on [FtpServer][] by [Xu Wang][], but is now a
complete rewrite with TFTP support by [Joachim Nilsson][], maintained at
[GitHub][].


[Joachim Nilsson]: http://troglobit.com
[the FTP]:         http://ftp.troglobit.com/uftpd/
[Xu Wang]:         https://github.com/xu-wang11/
[FtpServer]:       https://github.com/xu-wang11/FtpServer
[GitHub]:          https://github.com/troglobit/uftpd
[Finit]:           https://github.com/troglobit/finit
[lite]:            https://github.com/troglobit/libite
[libuEv]:          https://github.com/troglobit/libuev
[License]:         https://en.wikipedia.org/wiki/ISC_license
[License Badge]:   https://img.shields.io/badge/License-ISC-blue.svg
[Travis]:          https://travis-ci.org/troglobit/uftpd
[Travis Status]:   https://travis-ci.org/troglobit/uftpd.png?branch=master
[Coverity Scan]:   https://scan.coverity.com/projects/2947
[Coverity Status]: https://scan.coverity.com/projects/2947/badge.svg

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
