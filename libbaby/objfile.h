/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Object file handling */

#ifndef LIBBABY_OBJFILE_H
#define LIBBABY_OBJFILE_H

struct object_file {
  const char *path;
  FILE *stream;
};

extern int objfile_open_stream(struct object_file *file);
extern void objfile_close(struct object_file *file);

#endif
