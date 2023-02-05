# Baby Assembler

This is an assembler for a particular teaching variant of the Manchester Baby and outputs binary objects in the Logisim image format.

## Unique features

- Labels are accepted in place of addresses.
- New assembler directive `EJA` standing for Effective Jump Address, which stores a data word in the object file that points to the instruction before the given location, which is either an address or symbol.

## Example

```
make
./bas -a -o ldiv.out test/ldiv.asm
```
