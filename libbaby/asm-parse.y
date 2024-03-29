/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2024 Andrew Bower */

/* Grammar for Baby Assembler. */

%{
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "arch.h"
#include "asm.h"
#include "symbols.h"
#include "asm-ast.h"
#include "asm-parse.h"

int yylex(YYSTYPE *yylval, void *scanner);
void yyerror(YYLTYPE *yylval, struct ast_node **root, char const *);

static struct ast_node *ast_alloc(void) {
  struct ast_node *node = (struct ast_node *) calloc(1, sizeof *node);

  if (node == NULL) {
    perror("ast_alloc");
    exit(1);
  }

  node->heap = true;

  return node;
}

static struct ast_node *mk_node(struct ast_node contents) {
  struct ast_node *node = ast_alloc();
  *node = contents;
  return node;
}

/* Reverse a tuple-based list into a compact list.
 *
 * The proper form for a list shall be nested tuples of the form
 *   (val1, (val2, (val3, ... (valN, nil))))
 *
 * The compact list is represented as an array of the form
 *   [val1, val2, val3, ... valN]
 *
 * This function frees the converted tuples and returns a list in the
 * opposite order to the supplied tree.
 */
static struct ast_node *mk_list(struct ast_node *head) {
  struct ast_node *node = ast_alloc();
  struct ast_node *next;
  struct ast_node *ptr;
  int i;

  node->t = AST_LIST;

  for (ptr = head, node->v.list.length = 0; ptr->t == AST_TUPLE; ptr = ptr->v.tuple[1])
    node->v.list.length++;

  node->v.list.nodes = calloc(node->v.list.length, sizeof(struct ast_node));
  if (node->v.list.nodes == NULL) {
    perror("mk_list: calloc");
    exit(1);
   }

  for (ptr = head, i = node->v.list.length; ptr->t == AST_TUPLE; ptr = next) {
    next = ptr->v.tuple[1];
    node->v.list.nodes[--i] = *ptr->v.tuple[0];
    node->v.list.nodes[i].heap = false;
    free(ptr->v.tuple[0]);
    free(ptr);
  }

  return node;
}

static struct ast_node *mk_tuple (struct ast_node *l, struct ast_node *r) {
  return mk_node((struct ast_node) { .t = AST_TUPLE, .v.tuple = { l, r } });
}

static struct ast_node *mk_nil(void) {
  return mk_node((struct ast_node) { .t = AST_NIL });
}

static struct ast_node *mk_number(int base, char *str) {
  struct ast_node *node;
  node = mk_node((struct ast_node) { .t = AST_NUMBER, .v.number = strtol(str, NULL, base) });
  free(str);
  return node;
}

static struct ast_node *mk_name(str_idx_t str) {
  struct ast_node *node;
  node = mk_node((struct ast_node) { .t = AST_NAME, .v.str = str });
  return node;
}

static struct ast_node *mk_symbol(enum sym_type sym_type, str_idx_t str) {
  struct ast_node *node;
  node = mk_node((struct ast_node) { .t = AST_SYMBOL, .v.nameref = *sym_getref(sym_root_context(), sym_type, str) });
  return node;
}

static struct ast_node *mk_label(str_idx_t str) {
  struct ast_node *node = mk_symbol(SYM_T_LABEL, str);
  node->t = AST_LABEL;
  return node;
}

static struct ast_node *mk_org(struct ast_node *number) {
  number->t = AST_ORG;
  return number;
}

static struct ast_node *mk_instr(struct ast_node *mnemonic, struct ast_node *operands) {
  return mk_node((struct ast_node) { .t = AST_INSTR, .v.tuple = { mnemonic, operands } });
}

static struct ast_node *mk_semantic(enum ast_node_e type, struct ast_node *l, struct ast_node *r) {
  return mk_node((struct ast_node) { .t = type, .v.tuple = { l, r } });
}

static struct ast_node *mk_macro(str_idx_t macro,
                                 struct ast_node *arguments,
                                 struct ast_node *statements) {
  return mk_node((struct ast_node) { .t = AST_MACRO, .v.tuple = {
    mk_name(macro),
    mk_tuple(arguments, mk_list(statements))
  } });
}

/*
static struct ast_node *maybe_tuple(struct ast_node *l, struct ast_node *r) {
  if (l != NULL && l->t == AST_NIL) {
      free(l);
      l = NULL;
  }
  if (r != NULL && r->t == AST_NIL) {
      free(r);
      r = NULL;
  }
  if (l && !r) return l;
  if (r && !l) return r;
  if (l && r)
    return mk_node((struct ast_node) { .t = AST_TUPLE, .v.tuple = { l, r } });
  else
    return mk_nil();
}
*/

#define SAVE_DEBUG(x) (x)->debug.present = true; (x)->debug.loc = yylloc;

%}

%verbose
%define parse.trace
%define parse.error verbose
%define api.pure
%locations

%code requires {
typedef struct {
  int line;
  int col;
  int offset;
} src_pos_t;

typedef struct {
  src_pos_t start;
  src_pos_t end;
  bool last_char_was_newline;
} src_loc_t;

#define first_column start.col
#define last_column end.col
#define first_line start.col
#define last_line end.col
}

%initial-action {
  yylloc = (src_loc_t) { .start = { 1, 1, 0 }, .end = { 1, 0, -1 },
                         .last_char_was_newline = false };
}

%parse-param {struct ast_node **root}
%define api.location.type {src_loc_t}
%define api.value.type union
%token <char *> HEX OCTAL DECIMAL BINARY COLON EOL COMMA
%token <char *> MACRO ENDM CONTINUATION
%token <char *> MINUS PLUS
%token <str_idx_t> NAME
%nterm <struct ast_node *> file stmts stmt location instr
%nterm <struct ast_node *> number number_not_octal
%nterm <struct ast_node *> mnemonic operands expr eol
%nterm <struct ast_node *> macro arguments

%left MINUS PLUS

%%
file: eol stmts { *root = mk_list($2); }
    | stmts { *root = mk_list($1); };

eol: eol EOL { }
   | EOL { };

stmts: stmts stmt { $$ = mk_tuple($2, $1); }
     | stmt { $$ = mk_tuple($1, AST_NIL_NODE); };

stmt: location COLON eol { $$ = $1; SAVE_DEBUG($$); }
    | location COLON { $$ = $1; SAVE_DEBUG($$); }
    | instr eol { $$ = $1; SAVE_DEBUG($$); }
    | macro eol { $$ = $1; SAVE_DEBUG($$); };

macro: NAME MACRO arguments eol stmts ENDM { $$ = mk_macro($1, $3, $5); }

arguments: NAME COMMA arguments { $$ = mk_tuple(mk_name($1), $3); }
         | NAME { $$ = mk_tuple(mk_name($1), AST_NIL_NODE); }
         | %empty { $$ = mk_nil(); };

location: number_not_octal { $$ = mk_org($1); }
        | NAME { $$ = mk_label($1); };

instr: mnemonic operands { $$ = mk_instr($1, $2); }
     | mnemonic { $$ = mk_instr($1, AST_NIL_NODE); };

mnemonic: NAME { $$ = mk_symbol(SYM_T_MNEMONIC, $1); }

operands: expr COMMA operands { $$ = mk_tuple($1, $3); }
        | expr { $$ = mk_tuple($1, AST_NIL_NODE); };

expr: expr MINUS expr { $$ = mk_semantic(AST_MINUS, $1, $3); }
    | expr PLUS expr { $$ = mk_semantic(AST_PLUS, $1, $3); }
    | NAME { $$ = mk_symbol(SYM_T_LABEL, $1); }
    | number { $$ = $1; };

number: HEX { $$ = mk_number(16, $1); }
      | OCTAL { $$ = mk_number(10, $1); }
      | DECIMAL { $$ = mk_number(10, $1); }
      | BINARY { $$ = mk_number(2, $1); };

/* For compatibility with assembler conventions that pad decimal org with 0 */
number_not_octal:
        HEX { $$ = mk_number(16, $1); }
      | OCTAL { $$ = mk_number(10, $1); }
      | DECIMAL { $$ = mk_number(10, $1); }
      | BINARY { $$ = mk_number(2, $1); };

%%

