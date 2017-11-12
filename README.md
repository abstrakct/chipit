# Chipit

A very slow and very simple CHIP-8 emulator.
Written to teach myself more about emulators.
Written from scratch based on documentation found online.
Some minor parts inspired by code from other CHIP-8 emulators.

AFAIK there are no bugs at this time, the programs I've tested seem to run as expected.

Sound is not implemented.
No support for Mega-/Super-CHIP-8 or other variants.

Ideas for improvement:
* Function pointers instead of giant switch statements
* Better keymapping support
* Maybe use SDL instead of SFML - I suspect the pixel drawing is the main slowdown at this point.
* Add a debugger (i.e. a way to see the values of RAM and all registers live and step through the code).
* Add a disassembler (kind of already implemented, in a way...)