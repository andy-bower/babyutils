/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Architecture definitions for Manchester Baby. */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "symbols.h"
#include "arch.h"

const struct instr I_JMP = { OP_JMP, 1 };
const struct instr I_SUB = { OP_SUB, 1 };
const struct instr I_LDN = { OP_LDN, 1 };
const struct instr I_SKN = { OP_SKN, 0 };
const struct instr I_JRP = { OP_JRP, 1 };
const struct instr I_STO = { OP_STO, 1 };
const struct instr I_HLT = { OP_HLT, 0 };

/* Instruction menmonics and directives declared for this architecture.
 * Put preferred aliases first. */
struct mnemonic baby_mnemonics[] = {
  { "JMP", M_INSTR, .ins=&I_JMP },
  { "JRP", M_INSTR, .ins=&I_JRP },
  { "SUB", M_INSTR, .ins=&I_SUB },
  { "LDN", M_INSTR, .ins=&I_LDN },
  { "SKN", M_INSTR, .ins=&I_SKN },
  { "STO", M_INSTR, .ins=&I_STO },
  { "HLT", M_INSTR, .ins=&I_HLT },
  { "CMP", M_INSTR, .ins=&I_SKN },
  { "STP", M_INSTR, .ins=&I_HLT },
  { "NUM", M_DIRECTIVE, .dir=D_NUM },
  { "EJA", M_DIRECTIVE, .dir=D_EJA },
};
#define babysz (sizeof baby_mnemonics / sizeof *baby_mnemonics)
size_t num_baby_mnemonics;

static int instrsort(const void *a, const void *b) {
  const struct mnemonic *ma = (const struct mnemonic *) a;
  const struct mnemonic *mb = (const struct mnemonic *) b;
  return strcasecmp(ma->name, mb->name);
}

static int instrsearch(const void *key, const void *a) {
  return instrsort(&key, a);
}

struct instr_index {
  struct mnemonic *mnemonic;
  int order;
};

struct arch_state {
  struct instr_index *instr_index;
};

struct arch_state arch_state;

static int instrindexsort(const void *a, const void *b) {
  const struct instr_index *ia = (const struct instr_index *) a;
  const struct instr_index *ib = (const struct instr_index *) b;
  const struct mnemonic *ma = ia->mnemonic;
  const struct mnemonic *mb = ib->mnemonic;
  if (ma->type != mb->type)
    return ma->type - mb->type;
  else if (ma->type == M_DIRECTIVE || ma->ins->opcode == mb->ins->opcode)
    return ia->order - ib->order;
  else
    return ma->ins->opcode - mb->ins->opcode;
}

static int instrindexsearch(const void *opkey, const void *b) {
  const word_t *opcode = (const word_t *) opkey;
  const struct instr_index *ib = (const struct instr_index *) b;
  const struct mnemonic *mb = ib->mnemonic;
  if (mb->type != M_INSTR)
    return M_INSTR - mb->type;
  if (mb->ins->opcode != *opcode)
    return *opcode - mb->ins->opcode;
  else
    return 0 - ib->order;
}

void arch_init(struct strtab *strtab) {
  int i;
  int sub_order = 0;

  arch_state.instr_index = calloc(babysz, sizeof *arch_state.instr_index);
  if (arch_state.instr_index == NULL) {
    perror("allocating arch state\n");
    exit(1);
  }

  /* Sort the mnemonics alphabetically. This must come before any use
   * of the mnemonics by pointer (i.e. any use). */
  qsort(baby_mnemonics, babysz, sizeof *baby_mnemonics, instrsort);
  num_baby_mnemonics = babysz;

  /* Create an index of the mnemonics by opcode */
  for (i = 0; i < babysz; i++) {
    struct instr_index *entry = arch_state.instr_index + i;
    struct mnemonic *mnem = baby_mnemonics + i;
    entry->mnemonic = mnem;
    if (i > 0 &&
        mnem->type == M_INSTR && entry[-1].mnemonic->type == M_INSTR &&
        mnem->ins->opcode == entry[-1].mnemonic->ins->opcode)
      sub_order += 1;
    else
      sub_order = 0;
    entry->order = sub_order;
  }
  qsort(arch_state.instr_index, babysz, sizeof *arch_state.instr_index, instrindexsort);

  /* Register mnemonics as symbols */
  for (i = 0; i < babysz; i++) {
    struct mnemonic *m = baby_mnemonics + i;
    struct symref *ref = sym_getref(sym_root_context(), SYM_T_MNEMONIC, strtab_put(strtab, m->name));
    sym_setval(sym_root_context(), ref, true, (union symval) { .internal = m });
  }
}

void arch_finit(void) {
  free(arch_state.instr_index);
}

const struct mnemonic *arch_find_instr(const char *mnemonic) {
  return bsearch(mnemonic, baby_mnemonics, num_baby_mnemonics, sizeof *baby_mnemonics, instrsearch); 
}

int arch_find_opcode(word_t opcode, struct mnemonic **results, size_t max_results) {
  struct instr_index *entry;
  struct instr_index *end = arch_state.instr_index + num_baby_mnemonics;
  int i = 0;

  entry = bsearch(&opcode, arch_state.instr_index, num_baby_mnemonics, sizeof *arch_state.instr_index, instrindexsearch); 

  while (entry &&
         entry != end &&
         i < max_results &&
         entry->mnemonic->type == M_INSTR &&
         entry->mnemonic->ins->opcode == opcode)
    results[i++] = entry++->mnemonic;

  return i;
}
