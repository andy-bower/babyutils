/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Section handling. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "section.h"

int put_word(struct section *section, word_t word, struct asm_abstract *abs) {
  if (section->cursor < section->org) {
    fprintf(stderr, "cannot write to 0x%x before section start 0x%x\n",
            section->cursor, section->org);
    return ESPIPE;
  }
  if (section->cursor >= section->org + section->capacity) {
    size_t old_capacity = section->capacity;
    section->capacity = section->cursor - section->org + 0x400;
    section->data = realloc(section->data, sizeof(*section->data) * section->capacity);
    if (section->data == NULL) {
      fprintf(stderr, "error expanding section to %ud words\n", section->capacity);
      return errno;
    }
   memset(section->data + old_capacity, '\0', (section->capacity - old_capacity) * sizeof(*section->data));
  }
  if (section->data[section->cursor - section->org].debug != NULL) {
    fprintf(stderr, "section already includes data at 0x%08x\n", section->cursor);
    return EEXIST;
  }
  section->data[section->cursor   - section->org].debug = abs;
  section->data[section->cursor++ - section->org].value = word;
  if (section->cursor - section->org > section->length)
    section->length = section->cursor - section->org;
  return 0;
}


