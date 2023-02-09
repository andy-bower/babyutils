-- # SPDX-License-Identifier: MIT
-- # (c) Copyright 2023 Andrew Bower
--
-- Test JMP instruction

  ldn dat1
  sto check1
  jmp todest1
  sto check4
  hlt

dest1:
  sto check2
  ldn dat2
  sto check3
  hlt

dat1:
  num -0x11
todest1:
  eja dest1
dat2:
  num -0x22
dat3:
  num -0x33

28:
check1:
  num 1 -- should become 0x11
check2:
  num 2 -- should become 0x11
check3:
  num 3 -- should become 0x22
check4:
  num 4 -- should stay 4
