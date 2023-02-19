/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Writers for Manchester Baby. */

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

#include "arch.h"
#include "section.h"
#include "writer.h"

#define BITS_SSEM 1
#define BITS_ADDR 2

static bool verbose = false;

static const word_t fill_value = 0x0;

static int logisim_writer(FILE *stream, const struct section *section, int flags) {
  addr_t word;
  int rc;

  fprintf(stream, "v2.0 raw\n");
  for (word = 0; word < section->org + section->length; word++) {
    rc = fprintf(stream, "%08x\n", word < section->org ? fill_value : section->data[word - section->org].value);
    if (rc < 0)
      return errno;
  }

  if (verbose) {
    fprintf(stderr, "  words in output = 0x%x\n", word);
  }

  return 0;
}

static int bits_writer(FILE *stream, const struct section *section, int flags) {
  addr_t word;
  word_t tst;
  int rc;
  bool ssem = flags & BITS_SSEM;

  for (word = 0; word < section->org + section->length; word++) {
    word_t val = word < section->org ? fill_value : section->data[word - section->org].value;
    if (flags & BITS_ADDR)
      fprintf(stream, "%04d: ", word);
    for (tst = ssem ? 1 : 0x80000000UL; tst != 0; tst = ssem ? tst << 1 : tst >> 1)
      if ((rc = fputc(val & tst ? '1' : '0', stream)) == EOF)
        return errno;
    rc = fputc('\n', stream);
    if (rc == EOF)
      return errno;
  }

  if (verbose) {
    fprintf(stderr, "  words in output = 0x%x\n", word);
  }

  return 0;
}

static int binary_writer(FILE *stream, const struct section *section, int flags) {
  addr_t word;
  int rc;

  for (word = 0; word < section->org + section->length; word++) {
    rc = fwrite(&section->data[word - section->org].value, sizeof(word_t), 1, stream);
    if (rc < 1)
      return errno;
  }

  if (verbose) {
    fprintf(stderr, "  words in output = 0x%x\n", word);
  }

  return 0;
}

const struct format formats[] = {
  { WRITER_LOGISIM,      logisim_writer, 0 },
  { WRITER_BINARY,       binary_writer,  0 },
  { WRITER_BITS,         bits_writer,    0 },
  { WRITER_BITS ".ssem", bits_writer,    BITS_SSEM },
  { WRITER_BITS ".snp",  bits_writer,    BITS_SSEM | BITS_ADDR },
  { NULL,                NULL,           0 }
};

int write_section(const char *path, const struct section *section, const struct format *format) {
  FILE *file;
  int rc;

  if (strcmp(path, "-")) {
    file = fopen(path, "w");
    if (file == NULL) {
      perror("fopen");
      return 1;
    }
  } else {
    file = stdout;
  }

  if (verbose)
    fprintf(stderr, "Writing section\n  org = 0x%x\n  length = 0x%x\n",
            section->org, section->length);

  rc = format->writer(file, section, format->flags);

  if (verbose) {
    fprintf(stderr, "Written %s\n", path);
  }

  if (file != stdout)
    fclose(file);

  return rc;
}

