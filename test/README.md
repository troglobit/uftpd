Running the Test Suite
======================

The regression tests run under `make check`.  Each test runs in its own
mount, user, and network namespace (the `unshare` wrapper in
`TESTS_ENVIRONMENT`), so they need no root privileges, do not touch the
host network, and can run in parallel:

```console
$ ./autogen.sh        # only from a GIT checkout
$ ./configure
...
$ make -j20 check     # or just 'make check'
```

A single test can be run on its own:

```console
$ make check TESTS=ipv6.sh
...
```

Requirements
------------

The tests drive uftpd with ordinary client tools.  On Debian/Ubuntu:

```console
$ sudo apt-get install util-linux iproute2 procps python3 \
                       tftp-hpa tnftp ftp
```

| **Tool**  | **Package** | **Used by**                                            |
|-----------|-------------|--------------------------------------------------------|
| `unshare` | util-linux  | all tests (namespace isolation)                        |
| `ip`      | iproute2    | all tests (brings up loopback)                         |
| `ftp`     | tnftp / ftp | `ftp`, `maxfiles`, `ipv6`                              |
| `tnftp`   | tnftp       | `mlst`                                                 |
| `tftp`    | tftp-hpa    | `tftp`, `ipv6`                                         |
| `pgrep`   | procps      | `zombies`                                              |
| `python3` | python3     | `oack`, `dupack`, `lockstep`, `rollover`, `wrq`, `ipv6`, `zombies` |

`python3` is used where a test must craft or inspect raw TFTP packets
(checking the exact OACK bytes, replaying a stale ACK, withholding one
to see what the server does anyway, injecting a duplicate WRQ) — things
the CLI clients cannot do.  A test that is
missing its tool is skipped, not failed.


Kernel configuration
--------------------

The tests need unprivileged user namespaces.  These are enabled by
default on most distributions, but recent Ubuntu restricts them via
AppArmor; allow them with:

```console
$ sudo sysctl kernel.apparmor_restrict_unprivileged_userns=0
```
