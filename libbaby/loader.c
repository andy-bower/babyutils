/* SPDX-License-Identifier: MIT */
/* (c) Copyright 2023 Andrew Bower */

/* Simulator for Manchester Baby. */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <regex.h>

#include "arch.h"
#include "segment.h"
#include "memory.h"
#include "objfile.h"
#include "binfmt.h"
#include "loader.h"

enum regex {
  REG_SNP_IGNORE,
  REG_SNP_STMT,
  REG_PLAIN_BITS,
  REG_MAX
};

#define SNP_COMMENT "[[:space:]]*(;.*)?$"

static const char *regexes[] = {
  "^" SNP_COMMENT,
  "^([[:digit:]]+): ([01]{32})" SNP_COMMENT,
  "^([01]{32})" SNP_COMMENT,
};
static regex_t regex[REG_MAX];

int loaders_init(void) {
  int rc;
  int i;
  char errbuf[256];

  for (i = 0; i < REG_MAX; i++) {
    rc = regcomp(regex + i, regexes[i], REG_EXTENDED);
    if (rc != 0) {
      regerror(rc, regex + i, errbuf, sizeof errbuf);
      fprintf(stderr, "loaders_init: %s\n", errbuf);
      return EINVAL;
    }
  }
  return 0;
}

void loaders_finit(void) {
  int i;

  for (i = 0; i < REG_MAX; i++)
    regfree(regex + i);
}

static int binary_stat(const struct loader *loader, struct object_file *file, struct segment *segment) {
  struct stat statbuf;
  int rc;

  assert(segment);

  rc = stat(file->path, &statbuf);
  if (rc == -1)
    return rc;
 
  segment->load_address = 0x0;
  segment->exec_address = 0x0;
  segment->length = statbuf.st_size / sizeof(word_t);

  return 0;
}

static int binary_load(const struct loader *loader, struct object_file *file, const struct segment *segment, struct vm *vm) {
  int rc;
  int i;

  assert(file);
  assert(segment);
  assert(vm);

  if ((rc = objfile_open_stream(file)) != 0)
    return rc;

  for (i = 0; i < segment->length; i++) {
    word_t data;

    rc = fread(&data, sizeof data, 1, file->stream);
    if (rc < 1) {
      if (ferror(file->stream))
        rc = errno;
      break;
    }

    write_word(vm, segment->load_address + i, data);
  }

  return 0;
}

static int common_close(const struct loader *loader, struct object_file *file) {
  objfile_close(file);
  return 0;
}

static int bits_read(const struct loader *loader, struct object_file *file, struct segment *segment, struct vm *vm) {
  size_t linesz = 0;
  char *line = NULL;
  ssize_t linelen;
  int lineno = 0;
  int rc = 0;
  regmatch_t matches[4];
  int n_matches = sizeof matches / sizeof *matches;
  addr_t max_addr = 0;
  const bool strict = true;
  const bool ssem = loader->flags & BITS_SSEM;
  const bool snp = loader->flags & BITS_ADDR;

  assert(segment);

  /* This function is trusted by the caller to treat segment as 'const' if vm is non-null. */

  if (vm != NULL && segment->length == 0) {
    fprintf(stderr, "loader: must stat object file before loading\n");
    return EINVAL;
  }

  if ((rc = objfile_open_stream(file)) != 0)
    return rc;

  rewind(file->stream);

  for (lineno = 1; !feof(file->stream) && !ferror(file->stream); lineno++) {
    linelen = getline(&line,  &linesz, file->stream);
    if (linelen == -1) {
      rc = errno;
      break;
    }

    if (regexec(regex + (snp ? REG_SNP_STMT : REG_PLAIN_BITS), line, n_matches, matches, 0) == 0) {
      addr_t a;
      word_t v = 0;
      uword_t bit;
      const char *vc;

      if (matches[1].rm_so == -1 ||
          (snp && matches[2].rm_so == -1)) {
        rc = EINVAL;
        break;
      }

      line[matches[1].rm_eo] = '\0';
      if (snp) {
        line[matches[2].rm_eo] = '\0';
        a = strtoul(line + matches[1].rm_so, NULL, 10);
        if (strict && a != max_addr) {
          fprintf(stderr, "non-sequential address %d != %d\n", a, max_addr);
          rc = EINVAL;
          break;
        }
        vc = line + matches[2].rm_so;
      } else {
        a = max_addr;
        vc = line + matches[1].rm_so;
      }

      if (vm != NULL) {
        for (bit = ssem ? 1 : 0x80000000UL; bit != 0; bit = ssem ? bit << 1 : bit >> 1)
          if (*vc++ == '1')
            v |= bit;

        write_word(vm, segment->load_address + a, v);
      }

      max_addr = a + 1;

    } else if (regexec(regex + REG_SNP_IGNORE, line, n_matches, matches, 0) == REG_NOMATCH) {
      rc = EINVAL;
      break;
    }
  }

  free(line);

  if (rc == 0 && vm == NULL) {
   segment->load_address = 0x0;
   segment->exec_address = 0x0;
   segment->length = max_addr;
  }

  if (rc == EINVAL) {
      fprintf(stderr, "loader: %s: %s:%d: format error\n",
             loader->name, file->path, lineno);
  }

  return rc;
}

static int bits_stat(const struct loader *loader, struct object_file *file, struct segment *segment) {
  return bits_read(loader, file, segment, NULL);
}

static int bits_load(const struct loader *loader, struct object_file *file, const struct segment *segment, struct vm *vm) {
  return bits_read(loader, file, (struct segment *) segment, vm);
}

const struct loader loaders[] = {
  { READER_BINARY,                  binary_stat, binary_load, common_close, 0                     },
  { READER_BITS,                    bits_stat,   bits_load,   common_close, 0                     },
  { READER_BITS BITS_SUFFIX_SSEM,   bits_stat,   bits_load,   common_close, BITS_SSEM             },
  { READER_BITS BITS_SUFFIX_SNP,    bits_stat,   bits_load,   common_close, BITS_SSEM | BITS_ADDR },
  { NULL,                           NULL,        NULL,        NULL        , 0                     }
};

