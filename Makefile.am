sbin_PROGRAMS      = uftpd
dist_man8_MANS     = uftpd.8
uftpd_SOURCES      = uftpd.c uftpd.h common.c ftpcmd.c tftpcmd.c log.c
uftpd_CFLAGS       = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
uftpd_CFLAGS      += $(uev_CFLAGS) $(lite_CFLAGS)
uftpd_LDADD        = $(uev_LIBS)   $(lite_LIBS)
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
	@for file in $(SYMLINK); do \
		$(RM) $(DESTDIR)$(sbindir)/$$file; \
		$(RM) $(DESTDIR)$(man8dir)/$$file.8; \
	done

## Generate .deb package
package build-deb:
	@dpkg-buildpackage -uc -us -B

## Generate MD5 checksum file
MD5 = md5sum
md5-dist:
	@for file in $(DIST_ARCHIVES); do	\
		$(MD5) $$file > $$file.md5;	\
	done

## Check if tagged in git
release-hook:
	if [ ! `git tag | grep $(PACKAGE_VERSION)` ]; then				\
		echo;									\
		printf "\e[1m\e[41mCannot find release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[5mDo release anyway?\e[0m "; read yorn;			\
		if [ "$$yorn" != "y" -a "$$yorn" != "Y" ]; then				\
			printf "OK, aborting release.\n";				\
			exit 1;								\
		fi;									\
		echo;									\
	else										\
		echo;									\
		printf "\e[1m\e[42mFound GIT release tag $(PACKAGE_VERSION)\e[0m\n";	\
		printf "\e[1m\e[44m>>Remember to push tags!\e[0m\n";			\
		echo;									\
	fi


## Target to run when building a release
release: distcheck release-hook md5-dist
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do						\
		printf "$$file    \tDistribution tarball\n";				\
		printf "$$file.md5\t"; cat $$file.md5 | cut -f1 -d' ';			\
		mv $$file* ../;								\
	done
#	@for file in `ls ../$(PACKAGE)_$(VERSION)-*`; do				\
#		printf "$$file    \tDebian/Ubuntu file\n";				\
#	done
