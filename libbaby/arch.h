/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Architecture header for Manchester Baby. */

#ifndef LIBBABY_ARCH_H
#define LIBBABY_ARCH_H

#include <stdint.h>

struct instr {
  uint32_t opcode;
  uint32_t mask;
  int operands;
};

#define OP_JMP 00
#define OP_SUB 01
#define OP_LDN 02
#define OP_SKN 03
#define OP_JRP 04
#define OP_STO 06
#define OP_HLT 07

extern const struct instr I_JMP;
extern const struct instr I_SUB;
extern const struct instr I_LDN;
extern const struct instr I_SKN;
extern const struct instr I_JRP;
extern const struct instr I_STO;
extern const struct instr I_HLT;

#endif
