# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

this=baby
d=lib$(this)

$(d)_YACC=asm-parse.y
$(d)_LEX=asm-lex.l
$(d)_SRC=arch.c asm.c writer.c section.c loader.c objfile.c memory.c segment.c symbols.c asm-ast.c
$(d)_OBJ=$($(d)_SRC:.c=.o) $($(d)_YACC:.y=.o) $($(d)_LEX:.l=.o)
$(d)_DEP=$($(d)_SRC:.c=.d)
$(d)_GENERATED=$($(d)_YACC:.y=.c) $($(d)_YACC:.y=.h) $($(d)_LEX:.l=.c)

SUBDIRS+=$(d)
LIBS+=$(this)

%.c %.h: %.y
	$(YACC.y) -o$(<:.y=.c) -H$(<:.y=.h) $<

$(d)/asm-ast.o: $(d)/asm-parse.h

$(d).a: $(addprefix $d/,$($(d)_OBJ))
	$(AR) r $@ $^
