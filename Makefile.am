SYMLINK         = in.ftpd in.tftpd
EXTRA_DIST      = configure ChangeLog.md README.md LICENSE
doc_DATA        = README.md LICENSE
dist_man8_MANS  = uftpd.8
sbin_PROGRAMS   = uftpd
SUBDIRS         = libite libuev
uftpd_SOURCES   = uftpd.c common.c ftpcmd.c tftpcmd.c log.c
uftpd_CPPFLAGS  = -I.
uftpd_LDFLAGS   = -Llibite -Llibuev
uftpd_LDADD     = -lite -luev

# Hook in install to add uftpd --> in.ftpd, in.tftpd symlinks
install-exec-hook:
	@for file in $(SYMLINK); do						\
		$(LN_S) $(sbin_PROGRAMS) $(DESTDIR)$(sbindir)/$$file;		\
	done

build-deb:
	gbp buildpackage --git-ignore-new --git-ignore-branch			 \
			 --git-no-create-orig --git-upstream-branch=master

## Target to run when building a release
release: dist build-deb
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
	@for file in `ls ../$(PACKAGE)_$(VERSION)-*`; do			\
		printf "$$file    \tDebian/Ubuntu file\n";			\
	done
