/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

#ifndef LIBBABY_WRITER_H
#define LIBBABY_WRITER_H

#include "binfmt.h"

#define WRITER_LOGISIM "logisim"
#define WRITER_BINARY BINFMT_BINARY
#define WRITER_BITS BINFMT_BITS

struct format {
  const char *name;
  int (*writer)(FILE *stream, const struct section *section, int flags);
  const int flags;
};

extern const struct format formats[];

extern int write_section(const char *path, const struct section *section, const struct format *format);

#endif
