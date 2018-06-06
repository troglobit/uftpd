sbin_PROGRAMS      = uftpd
uftpd_SOURCES      = uftpd.c uftpd.h common.c ftpcmd.c tftpcmd.c log.c
uftpd_CPPFLAGS     = -D_GNU_SOURCE -D_BSD_SOURCE -D_DEFAULT_SOURCE
uftpd_CFLAGS       = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
uftpd_CFLAGS      += $(uev_CFLAGS) $(lite_CFLAGS)
uftpd_LDADD        = $(uev_LIBS)   $(lite_LIBS)
SYMLINK            = in.ftpd in.tftpd

# Hook in install to add uftpd --> in.ftpd, in.tftpd symlinks
install-exec-hook:
	@for file in $(SYMLINK); do \
		link=$(DESTDIR)$(sbindir)/$$file; \
		test -e $$link && continue; \
		$(LN_S) $(sbin_PROGRAMS) $$link; \
	done

uninstall-hook:
	@for file in $(SYMLINK); do \
		$(RM) $(DESTDIR)$(sbindir)/$$file; \
	done
