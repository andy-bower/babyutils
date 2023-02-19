/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

#ifndef LIBBABY_LOADER_H
#define LIBBABY_LOADER_H

#include "segment.h"
#include "memory.h"

#define READER_BINARY "binary"

struct loader {
  const char *name;
  int (*stat)(struct object_file *file, struct segment *segment);
  int (*load)(struct object_file *file, const struct segment *segment, struct vm *vm);
  int (*close)(struct object_file *file);
};

extern const struct loader loaders[];

#endif
