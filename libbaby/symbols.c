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
#include "symbols.h"

const char *sym_type_names[SYM_T_MAX] = {
  [ SYM_T_MNEMONIC] = "MNEMONIC",
  [ SYM_T_LABEL] = "LABEL",
};

struct sym_table {
  struct symbol *symbols;
  size_t count;
  size_t capacity;
  bool case_insensitive;
  bool sorted;
  bool pointers_valid;

  /* hash table used to deduplicate entries but not much use otherwise */
  struct hsearch_data htab;
};

struct sym_table sym_tabs[SYM_T_MAX] = { { NULL } };

static int symsort(const void *a, const void *b) {
  const struct symbol *sa = (const struct symbol *) a;
  const struct symbol *sb = (const struct symbol *) b;
  return strcmp(sa->ref.name, sb->ref.name);
}

static int symsearch(const void *key, const void *a) {
  return symsort(&(struct symbol) { .ref.name = key }, a);
}

static int symcasesort(const void *a, const void *b) {
  const struct symbol *sa = (const struct symbol *) a;
  const struct symbol *sb = (const struct symbol *) b;
  return strcasecmp(sa->ref.name, sb->ref.name);
}

static int symcasesearch(const void *key, const void *a) {
  return symcasesort(&(struct symbol) { .ref.name = key }, a);
}

void sym_sort(enum sym_type type) {
  struct sym_table *tab = sym_tabs + type;

  assert(type < SYM_T_MAX);

  qsort(tab->symbols, tab->count, sizeof tab->symbols[0],
        tab->case_insensitive ? symcasesort : symsort);
  tab->sorted = true;
  tab->pointers_valid = false;
}

const char *sym_type_name(enum sym_type type) {
  assert(type < SYM_T_MAX);

  return sym_type_names[type];
}

struct symbol *sym_lookup(enum sym_type type, const char *name) {
  struct sym_table *tab = &sym_tabs[type];
  struct symbol *sym;

  assert(type < SYM_T_MAX);
  assert(name);

  if (!tab->sorted)
    sym_sort(type);

  sym = bsearch(name, tab->symbols, tab->count, sizeof tab->symbols[0],
                tab->case_insensitive ? symcasesearch : symsearch);
  return sym; 
}

struct symref *sym_getref(enum sym_type type, const char *name) {
  struct sym_table *tab = sym_tabs + type;
  struct symbol *sym;

  assert(type < SYM_T_MAX);

  sym = sym_lookup(type, name);

  /* Return existing ref if found */
  if (sym)
    return &sym->ref;

  /* Add new ref if not */
  if (tab->count == tab->capacity) {
    tab->capacity = (tab->capacity == 0) ? 32 : tab->capacity << 1;
    tab->symbols = realloc(tab->symbols, sizeof tab->symbols[0] * tab->capacity);
  }
  sym = &tab->symbols[tab->count++];
  sym->ref.type = type;
  sym->ref.name = strdup(name);
  sym->defined = false;
  tab->sorted = false;

  return &sym->ref;
}

union symval sym_getval(struct symref *ref) {
  struct symbol *sym;

  sym = sym_lookup(ref->type, ref->name);

  if (sym == NULL || sym->ref.type != ref->type) {
    fprintf(stderr, "error: symbol '%s' not found\n", ref->name);
    return (union symval) { .numeric = 0 };
  }

  return sym->val;
}

void sym_setval(struct symref *ref, bool defined, union symval val) {
  struct symbol *sym;

  sym = sym_lookup(ref->type, ref->name);

  assert(sym);
  sym->defined = defined;
  sym->val = defined ? val : (union symval) { .numeric = 0 };
}

void sym_init(void) {
  int rc;
  int i;

  memset(sym_tabs, '\0', sizeof sym_tabs);

  for (i = 0; i < SYM_T_MAX; i++) {
    rc = hcreate_r(1000, &sym_tabs[i].htab);
    if (rc == 0) {
      perror("hcreate_r");
      exit(1);
    }
  }

  sym_tabs[SYM_T_MNEMONIC].case_insensitive = true;
}

void sym_finit(void) {
  int i, j;

  for (i = 0; i < SYM_T_MAX; i++) {
    hdestroy_r(&sym_tabs[i].htab);
    for (j = 0; j < sym_tabs[i].count; j++) {
      free((void *)sym_tabs[i].symbols[j].ref.name);
    }
  }
}

struct symref *sym_add(enum sym_type type, const char *name, bool defined, union symval value) {
  struct symref *symref;

  assert(type < SYM_T_MAX);

  symref = sym_getref(type, name);
  sym_setval(symref, defined, value);
  return symref;
}

void sym_print_table(enum sym_type type) {
  struct sym_table *tab = sym_tabs + type;
  int i;

  assert(type < SYM_T_MAX);

  if (!tab->sorted)
    sym_sort(type);

  fprintf(stderr, "Symbol table (%s):\n", sym_type_names[type]);
  for (i = 0; i < tab->count; i++) {
    struct symbol *sym = tab->symbols + i;
    fprintf(stderr, "  %-6s %20s 0x%08x\n",
            sym_type_names[sym->ref.type],
            sym->ref.name,
            sym->val.numeric);
  }
}

