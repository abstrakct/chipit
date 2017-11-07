/*
 *
 * CHIPATE - CHIP is A Terrible Emulator
 *
 * A CHIP-8 emulator.
 *
 */

#include <stdint.h>
#include <iostream>

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;

struct OpcodeNibbles {
    unsigned a : 4;
    unsigned b : 4;
    unsigned c : 4;
    unsigned d : 4;
};

union opcodeBits {
    u16 opcode;
    OpcodeNibbles n;
};

// The CHIP-8 has 4096 bytes of ram:
// 0x000 - 0x1FF - originally the CHIP-8 interpreter. In modern times commonly used for storing fonts.
// 0x200 - 0xE9F - program code
// 0xEA0 - 0xEFF - call stack, internal use and other variables
// 0xF00 - 0xFFF - display refresh
u8 ram[4096];

// Registers. The CHIP-8 has 16 8-bit registers, named V0 - VF.
// VF doubles as a flag for some instructions. VF is also carry flag.
// While in subtraction, it is the "not borrow" flag. In the draw instruction, VF is set upon pixel collision.
u8 v[16];

// An opcode is 2 bytes / 16 bits
u16 opcode;

// The stack is 48 bytes
u16 stack[24];

// Delay timer is intended for timing the events of games. Can be set and read.
u8 delaytimer = 60;
// Sound effects. A beeping sound is made when value is non-zero.
u8 soundtimer = 60;

// 16 input keys
u8 input[16];

// The display is 64x32 pixels. Color is monochrome, therefore try using bool.
bool pixels[64*32];

void parseOpcode(u16 op)
{
    opcodeBits bits;
    bits.opcode = op;

    std::cout << "nibbleA = " << bits.n.a << std::endl;
    std::cout << "nibbleB = " << bits.n.b << std::endl;
    std::cout << "nibbleC = " << bits.n.c << std::endl;
    std::cout << "nibbleD = " << bits.n.d << std::endl;
}

int main()
{
    parseOpcode(0000);
    parseOpcode(0100);
    parseOpcode(0010);
    parseOpcode(0001);
    return 0;
}
