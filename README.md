# Baby Assembler

This is an assembler for a particular teaching variant of the Manchester Baby and outputs binary objects in the Logisim RAM image format.

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.

## Options

```
OPTIONS
  -a, --listing            output listing
  -h, --help               output usage and exit
  -m, --map                output map
  -o, --output FILE|-      write object to FILE, default: b.out
  -O, --output-format FMT  use FMT output format, default: logisim
  -v, --verbose            output verbose information

./bas: supported output formats: logisim binary
```

## Example

```
make
./bas -a -o ldiv.out test/ldiv.asm
```
