/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Segment handling. */

#ifndef LIBBABY_SEGMENT_H
#define LIBBABY_SEGMENT_H

#include "arch.h"

struct segment {
  addr_t load_address;
  addr_t exec_address;
  addr_t length;
};

#endif
