# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -g
PREFIX ?= /usr/local

EXES=bas

all: $(EXES)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/man/man8
	gzip -c bas.1 > $(DESTDIR)$(PREFIX)/share/man/man1/bas.1.gz
	install -m 755 -t $(DESTDIR)$(PREFIX)/bin $(EXES)

bas: bas.o

clean:
	$(RM) bas bas.o
