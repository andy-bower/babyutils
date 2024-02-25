# Baby macro assembler, simulator and disassembler

This is a binutils-style command line tool chain for the Manchester Baby.

These tools were written to support chapter 7 of [Computer Architecture](https://nostarch.com/computerarchitecture) and as such can export files to import into Logisim RAM blocks.

The _babyutils_ assembler `bas` offers some of the features expected of a conventional command line tool, such as labels, macros and expression evaluation, allowing development to scale to larger programs when implementations include a larger number of store lines.

Users are encouraged to pair these tools with interesting simulators sporting fancy user interfaces out in the wild. Please raise an issue or submit a pull request if you find a simulator that requires an input format not supported by these tools or an assembly dialect with which `bas` is not yet compatible!

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.
- Logisim image output format.
- Macros are supported.
- Expressions are supported for instruction and macro operands.

## Roadmap

- [x] Disassembler ('bdump')
- [ ] Object file conversion tool ('bcopy')
- [x] Assembler macros
- [x] Assembler expressions
- [ ] Saving and resuming from saved machine state in simulator
- [ ] Simulator trace
- [ ] Multiple source files
- [ ] Multiple sections/segments
- [ ] Automatic data sections
- [ ] ELF file support
- [ ] Symbol export

## Using the utilities

To build and install the tools to `/usr/local`:

```
make
sudo make install
```

The tools may also be used in place in the source tree.

### Assembler Options

```
usage: ./bas [OPTIONS] SOURCE|-...
OPTIONS
  -a, --listing            output listing
  -h, --help               output usage and exit
  -m, --map                output map
  -o, --output FILE|-      write object to FILE, default: b.out
  -O, --output-format FMT  use FMT output format, default: bits.snp
  -v, --verbose            output verbose information

./bas: supported output formats: logisim binary bits bits.ssem bits.snp
```

### Simulator Options
```
usage: ./bsim [OPTIONS] OBJECT
OPTIONS
  -h, --help               output usage and exit
  -m, --memory WORDS       memory size in words, default: 32
  -I, --input-format FMT   use FMT output format, default: bits.snp
  -v, --verbose            output verbose information

./bsim: supported input formats: binary bits bits.ssem bits.snp
```

### Disassembler Options
```
usage: ./bdump [OPTIONS] OBJECT
OPTIONS
  -h, --help               output usage and exit
  -I, --input-format FMT   use FMT output format, default: bits.snp
  -v, --verbose            output verbose information

./bdump: supported input formats: binary bits bits.ssem bits.snp
```

### Example

See the assembly source files in the `test` directory for examples of accepted syntax.

#### Assemble, simulate, disassemble

```
./bas test/test-jmp.asm
./bsim b.out
./bdump b.out
```

#### Use of macros

```assembly
mneg MACRO
  STO tmp
  LDN tmp
  ENDM

madd2 \
  MACRO x, y
  LDN x
  SUB y
  MNEG
  ENDM

01: MADD2 a, b
    STO c
    STP

a: num 1
b: num 1 + 1
c: num 0 ; should become 3
tmp: num 0
```

## Author

(c) Copyright 2023-2024 Andrew Bower

SPDX-License-Identifier: MIT
