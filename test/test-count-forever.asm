-- # SPDX-License-Identifier: MIT
-- # (c) Copyright 2023 Andrew Bower
--
-- Test Infinite counting

01:
  ldn ctr
loop:
  sub m1
  jmp j_loop

j_loop:
  eja loop

ctr:
  num 0

m1:
  num -1

