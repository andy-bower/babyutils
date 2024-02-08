/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023-2024 Andrew Bower */

/* Simulator for Manchester Baby. */

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
#include "objfile.h"
#include "loader.h"

#define DEFAULT_MEMORY_SIZE 32
#define DEFAULT_OUTPUT_FILE "b.out"
#define DEFAULT_INPUT_FORMAT READER_BITS BITS_SUFFIX_SNP

struct instruction {
  const char *debug_name;
  const struct instr *ins;
};

struct instruction baby[] = {
  { "JMP", &I_JMP },
  { "JRP", &I_JRP },
  { "SUB", &I_SUB },
  { "LDN", &I_LDN },
  { "SKN", &I_SKN },
  { "STO", &I_STO },
  { "HLT", &I_HLT },
};
#define babysz (sizeof baby / sizeof *baby)

struct regs {
  word_t ac;
  word_t ci;
  word_t pi;
};

struct mc {
  struct vm vm;
  struct regs regs;
  uint64_t cycles;
  bool stopped;
};

int verbose;

static void dump_state(const struct mc *mc) {
  printf("cycles %12" PRIu64 " ac %08x ci %08x pi %08x%s\n",
         mc->cycles, mc->regs.ac, mc->regs.ci, mc->regs.pi,
         mc->stopped ? " STOP" : "");
}

static void sim_cycle(struct mc *mc) {
  struct arch_decoded d;
  word_t data = 0;    // Appease compiler

  if (verbose)
    dump_state(mc);

  /* t1: Fetch */
  mc->regs.pi = read_word(&mc->vm, ++mc->regs.ci);

  /* t2: Decode */
  d = arch_decode(mc->regs.pi);

  /* t3: Execute - data access */
  switch (d.opcode) {
  case OP_LDN:
  case OP_SUB:
  case OP_JMP:
  case OP_JRP:
    data = read_word(&mc->vm, d.operand);
    break;
  case OP_STO:
    write_word(&mc->vm, d.operand, mc->regs.ac);
    break;
  }

  /* t4: Execute */
  switch (d.opcode) {
  case OP_LDN:
    mc->regs.ac = -data;
    break;
  case OP_SUB:
    mc->regs.ac = mc->regs.ac - data;
    break;
  case OP_HLT:
    mc->stopped = true;
    break;
  }

  /* t5: Next-PC */
  switch (d.opcode) {
  case OP_SKN:
    if (mc->regs.ac < 0)
      mc->regs.ci++;
    break;
  case OP_JMP:
    mc->regs.ci = data;
    break;
  case OP_JRP:
    mc->regs.ci += data;
    break;
  }

  mc->cycles++;
}

int usage(FILE *to, int rc, const char *prog) {
  const struct loader *loader;

  fprintf(to, "usage: %s [OPTIONS] OBJECT\n"
    "OPTIONS\n"
    "  -h, --help               output usage and exit\n"
    "  -m, --memory WORDS       memory size in words, default: %d\n"
    "  -I, --input-format FMT   use FMT output format, default: %s\n"
    "  -v, --verbose            output verbose information\n"
    "\n"
    "SIGNALS\n"
    "  SIGINT  (Ctrl-C)         print registers and continue\n"
    "  SIGQUIT (Ctrl-\\)         stop after current instruction\n"
    "\n"
    "%s: supported input formats:",
    prog, DEFAULT_MEMORY_SIZE, DEFAULT_INPUT_FORMAT,
    prog);

  for (loader = loaders; loader->name; loader++)
    fprintf(to, " %s", loader->name);

  fprintf(to, "\n");
  return rc;
}

struct handshake {
  int sigint;
  int sigquit;
};

static struct handshake sig_req = { 0, 0};

static void signal_handler(int sig, siginfo_t *info, void *ucontext) {
  if (sig == SIGINT)
     sig_req.sigint++;
  else if (sig == SIGQUIT)
     sig_req.sigquit++;
}

static bool poll_sigint(struct handshake *sig_ack) {
  int req = sig_req.sigint;

  if (sig_ack->sigint != req) {
    sig_ack->sigint = req;
    return true;
  } else {
    return false;
  }
}

static bool poll_sigquit(struct handshake *sig_ack) {
  int req = sig_req.sigquit;

  if (sig_ack->sigquit != req) {
    sig_ack->sigquit = req;
    return true;
  } else {
    return false;
  }
}

int main(int argc, char *argv[]) {
  int c;
  int rc = 0;
  int option_index;
  struct mc mc = { 0 };
  struct page page0 = { 0 };
  struct segment segment = { 0 };
  struct object_file exe = { 0 };
  const struct loader *loader = NULL;
  addr_t requested_memory;
  addr_t memory_size = DEFAULT_MEMORY_SIZE;
  const char *input_format = DEFAULT_INPUT_FORMAT;
  struct handshake sig_ack = { 0, 0};

  const struct option options[] = {
    { "memory",        required_argument, 0,        'm' },
    { "input-format",  required_argument, 0,        'I' },
    { "help",          no_argument,       0,        'h' },
    { "verbose",       no_argument,       &verbose, 'v' },
    { NULL }
  };

  if (loaders_init() != 0)
    return 1;

  do {
    c = getopt_long(argc, argv, "hvm:I:", options, &option_index);
    switch (c) {
    case 'I':
      input_format = optarg;
      break;
    case 'h':
      return usage(stdout, 0, argv[0]);
    case 'm':
      /* Round up to power of two, default being the minimum */
      for (requested_memory = strtoul(optarg, NULL, 10);
           memory_size < requested_memory;
           memory_size <<=1);
      break;
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

  for(page0.size = memory_size;
      page0.size < segment.length;
      page0.size <<= 1);

  if (page0.size > 0x2000) {
    fprintf(stderr, "%d words exceeds maximum store size of %d\n",
            page0.size, 0x2000);
    rc = EHANDLED; /* ENOMEM */
    goto finish;
  }

  page0.data = calloc(page0.size, sizeof *page0.data);
  mc.vm.page0.base = 0;
  mc.vm.page0.size = page0.size;
  mc.vm.page0.phys = &page0;

  memory_checks(&mc.vm);

  fprintf(stderr, "Mapped fully aliased page of %d words of RAM\n",
          page0.size);

  rc = loader->load(loader, &exe, &segment, &mc.vm);
  if (rc != 0)
    goto finish;

  /* Handle signals over main simulation loop. */
  {
    struct sigaction new_action_int = { 0 };
    struct sigaction new_action_quit = { 0 };
    struct sigaction old_action_int;
    struct sigaction old_action_quit;

    new_action_int.sa_sigaction = signal_handler;
    new_action_quit.sa_sigaction = signal_handler;
    sigaction(SIGINT, &new_action_int, &old_action_int);
    sigaction(SIGQUIT, &new_action_quit, &old_action_quit);

    while (!mc.stopped && !poll_sigquit(&sig_ack)) {
      sim_cycle(&mc);
      if (poll_sigint(&sig_ack))
        dump_state(&mc);
    }

    sigaction(SIGINT, &old_action_int, NULL);
    sigaction(SIGQUIT, &old_action_quit, NULL);
  }

  dump_vm(&mc.vm);
  dump_state(&mc);

finish:
  if (rc != 0 && rc != EHANDLED)
    fprintf(stderr, "%s: %s\n", argv[0], strerror(rc));

  if (page0.data != NULL)
    free(page0.data);

  if (loader != NULL)
    loader->close(loader, &exe);

  loaders_finit();

  return rc == 0 ? 0 : 1;
}

