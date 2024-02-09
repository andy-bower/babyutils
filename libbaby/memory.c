/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Memory handling. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "memory.h"

void dump_vm(const struct vm *vm) {
  const struct mapped_page *mp;
  addr_t addr;

  mp = &vm->page0;

  for (addr = 0; addr < mp->phys->size; addr+= 4) {
    printf("%08x: %08x %08x %08x %08x\n",
           addr + mp->base,
           mp->phys->data[addr],
           mp->phys->data[addr + 1],
           mp->phys->data[addr + 2],
           mp->phys->data[addr + 3]);
  }
}

void memory_checks(struct vm *vm) {
  struct mapped_page *mapped_page = &vm->page0;
  addr_t a;

  assert(vm != NULL);

  assert(mapped_page->phys != NULL);

  /* Make sure page size is non-zero */
  assert(mapped_page->size != 0);

  /* Make sure physical page size is non-zero */
  assert(mapped_page->phys->size != 0);

  /* Make sure page size is a power of two */
  for (a = mapped_page->size; (a & 1) == 0; a >>= 1);
  assert(a == 1);

  /* Make sure virtual size is a multiple of physical size */
  assert(mapped_page->size % mapped_page->phys->size == 0);

  /* Make sure page is aligned to physical size */
  assert((mapped_page->base & (mapped_page->phys->size - 1)) == 0);
}

extern inline word_t read_word(struct vm *vm, addr_t addr);
extern inline void write_word(struct vm *vm, addr_t addr, word_t value);

