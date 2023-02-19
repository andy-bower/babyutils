/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Memory handling */

#ifndef LIBBABY_MEMORY_H
#define LIBBABY_MEMORY_H

#include "arch.h"

struct page {
  word_t *data;
  addr_t size;
};

struct mapped_page {
  struct page *phys;
  addr_t base;
  addr_t size;
};

struct vm {
  struct mapped_page page0;
};

inline word_t read_word(struct vm *vm, addr_t addr) {
  struct page *page = vm->page0.phys;

  /* Alias all of virtual memory to sole mapped page */
  return page->data[addr & (page->size - 1)];
}

inline void write_word(struct vm *vm, addr_t addr, word_t value) {
  struct page *page = vm->page0.phys;

  /* Alias all of virtual memory to sole mapped page */
  page->data[addr & (page->size - 1)] = value;
}

extern void memory_checks(struct vm *vm);
extern void dump_vm(const struct vm *vm);

#endif
