# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

this=baby
d=lib$(this)

$(d)_SRC=arch.c asm.c writer.c section.c loader.c objfile.c memory.c segment.c
$(d)_OBJ=$($(d)_SRC:.c=.o)
$(d)_DEP=$($(d)_SRC:.c=.d)

SUBDIRS+=$(d)
LIBS+=$(this)

$(d).a: $(addprefix $d/,$($(d)_OBJ))
	$(AR) r $@ $^

