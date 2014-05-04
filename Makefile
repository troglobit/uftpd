# uftpd -- the small no nonsense FTP server
#
# Copyright (c) 2013-2014  Xu Wang <wangxu.93@icloud.com>
# Copyright (c)      2014  Joachim Nilsson <troglobit@gmail.com>
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

.PHONY: all install uninstall clean distclean dist

#VERSION   ?= $(shell git tag -l | tail -1)
VERSION    ?= 1.0
BUGADDR     = https://github.com/troglobit/uftpd/issues
NAME        = uftpd
PKG         = $(NAME)-$(VERSION)
DEV         = $(NAME)-dev
ARCHIVE     = $(PKG).tar.xz
EXEC        = $(NAME)
MANUAL      = $(NAME).8
DISTFILES   = LICENSE README
OBJS        = uftpd.o daemonize.o ftpcmd.o string.o strlcpy.o strlcat.o log.o
SRCS        = $(OBJS:.o=.c)
DEPS        = $(addprefix .,$(SRCS:.c=.d))

# Installation paths, always prepended with DESTDIR if set
TOPDIR      = $(shell pwd)
prefix     ?= /usr
sysconfdir ?= /etc
sbindir    ?= /sbin
datadir     = $(prefix)/share/doc/$(NAME)
mandir      = $(prefix)/share/man/man8

CFLAGS     += -O2 -W -Wall -Werror -g

include common.mk

all: defs.h $(EXEC)

defs.h: Makefile
	@echo "#define VERSION \"$(VERSION)\"" >  $@
	@echo "#define BUGADDR \"$(BUGADDR)\"" >> $@

$(EXEC): $(OBJS)

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
	-@$(RM) $(OBJS) $(EXEC)

distclean: clean
	-@$(RM) $(JUNK) unittest *.o .*.d

check:
	$(CHECK) *.c *.h

dist:
	@echo "Building xz tarball of $(PKG) in parent dir..."
	git archive --format=tar --prefix=$(PKG)/ $(VERSION) | xz >../$(ARCHIVE)
	@(cd ..; md5sum $(ARCHIVE) | tee $(ARCHIVE).md5)

dev: distclean
	@echo "Building unstable xz $(DEV) in parent dir..."
	-@$(RM) -f ../$(DEV).tar.xz*
	@(dir=`mktemp -d`; mkdir $$dir/$(DEV); cp -a . $$dir/$(DEV); \
	  cd $$dir; tar --exclude=.git --exclude=contrib             \
                        -c -J -f $(DEV).tar.xz $(DEV);               \
	  cd - >/dev/null; mv $$dir/$(DEV).tar.xz ../; cd ..;        \
	  rm -rf $$dir; md5sum $(DEV).tar.xz | tee $(DEV).tar.xz.md5)

-include $(DEPS)
