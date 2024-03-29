/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Assembler for Manchester Baby. */

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

#define DEFAULT_OUTPUT_FILE "b.out"
#define DEFAULT_OUTPUT_FORMAT WRITER_BITS BITS_SUFFIX_SNP

extern FILE *yyin;

enum {
  VSYM_ORG,
};

static const char *vsyms[] = {
  [ VSYM_ORG ] = "$",
};

struct source {
  struct source_public public;
  FILE *stream;
  bool seekable;
  bool noclose;
  int lines;
  long *index;
  size_t indexsz;
};

int verbose;

static void init(void) {
  strtab_src = strtab_create();
  sym_init(strtab_src);
  arch_init(strtab_src);
}

static void finit(void) {
  arch_finit();
  sym_finit();
  strtab_destroy(strtab_src);
}

void yyerror(YYLTYPE *yylloc, struct ast_node **root, char const *error) {
  fprintf(stderr, "yyerror: %d.%d-%d.%d: %s\n",
          yylloc->start.line, yylloc->start.col,
          yylloc->end.line, yylloc->end.col,
          error);
}

struct asm_buf {
  struct asm_abstract *records;
  size_t sz;
  off_t ptr;
};

static void asm_buf_push(struct asm_buf *buf, struct asm_abstract *record) {
  if (buf->ptr == buf->sz) {
    buf->sz = (buf->sz == 0) ? 128 : buf->sz << 1;
    buf->records = realloc(buf->records, sizeof buf->records[0] * buf->sz);
  }
  if (buf->records == NULL) {
    perror("allocating abstract assembly buffer");
    exit(1);
  }
  buf->records[buf->ptr++] = *record;
}

static void asm_buf_free(struct asm_buf *buf) {
  free(buf->records);
  memset(buf, '\0', sizeof *buf);
}

enum eval_result {
  EVAL_OK,
  EVAL_PARTIAL,
  EVAL_ERROR,
};

static enum sym_subtype expr_to_symval(union symval *symval, struct ast_node *node) {
  if (node->t == AST_NUMBER) {
    symval->numeric = node->v.number;
    return SYM_ST_WORD;
  } else {
    symval->ast = node;
    return SYM_ST_AST;
  }
}

static enum eval_result eval_expr(struct sym_context *context, struct ast_node *node, bool allow_partial) {
  struct sym_context *sym_context;
  enum eval_result rc_a, rc_b;
  struct symbol *sym;
  union symval sv;
  num_t a, b;

  switch (node->t) {
  case AST_SYMBOL:
  case AST_LABEL:
    sym = sym_lookup_with_context(context, SYM_T_LABEL, node->v.nameref.name,
                                  SYM_LU_SCOPE_EXCLUDE_SPECIFIED_UNDEF, &sym_context,
                                  context);
    if (sym && sym->subtype == SYM_ST_AST && !allow_partial) {
      rc_a = eval_expr(sym_context, sym->val.ast, allow_partial);
      if (rc_a == EVAL_ERROR)
        return rc_a;
      if (rc_a == EVAL_OK)
        sym_setval(sym_context, &sym->ref, expr_to_symval(&sv, sym->val.ast), sv);
    }
    if (sym && sym->subtype == SYM_ST_WORD) {
      node->t = AST_NUMBER;
      node->v.number = sym->val.numeric;
    } else if (!allow_partial) {
      fprintf(stderr, "label undefined: %s\n", SSTR(node->v.nameref.name));
      return EVAL_ERROR;
    } else {
      return EVAL_PARTIAL;
    }
  case AST_NUMBER:
    return EVAL_OK;
  case AST_MINUS:
  case AST_PLUS:
    rc_a = eval_expr(context, node->v.tuple[0], allow_partial);
    rc_b = eval_expr(context, node->v.tuple[1], allow_partial);
    if (rc_a == EVAL_OK && rc_b == EVAL_OK && node->v.tuple[0]->t == node->v.tuple[1]->t) {
      a = node->v.tuple[0]->v.number;
      b = node->v.tuple[1]->v.number;
      a = node->t == AST_MINUS ? a - b : a + b;
      node->t = node->v.tuple[0]->t;
      ast_free_tree(node->v.tuple[0]);
      ast_free_tree(node->v.tuple[1]);
      node->v.number = a;
      return EVAL_OK;
    } else if (allow_partial && (rc_a == EVAL_PARTIAL || rc_b == EVAL_PARTIAL)) {
      return EVAL_PARTIAL;
    } else {
      return EVAL_ERROR;
    }
  default:
    fprintf(stderr, "eval: invalid ast node\n");
    return EVAL_ERROR;
  }
}

int assemble_one(struct sym_context *assembler_context,
                 struct section *section,
                 struct asm_abstract *abstract, bool first_pass) {
  int rc = 0;
  enum eval_result ev;

  if (abstract->flags & HAS_ORG) {
    section->cursor = abstract->org;
  }

  if (assembler_context)
    sym_add_num(assembler_context, SYM_T_LABEL, SSTRP(vsyms[VSYM_ORG]), section->cursor);

  /* Resolve operands on second pass */
  if (!first_pass && abstract->flags & HAS_INSTR) {
    num_t *evaluated_operands = &abstract->opr_effective;
    const int max_operands = 1;
    struct ast_node *node;
    int op_i = 0;

    for (node = abstract->operands; node->t != AST_NIL; node = node->v.tuple[1]) {
      struct ast_node *val = node->v.tuple[0];
      assert(node->t == AST_TUPLE);

      if (op_i == max_operands) {
        fprintf(stderr, "too many operands\n");
        return EHANDLED;
      }

      assembler_context->parent = abstract->context;
      ev = eval_expr(assembler_context, val, false);
      if (ev != EVAL_OK)
        return EHANDLED;
      assert(val->t == AST_NUMBER);
      evaluated_operands[op_i++] = val->v.number;
    }

    assert(op_i == abstract->n_operands);
  }

  if (verbose && !first_pass)
    asm_log_abstract(strtab_src, abstract);

  if (first_pass && abstract->flags & HAS_INSTR) {
    put_word(section, 0, NULL);
  }

  if (!first_pass && abstract->flags & HAS_INSTR) {
    const struct mnemonic *m = arch_find_instr(SSTR(abstract->instr.name));
    if (m == NULL) {
      fprintf(stderr, "no such mnemonic %s\n", SSTR(abstract->instr.name));
      return EINVAL;
    }

    if (m->type == M_INSTR) {
      word_t word = (m->ins->opcode << OPCODE_POS) & OPCODE_MASK;
      if (m->ins->operands == 1)
        word |= (abstract->opr_effective << OPERAND_POS) & OPERAND_MASK;
      put_word(section, word, abstract);
    } else if (m->type == M_DIRECTIVE) {
      switch (m->dir) {
      case D_NUM:
        rc = put_word(section, abstract->opr_effective, abstract);
        break;
      case D_EJA:
        rc = put_word(section, abstract->opr_effective - 1, abstract);
      }
    }
  }

  if (rc != 0) {
    fprintf(stderr, "error at %s:%d\n", abstract->source->path, abstract->line);
    rc = EHANDLED;
  }

  return rc;
}

void pass_one(struct sym_context *assembler_context, struct section *section, struct asm_buf *abstract) {
  int i;
  addr_t saved_cursor = section->cursor;

  for (i = 0; i < abstract->ptr; i++) {
    struct asm_abstract *a = &abstract->records[i];
    if (a->flags & HAS_LABEL)
      sym_add_num(a->context, SYM_T_LABEL, a->label.name, section->cursor);
    assemble_one(assembler_context, section, a, true);
  }

  section->cursor = saved_cursor;
}

int assemble(struct section *section,
             struct asm_buf *abstract) {
  struct sym_context *assembler_context;
  int rc = 0;
  int i;

  /* Create a dynamic symbol context to layer onto the real context
   * for current scope to add virtual symbols like current location. */
  assembler_context = sym_context_create(NULL);
  rc = sym_table_create(assembler_context, SYM_T_LABEL);

  if (verbose)
    fprintf(stderr, "Abstract assembly source:\n");
  for (i = 0; i < abstract->ptr; i++) {
    if (rc == 0)
      rc = assemble_one(assembler_context, section, &abstract->records[i], false);
  }

  sym_context_destroy(assembler_context);

  return rc;
}

long seek_to_line(struct source *source, int line) {
  if (source == NULL) {
    return -ENOENT;
  } else if (!source->seekable) {
    return -EOPNOTSUPP;
  } else if (line >= 1 && line <= source->lines) {
    long offset = source->index[line - 1];
    if (offset == -1) {
      return -ENOENT;
    } else {
      fseek(source->stream, offset, SEEK_SET);
      return offset;
    }
  } else {
    fprintf(stderr, "line out of range: %s:%d\n", source->public.path, line);
    return -ERANGE;
  }
}

int parse_stmts(struct sym_context *context,
                struct asm_buf *buf,
                struct ast_node *list,
                struct source *source) {
  struct ast_node *stmt;
  struct asm_abstract a;
  int stmt_i;

  assert(list->t == AST_LIST);

  for (stmt_i = 0; stmt_i < list->v.list.length; stmt_i++) {
    struct asm_abstract new_a = { 0 };

    stmt = &list->v.list.nodes[stmt_i];
    new_a.source = &source->public;
    new_a.line = stmt->debug.loc.start.line;
    new_a.context = context;

    if (stmt_i == 0)
      a = new_a;

    if (source->seekable) {
      if (a.line > source->lines)
        source->lines = a.line;
      if (source->lines >= source->indexsz) {
        source->indexsz <<= 1;
        source->index = realloc(source->index, sizeof *source->index * source->indexsz);
        memset(source->index + (source->indexsz >> 1), '\377', sizeof *source->index * (source->indexsz >> 1));
      }
      if (source->index[a.line - 1])
        source->index[a.line - 1] = stmt->debug.loc.start.offset;
    }

    switch (stmt->t) {
    case AST_LABEL:
      if (a.flags & (HAS_ORG | HAS_LABEL)) {
        asm_buf_push(buf, &a);
        a = new_a;
      }
      a.flags |= HAS_LABEL;
      a.label = stmt->v.nameref;
      break;
    case AST_ORG:
      if (a.flags & (HAS_ORG | HAS_LABEL)) {
        asm_buf_push(buf, &a);
        a = new_a;
      }
      a.flags |= HAS_ORG;
      a.org = stmt->v.number;
      break;
    case AST_MACRO:
      {
        struct mnemonic *m = calloc(1, sizeof *m);
        union symval sv;

        /* TODO: free mnemonic structure on exit */
        assert(m != NULL);
        assert(stmt->v.tuple[0]->t == AST_NAME);
        assert(stmt->v.tuple[1]->t == AST_TUPLE);

        /* TODO: lose the cast! */
        m->name = (char *) SSTR(stmt->v.tuple[0]->v.str);
        m->type = M_MACRO;
        m->ast = stmt->v.tuple[1];
        sv.internal = m;

        sym_add(context, SYM_T_MNEMONIC,
                stmt->v.tuple[0]->v.str, true, sv);
      }
      break;
    case AST_INSTR:
      assert(stmt->v.tuple[0]->t == AST_SYMBOL);
      {
        /* Handle macro application */
        struct mnemonic *m = (struct mnemonic *) sym_getval(context, &stmt->v.tuple[0]->v.nameref).internal;
        if (m) {
          struct sym_context *new_context;
          struct ast_node *macro = m->ast;
          struct ast_node *actual_args;
          struct ast_node *formal_args;

          if (m->type == M_MACRO) {
            int rc;
            if (a.flags) {
              /* Flush out any old stuff first */
              asm_buf_push(buf, &a);
              a = new_a;
            }
            new_context = sym_context_create(context);
            rc = sym_table_create(new_context, SYM_T_LABEL);
            if (rc != 0) return rc;

            for (actual_args = stmt->v.tuple[1],
                 formal_args = macro->v.tuple[0];
                 actual_args->t == AST_TUPLE ||
                 formal_args->t == AST_TUPLE;
                 actual_args = actual_args->v.tuple[1],
                 formal_args = formal_args->v.tuple[1]) {
              struct ast_node *copy;
              enum eval_result ev;
              union symval sv;

              if (formal_args->t == AST_NIL) {
                fprintf(stderr, "too many arguments to macro %s\n", m->name);
                return EINVAL;
              } else if (actual_args->t == AST_NIL) {
                fprintf(stderr, "insuficient arugments to macro %s\n", m->name);
                return EINVAL;
              }
              copy = ast_copy_tree(actual_args->v.tuple[0], NULL);
              ev = eval_expr(new_context, copy, true);
              if (ev == EVAL_ERROR)
                return EINVAL;
              sym_add(new_context, SYM_T_LABEL,
                      formal_args->v.tuple[0]->v.str,
                      expr_to_symval(&sv, copy), sv);
            }
            if (verbose) {
              fprintf(stderr, "local symbol table for application of macro %s\n", m->name);
              sym_print_table(new_context, SYM_T_LABEL);
            }

            rc = parse_stmts(new_context, buf, macro->v.tuple[1], source);
            if (rc != 0) return rc;
            continue;
          }
        }
      }

      a.flags |= HAS_INSTR;
      a.instr = stmt->v.tuple[0]->v.nameref;

      a.n_operands = ast_count_list(stmt->v.tuple[1]);
      a.operands = stmt->v.tuple[1];

      asm_buf_push(buf, &a);
      a = new_a;
      break;
    default:
      assert(!"unexpected AST node type in statement list");
    }
  }

  return 0;
}

static ssize_t parse(struct ast_node **root, struct asm_buf *buf, struct source *source) {
  char srcline[1024];
  int rc = 0;

  srcline[sizeof srcline - 1] = EOF;

  if (source->stream == NULL) {
    if (!strcmp(source->public.path, "-")) {
      free(source->public.leaf);
      source->public.leaf = strdup("stdin");
      source->stream = stdin;
      source->seekable = false;
      source->noclose = true;
    } else {
      source->stream = fopen(source->public.path, "r");
      source->seekable = true;
      source->noclose = false;
      if (source->stream == NULL) {
        perror("fopen");
        return -EHANDLED;
      }
    }
  } else if (source->seekable) {
    rewind(source->stream);
  }

  if (source->seekable) {
    source->lines = 0;
    source->indexsz = 40;
    source->index = calloc(source->indexsz, sizeof *source->index);
    if (source->index == NULL) {
      rc = -errno;
      goto finish;
    }
    memset(source->index, '\377', source->indexsz * sizeof *source->index);
  }

  yyin = source->stream;

  rc = yyparse(root);
  if (rc != 0) {
    rc = EHANDLED;
    goto finish;
  }

  if (verbose) {
    fprintf(stderr, "Abstract Syntax Tree:\n");
    ast_plot_tree(stderr, *root);
    fprintf(stderr, "\n");
  }

  rc = parse_stmts(sym_root_context(), buf, *root, source);

finish:
  return rc;
}

int usage(FILE *to, int rc, const char *prog) {
  int i;

  fprintf(to, "usage: %s [OPTIONS] SOURCE|-...\n"
    "OPTIONS\n"
    "  -a, --listing            output listing\n"
    "  -h, --help               output usage and exit\n"
    "  -m, --map                output map\n"
    "  -o, --output FILE|-      write object to FILE, default: %s\n"
    "  -O, --output-format FMT  use FMT output format, default: %s\n"
    "  -v, --verbose            output verbose information\n"
    "\n"
    "%s: supported output formats:",
    prog, DEFAULT_OUTPUT_FILE, DEFAULT_OUTPUT_FORMAT,
    prog);

  for (i = 0; formats[i].name != NULL; i++)
    fprintf(to, " %s", formats[i].name);

  fprintf(to, "\n");
  return rc;
}

int main(int argc, char *argv[]) {
  int i;
  int c;
  addr_t a;
  int rc = 0;
  int map = 0;
  int listing = 0;
  int num_sources;
  int option_index;
  struct ast_node *ast = NULL;
  struct section section = { 0 };
  struct asm_buf abstract = { 0 };
  struct source *sources = NULL;
  const struct format *format;
  const char *output = DEFAULT_OUTPUT_FILE;
  const char *output_format = DEFAULT_OUTPUT_FORMAT;

  const struct option options[] = {
    { "output-format", required_argument, 0,        'O' },
    { "output",        required_argument, 0,        'o' },
    { "help",          no_argument,       0,        'h' },
    { "listing",       no_argument,       &listing, 'a' },
    { "map",           no_argument,       &map,     'm' },
    { "verbose",       no_argument,       &verbose, 'v' },
    { NULL }
  };

  init();

  do {
    c = getopt_long(argc, argv, "ahmvo:O:", options, &option_index);
    switch (c) {
    case 'O':
      output_format = optarg;
      break;
    case 'a':
      listing = c;
      break;
    case 'h':
      return usage(stdout, 0, argv[0]);
    case 'm':
      map = c;
      break;
    case 'o':
      output = optarg;
      break;
    case 'v':
      verbose = c;
      break;
    }
  } while (c != -1 && c != '?' && c != ':');

  if (c != -1)
    return usage(stderr, 1, argv[0]);

  for (i = 0; formats[i].name != NULL; i++) {
    if (!strcmp(output_format, formats[i].name))
      break;
  }
  if (formats[i].name == NULL) {
    fprintf(stderr, "No such output format: %s\n", output_format);
    rc = EHANDLED; /* EINVAL */
  } else {
    format = formats + i;
  }

  if (optind == argc) {
    fprintf(stderr, "No source specified\n");
    usage(stderr, 1, argv[0]);
    rc = EHANDLED; /* ENOENT */
  }

  num_sources = argc - optind;
  sources = calloc(num_sources, sizeof *sources);
  for (i = 0; rc == 0 && optind < argc; i++, optind++) {
    struct source *source = sources + i;
    char *str;

    source->public.path = strdup(argv[optind]);
    str = strdup(source->public.path);
    source->public.leaf = strdup(basename(str));
    free(str);

    rc = parse(&ast, &abstract, source);
    if (rc == 0) {
      pass_one(NULL, &section, &abstract);

      if (verbose) {
        struct sym_context *context = sym_root_context();
        int i;

        for (i = 0; i < SYM_T_MAX; i++)
          sym_print_table(context, i);
      }
      rc = assemble(&section, &abstract);
    }
  }

  if(rc == 0 && listing) {
    char srcline[60];

    printf("Listing:\n");

    for (a = section.org; a < section.org + section.length; a++) {
      struct sectiondata *sd = section.data + (a - section.org);
      struct source *src = sd->debug ? (struct source *) sd->debug->source : NULL;
      char *str;
      size_t len = -1;

      if (sd->debug && sd->debug->line > 0 && seek_to_line(src, sd->debug->line) >= 0 &&
          !feof(src->stream) && !ferror(src->stream)) {
        str = fgets(srcline, sizeof srcline, src->stream);
        if (str != NULL) {
          len = strlen(str);
          if (str[len - 1] == '\n')
            str[len - 1] = '\0';
        }
      }
      printf("  %08x: %08x %10.10s:%-5d %-60.60s\n",
             a, sd->value,
             src ? src->public.leaf : "",
             src ? sd->debug->line : 0,
             len != -1 ? str : "");
    }
  }

  if (rc == 0)
    rc = write_section(output, &section, format);

  if (rc == 0 && map) {
    printf("Sections:\n");
    printf("  [%-8.8s  %-8.8s] %-8.8s\n",
           "START","END", "LENGTH");
    printf("  [%08x, %08x] %08x\n",
           section.org, section.org + section.length - 1, section.length);
  }

  if (rc != 0 && rc != EHANDLED)
    fprintf(stderr, "%s: %s\n", argv[0], strerror(rc));

  if (sources != NULL) {
    for (i = 0; i < num_sources; i++) {
      struct source *source = sources + i;
      if (source->stream != NULL) {
        free(source->index);
        free(source->public.path);
        free(source->public.leaf);
        if (!source->noclose)
          fclose(source->stream);
      }
    }
    free(sources);
  }
  asm_buf_free(&abstract);
  section_free(&section);
  if (ast)
    ast_free_tree(ast);
  finit();
  /* TODO: destroy additional symbol contexts! */

  return rc == 0 ? 0 : 1;
}

