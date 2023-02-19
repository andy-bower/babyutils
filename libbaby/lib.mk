# SPDX-License-Identifier: MIT
# (c) Copyright 2023 Andrew Bower

this=baby
d=lib$(this)

object=arch writer section loader objfile memory segment

INCDIRS+=$(d)
LIBS+=$(this)

$(d).a: $(addprefix $d/,$(addsuffix .o,$(object)))
	$(AR) r $@ $^

