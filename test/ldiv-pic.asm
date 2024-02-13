-- Long division
--   From http://madrona.ca/e/SSEM/programs/div.html

target:
    EJA j_target   -- jump address; encodes SKN, AKA NOP at start
start:
    LDN dividend   -- Accumulator := -A
    STO dividend   -- Store as -A
j_iter_divid:
    LDN dividend   -- Accumulator := -(-A) i.e., +A
    SUB divisor_fp -- Subtract B*2^n ; Accumulator = A - B*2^n
    SKN            -- Skip if (A-B*2^n) is Negative
    JMP target     --   otherwise go to line 20 ( A-B*2^n >= 0 )
    LDN dividend   -- Accumulator := -(-A)
    STO dividend   -- Store as +A
    LDN quotient   -- Accumulator := -Quotient
    SUB quotient   -- Accumulator := -Quotient - Quotient (up-shift)
    STO quotient   -- Store -2*Quotient as Quotient (up-shifted)
j_iter_sub:
    LDN dividend   -- Accumulator := -A
    SUB dividend   -- Accumulator := -A-A (up-shift A)
    STO dividend   -- Store -2*A (up-shifted A)
    LDN quotient   -- Accumulator := -Quotient
    STO quotient   -- Store as +Quotient (restore shifted Quotient)
    SKN            -- Skip if MSB of Quotient is 1 (at end)
    JMP iter_divid -- otherwise go to line 3 (repeat)
    HLT            -- Stop ; Quotient in line 28
j_target:
    STO dividend   -- From line 6 - Store A-B*2^n as A
    LDN d          -- Routine to set bit d of Quotient
    SUB quotient   --   and up-shift
    SUB quotient   --   Quotient
    STO quotient   -- Store -(2*Quotient)-1 as Quotient
    JMP iter_sub   -- Go to line 12
iter_divid:
    EJA j_iter_divid -- jump address
iter_sub:
    EJA j_iter_sub   -- jump address

-- Variables
quotient:
    NUM 0          -- Quotient (Answer appears here)
d:
    NUM 0x20000000 -- 2^d where d=31-n, see line 30 for n
divisor_fp:
    NUM 20         -- B (Divisor*2^n) (example: 5*2^2=20)
dividend:
    NUM 36         -- A (initial Dividend) (example: 36/5=7)
