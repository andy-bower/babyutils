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
#include "asm.h"

void asm_log_abstract(struct asm_abstract *abstract) {
    fprintf(stderr, "  %-3s %-5s %-5s %4d: 0x%08x %-10s %-4s 0x%08x %-10s %s:%d\n",
            abstract->flags & HAS_ORG ? "ORG" : "",
            abstract->flags & HAS_LABEL ? "LABEL" : "",
            abstract->flags & HAS_INSTR ? "INSTR" : "",
            abstract->n_operands,
            abstract->org,
            abstract->flags & HAS_LABEL ? abstract->label : "",
            abstract->flags & HAS_INSTR ? abstract->instr : "",
            abstract->opr_effective,
            abstract->opr_type == OPR_SYM ? abstract->opr_str : "",
            abstract->source ? abstract->source->leaf : "",
            abstract->line);
}

