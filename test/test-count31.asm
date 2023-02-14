-- # SPDX-License-Identifier: MIT
-- # (c) Copyright 2023 Andrew Bower
--
-- Count 2^31 times

01:
  ldn m1
loop:
  sub m1
  skn
  jmp j_loop
  hlt

j_loop:
  eja loop

m1:
  num -1

