/* (c) Copyright 2023-2024 Andrew Bower */

/* Name (symbol) management */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <search.h>
#include <sys/types.h>
#include <assert.h>

#include "butils.h"
#include "arch.h"
#include "asm.h"
#include "strtab.h"
#include "symbols.h"

/* TODO: stop sorting the symbols by name now that we use indexes
 * into a string table as the name reference. In any case this
 * was only a temporary solution pending more appropriate
 * data structure. */

const char *sym_type_names[SYM_T_MAX] = {
  [ SYM_T_MNEMONIC ] = "MNEMONIC",
  [ SYM_T_LABEL ] = "LABEL",
};

const char sym_subtype_id[] = {
  [ SYM_ST_UNDEF ] = 'U',
  [ SYM_ST_WORD ] = 'D',
  [ SYM_ST_MNEM ] = 'M',
  [ SYM_ST_AST ] = 'A',
};


struct sym_table {
  struct symbol *symbols;
  size_t count;
  size_t capacity;
  bool case_insensitive;
  bool sorted;
};

static struct sym_context *sym_global_context = NULL;
static struct strtab *sym_strtab;

static const char *str_text(str_idx_t idx) {
  return strtab_get(sym_strtab, idx);
}

static int symsort(const void *a, const void *b) {
  const struct symbol *sa = (const struct symbol *) a;
  const struct symbol *sb = (const struct symbol *) b;
  return strcmp(str_text(sa->ref.name), str_text(sb->ref.name));
}

static int symsearch(const void *key, const void *a) {
  return symsort(&(struct symbol) { .ref.name = (str_idx_t) key }, a);
}

static int symcasesort(const void *a, const void *b) {
  const struct symbol *sa = (const struct symbol *) a;
  const struct symbol *sb = (const struct symbol *) b;
  return strcasecmp(str_text(sa->ref.name), str_text(sb->ref.name));
}

static int symcasesearch(const void *key, const void *a) {
  return symcasesort(&(struct symbol) { .ref.name = (str_idx_t) key }, a);
}

void sym_sort(struct sym_context *context, enum sym_type type) {
  struct sym_table *tab = context->tables[type];

  assert(type < SYM_T_MAX);

  qsort(tab->symbols, tab->count, sizeof tab->symbols[0],
        tab->case_insensitive ? symcasesort : symsort);
  tab->sorted = true;
}

const char *sym_type_name(enum sym_type type) {
  assert(type < SYM_T_MAX);

  return sym_type_names[type];
}

struct symbol *sym_lookup_with_context(struct sym_context *context,
                                       enum sym_type type,
                                       str_idx_t name,
                                       enum sym_lookup_scope scope,
                                       struct sym_context **found_context,
                                       struct sym_context *specific_context) {
  struct sym_table *tab;
  struct symbol *sym = NULL;

  assert(type < SYM_T_MAX);

  while (context && !sym) {
    tab = context->tables[type];
    if ((scope != SYM_LU_SCOPE_EXCLUDE_SPECIFIED || context != specific_context) &&
        tab) {
      if (!tab->sorted)
        sym_sort(context, type);

      sym = bsearch((void *) name, tab->symbols, tab->count, sizeof tab->symbols[0],
                    tab->case_insensitive ? symcasesearch : symsearch);
    }
    if (!sym)
      context = scope == SYM_LU_SCOPE_LOCAL ? NULL : context->parent;
  }

  *found_context = context;

  return sym; 
}

struct symbol *sym_lookup(struct sym_context *context, enum sym_type type, str_idx_t name, enum sym_lookup_scope scope) {
  struct sym_context *found_context;

  return sym_lookup_with_context(context, type, name, scope, &found_context, NULL);
}

struct symref *sym_getref(struct sym_context *context, enum sym_type type, str_idx_t name) {
  struct sym_table *tab;
  struct symbol *sym;

  assert(type < SYM_T_MAX);

  sym = sym_lookup(context, type, name, true);

  /* Return existing ref if found */
  if (sym)
    return &sym->ref;

  /* Add new ref if not */
  tab = context->tables[type];
  if (tab->count == tab->capacity) {
    tab->capacity = (tab->capacity == 0) ? 32 : tab->capacity << 1;
    tab->symbols = realloc(tab->symbols, sizeof tab->symbols[0] * tab->capacity);
  }
  sym = &tab->symbols[tab->count++];
  sym->ref.type = type;
  sym->ref.name = name;
  sym->subtype = SYM_ST_UNDEF;
  tab->sorted = false;

  return &sym->ref;
}

union symval sym_getval(struct sym_context *context, struct symref *ref) {
  struct symbol *sym;

  sym = sym_lookup(context, ref->type, ref->name, false);

  if (sym == NULL || sym->ref.type != ref->type) {
    fprintf(stderr, "error: symbol '%s' not found\n", strtab_get(sym_strtab, ref->name));
    return (union symval) { .numeric = 0 };
  }

  return sym->val;
}

void sym_setval(struct sym_context *context, struct symref *ref,
                enum sym_subtype subtype, union symval val) {
  struct symbol *sym;

  sym = sym_lookup(context, ref->type, ref->name, true);

  assert(sym);
  sym->subtype = subtype;
  sym->val = sym->subtype == SYM_ST_UNDEF ? SYM_VAL_NUL : val;
}

int sym_table_create(struct sym_context *context, enum sym_type type) {
  struct sym_table *table;

  assert(context->tables[type] == NULL);
  table = (struct sym_table *) calloc(1, sizeof *table);
  if (table == NULL)
    return errno;

  if (type == SYM_T_MNEMONIC)
    table->case_insensitive = true;

  context->tables[type] = table;

  return 0;
}

void sym_table_destroy(struct sym_context *context, enum sym_type type) {
  struct sym_table *table = context->tables[type];

  assert(table);
  free(table->symbols);
  free(table);
  context->tables[type] = NULL;
}

struct sym_context *sym_context_create(struct sym_context *parent) {
  struct sym_context *context = (struct sym_context *) calloc(1, sizeof *context);
  context->parent = parent;
  return context;
}

void sym_context_destroy(struct sym_context *context) {
 int i;

  assert(context);
  for (i = 0; i < SYM_T_MAX; i++) {
    if (context->tables[i])
      sym_table_destroy(context, i);
  }

  free(context);
}

void sym_init(struct strtab *strtab) {
  int rc;
  int i;

  sym_global_context = sym_context_create(NULL);
  sym_strtab = strtab;
  if (sym_global_context == NULL) {
    perror("creating global context");
    exit(1);
  }

  /* Root/global context has one of each symbol table type. */
  for (i = 0; i < SYM_T_MAX; i++) {
    rc = sym_table_create(sym_global_context, i);
    if (rc != 0) {
      perror("sym_table_create");
      exit(1);
    }
  }
}

void sym_finit(void) {
  sym_context_destroy(sym_global_context);
  sym_global_context = NULL;
}

struct sym_context *sym_root_context(void) {
  return sym_global_context;
}

struct symref *sym_add(struct sym_context *context,
                       enum sym_type type,
                       str_idx_t name,
                       enum sym_subtype subtype,
                       union symval value) {
  struct symref *symref;

  assert(type < SYM_T_MAX);

  symref = sym_getref(context, type, name);
  sym_setval(context, symref, subtype, value);
  return symref;
}

void sym_print_table(struct sym_context *context, enum sym_type type) {
  struct sym_table *tab = context->tables[type];
  int i;

  assert(type < SYM_T_MAX);

  if (!tab->sorted)
    sym_sort(context, type);

  fprintf(stderr, "Symbol table (%s):\n", sym_type_names[type]);
  for (i = 0; i < tab->count; i++) {
    struct symbol *sym = tab->symbols + i;
    bool extra_info = sym->subtype == SYM_ST_MNEM;
    char extra[60];

    if (extra_info)
      mnemonic_debug_str(extra, sizeof extra, (struct mnemonic *) sym->val.internal);

    fprintf(stderr, "  %-10s 0x%08x %c %-20s %s%s%s\n",
            sym_type_names[sym->ref.type],
            sym->val.numeric,
            sym_subtype_id[sym->subtype],
            str_text(sym->ref.name),
            extra_info ? "(" : "",
            extra_info ? extra : "",
            extra_info ? ")" : "");
  }
}

