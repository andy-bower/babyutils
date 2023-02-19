/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Section handling. */

#ifndef LIBBABY_SECTION_H
#define LIBBABY_SECTION_H

#include "arch.h"

struct abstract;

struct sectiondata {
  word_t value;
  struct abstract *debug;
};

struct section {
  addr_t capacity;
  addr_t length;
  addr_t org;
  addr_t cursor;
  struct sectiondata *data;
};

extern int put_word(struct section *section, word_t word, struct abstract *abs);

#endif
