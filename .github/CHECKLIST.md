Release Checklist
=================

Steps to cut a new uftpd release.  The version number is `X.Y` for a
release and `X.Y-rcN` for a release candidate; the GIT tag is the same
with a leading `v`.

Everything below happens on `master`, in the order given.  Nothing is
pushed until the whole list is ticked off -- a pushed tag is a released
tag, and the only recovery from a bad release is the next one.


Before Tagging
--------------

 - [ ] `master` is green in [Bob the Builder][], **including the
       `debian` job**.  That job builds the `.deb` and runs lintian, so
       packaging breakage surfaces here rather than in the release job,
       which only runs once the tag is already immutable
 - [ ] `ChangeLog.md`: add a `[vX.Y][] - YYYY-MM-DD` section with the
       Changes and Fixes for *this* release, and add the matching
       `[vX.Y]: .../compare/vX.Y-1...vX.Y` link at the bottom
 - [ ] `debian/changelog`: add a matching `uftpd (X.Y) stable;` entry.
       Easiest is `dch -v X.Y --distribution stable`, which stamps the
       trailer with your `DEBEMAIL`/`DEBFULLNAME` identity.  The entry
       covers what changed in the *package*, so list the packaging
       changes too, not just the upstream ones
 - [ ] `configure.ac`: bump the version in `AC_INIT()`
 - [ ] Commit the three together: `git commit -s -m "Bump version for
       vX.Y release"`

> **Note:** `debian/changelog` is easy to forget because nothing in the
> build fails without it -- the `.deb` just silently keeps the old
> version.  It was missed for v2.16.


Verify
------

 - [ ] `./autogen.sh && ./configure`
 - [ ] `make check` -- see [test/README.md][] for what the suite needs
 - [ ] `make distcheck`
 - [ ] `make package`, if you can (see [Packaging][] below)


Tag and Push
------------

 - [ ] `git tag -a vX.Y -m "uftpd vX.Y"`
 - [ ] `git push`
 - [ ] `git push --tags`

Pushing the tag starts [Release General][], which builds the tarball and
the `.deb`, extracts the top `ChangeLog.md` section as the release body,
and publishes the GitHub release.  Release candidates -- `-alpha`,
`-beta`, `-rc[0-9]*` -- are marked as pre-releases automatically.

 - [ ] Check the workflow went green and the release page looks right


After
-----

 - [ ] Upload the package to [deb.troglobit.com][].  The `.deb` on the
       release page is built on `ubuntu-latest` and links against that
       runner's libuEv/libite, so it is not a substitute for a package
       built for each supported distribution
 - [ ] Announce


Packaging
---------

The packaging targets Ubuntu 24.04 LTS, which is what `ubuntu-latest`
resolves to and what the maintainer runs.  Two consequences:

 - `debian/control` declares `Standards-Version: 4.6.2`.  That is what
   lintian 2.117 (24.04) considers current; declaring the newer 4.7.2
   earns a `newer-standards-version` warning on every build.  Bump it
   when CI moves to a newer runner
 - `debian/control` declares `debhelper-compat (= 13)`

Building the package needs a few tools beyond the normal build deps:

```console
$ sudo apt install devscripts debhelper lintian po-debconf
$ ./autogen.sh
$ ./configure
$ make package
```

`make package` runs `debuild ... --lintian-opts --profile debian`, so
lintian errors fail the build.  Both the source and binary packages are
expected to be entirely lintian clean.

`dpkg-shlibdeps` resolves the libuEv and libite dependencies from the
installed *packages*, so `make package` fails if you run those libraries
from a source install under `/usr/local`:

```
dpkg-shlibdeps: error: no dependency information found for
/usr/local/lib/libite.so.5
```

Install `libuev-dev` and `libite-dev` from apt to build the package
locally, or just let the `debian` CI job do it for you.

When changing `debian/templates`, refresh the translation template:

```console
$ debconf-updatepo --podir debian/po
```

[Bob the Builder]:  https://github.com/troglobit/uftpd/actions/workflows/build.yml
[Release General]:  https://github.com/troglobit/uftpd/actions/workflows/release.yml
[deb.troglobit.com]: https://deb.troglobit.com/
[test/README.md]:   https://github.com/troglobit/uftpd/blob/master/test/README.md
[Packaging]:        #packaging
