/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Architecture definitions for Manchester Baby. */

#include "arch.h"

const struct instr I_JMP = { OP_JMP, 07, 1 };
const struct instr I_SUB = { OP_SUB, 07, 1 };
const struct instr I_LDN = { OP_LDN, 07, 1 };
const struct instr I_SKN = { OP_SKN, 07, 0 };
const struct instr I_JRP = { OP_JRP, 07, 1 };
const struct instr I_STO = { OP_STO, 07, 1 };
const struct instr I_HLT = { OP_HLT, 07, 0 };
