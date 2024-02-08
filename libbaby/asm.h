/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Assembly language definitions. */

#ifndef LIBBABY_ASM_H
#define LIBBABY_ASM_H

#include <stdint.h>
#include <sys/types.h>

/* Constants */

#define HAS_ORG   01
#define HAS_LABEL 02
#define HAS_INSTR 04

/* Types */

struct source_public {
  char *path;
  char *leaf;
};

struct asm_abstract {
  int flags;
  int n_operands;
  addr_t org;
  char *label;
  char *instr;
  enum operand_type opr_type;
  char *opr_str;
  num_t opr_num;
  num_t opr_effective;
  struct source_public *source;
  int line;
};

/* Public functions */

void asm_log_abstract(struct asm_abstract *abstract);

#endif
