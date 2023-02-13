/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

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
#include <sys/types.h>
#include <sys/stat.h>

#include "arch.h"

#define READER_BINARY "binary"

#define DEFAULT_MEMORY_SIZE 32
#define DEFAULT_OUTPUT_FILE "b.out"
#define DEFAULT_INPUT_FORMAT READER_BINARY

#define EHANDLED 224

typedef uint32_t addr_t;
typedef int32_t word_t;

struct segment {
  addr_t load_address;
  addr_t exec_address;
  addr_t length;
};

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

struct page {
  word_t *data;
  addr_t size;
};

struct mapped_page {
  struct page *phys;
  addr_t base;
  addr_t size;
};

struct vm {
  struct mapped_page page0;
};

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

struct object_file {
  const char *path;
  FILE *stream;
};

struct format {
  const char *name;
  int (*stat)(struct object_file *file, struct segment *segment);
  int (*load)(struct object_file *file, const struct segment *segment, struct vm *vm);
  int (*close)(struct object_file *file);
};

static void dump_state(const struct mc *mc) {
  printf("cycles %12lu ac %08x ci %08x pi %08x%s\n",
         mc->cycles, mc->regs.ac, mc->regs.ci, mc->regs.pi,
         mc->stopped ? " STOP" : "");
}

static void dump_vm(const struct vm *vm) {
  const struct mapped_page *mp;
  addr_t addr;

  mp = &vm->page0;

  for (addr = 0; addr < mp->phys->size; addr+= 4) {
    printf("%08x: %08x %08x %08x %08x\n",
           addr + mp->base,
           mp->phys->data[addr],
           mp->phys->data[addr + 1],
           mp->phys->data[addr + 2],
           mp->phys->data[addr + 3]);
  }
}

static void memory_checks(struct vm *vm) {
  struct mapped_page *mapped_page = &vm->page0;
  addr_t a;

  assert(vm != NULL);

  assert(mapped_page->phys != NULL);

  /* Make sure page size is non-zero */
  assert(mapped_page->size != 0);

  /* Make sure phsyical page size is non-zero */
  assert(mapped_page->phys->size != 0);

  /* Make sure page size is a power of two */
  for (a = mapped_page->size; (a & 1) == 0; a >>= 1);
  assert(a == 1);

  /* Make sure virtual size is a multiple of physical size */
  assert(mapped_page->size % mapped_page->phys->size == 0);

  /* Make sure page is aligned to physical size */ 
  assert((mapped_page->base & (mapped_page->phys->size - 1)) == 0);
}

static inline word_t read_word(struct vm *vm, addr_t addr) {
  struct page *page = vm->page0.phys;

  /* Alias all of virtual memory to sole mapped page */
  return page->data[addr & (page->size - 1)];
}

static inline void write_word(struct vm *vm, addr_t addr, word_t value) {
  struct page *page = vm->page0.phys;

  /* Alias all of virtual memory to sole mapped page */
  page->data[addr & (page->size - 1)] = value;
}

static void sim_cycle(struct mc *mc) {
  word_t opcode;
  word_t operand;
  word_t data = 0;    // Appease compiler
  word_t next_pc = 0; // Appease compiler

  if (verbose)
    dump_state(mc);

  /* t1: Fetch */
  mc->regs.pi = read_word(&mc->vm, mc->regs.ci);

  /* t2: Decode */
  opcode = mc->regs.pi & I_JMP.mask;
  if (mc->regs.pi >= 0)
    operand = mc->regs.pi >> 3;
  else
    operand = -((-mc->regs.pi) >> 3);

  /* t3: Execute - data access */
  switch (opcode) {
  case OP_LDN:
  case OP_SUB:
  case OP_JMP:
    data = read_word(&mc->vm, operand);
    break;
  case OP_STO:
    write_word(&mc->vm, operand, mc->regs.ac);
    break;
  }

  /* t4: Execute */
  switch (opcode) {
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
  switch (opcode) {
  case OP_SKN:
    if (mc->regs.ac < 0)
      next_pc = mc->regs.ci + 1;
    else
      next_pc = mc->regs.ci;
    break;
  case OP_JMP:
    next_pc = data;
    break;
  default:
    next_pc = mc->regs.ci;
  }
  mc->regs.ci = next_pc + 1;

  mc->cycles++;
}

static int binary_stat(struct object_file *file, struct segment *segment) {
  struct stat statbuf;
  int rc;

  assert(segment);

  rc = stat(file->path, &statbuf);
  if (rc == -1)
    return rc;
 
  segment->load_address = 0x0;
  segment->exec_address = 0x0;
  segment->length = statbuf.st_size / sizeof(word_t);

  return 0;
}

static int binary_load(struct object_file *file, const struct segment *segment, struct vm *vm) {
  int rc;
  int i;

  assert(file);
  assert(segment);
  assert(vm);

  if (file->stream == NULL) {
    file->stream = fopen(file->path, "rb");
    if (file->stream == NULL)
      return errno;
  }

  for (i = 0; i < segment->length; i++) {
    word_t data;

    rc = fread(&data, sizeof data, 1, file->stream);
    if (rc < 1) {
      if (ferror(file->stream))
        rc = errno;
      break;
    }

    write_word(vm, segment->load_address + i, data);
  }

  return 0;
}

static int binary_close(struct object_file *file) {
  if (file->stream != NULL) {
    fclose(file->stream);
    file->stream = NULL;
  }
  return 0;
}

const static struct format formats[] = {
  { READER_BINARY, binary_stat, binary_load, binary_close },
};
#define formatsz sizeof formats / sizeof *formats

int usage(FILE *to, int rc, const char *prog) {
  int i;

  fprintf(to, "usage: %s [OPTIONS] OBJECT\n"
    "OPTIONS\n"
    "  -h, --help               output usage and exit\n"
    "  -m, --memory WORDS       memory size in words, default: %d\n"
    "  -I, --input-format FMT   use FMT output format, default: %s\n"
    "  -v, --verbose            output verbose information\n"
    "\n"
    "%s: supported input formats:",
    prog, DEFAULT_MEMORY_SIZE, DEFAULT_INPUT_FORMAT,
    prog);

  for (i = 0; i < formatsz; i++)
    fprintf(to, " %s", formats[i].name);

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
  int i;
  int c;
  int rc = 0;
  int option_index;
  struct mc mc = { 0 };
  struct page page0 = { 0 };
  struct segment segment = { 0 };
  const struct format *format = NULL;
  struct object_file exe = { 0 };
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

  for (i = 0; i < formatsz; i++) {
    if (!strcmp(input_format, formats[i].name))
      break;
  }
  if (i == formatsz) {
    fprintf(stderr, "No such format: %s\n", input_format);
    rc = EHANDLED; /* EINVAL */
    goto finish;
  } else {
    format = formats + i;
  }

  if (optind == argc) {
    fprintf(stderr, "No source specified\n");
    rc = EHANDLED; /* ENOENT */
  }

  if (argc - optind != 1)
    return usage(stderr, 1, argv[0]);
  exe.path = argv[optind++];

  rc = format->stat(&exe, &segment);
  if (rc != 0)
    return rc;

  for(page0.size = memory_size;
      page0.size < segment.length;
      page0.size <<= 1);
  page0.data = calloc(page0.size, sizeof *page0.data);
  mc.vm.page0.base = 0;
  mc.vm.page0.size = page0.size;
  mc.vm.page0.phys = &page0;

  memory_checks(&mc.vm);

  fprintf(stderr, "Mapped fully aliased page of %d words of RAM\n",
          page0.size);

  rc = format->load(&exe, &segment, &mc.vm);
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

  if (format != NULL)
    format->close(&exe);

  return rc == 0 ? 0 : 1;
}

