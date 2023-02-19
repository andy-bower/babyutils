# Baby assembler and simulator

This is an assembler and simulator for the Manchester Baby.

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.
- Logisim image output format.

## Assembler Options

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

## Example

See the assembly source files in the `test` directory for examples of accepted syntax.

```
make
./bas -a test/test-jmp.asm
./bsim b.out
```

## Author

(c) Copyright 2023 Andrew Bower

SPDX-License-Identifier: MIT
