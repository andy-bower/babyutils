# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -g
PREFIX ?= /usr/local
INSTALL ?= install
MANDIR ?= share/man
DOCDIR ?= share/doc/$(name)
LICENSESDIR ?= share/doc/$(name)

EXES=bas
name=babyutils

r:=$(DESTDIR)$(PREFIX)

all: $(EXES)

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

clean:
	$(RM) bas bas.o
