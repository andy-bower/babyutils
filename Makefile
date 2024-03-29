# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror -MMD -MP -D_GNU_SOURCE
LDFLAGS ?= -g
prefix ?= /usr/local
INSTALL ?= install
MANDIR ?= share/man
DOCDIR ?= share/doc/$(name)
LICENSESDIR ?= share/doc/$(name)
YACC.y=bison

name=babyutils

.PHONY: all all_targets install test
all: all_targets

include libbaby/lib.mk

INCDIRS=$(SUBDIRS)

EXES=bas bsim bdump
CFLAGS+=$(addprefix -I,$(INCDIRS))
LDFLAGS+=-L.
LIBFILES=$(foreach lib,$(LIBS),lib$(lib).a)
LDLIBS=$(addprefix -l,$(LIBS))

r:=$(DESTDIR)$(prefix)

DEP=*.d $(foreach d,$(SUBDIRS),$(addprefix $d/,$($(d)_DEP)))
GENERATED=$(foreach d,$(SUBDIRS),$(addprefix $d/,$($(d)_GENERATED)))

all_targets: $(LIBFILES) $(EXES) $(GENERATED)

-include $(DEP)

install: all
	mkdir -p $r/bin
	mkdir -p $r/share/man/man1
	mkdir -p $r/share/doc/babyutils/examples
	[ -z "$(LICENSESDIR)" ] || mkdir -p $r/$(LICENSESDIR)
	gzip -c bas.1 > $r/$(MANDIR)/man1/bas.1.gz
	gzip -c bsim.1 > $r/$(MANDIR)/man1/bsim.1.gz
	$(INSTALL) -m 755 -t $r/bin $(EXES)
	$(INSTALL) -m 644 -t $r/$(DOCDIR)/examples test/*.asm
	$(INSTALL) -m 644 -t $r/$(DOCDIR) README.md
	[ -z "$(LICENSESDIR)" ] || $(INSTALL) -m 644 -t $r/$(LICENSESDIR) COPYING

uninstall:
	$(RM) $r/bin/bas
	$(RM) $r/bin/sim
	$(RM) $r/$(MANDIR)/man1/bas.1.gz
	$(RM) $r/$(MANDIR)/man1/bsim.1.gz
	$(RM) -r $r/$(DOCDIR)
	$(RM) -r $r/$(LICENSESDIR)

bas: bas.o libbaby.a

bsim: bsim.o libbaby.a

bdump: bdump.o libbaby.a

clean:
	$(RM) $(EXES) $(LIBFILES) bas.o bsim.o bdump.o libbaby/*.o test/*.out $(DEP) $(GENERATED)

test: bas bsim
	./bas -m -O bits.snp -o test/test-jmp.out test/test-jmp.asm
	timeout -s QUIT 1 ./bsim -I bits.snp test/test-jmp.out | grep '^0000001c: 00000011 00000011 00000022'
