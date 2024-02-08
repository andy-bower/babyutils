# Baby assembler, simulator and disassembler

This is an assembler and simulator for the Manchester Baby.

The purpose of this suite is to provide a binutils-style offering to make it quick and easy to write assembly language programs in a modern manner.

Third party applications already offer nice user interfaces for simulation of the Manchester Baby so an important requirement of this toolchain is to be able to interoperate with such applications by supporting suitable object file formats, such as the `.snp` snapshot format.

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.
- Logisim image output format.

## Roadmap

- [ ] Multiple sections
- [ ] ELF file support
- [ ] Symbol export
- [x] Disassembler ('bdump')
- [ ] Object file conversion tool ('bcopy')
- [ ] Assembler macros
- [ ] Assembler expressions
- [ ] Saving and resuming from saved machine state in simulator
- [ ] Simulator trace

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

### Disassmebler Options
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

```
./bas -a test/test-jmp.asm
./bsim b.out
./bdump b.out
```

## Author

(c) Copyright 2023-2024 Andrew Bower

SPDX-License-Identifier: MIT
