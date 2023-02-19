/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Object file handling */

#ifndef LIBBABY_OBJFILE_H
#define LIBBABY_OBJFILE_H

struct object_file {
  const char *path;
  FILE *stream;
};

#endif
