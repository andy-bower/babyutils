/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Object file handling */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "objfile.h"

int objfile_open_stream(struct object_file *file) {
  if (file->stream == NULL) {
    file->stream = fopen(file->path, "rb");
  }
  return file->stream == NULL ? errno : 0;
}

void objfile_close(struct object_file *file) {
  if (file->stream != NULL)
    fclose(file->stream);
  file->stream = NULL;
}
