/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* AST functions for Manchester Baby assembly. */

#include <assert.h>
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
#include "section.h"
#include "writer.h"
#include "binfmt.h"
#include "symbols.h"
#include "asm.h"
#include "asm-ast.h"
#include "asm-parse.h"

void ast_plot_tree(FILE *out, struct ast_node *node) {
  int i;

  switch (node->t) {
  case AST_MACRO:
  case AST_INSTR:
    fprintf(out, node->t == AST_MACRO ? "Macro" : "Instr");
  case AST_TUPLE:
    fprintf(out, "(");
    ast_plot_tree(out, node->v.tuple[0]);
    fprintf(out, ", ");
    ast_plot_tree(out, node->v.tuple[1]);
    fprintf(out, ")");
    break;
  case AST_NIL:
    fprintf(out, "nil");
    break;
  case AST_ORG:
    fprintf(out, "Org ");
  case AST_NUMBER:
    fprintf(out, "%d", node->v.number);
    break;
  case AST_LABEL:
    fprintf(out, "Label ");
  case AST_NAME:
    fprintf(out, "%s:%s", sym_type_name(node->v.nameref.type), node->v.nameref.name);
    break;
  case AST_LIST:
    fprintf(out, "[");
    for (i = 0; i < node->v.list.length; i++) {
      if (i != 0)
        fprintf(out, ",\n");
      ast_plot_tree(out, &node->v.list.nodes[i]);
    }
    fprintf(out, "]");
    break;
  default:
    fprintf(out, "<UNKNOWN-NODE-TYPE>");
  }
};

void ast_free_tree(struct ast_node *node) {
  int i;

  switch (node->t) {
  case AST_INSTR:
  case AST_MACRO:
  case AST_TUPLE:
    ast_free_tree(node->v.tuple[0]);
    ast_free_tree(node->v.tuple[1]);
    break;
  case AST_LIST:
    for (i = 0; i < node->v.list.length; i++)
      ast_free_tree(&node->v.list.nodes[i]);
    break;
  case AST_NIL:
  case AST_ORG:
  case AST_NUMBER:
  case AST_LABEL:
  case AST_NAME:
    break;
  default:
    fprintf(stderr, "ast_free_tree: <UNKNOWN-NODE-TYPE>");
  }
};

size_t ast_count_list(struct ast_node *node) {
  switch (node->t) {
  case AST_TUPLE:
    return 1 + ast_count_list(node->v.tuple[1]);
    break;
  case AST_LIST:
    return node->v.list.length;
    break;
  case AST_NIL:
    return 0;
  default:
    assert(!"ast_count_list: not a list node");
  }
}
