# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

CFLAGS ?= -g -Wall -Werror
LDFLAGS ?= -g

babyas: babyas.o

clean:
	$(RM) babyas babyas.o
