/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* AST for Baby Assembler. */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "arch.h"
#include "asm.h"
#include "asm-parse.h"
#include "symbols.h"

struct ast_debug {
  YYLTYPE loc;
  bool present;
};

enum ast_node_e {
  AST_TUPLE,
  AST_INSTR,
  AST_LABEL,
  AST_ORG,
  AST_NIL,
  AST_NUMBER,
  AST_NAME,
  AST_LIST,
  AST_MACRO,
};

struct ast_node;

union ast_node_u {
  struct ast_node *tuple[2];
  struct symref nameref;
  word_t number;
  struct {
    size_t length;
    struct ast_node *nodes;
  } list;
};

struct ast_node {
  enum ast_node_e t;
  union ast_node_u v;
  struct ast_debug debug;
};

extern void ast_plot_tree(FILE *stream, struct ast_node *node);
extern void ast_free_tree(struct ast_node *node);
size_t ast_count_list(struct ast_node *node);
