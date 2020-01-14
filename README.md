# Chipit

A very simple CHIP-8 emulator.
Written to teach myself more about emulators.
Written from scratch based on documentation found online.
Some minor parts inspired by code from other CHIP-8 emulators.

AFAIK there is only one bug: some CHIP-8 programs causes the emulator to segfault - but I don't know why. It's fairly rare anyway.
So apart from that there are no bugs I am aware of at this time. The programs I've tested seem to run as expected.

Sound is not implemented.
No support for Mega-/Super-CHIP-8 or other variants.

Ideas for improvement:
* Function pointers instead of giant switch statements
* Better keymapping support
* (partially done) Add a debugger (i.e. a way to see the values of RAM and all registers live and step through the code).
* (more or less done) Add a disassembler
