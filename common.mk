# Top directory for building complete system, fall back to this directory
ROOTDIR    ?= $(TOPDIR)

# Some junk files we always want to remove in distclean
JUNK        = *~ *.bak *.map .*.d DEADJOE *.gdb *.elf core core.*

# Tools
RM         ?= rm -f
CC         ?= $(CROSS)gcc
MAKE       := @$(MAKE)
MAKEFLAGS   = --no-print-directory --silent
CHECK      := cppcheck $(CPPFLAGS) --quiet --enable=all
INSTALL    := install --backup=off
STRIPINST  := $(INSTALL) -s --strip-program=$(CROSS)strip -m 0755

# Pretty printing and GCC -M for auto dep files
%.o: %.c
	@printf "  CC      $(subst $(ROOTDIR)/,,$(shell pwd)/$@)\n"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -MMD -MP -o $@ $<

# Pretty printing and create .map files
%: %.o
	@printf "  LINK    $(subst $(ROOTDIR)/,,$(shell pwd)/$@)\n"
	@$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map,$@.map -o $@ $^ $(LDLIBS$(LDLIBS-$(@)))

