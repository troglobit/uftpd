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


Usage
-----

```
uftpd [-hnsv] [-l LEVEL] [-o ftp=PORT,tftp=PORT,writable] [PATH]

  -h         Show this help text
  -l LEVEL   Set log level: none, err, info, notice (default), debug
  -n         Run in foreground, do not detach from controlling terminal
  -o OPT     Options:
                      ftp=PORT
                      tftp=PORT
                      writable
  -s         Use syslog, even if running in foreground, default w/o -n
  -v         Show program version

The optional 'PATH' defaults to the $HOME of the /etc/passwd user 'ftp'
Bug report address: https://github.com/troglobit/uftpd/issues
```

To start uftpd in the background as an FTP/TFTP server:

    uftpd

If the `ftp` user does not exist on your system, `uftpd` defaults to
serve files from the `/srv/ftp` directory.  To serve another directory,
simply append that directory to the argument list.

Use `sudo`, or set `CAP_NET_BIND_SERVICE` capabilities, on `uftpd` to
allow regular users to start `uftpd` on privileged (standard) ports,
i.e. `< 1024`:

    sudo setcap cap_net_bind_service+ep uftpd

To change port on either FTP or TFTP, use:

    uftpd -o ftp=PORT,tftp=PORT

Set `PORT` to zero (0) to disable either service.

New sessions are droppbed by default if uftpd detects the FTP root is
writable.  To allow writable FTP root:

    uftpd -o writable PATH


Running from inetd
------------------

Rarely used services like FTP/TFTP are good candidates to run from the
Internet super server, inetd.  On Debian and Ubuntu based distributions
we recommend `openbsd-inetd`.

Use the following two lines in `/etc/inetd.conf`, notice how `in.ftpd`
and `in.tftpd` are symlinks to the `uftpd` binary:

    ftp     stream  tcp nowait  root    /usr/sbin/in.ftpd
    tftp    dgram   udp wait    root    /usr/sbin/in.tftpd

Remember to activate your changes to inetd by reloading the service or
sending `SIGHUP` to it.  Another inetd server may use different syntax.
Like the inetd that comes built-in to [Finit][], in `/etc/finit.conf`:

    inetd ftp/tcp   nowait /usr/sbin/in.ftpd  -- The uftpd FTP server
    inetd tftp/udp    wait /usr/sbin/in.tfptd -- The uftpd TFTP server


Caveat
------

uftpd is primarily not targeted at secure installations, it is targeted
at users in need of a *simple* FTP/TFTP server.

uftpd allows symlinks outside the FTP root, as well as a group writable
FTP home directory &mdash; user-friendly features that potentially can
cause security breaches, but also very useful for people who just want
their FTP server to work.  A lot of care has been taken, however, to
lock down and secure uftpd by default.


Build & Install
---------------

`uftpd` depends on two other projects to build from source, [libuEv][]
and [lite][].  See their respective README for details, there should be
no real surprises, both use the familiar configure, make, make install.

To find the two libraries uftpd depends on `pkg-config`.  The package
name for your Linux distribution varies, on Debian/Ubuntu systems:

```shell
user@example:~/> sudo apt install pkg-config
```

uftpd, as well as its dependencies, can be built as `.deb` packages on
Debian or Ubuntu based distributions.  Download and install each of the
dependencies, and then run

    ./autogen.sh      <--- Only needed if using GIT sources
    ./configure
    make package

The `.deb` package takes care of setting up `/etc/inetd.conf`, create an
`ftp` user and an `/srv/ftp` home directory with write permissions for
all members of the `users` group.

If you are using a different Linux or UNIX distribution, check the
output from `./configure --help`, followed by `make all install`.
For instance, building on [Alpine Linux](https://alpinelinux.org/):

    PKG_CONFIG_LIBDIR=/usr/local/lib/pkgconfig ./configure \
	    --prefix=/usr --localstatedir=/var --sysconfdir=/etc

Provided the library dependencies were installed in `/usr/local/`.  This
`PKG_CONFIG_LIBDIR` trick may be needed on other GNU/Linux, or UNIX,
distributions as well.


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
