/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Architecture header for Manchester Baby. */

#ifndef LIBBABY_ARCH_H
#define LIBBABY_ARCH_H

#include <stdint.h>
#include <sys/types.h>

/* Types */

typedef uint32_t addr_t;
typedef int32_t word_t;
typedef uint32_t uword_t;
typedef uint32_t num_t;

struct instr {
  uint32_t opcode;
  int operands;
};

enum operand_type {
  OPR_NONE,
  OPR_NUM,
  OPR_SYM,
};

enum mnem_type {
  M_INSTR,
  M_DIRECTIVE,
};

enum directive {
  D_NUM,
  D_EJA,
};

struct mnemonic {
  char *name;
  enum mnem_type type;
  union {
    const struct instr *ins;
    enum directive dir;
  };
};

struct arch_decoded {
  word_t opcode;
  word_t operand;
  word_t data;
};

/* Constants */

#define OP_JMP 00
#define OP_JRP 01
#define OP_LDN 02
#define OP_STO 03
#define OP_SUB 04
#define OP_SUB_ALIAS 05
#define OP_SKN 06
#define OP_HLT 07

#define OPCODE_MASK  0x0000E000
#define OPERAND_MASK 0x00001FFF
#define OPDATA_MASK  0xFFFF0000
#define OPCODE_POS   13
#define OPERAND_POS  0
#define OPDATA_POS   16

extern const struct instr I_JMP;
extern const struct instr I_SUB;
extern const struct instr I_LDN;
extern const struct instr I_SKN;
extern const struct instr I_JRP;
extern const struct instr I_STO;
extern const struct instr I_HLT;

static inline struct arch_decoded arch_decode(word_t instr)
{
  return (struct arch_decoded) {
    .opcode = (instr & OPCODE_MASK) >> OPCODE_POS,
    .operand = (instr & OPERAND_MASK) >> OPERAND_POS,
    .data = (instr & OPDATA_MASK) >> OPDATA_POS,
  };
}

extern void arch_init(void);
extern void arch_finit(void);
extern const struct mnemonic *arch_find_instr(const char *mnemonic);
extern int arch_find_opcode(word_t opcode, struct mnemonic **results, size_t max_results);
#endif
