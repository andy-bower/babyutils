/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* Name (symbol) management */

#ifndef LIBBABY_SYMBOLS_H
#define LIBBABY_SYMBOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include "arch.h"

/* Types */

enum sym_type {
  SYM_T_MNEMONIC,
  SYM_T_LABEL,
  SYM_T_MAX
};

struct symref {
  enum sym_type type;
  const char *name;
};

union symval {
  addr_t numeric;
  void *internal;
};

struct symbol {
  struct symref ref;
  union symval val;
  bool defined:1;
};

/* Public functions */

extern void sym_init(void);
extern void sym_finit(void);

const char *sym_type_name(enum sym_type type);

/* Look up a symbol by (type, name). Return NULL if not found. */
extern struct symbol *sym_lookup(enum sym_type type, const char *name);

/* Look up a symbol by (type, name), Return a new or existing reference
 * depending on whether the symbol was already found. */
extern struct symref *sym_getref(enum sym_type type, const char *name);

/* Look up and return the value for a symbol which already exists. */
extern union symval sym_getval(struct symref *ref);

/* Set the value for a symbol which already exists. */
extern void sym_setval(struct symref *ref, bool defined, union symval value);

/* Set a symbol value, adding if necessary. */
extern struct symref *sym_add(enum sym_type type, const char *name, bool defined, union symval value);

static inline struct symref *sym_add_num(enum sym_type type, const char *name, num_t value) {
  return sym_add(type, name, true, (union symval) { .numeric = value });
}

extern void sym_sort(enum sym_type type);
extern void sym_print_table(enum sym_type type);

#endif
