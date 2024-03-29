/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Assembly language definitions. */

#ifndef LIBBABY_ASM_H
#define LIBBABY_ASM_H

#include <stdint.h>
#include <sys/types.h>

#include "strtab.h"
#include "symbols.h"

/* Constants */

#define HAS_ORG   01
#define HAS_LABEL 02
#define HAS_INSTR 04

#define SSTR(x) strtab_get(strtab_src, x)
#define SSTRP(x) strtab_put(strtab_src, x)

/* Types */

struct source_public {
  char *path;
  char *leaf;
};

struct asm_abstract {
  struct sym_context *context;
  int flags;
  int n_operands;
  addr_t org;
  struct symref label;
  struct symref instr;
  struct ast_node *operands;
  num_t opr_effective;
  struct source_public *source;
  int line;
};

extern struct strtab *strtab_src;

/* Public functions */

extern void asm_log_abstract(struct strtab *strtab, struct asm_abstract *abstract);
extern size_t mnemonic_debug_str(char *buf, size_t sz, struct mnemonic *mnemonic);

#endif
