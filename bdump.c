/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Dissassmbler for Manchester Baby. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "butils.h"
#include "arch.h"
#include "symbols.h"
#include "asm.h"
#include "objfile.h"
#include "loader.h"

#define DEFAULT_INPUT_FORMAT READER_BITS BITS_SUFFIX_SNP

/* Abstract disassembly */
struct dis_abstract {
  struct asm_abstract alts[2];
  int n_alts;
  struct mnemonic *instrs[2];
  int n_instrs;
  struct arch_decoded parts;
  word_t w;
};

int verbose;

int usage(FILE *to, int rc, const char *prog) {
  const struct loader *loader;

  fprintf(to, "usage: %s [OPTIONS] OBJECT\n"
    "OPTIONS\n"
    "  -h, --help               output usage and exit\n"
    "  -I, --input-format FMT   use FMT output format, default: %s\n"
    "  -v, --verbose            output verbose information\n"
    "\n"
    "%s: supported input formats:",
    prog, DEFAULT_INPUT_FORMAT,
    prog);

  for (loader = loaders; loader->name; loader++)
    fprintf(to, " %s", loader->name);

  fprintf(to, "\n");
  return rc;
}

void disassemble_instruction(struct dis_abstract *a, word_t word)
{
  a->w = word;
  a->parts = arch_decode(word);
  a->n_instrs = arch_find_opcode(a->parts.opcode, a->instrs, sizeof a->instrs / sizeof a->instrs[0]);
}

int render_instr(char *buf, size_t max, size_t *ptr, struct asm_abstract *a) {
  int ret;
  int i;

  ret = snprintf(buf + *ptr, max - *ptr, "%s", a->instr.name);
  if (ret < 0) {
    perror("snprintf");
    return errno;
  } else {
    *ptr += ret;
  }

  for (i = 0; i < a->n_operands && i < 1; i++) {
    ret = snprintf(buf + *ptr, max - *ptr, " %" PRId32, a->opr_num);
    if (ret < 0) {
      perror("snprintf");
      return errno;
    } else {
      *ptr += ret;
    }
  }

  return 0;
}

int disassemble_section(struct segment *segment, struct vm *vmem) {
  enum { DATA, INSTR, MAX } type[MAX] = { DATA, MAX };
  struct dis_abstract *ad;
  struct sym_context *context = sym_root_context();
  addr_t addr;
  int i;

  printf("-- disassembly\n\n");

  /* Allocate space for abstract disassembly. */
  ad = calloc(segment->length, sizeof *ad);
  if (!ad)
    return errno;

  for (addr = 0; addr < segment->length; addr++) {
    disassemble_instruction(ad + addr, read_word(vmem, addr));
  }

  for (addr = 0; addr < segment->length; addr++) {
    struct dis_abstract *d = ad + addr;
    struct mnemonic *m = d->instrs[0];
    char auto_label = '\0';

    if (d->parts.data != 0 ||
        (ad->n_instrs >0 &&
         m->type == M_INSTR &&
         m->ins->operands == 0 &&
         d->parts.operand != 0) ||
        d->w == 0) {
	if (type[0] == INSTR)
	  auto_label = 'D';
	type[0] = DATA;
	type[1] = INSTR;
    } else {
	if (type[0] == DATA)
	  auto_label = 'L';
        type[0] = INSTR;
        type[1] = DATA;
    } 

    if (addr == 1) {
      d->alts[0].flags |= HAS_LABEL | HAS_ORG;
      d->alts[0].label = *sym_add_num(context, SYM_T_LABEL, "_start", addr);
    } else if (auto_label) {
      d->alts[0].flags |= HAS_ORG;
    }

    for (i = 0; i < sizeof type / sizeof *type; i++) {
      struct asm_abstract *a = d->alts + i;

      a->org = addr;

      switch (type[i]) {
      case DATA:
        a->opr_type = OPR_NUM;
        a->n_operands = 1;
        a->instr = *sym_getref(context, SYM_T_MNEMONIC, "NUM");
        a->opr_effective = d->w;
        d->n_alts++;
        break;
      case INSTR:
        a->opr_type = OPR_NUM;
        a->n_operands = m->ins->operands;
        a->instr = *sym_getref(context, SYM_T_MNEMONIC, m->name);
        a->opr_effective = d->parts.operand;
        d->n_alts++;
        break;
      default:
        break;
      }
      a->opr_num = a->opr_effective;
      a->flags |= HAS_INSTR;
    }
  }

  for (addr = 0; addr < segment->length; addr++) {
    struct dis_abstract *d = ad + addr;
    struct asm_abstract *a = d->alts + 0;
    struct asm_abstract *alt = d->n_alts > 1 ? d->alts + 1 : NULL;
    char buf1[256];
    char buf2[256];
    size_t ptr = 0;
    size_t max = sizeof buf1;

    if (verbose)
      asm_log_abstract(&ad[addr].alts[0]);

    if (a->flags & HAS_LABEL)
      printf("%s:\n", a->label.name);

    if (a->flags & HAS_ORG)
      printf("%02d:\n", addr);

    if (a->flags & HAS_INSTR) {
      render_instr(buf1, max, &ptr, a);
    }

    if (alt && alt->flags & HAS_INSTR) {
      ptr = 0;
      render_instr(buf2, max, &ptr, alt);
      printf("  %-20s; %s\n", buf1, buf2);
    } else {
      printf("  %s\n", buf1);
    }
  }

  free(ad);
  return 0;
}

static void init(void) {
  sym_init();
  arch_init();
}

static void finit(void) {
  loaders_finit();
  arch_init();
  sym_init();
}

int main(int argc, char *argv[]) {
  int c;
  int rc = 0;
  struct vm vmem;
  int option_index;
  struct page mapped_section = { 0 };
  struct segment segment = { 0 };
  struct object_file exe = { 0 };
  const struct loader *loader = NULL;
  const char *input_format = DEFAULT_INPUT_FORMAT;

  const struct option options[] = {
    { "input-format",  required_argument, 0,        'I' },
    { "help",          no_argument,       0,        'h' },
    { "verbose",       no_argument,       &verbose, 'v' },
    { NULL }
  };

  if (loaders_init() != 0)
    return 1;

  init();

  do {
    c = getopt_long(argc, argv, "hvI:", options, &option_index);
    switch (c) {
    case 'I':
      input_format = optarg;
      break;
    case 'h':
      return usage(stdout, 0, argv[0]);
    case 'v':
      verbose = c;
      break;
    }
  } while (c != -1 && c != '?' && c != ':');

  if (c != -1)
    return usage(stderr, 1, argv[0]);

  for (loader = loaders; loader->name; loader++)
    if (!strcmp(input_format, loader->name))
      break;
  if (loader->name == NULL) {
    loader = NULL;
    fprintf(stderr, "No such format: %s\n", input_format);
    rc = EHANDLED; /* EINVAL */
    goto finish;
  }

  if (optind == argc) {
    fprintf(stderr, "No source specified\n");
    rc = EHANDLED; /* ENOENT */
  }

  if (argc - optind != 1)
    return usage(stderr, 1, argv[0]);
  exe.path = argv[optind++];

  rc = loader->stat(loader, &exe, &segment);
  if (rc != 0)
    return rc;

  mapped_section.size = segment.length;
  mapped_section.data = calloc(segment.length, sizeof *mapped_section.data);
  vmem.page0.base = 0;
  vmem.page0.size = mapped_section.size;
  vmem.page0.phys = &mapped_section;

  rc = loader->load(loader, &exe, &segment, &vmem);
  if (rc != 0)
    goto finish;

  rc = disassemble_section(&segment, &vmem);

finish:
  if (rc != 0 && rc != EHANDLED)
    fprintf(stderr, "%s: %s\n", argv[0], strerror(rc));

  if (mapped_section.data != NULL)
    free(mapped_section.data);

  if (loader != NULL)
    loader->close(loader, &exe);

  finit();

  return rc == 0 ? 0 : 1;
}

