/* (c) Copyright 2024 Andrew Bower */

/* String tables */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <errno.h>
#include <search.h>
#include <sys/types.h>
#include <assert.h>

#include "strtab.h"

#define STRTAB_INITIAL_SIZE 1024
#define STRTAB_MIN_HTAB_SIZE 200

struct strtab {
  /* String table */
  char *buf;
  size_t sz;
  str_idx_t ptr;

  /* Metadata */
  unsigned int n_entries;

  /* Hash table for existence checks */
  struct hsearch_data htab;
  size_t htab_size;
};

static int rebuild_htab(struct strtab *table, bool had_enomem) {
  size_t min_size = 0;
  str_idx_t ptr;
  ENTRY *r;
  int rc;
  int i;

  if (min_size < table->htab_size)
    min_size = table->htab_size;

  if (min_size < table->sz / 8)
    min_size = table->sz / 8;

  if (min_size < table->n_entries * 2)
    min_size = table->n_entries * 2;

  if (min_size < STRTAB_MIN_HTAB_SIZE)
    min_size = STRTAB_MIN_HTAB_SIZE;

  if (min_size == table->htab_size && had_enomem)
    min_size <<= 1;

  if (min_size != table->htab_size) {
    if (table->htab_size != 0)
      hdestroy_r(&table->htab);

    table->htab_size = min_size;
    memset(&table->htab, '\0', sizeof table->htab);
    if (0 == hcreate_r(table->htab_size, &table->htab))
      return errno;

    for (ptr = 0, i = 0; ptr < table->ptr; i++) {
      ENTRY e = {
        .key = table->buf + ptr,
        .data = (void *) ptr
      };
      rc = hsearch_r(e, ENTER, &r, &table->htab);
      if (rc == 0 && errno == ENOMEM)
        return rebuild_htab(table, true);
      else if (r == 0)
        return errno;
      ptr += strlen(e.key) + 1;
    }
    assert(i == table->n_entries);
  }

  return 0;
}

struct strtab *strtab_create(void) {
  struct strtab *table;
  int rc = 0;

  table = (struct strtab *) calloc(1, sizeof *table);
  if (table != NULL) {
    table->sz = STRTAB_INITIAL_SIZE;
    table->buf = calloc(table->sz, sizeof *table->buf);
    if (table->buf == NULL) {
      rc = errno;
      free(table);
      table = NULL;
    }
  } else {
    rc = errno;
  }

  if (table) {
    rc = rebuild_htab(table, false);
    if (rc != 0) {
      errno = rc;
      free(table->buf);
      free(table);
      table = NULL;
    }
  }

  return table;
}

void strtab_destroy(struct strtab *table) {
  assert(table);

  if (table->htab_size != 0)
    hdestroy_r(&table->htab);

  free(table->buf);
  free(table);
}

str_idx_t strtab_put(struct strtab *table, const char *str) {
  ENTRY e = {
    .key = (char *) str /* surely hsearch should take a const?
                         * TODO: Let's replace with a better data structure. */
  };
  ENTRY *r;
  size_t len;
  int rc;

  assert(table);

  /* Return index of string if already stored */
  rc = hsearch_r(e, FIND, &r, &table->htab);
  if (rc != 0)
    return (str_idx_t) r->data;
  assert(errno == ESRCH);

  /* Else insert into the string table */
  len = strlen(str);
  if (len >= table->sz - table->ptr) {

    /* Expand table if necessary */
    size_t old_size = table->sz;
    table->sz = old_size << 2;
    if (table->sz - old_size <= len)
      table->sz = old_size + len + 1;
    table->buf = realloc(table->buf, table->sz * sizeof *table->buf);
    memset(table->buf + old_size, '\0', table->sz - old_size);
  }
  e.data = (void *) table->ptr;
  strcpy(table->buf + table->ptr, str);
  table->ptr += len + 1;

  /* Add to hash table if space, rebuilding if necessary */
  rc = hsearch_r(e, ENTER, &r, &table->htab);
  rebuild_htab(table, rc == 0);

  return (str_idx_t) e.data;
}

const char *strtab_get(struct strtab *table, str_idx_t idx) {
  return table->buf + idx;
}
