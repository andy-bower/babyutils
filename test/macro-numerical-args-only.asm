-- # SPDX-License-Identifier: MIT
-- # (c) Copyright 2024 Andrew Bower
--
-- Test macros

mld \
  MACRO x
  ldn zero
  sub x
  ENDM

madd \
  MACRO x
  sto tmp
  ldn tmp
  sub x
  sto tmp
  ldn tmp
  ENDM

28:
zero:
  num 0
tmp:
  num 0

01:
start:
  mld 16
  mld 17 - 1
  madd 18
  sto 12
  hlt

16:
a:
  num 3
b:
  num 5
c:
  num 0    -- should become 8 at end of test
