Change Log
==========

All notable changes to the project are documented in this file.

[v2.6][UNRELEASED]
------------------

Bug fix release.

### Fixes

- Issue #16: 100% CPU when client session exits
- Flush stdout logging when running in the foreground


[v2.5][] - 2018-06-06
---------------------

The VLC Android app release.

### Changes
- Support for `ABOR` FTP command, issue #14
- Support for `REST` FTP command, issue #13
- Support for `EPSV` and `EPSV ALL` FTP commands, issue #11
- Basic support for `MLST` and `MLSD` FTP commands to provide support
  for the VLC android app., issue #9 and #12
- Add `OPTS MLST <ARG>` to let client manage order of facts listed
  in `MLST` and `MLSD` calls
- Add `CDUP` FTP convenience command, alias to `CWD ..`
- Add `DELE` FTP command to delete files
- Add `MKD` and `RMD` FTP commands to create and remove directories
- Refactor `LIST`, `RETR`, `STOR` and `PASV` FTP commands for speed

### Fixes
- Really fix 100% CPU problem, issue #9.  Multiple failure modes in
  libuEv and improper handling of `waitpid()` in event loop callback
- Use libuEv callback also for `PASV` FTP connections
- Fix `NLST` + `LIST` line endings, must be \r\n


[v2.4][] - 2017-09-03
---------------------

Bug fix release.

### Changes
- Handle non-chrooted use-cases better, ensure CWD starts with /
- Increased default inactivty timer: 20 sec --> 180 sec
- Ensure FTP `PASV` and `PORT` sockets are set non-blocking to prevent
  blocking the event loop
- [README.md][] updates, add usage section and improve build + install

### Fixes
- Fix 100% CPU issue.  Triggered sometimes when a user issued `CWD ..`


[v2.3][] - 2017-03-22
---------------------

Bug fix release.

### Changes
- Add support for `MDTM`, modify time, some clients rely this
- Add support for correct `SIZE` when in ASCII mode
- Add basic code of conduct to project
- Add contributing guidelines, automatically referenced by GitHub
  when filing a bug report or pull request

### Fixes
- Fix 100% CPU bug caused by `RETR` of non-regular file or directory
- Fix segfault on missing FTP home
- Fix ordering issue in fallback FTP user handling, introduced in v2.2
- Fix error message on `CWD` to non-directory
- Fix `.deb` generation and debconf installation/reconfigure issues


[v2.2][] - 2017-03-14
---------------------

### Changes
- Sort directories first in FTP `LIST` command
- Make sure to exit all lingering FTP sessions on exit
- Logging: reduced verbosity of common FTP commands
- Logging: show client address on failed file retrieval
- Full Debian/Ubuntu `.deb` build support, including debconf,
  asking user what services (FTP and/or TFTP) to run.
- Verify FTP/TFTP root directory is not writable by default
- New option to allow writable FTP/TFTP root, disabled by default

### Fixes
- Fix FTP directory listings, was off-by-one, one entry missing
- Issue #7: Spelling error in `README.md`
- Issue #8: Install missing symlinks for `in.ftpd.8` and `in.tftpd.8`


[v2.1][] - 2016-06-05
---------------------

### Changes
- Remove GIT submodules for libuEv and libite, these two libraries
  are now required to be installed separately.
- The output from `uftpd -v` now only shows the version.


[v2.0.2][] - 2016-02-02
-----------------------

Minor fix release.

### Fixes
- Distribution build fixes for companion libraries
- Missing critical files in uftpd distribution


[v2.0.1][] - 2016-02-02
-----------------------

Minor fix release.

### Changes
- Upgrade to [libite][] v1.4.2 (GCC 6 bug fixes)

### Fixes
- IPv6 address conversion error, found by GCC 6
- Make install of symlinks for `in.tftpd` & `in.ftpd` idempotent. Check
  any existing `in.ftpd` and `in.tftpd` symlinks before bugging out.
  Fixes problem of uftpd install failing on already existing symlinks.


[v2.0][] - 2016-01-22
---------------------

Sleak, smart, simple ... UNIX

### Changes
- Greatly simplified command line syntax
- Run inetd services by calling `in.ftpd` and `in.tftpd` symlinks
- Migrate to GNU configure and build system
- Update and simplify man page
- Build statically against bundled versions of libite (LITE) and libuEv
- Update bundled libuEv to v1.3.0
- Update bundled libite to v1.4.1

### Fixes
- Do not allow VERSION to be overloaded by build system
- Do not enforce any optimization in Makefile, this is up to the user
- Minor fixes to redundant error messages when running as a regular user


[v1.9.1][] - 2015-09-27
-----------------------

Minor fix release.

### Changes
- Upgrade to [libuEv][] v1.2.3 (bug fixes)
- Upgrade to [libite][] v1.1.1 (bug fixes)
- Add support for linking against external libuEv and libite

### Fixes
- Misc. README updates
- Check if libite or libuEv are missing as submodules


[v1.9][] - 2015-07-23
---------------------

Bug fix release.  FTP and TFTP sessions can now run fully in parallel,
independent of each other.  Also improved compatibility with Firefox
built-in FTP client and wget.

### Changes
- Upgrade to [libuEv][] v1.2.1+ for improved error handling and a much
  cleaner API.
- Major refactor of both FTP and TFTP servers to use libuEv better.
- Move to use [libite][] v1.0.0 for `strlcpy()`, `strlcat()`, `pidfile()`
  and more.
- Add proper session timout to TFTP, like what FTP already has.
- Add support for `NLST` FTP command, needed for multiple get operations.
  This fixes issue #2, thanks to @oz123 on GitHub for pointing this out!
- Add support for `FEAT` and `HELP` FTP commands used by some clients.

### Fixes
- Fix issue #3: do not sleep 2 sec before exiting.  Simply forward the
  `SIGTERM` to any FTP/TFTP session in progress, yield the CPU to let
  the child sessions handle the signal, and then exit.  Much quicker!
- Fix issue #4: due to an ordering bug between the main process calling
  `daemon()` and `sig_init()`, we never got the `SIGCHILD` to be able to
  reap any exiting FTP/TFTP sessions.  This resulted in zombies(!) when
  *not* being called as `uftpd -n`
- Fix issue #5: `LIST` and `NLST` ignores path argument sent by client.
- Fix issue #6: FTP clients not detecting session timeout.  Caused by
  uftpd not performing a proper `shutdown()` on the client socket(s)
  before `close()`.
- Fix problem with [libuEv][] not being properly cleaned on `distclean`.
- Fix problem with uftpd not exiting client session properly when client
  simply closes the connection.


[v1.8][] - 2015-02-02
---------------------

### Changes
- Updated [README.md][]
- Add [TODO.md][]
- Add [CHANGELOG.md][], attempt to align with <http://keepachangelog.com>
- From now on [Travis-CI][] only runs when pushing to the dev branch,
  so all new development must be done there.
- Upgrade to [libuEv][] v1.0.4

### Fixes
- Fix insecure `chroot()` reported in [Coverity Scan][] CID #54523.
- Minor cleanup fixes.


[v1.7][] - 2014-12-21
---------------------

The TFTP Blocksize Negotiation release.

### Changes
- Support for [RFC 2348][], TFTP blocksize negotiation
- Support for custom server directory, instead of FTP user's `$HOME`
- Log to `stderr` when running in foreground and debug is enabled


[v1.6][] - 2014-09-12
---------------------

Fix missing [libuEv][] directory content generated by <kbd>make dist</kbd>
in [v1.3][], [v1.4][], and [v1.5][].

### Fixes
- Since the introduction of the event library [libuEv][] the <kbd>make
  dist</kbd> target has failed to include the libuev sub-directory.
  This is due to the `git archive` command unfortunately not supporting
  git sub-modules.


[v1.5][] - 2014-09-12 [YANKED]
------------------------------

Major fix release, lots of issues reported by [Coverity Scan][] fixed.
For details, see <https://scan.coverity.com/projects/2947>

**Note:** This release has been *yanked* from distribution due to the
tarball (generated by the <kbd>make dist</kbd>) missing the required
libuEv library.  Instead, use [v1.6][] or later, where this is fixed, or
roll your own build of this release from the GIT source tree.

### Changes
- Add support for [Travis-CI][], continuous integration with GitHub
- Add support for [Coverity Scan][], the best static code analyzer,
  integrated with [Travis-CI][] -- scan runs for each push to master

### Fixes
- Fix nasty invalid `sizeof()` argument to `recv()` causing uftpd to
  only read 4/8 bytes (32/64 bit arch) at a time from the FTP socket.
  This should greatly reduce CPU utilisation and improve xfer speeds.
  Found by [Coverity Scan][].
- Fix minor resource leak in `ftp_session()` when `getsockname()` or
  `getpeername()` fail.  Minor fix because the session exits and the OS
  usually frees resources at that point, unless you're using uClinux.
  Found by [Coverity Scan][].
- Various fixes for unchecked API return values, prevents propagation of
  errors.  Also, make sure to clear input data before calling API's.
  Found by [Coverity Scan][].
- Fix oversight in checking for invalid/missing FTP username.
  Found by [Coverity Scan][].
- Fix potential attack vector.  Make sure to always store a NUL string
  terminator in all received FTP commands so the parser does not go out
  of bounds. Found by [Coverity Scan][].
- Fix parallel build problems in `Makefile`.


[v1.4][] - 2014-09-04 [YANKED]
------------------------------

**Note:** This release has been *yanked* from distribution due to the
tarball (generated by the <kbd>make dist</kbd>) missing the required
libuEv library.  Instead, use [v1.6][] or later, where this is fixed, or
roll your own build of this release from the GIT source tree.

### Changes
- Update documentation, both built-in usage text and man page.

### Fixes
- Fix bug in inetd.conf installed by .deb package for TFTP service.
  Inetd forked off a new TFTP session for each connection attempt.


[v1.3][] - 2014-09-04 [YANKED]
------------------------------

Added support for TFTP, [RFC 1350][].  Integration of the asynchronous
event library [libuEv][], to serialize all events.  Massive refactoring.

**Note:** This release has been *yanked* from distribution due to the
tarball (generated by the <kbd>make dist</kbd>) missing the required
libuEv library.  Instead, use [v1.6][] or later, where this is fixed, or
roll your own build of this release from the GIT source tree.

### Changes
- Incompatible changes to the command line arguments, compared to v1.2!
- Add libuEv as a GIT submodule, handles signals, timers, and all I/O.
- Refactor all signal handling, timers, and socket `poll()` calls to
  use libuEv instead.  Much cleaner and maintaiable code as a result.
- Clarify copyright claims, not much remains of the original [FtpServer][]
  code, by [Xu Wang][].


[v1.2][] - 2014-05-19
---------------------

### Changes
- Add support for logging to stdout as well as syslog.

### Fixes
- Fix embarrassing problem with listing big/average sized directories.


[v1.1][] - 2014-05-04
---------------------

Haunted zombie (¬°-°)¬ release.

### Changes
- Add strict FTP session inactivity timer, 20 sec.
- Change some logs to informational, only seen in verbose `-V` mode.
- Revise .deb package slightly and add support for creating an FTP user
  and group on the system.  This is used to both find the default FTP
  home directory, to serve files from, and also the UID/GID to drop to
  when being started as root.

### Fixes
- Fix zombie problem.  Forked off FTP sessions did not exit properly and
  were not `wait()`'ed for properly, so uftpd left a zombie processes
  lingering after each session.
- Fix ordering bug in security mechanism "drop privs"


[v1.0][] - 2014-05-04
---------------------

First official uftpd release! :-)

### Changes
- Forked from [FtpServer][], by [Xu Wang][].
- Add permissive [ISC license][].
- Massive refactor, code cleanup/renaming and "UNIX'ification":
  - Add actual command line parser.
  - Cleanup all log messages.
  - Reindent to use Linux KNF.
  - Use system's FTP user to figure out FTP home directory, with
    built-in fallback to `/srv/ftp`
  - Use system's `ftp/tcp` port from `/etc/services`.
  - Chroot to FTP home directory.
  - Support for dropping privileges if a valid FTP user exists.
  - Use `fork()` instead of pthreads for FTP client sessions.
  - Daemonize uftpd by default, detach from controlling terminal and
    reparent to PID 1 (init).
  - Add support for running as an `inetd` service.
  - Add wrapper for `syslog()` instead of using `stdout/stderr`.
  - Add basic `uftpd.8` man page.
- Add OpenBSD `strlcat()` and `strlcpy()` safe string functions.
- Add support for NOOP (keepalive sent by some clients).
- Add support for SIZE.
- Add support for TYPE, at least `IMAGE/BINARY`.
- Add basic dependency handling to Makefile.
- Add support for building Debian .deb packages.

### Fixes
- Handle "walking up to parent" attacks in several FTP functions.
- Fix memory leaks in `recv_mesg()` caused by dangerous homegrown string
  functions.  Replaced with safer OpenBSD variants.
- Fix absolute paths in FTP `LIST` command.
- Fix Firefox FTP mode `LIST` compatibility issue.
- Fix "bare linefeeds" warning from certain FTP clients in ASCII mode.
  Lines must end in the old `\r\n` format, rather than UNIX `\n`.


[UNRELEASED]:    https://github.com/troglobit/uftpd/compare/v2.5...HEAD
[v2.5]:          https://github.com/troglobit/uftpd/compare/v2.5...v2.5
[v2.4]:          https://github.com/troglobit/uftpd/compare/v2.3...v2.4
[v2.3]:          https://github.com/troglobit/uftpd/compare/v2.2...v2.3
[v2.2]:          https://github.com/troglobit/uftpd/compare/v2.1...v2.2
[v2.1]:          https://github.com/troglobit/uftpd/compare/v2.0.2...v2.1
[v2.0.2]:        https://github.com/troglobit/uftpd/compare/v2.0.1...v2.0.2
[v2.0.1]:        https://github.com/troglobit/uftpd/compare/v2.0...v2.0.1
[v2.0]:          https://github.com/troglobit/uftpd/compare/v1.9.1...v2.0
[v1.9.1]:        https://github.com/troglobit/uftpd/compare/v1.9...v1.9.1
[v1.9]:          https://github.com/troglobit/uftpd/compare/v1.8...v1.9
[v1.8]:          https://github.com/troglobit/uftpd/compare/v1.7...v1.8
[v1.7]:          https://github.com/troglobit/uftpd/compare/v1.6...v1.7
[v1.6]:          https://github.com/troglobit/uftpd/compare/v1.5...v1.6
[v1.5]:          https://github.com/troglobit/uftpd/compare/v1.4...v1.5
[v1.4]:          https://github.com/troglobit/uftpd/compare/v1.3...v1.4
[v1.3]:          https://github.com/troglobit/uftpd/compare/v1.2...v1.3
[v1.2]:          https://github.com/troglobit/uftpd/compare/v1.1...v1.2
[v1.1]:          https://github.com/troglobit/uftpd/compare/v1.0...v1.1
[v1.0]:          https://github.com/troglobit/uftpd/compare/v0.1...v1.1
[libuEv]:        https://github.com/troglobit/libuev
[libite]:        https://github.com/troglobit/libite
[ISC license]:   http://en.wikipedia.org/wiki/ISC_license
[RFC 1350]:      http://tools.ietf.org/html/rfc1350
[RFC 2348]:      http://tools.ietf.org/html/rfc2348
[Xu Wang]:       https://github.com/xu-wang11/
[FtpServer]:     https://github.com/xu-wang11/FtpServer
[Travis-CI]:     https://travis-ci.org/troglobit/uftpd
[Coverity Scan]: https://scan.coverity.com/projects/2947
[TODO.md]:       https://github.com/troglobit/uftpd/blob/master/docs/TODO.md
[README.md]:     https://github.com/troglobit/uftpd/blob/master/README.md
[CHANGELOG.md]:  https://github.com/troglobit/uftpd/blob/master/CHANGELOG.md
