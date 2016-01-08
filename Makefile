# uftpd -- the no nonsense FTP/TFTP server
#
# Copyright (c) 2014, 2015  Joachim Nilsson <troglobit@gmail.com>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
.PHONY: all install uninstall clean distclean dist submodules

#VERSION     = $(shell git tag -l | tail -1)
VERSION     = 1.9.1
BUGADDR     = https://github.com/troglobit/uftpd/issues
NAME        = uftpd
PKG         = $(NAME)-$(VERSION)
DEV         = $(NAME)-dev
ARCHTOOL    = `which git-archive-all`
ARCHIVE     = $(PKG).tar
ARCHIVEZ    = ../$(ARCHIVE).xz
EXEC        = $(NAME)
MANUAL      = $(NAME).8
DISTFILES   = LICENSE README
OBJS        = uftpd.o common.o ftpcmd.o tftpcmd.o log.o
SRCS        = $(OBJS:.o=.c)
DEPS        = $(SRCS:.c=.d)

DEPLIBS    :=
TOPDIR     := $(shell pwd)

CFLAGS     += -W -Wall -Wextra
CPPFLAGS   += -DVERSION='"$(VERSION)"' -DBUGADDR='"$(BUGADDR)"'

ifneq ($(MAKECMDGOALS),distclean)
-include config.mk
endif
include common.mk


all: $(EXEC)

$(DEPLIBS): submodules Makefile
	+@$(MAKE) STATIC=1 -C `dirname $@` all

$(EXEC): $(OBJS) $(DEPLIBS)

submodules:
	@if [ ! -e libuev/Makefile -o ! -e libite/Makefile ]; then	\
		git submodule update --init;				\
	fi

strip:
	@strip $(EXEC)
	@size  $(EXEC)

install-exec: all
	@$(INSTALL) -d $(DESTDIR)$(sbindir)
	@for file in $(EXEC); do                                        \
		printf "  INSTALL $(DESTDIR)$(sbindir)/$$file\n";   	\
		$(STRIPINST) $$file $(DESTDIR)$(sbindir)/$$file; 	\
	done

install-data:
	@$(INSTALL) -d $(DESTDIR)$(datadir)
	@$(INSTALL) -d $(DESTDIR)$(mandir)
	@for file in $(DISTFILES); do	                                \
		printf "  INSTALL $(DESTDIR)$(datadir)/$$file\n";	\
		$(INSTALL) -m 0644 $$file $(DESTDIR)$(datadir)/$$file;	\
	done
	@printf "  INSTALL $(DESTDIR)$(mandir)/$(MANUAL)\n"
	$(INSTALL) -m 0644 $(MANUAL) $(DESTDIR)$(mandir)/$(MANUAL)

install: install-exec install-data

uninstall-exec:
	-@for file in $(EXEC); do 					\
		printf "  REMOVE  $(DESTDIR)$(sbindir)/$$file\n";   	\
		rm $(DESTDIR)$(sbindir)/$$file 2>/dev/null; 		\
	done
	-@rmdir $(DESTDIR)$(sbindir) 2>/dev/null

uninstall-data:
	@for file in $(DISTFILES); do	                                \
		printf "  REMOVE  $(DESTDIR)$(datadir)/$$file\n";	\
		rm $(DESTDIR)$(datadir)/$$file 2>/dev/null;		\
	done
	@printf "  REMOVE  $(DESTDIR)$(mandir)/$(MANUAL)\n"
	-@rm $(DESTDIR)$(mandir)/$(MANUAL)
	-@rmdir $(DESTDIR)$(mandir)
	-@rmdir $(DESTDIR)$(datadir)

uninstall: uninstall-exec uninstall-data

clean:
	+@$(MAKE) -C libuev $@
	+@$(MAKE) -C libite $@
	-@$(RM) $(OBJS) $(EXEC)

distclean: clean
	+@$(MAKE) -C libuev $@
	+@$(MAKE) -C libite $@
	-@$(RM) $(JUNK) config.mk *.o .*.d

check:
	$(CHECK) *.c *.h

package:
	dpkg-buildpackage -b -uc -tc

dist:
	@if [ x"$(ARCHTOOL)" = x"" ]; then \
		echo "Missing git-archive-all from https://github.com/Kentzo/git-archive-all"; \
		exit 1; \
	fi
	@if [ -e $(ARCHIVEZ) ]; then \
		echo "Distribution already exists."; \
		exit 1; \
	fi
	@echo "Building xz tarball of $(PKG) in parent dir..."
	@$(ARCHTOOL) ../$(ARCHIVE)
	@xz ../$(ARCHIVE)
	@md5sum $(ARCHIVEZ) | tee $(ARCHIVEZ).md5

dev: distclean
	@echo "Building unstable xz $(DEV) in parent dir..."
	-@$(RM) -f ../$(DEV).tar.xz*
	@(dir=`mktemp -d`; mkdir $$dir/$(DEV); cp -a . $$dir/$(DEV); \
	  cd $$dir; tar --exclude=.git -c -J -f $(DEV).tar.xz $(DEV);\
	  cd - >/dev/null; mv $$dir/$(DEV).tar.xz ../; cd ..;        \
	  rm -rf $$dir; md5sum $(DEV).tar.xz | tee $(DEV).tar.xz.md5)

-include $(DEPS)
