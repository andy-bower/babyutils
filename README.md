# Baby assembler and simulator

This is an assembler and simulator for a particular teaching variant of the Manchester Baby and outputs binary objects in the Logisim RAM image format.

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.

## Assembler Options

```
usage: ./bas [OPTIONS] [SOURCE|-]...
OPTIONS
  -a, --listing            output listing
  -h, --help               output usage and exit
  -m, --map                output map
  -o, --output FILE|-      write object to FILE, default: b.out
  -O, --output-format FMT  use FMT output format, default: binary
  -v, --verbose            output verbose information

./bas: supported output formats: logisim binary bits bits.ssem bits.snp
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
