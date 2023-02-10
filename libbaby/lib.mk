# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

this=baby
d=lib$(this)

INCDIRS+=$(d)
LIBS+=$(this)

$(d).a: $(d)/arch.o
	$(AR) r $@ $<

