/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

#ifndef LIBBABY_WRITER_H
#define LIBBABY_WRITER_H

#define WRITER_LOGISIM "logisim"
#define WRITER_BINARY "binary"
#define WRITER_BITS "bits"

struct format {
  const char *name;
  int (*writer)(FILE *stream, const struct section *section, int flags);
  const int flags;
};

extern const struct format formats[];

extern int write_section(const char *path, const struct section *section, const struct format *format);

#endif
