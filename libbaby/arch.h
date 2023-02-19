/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Architecture header for Manchester Baby. */

#ifndef LIBBABY_ARCH_H
#define LIBBABY_ARCH_H

#include <stdint.h>

typedef uint32_t addr_t;
typedef int32_t word_t;
typedef uint32_t num_t;

struct instr {
  uint32_t opcode;
  int operands;
};

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
#define OPCODE_POS   13
#define OPERAND_POS  0

extern const struct instr I_JMP;
extern const struct instr I_SUB;
extern const struct instr I_LDN;
extern const struct instr I_SKN;
extern const struct instr I_JRP;
extern const struct instr I_STO;
extern const struct instr I_HLT;

#endif
