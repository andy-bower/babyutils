/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* Name (symbol) management */

#ifndef LIBBABY_SYMBOLS_H
#define LIBBABY_SYMBOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include "arch.h"
#include "strtab.h"

/* Types */

enum sym_type {
  /* Symbols that are mnemonics (instruction, directive or macro) */
  SYM_T_MNEMONIC,

  /* Symbols that are location labels or numeric constants */
  SYM_T_LABEL,

  SYM_T_MAX
};

enum sym_subtype {
  SYM_ST_UNDEF,
  SYM_ST_MNEM,
  SYM_ST_WORD,
  SYM_ST_AST,
};

enum sym_lookup_scope {
  SYM_LU_SCOPE_DEFAULT,
  SYM_LU_SCOPE_LOCAL,
  SYM_LU_SCOPE_EXCLUDE_SPECIFIED_UNDEF,
};

struct symref {
  enum sym_type type;
  str_idx_t name;
};

union symval {
  addr_t numeric;
  void *internal;
  struct ast_node *ast;
  struct mnemonic mnem;
};

#define SYM_VAL_NUL ((union symval) { .numeric = 0 })

struct symbol {
  struct symref ref;
  union symval val;
  enum sym_subtype subtype;
};

/* Opaque types */
struct syn_table;

struct sym_context {
  struct sym_context *parent;
  struct sym_table *tables[SYM_T_MAX];
};

/* Public functions */

extern void sym_init(struct strtab *strtab);
extern void sym_finit(void);
struct sym_context *sym_root_context(void);

const char *sym_type_name(enum sym_type type);

extern struct symbol *sym_lookup_with_context(struct sym_context *context, enum sym_type type, str_idx_t name,
                                              enum sym_lookup_scope scope, struct sym_context **found_context,
                                              struct sym_context *specific_contexst);

 /* Look up a symbol by (type, name). Return NULL if not found. */
extern struct symbol *sym_lookup(struct sym_context *context, enum sym_type type, str_idx_t name, enum sym_lookup_scope scope);

/* Look up a symbol by (type, name), Return a new or existing reference
 * depending on whether the symbol was already found. */
extern struct symref *sym_getref(struct sym_context *context, enum sym_type type, str_idx_t name);

/* Look up and return the value for a symbol which already exists. */
extern union symval sym_getval(struct sym_context *context, struct symref *ref);

/* Set the value for a symbol which already exists. */
extern void sym_setval(struct sym_context *context, struct symref *ref, enum sym_subtype subtype, union symval value);

/* Set a symbol value, adding if necessary. */
extern struct symref *sym_add(struct sym_context *context, enum sym_type type, str_idx_t name, enum sym_subtype subtype, union symval value);

static inline struct symref *sym_add_num(struct sym_context *context, enum sym_type type, str_idx_t name, num_t value) {
  return sym_add(context, type, name, SYM_ST_WORD, (union symval) { .numeric = value });
}

extern void sym_sort(struct sym_context *context, enum sym_type type);
extern void sym_print_table(struct sym_context *context, enum sym_type type);

extern struct sym_context *sym_context_create(struct sym_context *parent);
extern void sym_context_destroy(struct sym_context *context);
extern int sym_table_create(struct sym_context *context, enum sym_type type);

#endif
