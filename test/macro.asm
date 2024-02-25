-- # SPDX-License-Identifier: MIT
-- # (c) Copyright 2024 Andrew Bower
--
-- Test macros

mld \
  MACRO x
  ldn x
  mneg
  ENDM

madd \
  MACRO x
  sto tmp
  ldn tmp
  sub x
  mneg
  ENDM

mneg \
  MACRO
  sto tmp
  ldn tmp
  ENDM

madd2 MACRO x, y
  LDN x
  SUB y
  MNEG
  ENDM

28:
zero:
  num 0
tmp:
  num 0

01:
start:
  mld a
  madd b
  sto c
  madd2 a, b
  sto d
  hlt

a:
  num 3
b:
  num 8 - 3
c:
  num 0    -- should become 8 at end of test
d:
  num 0    ; should become 8 at end of test
