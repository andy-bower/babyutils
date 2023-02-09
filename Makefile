# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -g
PREFIX ?= /usr/local

EXES=bas

all: $(EXES)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/man/man8
	gzip -c bas.8 > $(DESTDIR)$(PREFIX)/share/man/man8/bas.8.gz
	install -m 755 -t $(DESTDIR)$(PREFIX)/bin $(EXES)

bas: bas.o

clean:
	$(RM) bas bas.o
