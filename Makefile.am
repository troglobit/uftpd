sbin_PROGRAMS      = uftpd
dist_man8_MANS     = uftpd.8
uftpd_SOURCES      = uftpd.c uftpd.h common.c ftpcmd.c tftpcmd.c log.c
uftpd_CFLAGS       = -W -Wall -Wextra -std=gnu99 $(uev_CFLAGS) $(lite_CFLAGS)
uftpd_LDADD        =  $(uev_LIBS) $(lite_LIBS)
SYMLINK            = in.ftpd in.tftpd
doc_DATA           = README.md LICENSE
EXTRA_DIST         = README.md LICENSE ChangeLog.md

# Hook in install to add uftpd --> in.ftpd, in.tftpd symlinks
install-exec-hook:
	@for file in $(SYMLINK); do \
		link=$(DESTDIR)$(sbindir)/$$file; \
		test -e $$link && continue; \
		$(LN_S) $(sbin_PROGRAMS) $$link; \
	done

# Hook in install to add uftpd.8 --> in.ftpd.8, in.tftpd.8 symlinks
install-data-hook:
	@for file in $(SYMLINK); do \
		link=$(DESTDIR)$(man8dir)/$$file.8; \
		test -e $$link && continue; \
		$(LN_S) $(dist_man8_MANS) $$link; \
	done

uninstall-hook:
	for file in $(SYMLINK); do \
		$(RM) $(DESTDIR)$(sbindir)/$$file; \
		$(RM) $(DESTDIR)$(man8dir)/$$file.8; \
	done

package build-deb:
	dpkg-buildpackage -uc -us -B

## Target to run when building a release
## Temprarily removed: build-deb
release: dist
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
#	@for file in `ls ../$(PACKAGE)_$(VERSION)-*`; do			\
#		printf "$$file    \tDebian/Ubuntu file\n";			\
#	done
