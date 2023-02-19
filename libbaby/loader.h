/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

#ifndef LIBBABY_LOADER_H
#define LIBBABY_LOADER_H

#include "segment.h"
#include "binfmt.h"
#include "memory.h"

#define READER_BINARY BINFMT_BINARY
#define READER_BITS BINFMT_BITS

struct loader;

struct loader {
  const char *name;
  int (*stat)(const struct loader *loader, struct object_file *file, struct segment *segment);
  int (*load)(const struct loader *loader, struct object_file *file, const struct segment *segment, struct vm *vm);
  int (*close)(const struct loader *loader, struct object_file *file);
  int flags;
};

extern const struct loader loaders[];

extern int loaders_init(void);
extern void loaders_finit(void);

#endif
