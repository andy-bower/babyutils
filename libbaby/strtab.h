/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* String tables */

#ifndef LIBBABY_STRTAB_H
#define LIBBABY_STRTAB_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <search.h>

/* Types */

typedef intptr_t str_idx_t;
struct strtab;

/* Public functions */

extern struct strtab *strtab_create(void);
extern void strtab_destroy(struct strtab *strtab);
extern str_idx_t strtab_put(struct strtab *strtab, const char *str);
const char *strtab_get(struct strtab *strtab, str_idx_t);

#endif
