/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Assembler for Manchester Baby. */

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

#define DEFAULT_OUTPUT_FILE "b.out"
#define DEFAULT_OUTPUT_FORMAT WRITER_BITS BITS_SUFFIX_SNP

#define HAS_ORG   01
#define HAS_LABEL 02
#define HAS_INSTR 04

enum operand_type {
  OPR_NONE,
  OPR_NUM,
  OPR_SYM,
};

struct source {
  char *path;
  char *leaf;
  FILE *stream;
  bool seekable;
  bool noclose;
  int lines;
  long *index;
  size_t indexsz;
};

struct abstract {
  int flags;
  int n_operands;
  addr_t org;
  char *label;
  char *instr;
  enum operand_type opr_type;
  char *opr_str;
  num_t opr_num;
  struct source *source;
  int line;
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

enum sym_type {
  SYM_LABEL,
};

struct symbol {
  char *name;
  enum sym_type type;
  addr_t value;
};

enum lex {
  LEX_START,
  LEX_INSTR,
  LEX_OPERAND,
  LEX_SURPLUS_OPERANDS,
  LEX_COMMENT,
};

struct mnemonic baby[] = {
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
#define babysz (sizeof baby / sizeof *baby)

int verbose;

static int instrsort(const void *a, const void *b) {
  const struct mnemonic *ma = (const struct mnemonic *) a;
  const struct mnemonic *mb = (const struct mnemonic *) b;
  return strcasecmp(ma->name, mb->name);
}

static int instrsearch(const void *key, const void *a) {
  return instrsort(&key, a);
}

static int symsort(const void *a, const void *b) {
  const struct symbol *sa = (const struct symbol *) a;
  const struct symbol *sb = (const struct symbol *) b;
  return strcmp(sa->name, sb->name);
}

static int symsearch(const void *key, const void *a) {
  return symsort(&key, a);
}

static void init(void) {
  qsort(baby, babysz, sizeof *baby, instrsort);
}

void free_abs(struct abstract *abstract) {
  if (abstract->label)
    free(abstract->label);
  if (abstract->instr)
    free(abstract->instr);
  if (abstract->opr_str)
    free(abstract->opr_str);
}

int assemble_one(struct section *section,
                 struct symbol *symbols, size_t symbol_count,
                 struct abstract *abstract, bool first_pass) {
  num_t operand;
  int rc = 0;

  if (!first_pass && abstract->opr_type == OPR_SYM) {
    struct symbol *sym;
    sym = bsearch(abstract->opr_str, symbols, symbol_count, sizeof *symbols, symsearch);
    if (sym == NULL || sym->type != SYM_LABEL) {
      fprintf(stderr, "error: symbol '%s' not found\n", abstract->opr_str);
      return ENOENT;
    }
    operand = sym->value;
  } else {
    operand = abstract->opr_num;
  }

  if (verbose && !first_pass)
    fprintf(stderr, "  %-3s %-5s %-5s %4d: 0x%08x %-10s %-4s 0x%08x %-10s %s:%d\n",
            abstract->flags & HAS_ORG ? "ORG" : "",
            abstract->flags & HAS_LABEL ? "LABEL" : "",
            abstract->flags & HAS_INSTR ? "INSTR" : "",
            abstract->n_operands,
            abstract->org,
            abstract->flags & HAS_LABEL ? abstract->label : "",
            abstract->flags & HAS_INSTR ? abstract->instr : "",
            operand,
            abstract->opr_type == OPR_SYM ? abstract->opr_str : "",
            abstract->source->leaf,
            abstract->line);

  if (abstract->flags & HAS_ORG) {
    section->cursor = abstract->org;
  }

  if (first_pass && abstract->flags & HAS_INSTR) {
    put_word(section, 0, NULL);
  }

  if (!first_pass && abstract->flags & HAS_INSTR) {
    const struct mnemonic *m;
    m = bsearch(abstract->instr, baby, babysz, sizeof *baby, instrsearch);
    if (m == NULL) {
      fprintf(stderr, "no such mnemonic %s\n", abstract->instr);
      return EINVAL;
    }

    if (m->type == M_INSTR) {
      word_t word = (m->ins->opcode << OPCODE_POS) & OPCODE_MASK;
      if (m->ins->operands == 1)
        word |= (operand << OPERAND_POS) & OPERAND_MASK;
      put_word(section, word, abstract);
    } else if (m->type == M_DIRECTIVE) {
      switch (m->dir) {
      case D_NUM:
        rc = put_word(section, operand, abstract);
        break;
      case D_EJA:
        rc = put_word(section, operand - 1, abstract);
      }
    }
  }

  if (rc != 0) {
    fprintf(stderr, "error at %s:%d\n", abstract->source->path, abstract->line);
    rc = EHANDLED;
  }

  return rc;
}

int pass_one(struct section *section, struct symbol **symbols, struct abstract *abstract, size_t length) {
  struct symbol *syms = NULL;
  size_t symsz = 0;
  off_t symptr = 0;
  int i;
  addr_t saved_cursor = section->cursor;

  for (i = 0; i < length; i++) {
    if (abstract[i].flags & HAS_LABEL) {
      if (symptr == symsz) {
        symsz = (symsz == 0) ? 32 : symsz << 1;
        syms = realloc(syms, sizeof *syms * symsz);
      }
      syms[symptr].type = SYM_LABEL;
      syms[symptr].name = strdup(abstract[i].label);
      syms[symptr].value = section->cursor;
      symptr++;
    }
    assemble_one(section, NULL, 0, abstract + i, true);
  }

  qsort(syms, symptr, sizeof *syms, symsort);

  section->cursor = saved_cursor;
  *symbols = syms;
  return symptr;
}

int assemble(struct section *section,
             struct symbol *symbols,
             size_t symbol_count,
             struct abstract *abstract,
             size_t abstract_count) {
  int rc = 0;
  int i;

  if (verbose)
    fprintf(stderr, "Abstract assembly source:\n");
  for (i = 0; i < abstract_count; i++) {
    if (rc == 0)
      rc = assemble_one(section, symbols, symbol_count, abstract + i, false);
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
    fseek(source->stream, offset, SEEK_SET);
    return offset;
  } else {
    fprintf(stderr, "line out of range: %s:%d\n", source->path, line);
    return -ERANGE;
  }
}

ssize_t lex(struct abstract **abs_ret, struct source *source) {
  struct abstract *buf = NULL;
  char srcline[1024];
  size_t bufsz = 0;
  off_t bufptr = 0;
  int rc = 0;
  int line_num;

  srcline[sizeof srcline - 1] = EOF;

  if (source->stream == NULL) {
    if (!strcmp(source->path, "-")) {
      free(source->leaf);
      source->leaf = strdup("stdin");
      source->stream = stdin;
      source->seekable = false;
      source->noclose = true;
    } else {
      source->stream = fopen(source->path, "r");
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
  }

  for (line_num = 1;rc == 0 && !feof(source->stream); line_num++) {
    char *tok, *end, *saveptr, *line;
    struct abstract abstract = { .source = source, .line = line_num };
    enum lex state = LEX_START;

    if (source->seekable) {
      if (source->lines == source->indexsz) {
        source->indexsz <<= 1;
        source->index = realloc(source->index, sizeof *source->index * source->indexsz);
      }
      source->index[line_num - 1] = ftell(source->stream);
      source->lines++;
    }

    if (fgets(srcline, sizeof srcline, source->stream) == NULL) {
      rc = errno;
      goto finish;
    }
    if (!feof(source->stream) && !strchr(srcline, '\n')) {
      fprintf(stderr, "%s:%d: line too long\n", source->leaf, line_num);
      rc = EHANDLED;
      goto finish;
    }

    for (line = srcline; (tok = strtok_r(line, " \t\n", &saveptr)) != NULL; line = NULL) {
      size_t toklen = strlen(tok);

      if (state == LEX_START) {
        addr_t a = strtoul(tok, &end, 10);
        if (end[0] == '\0' ||
            (end[0] == ':' &&
             end[1] == '\0')) {
          *end = '\0';
          abstract.org = a;
          abstract.flags |= HAS_ORG;
        } else if (tok[toklen - 1] == ':') {
          if (end == tok) {
            tok[toklen - 1] = '\0';
            abstract.flags |= HAS_LABEL;
            abstract.label = strdup(tok);
          } else {
            fprintf(stderr, "%s:%d: label cannot begin with a digit: %s\n",
                    source->leaf, line_num, tok);
            rc = EHANDLED;
            goto finish;
          }
        } else {
          state = LEX_INSTR;
        }
      }
      if (strncmp(tok, "--", 2) == 0 ||
          strncmp(tok, ";", 1) == 0) {
        state = LEX_COMMENT;
      }
      if (state == LEX_INSTR) {
        abstract.flags |= HAS_INSTR;
        abstract.instr = strdup(tok);
        state = LEX_OPERAND;
      } else if (state == LEX_OPERAND) {
        abstract.opr_num = strtoul(tok, &end, 0);
        if (end == tok) {
          abstract.opr_type = OPR_SYM;
          abstract.opr_str = strdup(tok);
        } else {
          abstract.opr_type = OPR_NUM;
        }
        state = LEX_SURPLUS_OPERANDS;
      } else if (state == LEX_SURPLUS_OPERANDS) {
        fprintf(stderr, "surplus operand: %s\n", tok);
        rc = EHANDLED;
        goto finish;
      }
    }

    if (bufptr == bufsz) {
      bufsz = (bufsz == 0) ? 128 : bufsz << 1;
      buf = realloc(buf, sizeof *buf * bufsz);
    }
    buf[bufptr++] = abstract;
  }

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
  struct abstract *abstract = NULL;
  size_t abstract_count;
  struct source *sources = NULL;
  struct symbol *symbols = NULL;
  size_t symbol_count = 0;
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

    source->path = strdup(argv[optind]);
    str = strdup(source->path);
    source->leaf = strdup(basename(str));
    free(str);

    rc = lex(&abstract, source);
    if (rc < 0) {
      rc = -rc;
    } else {
      abstract_count = rc;

      symbol_count = pass_one(&section, &symbols, abstract, abstract_count);

      if (verbose) {
        fprintf(stderr, "Symbol table:\n");
        for (i = 0; i < symbol_count; i++) {
          fprintf(stderr, "  %-6s %20s 0x%08x\n",
                  symbols[i].type == SYM_LABEL ? "LABEL" : "",
                  symbols[i].name,
                  symbols[i].value);
        }
      }

      rc = assemble(&section, symbols, symbol_count, abstract, abstract_count);
    }
  }

  if(rc == 0 && listing) {
    char srcline[60];

    printf("Listing:\n");

    for (a = section.org; a < section.org + section.length; a++) {
      struct sectiondata *sd = section.data + (a - section.org);
      char *str;
      size_t len = -1;

      if (sd->debug && seek_to_line(sd->debug->source, sd->debug->line) >= 0) {
        str = fgets(srcline, sizeof srcline, sd->debug->source->stream);
        len = strlen(str);
        if (str[len - 1] == '\n')
          str[len - 1] = '\0';
      }
      printf("  %08x: %08x %10.10s:%-5d %-60.60s\n",
             a, sd->value,
             sd->debug && sd->debug->source ? sd->debug->source->leaf : "",
             sd->debug && sd->debug->source ? sd->debug->line : 0,
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
        free(source->path);
        free(source->leaf);
        if (!source->noclose)
          fclose(source->stream);
      }
    }
    free(sources);
  }
  free(symbols);
  free(abstract);

  return rc == 0 ? 0 : 1;
}

