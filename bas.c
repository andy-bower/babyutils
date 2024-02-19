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
  sym_init();
  arch_init();
}

static void finit(void) {
  arch_finit();
  sym_finit();
}

void yyerror(YYLTYPE *yylloc, struct ast_node **root, char const *error) {
  fprintf(stderr, "yyerror: %d.%d-%d.%d: %s\n",
          yylloc->start.line, yylloc->start.col,
          yylloc->end.line, yylloc->end.col,
          error);
}

void free_abs(struct asm_abstract *abstract) {
}

int assemble_one(struct section *section,
                 struct asm_abstract *abstract, bool first_pass) {
  struct symbol *sym;
  int rc = 0;

  if (!first_pass && abstract->opr_type == OPR_SYM) {
    sym = sym_lookup(abstract->context, SYM_T_LABEL, abstract->operand_sym.name);
    if (!sym)
      return ENOENT;
    abstract->opr_effective = sym->val.numeric;
  } else {
    abstract->opr_effective = abstract->opr_num;
  }

  if (verbose && !first_pass)
    asm_log_abstract(abstract);

  if (abstract->flags & HAS_ORG) {
    section->cursor = abstract->org;
  }

  if (first_pass && abstract->flags & HAS_INSTR) {
    put_word(section, 0, NULL);
  }

  if (!first_pass && abstract->flags & HAS_INSTR) {
    const struct mnemonic *m = arch_find_instr(abstract->instr.name);
    if (m == NULL) {
      fprintf(stderr, "no such mnemonic %s\n", abstract->instr.name);
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

void pass_one(struct section *section, struct asm_abstract *abstract, size_t length) {
  int i;
  addr_t saved_cursor = section->cursor;

  for (i = 0; i < length; i++) {
    if (abstract[i].flags & HAS_LABEL)
      sym_add_num(abstract->context, SYM_T_LABEL, abstract[i].label.name, section->cursor);
    assemble_one(section, abstract + i, true);
  }

  section->cursor = saved_cursor;
}

int assemble(struct section *section,
             struct asm_abstract *abstract,
             size_t abstract_count) {
  int rc = 0;
  int i;

  if (verbose)
    fprintf(stderr, "Abstract assembly source:\n");
  for (i = 0; i < abstract_count; i++) {
    if (rc == 0)
      rc = assemble_one(section, abstract + i, false);
    free_abs(abstract + i);
  }

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

ssize_t parse(struct asm_abstract **abs_ret, struct source *source) {
  struct asm_abstract *buf = NULL;
  struct asm_abstract a = { 0 };
  struct ast_node *root;
  char srcline[1024];
  size_t bufsz = 0;
  off_t bufptr = 0;
  int rc = 0;
  int line_num __attribute__((unused));
  int stmt_i;
  struct ast_node *stmt;

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

  rc = yyparse(&root);
  if (rc != 0) {
    rc = EHANDLED;
    goto finish;
  }

  if (verbose) {
    fprintf(stderr, "Abstract Syntax Tree:\n");
    ast_plot_tree(stderr, root);
    fprintf(stderr, "\n");
  }

  assert(root->t == AST_LIST);

  for (stmt_i = 0; stmt_i < root->v.list.length; stmt_i++) {
    struct asm_abstract new_a = { 0 };
    if (bufptr == bufsz) {
      bufsz = (bufsz == 0) ? 128 : bufsz << 1;
      buf = realloc(buf, sizeof *buf * bufsz);
    }

    stmt = &root->v.list.nodes[stmt_i];
    new_a.source = &source->public;
    new_a.line = stmt->debug.loc.start.line;
    new_a.context = sym_root_context();

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

    if (stmt->t == AST_LABEL) {
      if (a.flags & (HAS_ORG | HAS_LABEL)) {
        buf[bufptr++] = a; a = new_a;
      }
      a.flags |= HAS_LABEL;
      a.label = stmt->v.nameref;
    }

    if (stmt->t == AST_ORG) {
      if (a.flags & (HAS_ORG | HAS_LABEL)) {
        buf[bufptr++] = a; a = new_a;
      }
      a.flags |= HAS_ORG;
      a.org = stmt->v.number;
    }

    if (stmt->t == AST_INSTR) {
      a.flags |= HAS_INSTR;
      assert(stmt->v.tuple[0]->t == AST_NAME);
      a.instr = stmt->v.tuple[0]->v.nameref;
      a.n_operands = ast_count_list(stmt->v.tuple[1]);
      if (stmt->v.tuple[1]->t == AST_TUPLE) {
        struct ast_node *operand = stmt->v.tuple[1]->v.tuple[0];
        switch (operand->t) {
        case AST_LABEL:
        case AST_NAME:
          a.opr_type = OPR_SYM;
          a.operand_sym = operand->v.nameref;
          break;
        case AST_NUMBER:
          a.opr_type = OPR_NUM;
          a.opr_num = operand->v.number;
          break;
        default:
          assert(!"invalid child node to instruction");
        }
      } else {
        assert(stmt->v.tuple[1]->t == AST_NIL);
      }
      buf[bufptr++] = a; a = new_a;
    }
  }

  ast_free_tree(root);

finish:
  *abs_ret = buf;
  return rc == 0 ? bufptr : -rc;
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
  struct section section = { 0 };
  struct asm_abstract *abstract = NULL;
  size_t abstract_count;
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

    rc = parse(&abstract, source);
    if (rc < 0) {
      rc = -rc;
    } else {
      abstract_count = rc;

      pass_one(&section, abstract, abstract_count);

      if (verbose) {
        struct sym_context *context = sym_root_context();
        int i;

        for (i = 0; i < SYM_T_MAX; i++)
          sym_print_table(context, i);
      }
      rc = assemble(&section, abstract, abstract_count);
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
        free(source->public.path);
        free(source->public.leaf);
        if (!source->noclose)
          fclose(source->stream);
      }
    }
    free(sources);
  }
  free(abstract);

  finit();

  return rc == 0 ? 0 : 1;
}

