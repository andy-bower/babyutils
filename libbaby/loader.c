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
#include "segment.h"
#include "memory.h"
#include "objfile.h"
#include "loader.h"

#define READER_BINARY "binary"

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

const struct loader loaders[] = {
  { READER_BINARY, binary_stat, binary_load, binary_close },
  { NULL,          NULL,        NULL,        NULL         }
};

