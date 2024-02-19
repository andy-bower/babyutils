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
  /* Symbols that are mnemonics (instruction, directive or macro) */
  SYM_T_MNEMONIC,

  /* Symbols that are labels for absolute locations */
  SYM_T_LABEL,

  /* Names without associated values. Used for scoped names such as
   * macro arguments. These names are likely to be copied to create
   * semantic symbols of some kind later that are bound to appropriate scope.
   */
  SYM_T_RAW,

  /* Macros */
  SYM_T_MACRO,

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

/* Opaque types */
struct syn_table;

struct sym_context {
  struct sym_context *parent;
  struct sym_table *tables[SYM_T_MAX];
};

/* Public functions */

extern void sym_init(void);
extern void sym_finit(void);
struct sym_context *sym_root_context(void);

const char *sym_type_name(enum sym_type type);

/* Look up a symbol by (type, name). Return NULL if not found. */
extern struct symbol *sym_lookup(struct sym_context *context, enum sym_type type, const char *name);

/* Look up a symbol by (type, name), Return a new or existing reference
 * depending on whether the symbol was already found. */
extern struct symref *sym_getref(struct sym_context *context, enum sym_type type, const char *name);

/* Look up and return the value for a symbol which already exists. */
extern union symval sym_getval(struct sym_context *context, struct symref *ref);

/* Set the value for a symbol which already exists. */
extern void sym_setval(struct sym_context *context, struct symref *ref, bool defined, union symval value);

/* Set a symbol value, adding if necessary. */
extern struct symref *sym_add(struct sym_context *context, enum sym_type type, const char *name, bool defined, union symval value);

static inline struct symref *sym_add_num(struct sym_context *context, enum sym_type type, const char *name, num_t value) {
  return sym_add(context, type, name, true, (union symval) { .numeric = value });
}

extern void sym_sort(struct sym_context *context, enum sym_type type);
extern void sym_print_table(struct sym_context *context, enum sym_type type);

#endif
