-- Long division
--   From http://madrona.ca/e/SSEM/programs/div.html

00: NUM 19         -- jump address; encodes SKN, AKA NOP at start
    LDN dividend   -- Accumulator := -A
    STO dividend   -- Store as -A
    LDN dividend   -- Accumulator := -(-A) i.e., +A
    SUB divisor_fp -- Subtract B*2^n ; Accumulator = A - B*2^n
    SKN            -- Skip if (A-B*2^n) is Negative
06: JMP 0          --   otherwise go to line 20 ( A-B*2^n >= 0 )
    LDN dividend   -- Accumulator := -(-A)
    STO dividend   -- Store as +A
    LDN quotient   -- Accumulator := -Quotient
    SUB quotient   -- Accumulator := -Quotient - Quotient (up-shift)
    STO quotient   -- Store -2*Quotient as Quotient (up-shifted)
12: LDN dividend   -- Accumulator := -A
    SUB dividend   -- Accumulator := -A-A (up-shift A)
    STO dividend   -- Store -2*A (up-shifted A)
    LDN quotient   -- Accumulator := -Quotient
    STO quotient   -- Store as +Quotient (restore shifted Quotient)
    SKN            -- Skip if MSB of Quotient is 1 (at end)
18: JMP 26         --   otherwise go to line 3 (repeat)
19: HLT            -- Stop ; Quotient in line 28
20: STO dividend   -- From line 6 - Store A-B*2^n as A
    LDN d          -- Routine to set bit d of Quotient
    SUB quotient   --   and up-shift
    SUB quotient   --   Quotient
    STO quotient   -- Store -(2*Quotient)-1 as Quotient
    JMP 27         -- Go to line 12
26: EJA 3          -- jump address
27: EJA 12         -- jump address

-- Variables
quotient:
    NUM 0          -- Quotient (Answer appears here)
d:
    NUM 0x20000000 -- 2^d where d=31-n, see line 30 for n
divisor_fp:
    NUM 20         -- B (Divisor*2^n) (example: 5*2^2=20)
dividend:
    NUM 36         -- A (initial Dividend) (example: 36/5=7)
