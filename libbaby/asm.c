/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Assembler for Manchester Baby. */

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
#include <sys/types.h>

#include "butils.h"
#include "arch.h"
#include "symbols.h"
#include "asm.h"
#include "asm-ast.h"

struct strtab *strtab_src;

void asm_log_abstract(struct strtab *strtab,
                      struct asm_abstract *abstract) {
    fprintf(stderr, "  %-3s %-5s %-5s %4d: 0x%08x %-10s %-4s 0x%08x %s:%d\n",
            abstract->flags & HAS_ORG ? "ORG" : "",
            abstract->flags & HAS_LABEL ? "LABEL" : "",
            abstract->flags & HAS_INSTR ? "INSTR" : "",
            abstract->n_operands,
            abstract->org,
            abstract->flags & HAS_LABEL ? strtab_get(strtab, abstract->label.name) : "",
            abstract->flags & HAS_INSTR ? strtab_get(strtab, abstract->instr.name) : "",
            abstract->opr_effective,
            abstract->source ? abstract->source->leaf : "",
            abstract->line);
}

size_t mnemonic_debug_str(char *buf, size_t sz, struct mnemonic *mnemonic) {
  switch (mnemonic->type) {
  case M_INSTR:
    return snprintf(buf, sz, "%-10s 0x%x %d operands", "INSTR",
                    mnemonic->ins->opcode, mnemonic->ins->operands);
  case M_DIRECTIVE:
    return snprintf(buf, sz, "%-10s %d", "DIRECTIVE",
                    mnemonic->dir);
  case M_MACRO:
    return snprintf(buf, sz, "%-10s %zd stmts %zd operands", "MACRO",
                    ast_count_list(mnemonic->ast->v.tuple[1]),
                    ast_count_list(mnemonic->ast->v.tuple[0]));
  default:
    return snprintf(buf, sz, "%s", "");
  }
}

