/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Assembler for Manchester Baby.
 * Outputs logisim image format. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>

#define DEFAULT_OUTPUT_FILE "b.out"

#define HAS_ORG   01
#define HAS_LABEL 02
#define HAS_INSTR 04

typedef uint32_t addr_t;
typedef uint32_t word_t;
typedef uint32_t num_t;

struct section {
  addr_t capacity;
  addr_t length;
  addr_t org;
  addr_t cursor;
  word_t *data;
};

enum operand_type {
  OPR_NONE,
  OPR_NUM,
  OPR_SYM,
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
};

struct instr {
  uint32_t opcode;
  uint32_t mask;
  int operands;
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

const struct instr I_JMP = { 00, 07, 1 };
const struct instr I_SUB = { 01, 07, 1 };
const struct instr I_LDN = { 02, 07, 1 };
const struct instr I_SKN = { 03, 07, 0 };
const struct instr I_STO = { 06, 07, 1 };
const struct instr I_HLT = { 07, 07, 0 };

struct mnemonic baby[] = {
  { "JMP", M_INSTR, .ins=&I_JMP },
  { "SUB", M_INSTR, .ins=&I_SUB },
  { "LDN", M_INSTR, .ins=&I_LDN },
  { "SKN", M_INSTR, .ins=&I_SKN },
  { "STO", M_INSTR, .ins=&I_STO },
  { "HLT", M_INSTR, .ins=&I_HLT },
  { "NUM", M_DIRECTIVE, .dir=D_NUM },
  { "EJA", M_DIRECTIVE, .dir=D_EJA },
};
#define babysz (sizeof baby / sizeof *baby)

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

static int put_word(struct section *section, word_t word) {
  if (section->cursor < section->org) {
    fprintf(stderr, "cannot write to 0x%x before section start 0x%x\n",
            section->cursor, section->org);
    return ESPIPE;
  }
  if (section->cursor >= section->org + section->capacity) {
    section->capacity = section->cursor - section->org + 0x400;
    section->data = realloc(section->data, sizeof(word_t) * section->capacity);
    if (section->data == NULL) {
      fprintf(stderr, "error expanding section to %ud words\n", section->capacity);
      return errno;
    }
  }
  section->data[section->cursor++ - section->org] = word;
  if (section->cursor - section->org > section->length)
    section->length = section->cursor - section->org;
  return 0;
}

int assemble_one(struct section *section,
                 struct symbol *symbols, size_t symbol_count,
                 struct abstract *abstract, bool first_pass) {
  word_t word = 0;
  num_t operand;

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

  if (!first_pass)
    fprintf(stderr, "  %-3s %-5s %-5s %4d: 0x%08x %-10s %-4s 0x%08x %s\n",
            abstract->flags & HAS_ORG ? "ORG" : "",
            abstract->flags & HAS_LABEL ? "LABEL" : "",
            abstract->flags & HAS_INSTR ? "INSTR" : "",
            abstract->n_operands,
            abstract->org,
            abstract->flags & HAS_LABEL ? abstract->label : "",
            abstract->flags & HAS_INSTR ? abstract->instr : "",
            operand,
            abstract->opr_type == OPR_SYM ? abstract->opr_str : "");

  if (abstract->flags & HAS_ORG) {
    section->cursor = abstract->org;
  }

  if (first_pass && abstract->flags & HAS_INSTR) {
    put_word(section, 0);
  }

  if (!first_pass && abstract->flags & HAS_INSTR) {
    const struct mnemonic *m;
    m = bsearch(abstract->instr, baby, babysz, sizeof *baby, instrsearch);
    if (m == NULL) {
      fprintf(stderr, "no such mnemonic %s\n", abstract->instr);
      return EINVAL;
    }

    if (m->type == M_INSTR) {
      word = (word & ~m->ins->mask) | (m->ins->opcode & m->ins->mask);
      if (m->ins->operands == 1)
        word |= operand << 3;
      put_word(section, word);
    } else if (m->type == M_DIRECTIVE) {
      switch (m->dir) {
      case D_NUM:
        put_word(section, operand);
        break;
      case D_EJA:
        put_word(section, operand - 1);
      }
    }
  }

  return 0;
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

int assemble(struct section *section, struct abstract *abstract, size_t length) {
  int rc = 0;
  int i;
  struct symbol *symbols;
  size_t symbol_count;

  symbol_count = pass_one(section, &symbols, abstract, length);

  fprintf(stderr, "Symbol table:\n");
  for (i = 0; i < symbol_count; i++) {
    fprintf(stderr, "  %-6s %20s 0x%08x\n",
            symbols[i].type == SYM_LABEL ? "LABEL" : "",
            symbols[i].name,
            symbols[i].value);
  }

  fprintf(stderr, "Assembly:\n");
  for (i = 0; i < length; i++) {
    if (rc == 0)
      rc = assemble_one(section, symbols, symbol_count, abstract + i, false);
    free_abs(abstract + i);
  }

  free(symbols);
  free(abstract);
  return rc;
}

/* TODO: make name tables available to lexer, or rather have
 * have a big name hash table */
ssize_t lex(struct abstract **abs_ret, const char *source) {
  FILE *src = fopen(source, "r");
  struct abstract *buf = NULL;
  char srcline[1024];
  size_t bufsz = 0;
  off_t bufptr = 0;
  int rc = 0;

  srcline[sizeof srcline - 1] = EOF;

  if (src == NULL) {
    perror("fopen");
    return 1;
  }

  while (rc == 0 && !feof(src)) {
    char *tok, *end, *saveptr, *line;
    struct abstract abstract = { 0 };
    enum lex state = LEX_START;

    if (fgets(srcline, sizeof srcline, src) == NULL) {
      rc = errno;
      goto finish;
    }
    if (srcline[sizeof srcline - 1] != EOF) {
      rc = E2BIG;
      goto finish;
    }

    for (line = srcline; (tok = strtok_r(line, " \t\n", &saveptr)) != NULL; line = NULL) {
      size_t toklen = strlen(tok);

      if (state == LEX_START) {
        if (tok[toklen - 1] == ':') {
          tok[toklen -1] = '\0';
          abstract.org = strtoul(tok, &end, 10);
          if (end == tok) {
            abstract.flags |= HAS_LABEL;
            abstract.label = strdup(tok);
          } else {
            abstract.flags |= HAS_ORG;
          }
        } else {
          state = LEX_INSTR;
        }
      }
      if (strcmp(tok, "--") == 0) {
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
        rc = E2BIG;
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
  fclose(src);
  return rc == 0 ? bufptr : -rc;
}

int write_section(const char *path, const struct section *section) {
  FILE *file = fopen(path, "w");
  addr_t word;
  word_t fill_value = 0x0;

  if (file == NULL) {
    perror("fopen");
    return 1;
  }

  fprintf(stderr, "Writing section\n  org = 0x%x\n  length = 0x%x\n",
          section->org, section->length);

  fprintf(file, "v2.0 raw\n");
  for (word = 0; word < section->org + section->length; word++) {
    fprintf(file, "%08x\n", word < section->org ? fill_value : section->data[word - section->org]);
  }

  fprintf(stderr, "  words in output = 0x%x\n", word);
  fprintf(stderr, "Written %s\n", path);

  fclose(file);
  return 0;
}

int usage(int rc, const char *prog) {
  fprintf(stderr, "usage: %s [OPTIONS] [SOURCE]...\n"
    "OPTIONS\n"
    "  -o, --output FILE    write object to FILE, default: %s\n"
    "  -h, --help           output usage and exit\n",
    prog, DEFAULT_OUTPUT_FILE);
  return rc;
}

int main(int argc, char *argv[]) {
  int c;
  int rc = 0;
  int option_index;
  size_t abstract_len;
  struct section section = { 0 };
  struct abstract *abstract;
  const char *output = DEFAULT_OUTPUT_FILE;

  const struct option options[] = {
    { "output", required_argument, 0, 'o' },
    { "help",   no_argument,       0, 'h' },
    { NULL }
  };

  init();

  do {
    c = getopt_long(argc, argv, "ho:", options, &option_index);
    switch (c) {
    case 'h':
      return usage(0, argv[0]);
    case 'o':
      output = optarg;
      break;
    }
  } while (c != -1 && c != '?' && c != ':');

  if (c != -1)
    return usage(1, argv[0]);

  if (optind == argc) {
    fprintf(stderr, "No source specified\n");
    rc = ENOENT;
  }

  while (rc == 0 && optind < argc) {
    rc = lex(&abstract, argv[optind++]);
    if (rc < 0) {
      rc = -rc;
      free(abstract);
    } else {
      abstract_len = rc;
      rc = assemble(&section, abstract, abstract_len);
    }
  }

  if (rc == 0)
    rc = write_section(output, &section);

  if (rc != 0)
    fprintf(stderr, "babyas: %s\n", strerror(rc));

  return rc == 0 ? 0 : 1;
}

