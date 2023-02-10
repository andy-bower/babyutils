# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -g
PREFIX ?= /usr/local
INSTALL ?= install
MANDIR ?= share/man
DOCDIR ?= share/doc/$(name)
LICENSESDIR ?= share/doc/$(name)

name=babyutils

.PHONY: all all_targets install
all: all_targets

include libbaby/lib.mk

EXES=bas bsim
CFLAGS+=$(addprefix -I,$(INCDIRS))
LDFLAGS+=-L.
LIBFILES=$(foreach lib,$(LIBS),lib$(lib).a)
LDLIBS=$(addprefix -l,$(LIBS))

r:=$(DESTDIR)$(PREFIX)

all_targets: $(LIBFILES) $(EXES)

install: all
	mkdir -p $r/bin
	mkdir -p $r/share/man/man1
	mkdir -p $r/share/doc/babyutils/examples
	[ -z "$(LICENSESDIR)" ] || mkdir -p $r/$(LICENSESDIR)
	gzip -c bas.1 > $r/$(MANDIR)/man1/bas.1.gz
	$(INSTALL) -m 755 -t $r/bin $(EXES)
	$(INSTALL) -m 644 -t $r/$(DOCDIR)/examples test/*.asm
	$(INSTALL) -m 644 -t $r/$(DOCDIR) README.md
	[ -z "$(LICENSESDIR)" ] || $(INSTALL) -m 644 -t $r/$(LICENSESDIR) COPYING

uninstall:
	$(RM) $r/bin/bas
	$(RM) $r/$(MANDIR)/man1/bas1.gz
	$(RM) -r $r/$(DOCDIR)
	$(RM) -r $r/$(LICENSESDIR)

bas: bas.o

bsim: bsim.o

clean:
	$(RM) $(EXES) $(LIBFILES) bas.o bsim.o libbaby/*.o
