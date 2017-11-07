/*
 *
 * CHIPIT
 *
 * A CHIP-8 emulator.
 *
 */

#include <unistd.h>
#include <stdint.h>
#include <fmt/format.h>
#include <iostream>
#include <string>

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;

struct OpcodeNibbles {
    u16 d : 4;
    u16 c : 4;
    u16 b : 4;       // second 4 bits
    u16 a : 4;       // first 4 bits
};

struct OpcodeBytes {
    u16 b : 8;
    u16 a : 8;
};

struct LowerThree {
    u16 b : 12;
    u16 a : 4;
};

union opcodeBits {
    OpcodeNibbles n;
    OpcodeBytes b;
    LowerThree t;
    u16 opcode;
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
#define V0 v[0x0]
#define V1 v[0x1]
#define V2 v[0x2]
#define V3 v[0x3]
#define V4 v[0x4]
#define V5 v[0x5]
#define V6 v[0x6]
#define V7 v[0x7]
#define V8 v[0x8]
#define V9 v[0x9]
#define VA v[0xa]
#define VB v[0xb]
#define VC v[0xc]
#define VD v[0xd]
#define VE v[0xe]
#define VF v[0xf]

// An opcode is 2 bytes / 16 bits
u16 opcode;

// The stack is 48 bytes
u16 stack[24];
u8 stackptr = -1;

// I - 16 bit register for memory address
u16 I;

// PC - program counter
u16 pc;

// Delay timer is intended for timing the events of games. Can be set and read.
u8 delaytimer = 0;
// Sound effects. A beeping sound is made when value is non-zero.
u8 soundtimer = 0;

// 16 input keys
u8 key[16];

// The display is 64x32 pixels. Color is monochrome, therefore we will try using bool.
bool pixels[64*32];

void decodeOpcode(u16 op)
{
    opcodeBits bits;
    bits.opcode = op;

    //fmt::print("\nDecoding opcode -> hex: {0:x}  bin: {0:0>16b} / ", op);
    //fmt::print("{0:0>4b} ",  bits.n.a);
    //fmt::print("{0:0>4b} ",  bits.n.b);
    //fmt::print("{0:0>4b} ",  bits.n.c);
    //fmt::print("{0:0>4b}\n", bits.n.d);

    if(bits.b.a == 0 && bits.b.b == 0) {
        fmt::print("{0:0>2X}{1:0>2X}\n", bits.b.a, bits.b.b);
        return;
    }

    switch (bits.n.a) {                      // check the first nibble (highest 4 bits)
        case 0:
            if(bits.n.b == 0) {
                if(bits.b.b == 0xE0) {       // Clear the screen
                    fmt::print("00E0: Clear the screen");
                }
                if(bits.b.b == 0xEE) {       // Return from subroutine
                    fmt::print("00EE: Return from subroutine");
                }
            } else {
                fmt::print("0{0:0>3X}: Call RCA 1802 program at address {0:0>3X}", bits.t.b);
            }
            break;
        case 1:
            fmt::print("1{0:X}: Jump to address {0:#x}", bits.t.b);
            break;
        case 2:
            fmt::print("2{0:0>3X}: Call subroutine at address {0:0>3x}", bits.t.b);
            break;
        case 3:
            fmt::print("3{0:X}{1:0>2X}: Skip next instruction if V{0:X} == {1:X}", bits.n.b, bits.b.b);
            break;
        case 4:
            fmt::print("4{0:X}{1:0>2X}: Skip next instruction if V{0:X} != {1:X}", bits.n.b, bits.b.b);
            break;
        case 5:
            fmt::print("5{0:X}{1:X}0: Skip next instruction if V{0:X} == V{1:X}", bits.n.b, bits.n.c);
            break;
        case 6:
            fmt::print("6{0:X}{1:0>2X}: V{0:X} = {1:X}", bits.n.b, bits.b.b);
            break;
        case 7:
            fmt::print("7{0:X}{1:0>2X}: V{0:X} += {1:X}", bits.n.b, bits.b.b);
            break;
        case 8:
            switch(bits.n.d) {
                case 0x0:
                    fmt::print("8{0:X}{1:X}0: V{0:X} = V{1:X}", bits.n.b, bits.n.c);
                    break; 
                case 0x1:
                    fmt::print("8{0:X}{1:X}1: V{0:X} = V{0:X} OR V{1:X} (bitwise OR)", bits.n.b, bits.n.c);
                    break; 
                case 0x2:
                    fmt::print("8{0:X}{1:X}2: V{0:X} = V{0:X} AND V{1:X} (bitwise AND)", bits.n.b, bits.n.c);
                    break; 
                case 0x3:
                    fmt::print("8{0:X}{1:X}3: V{0:X} = V{0:X} XOR V{1:X} (bitwise XOR)", bits.n.b, bits.n.c);
                    break; 
                case 0x4:
                    fmt::print("8{0:X}{1:X}4: V{0:X} += V{1:X} - VF set to 1 when there's a carry.", bits.n.b, bits.n.c);
                    break; 
                case 0x5:
                    fmt::print("8{0:X}{1:X}5: V{0:X} -= V{1:X} - VF set to 0 when there's a borrow, 1 if not.", bits.n.b, bits.n.c);
                    break; 
                case 0x6:
                    fmt::print("8{0:X}{1:X}6: V{0:X} = (V{1:X} >> 1). VF is set to the value of the LSB of V{1:X} before the shift.", bits.n.b, bits.n.c);
                    break; 
                case 0x7:
                    fmt::print("8{0:X}{1:X}7: V{0:X} = V{1:X} - V{0:X}. VF is set to 0 when there's a borrow.", bits.n.b, bits.n.c);
                    break; 
                case 0xE:
                    fmt::print("8{0:X}{1:X}E: V{0:X} = (V{1:X} << 1). VF is set to the value of the MSB of V{1:X} before the shift.", bits.n.b, bits.n.c);
                    break; 
                default:
                    break;
            }
            break;
        case 9:
            fmt::print("9{0:X}{1:X}0: Skip next instruction if V{0:X} != V{1:X}", bits.n.b, bits.n.c);
            break;
        case 0xA:
            fmt::print("A{0:0>3X}: Set I to the address {0:0>3X}", bits.t.b);
            break;
        case 0xB:
            fmt::print("B{0:0>3X}: PC = V0 + {0:0>3X} (jump to address {0:0>3X} + V0)", bits.t.b);
            break;
        case 0xC:
            fmt::print("C{0:X}{1:0>2X}: V{0:X} = rand() & {1:X} (Set V{0:X} to result of a bitwise AND operation on a random number and {1:X}.)", bits.n.b, bits.b.b);
            break;
        case 0xD:
            fmt::print("D{0:X}{1:X}{2:X}: Draw sprite at V{0:X},V{1:X} with height {2:d} pixels.", bits.n.b, bits.n.c, bits.n.d);
            break;
        case 0xE:
            if(bits.b.b == 0x9E) {
                fmt::print("E{0:X}9E: Skip next instruction if key stored in V{0:X} is pressed.", bits.n.b);
            }
            if(bits.b.b == 0xA1) {
                fmt::print("E{0:X}A1: Skip next instruction if key stored in V{0:X} is not pressed.", bits.n.b);
            }
            break;
        case 0xF:
            switch (bits.b.b) {
                case 0x07:
                    fmt::print("F{0:X}07: Set V{0:X} to the value of the delay timer.", bits.n.b);
                    break;
                case 0x0A:
                    fmt::print("F{0:X}0A: Wait for keypress and store it in V{0:X}. Blocking operation - all instruction halted until next key event.", bits.n.b);
                    break;
                case 0x15:
                    fmt::print("F{0:X}15: Set delay timer to V{0:X}", bits.n.b);
                    break;
                case 0x18:
                    fmt::print("F{0:X}18: Set sound timer to V{0:X}", bits.n.b);
                    break;
                case 0x1E:
                    fmt::print("F{0:X}1E: I += V{0:X}", bits.n.b);
                    break;
                case 0x29:
                    fmt::print("F{0:X}29: Set I to the location of the sprite for the character in V{0:X}", bits.n.b);
                    break;
                case 0x33:
                    fmt::print("F{0:X}33: BCD(V{0:X}) - binary coded decimal representation - see wikipedia.....", bits.n.b);
                    break;
                case 0x55:
                    fmt::print("F{0:X}55: Store V0-V{0:X} in memory starting at address in I. I += 1 for each value written.", bits.n.b);
                    break;
                case 0x66:
                    fmt::print("F{0:X}66: Load V0-V{0:X} with values from memory starting at address in I. I += 1 for each value written.", bits.n.b);
                    break;


                default:
                    break;
            }
            break;
        default:
            break;
    }
    fmt::print("\n");
}

void testOpcodes()
{
    decodeOpcode(0x0420);
    decodeOpcode(0x00E0);
    decodeOpcode(0x00EE);
    decodeOpcode(0x1577);
    decodeOpcode(0x29A3);
    decodeOpcode(0x3a6B);
    decodeOpcode(0x43A2);
    decodeOpcode(0x5170);
    decodeOpcode(0x65CD);
    decodeOpcode(0x74DC);
    decodeOpcode(0x8120);
    decodeOpcode(0x8121);
    decodeOpcode(0x8122);
    decodeOpcode(0x8123);
    decodeOpcode(0x8124);
    decodeOpcode(0x8125);
    decodeOpcode(0x8126);
    decodeOpcode(0x8127);
    decodeOpcode(0x812E);
    decodeOpcode(0x9270);
    decodeOpcode(0xA468);
    decodeOpcode(0xB357);
    decodeOpcode(0xC599);
    decodeOpcode(0xD388);
    decodeOpcode(0xE49E);
    decodeOpcode(0xE4A1);
    decodeOpcode(0xFD07);
    decodeOpcode(0xFC0A);
    decodeOpcode(0xF815);
    decodeOpcode(0xF818);
    decodeOpcode(0xF81E);
    decodeOpcode(0xF829);
    decodeOpcode(0xF833);
    decodeOpcode(0xF855);
    decodeOpcode(0xF866);

}

void dumpProgram(int start, int length)
{
    u16 opcode = 0;
    for(int i = 0; i < length; i+=2) {
        opcode = (ram[start + i] << 8) | ram[start + i + 1];
        fmt::print("{0:0>4x} - ", start + i);
        decodeOpcode(opcode);
    }
}

int executeOpcode()
{
    opcodeBits bits;
    bits.opcode = (ram[pc] << 8) | ram[pc+1];

    //fmt::print("\nDecoding opcode -> hex: {0:x}  bin: {0:0>16b} / ", op);
    //fmt::print("{0:0>4b} ",  bits.n.a);
    //fmt::print("{0:0>4b} ",  bits.n.b);
    //fmt::print("{0:0>4b} ",  bits.n.c);
    //fmt::print("{0:0>4b}\n", bits.n.d);

    if(bits.b.a == 0 && bits.b.b == 0) {
        fmt::print("{0:0>2X}{1:0>2X}\n", bits.b.a, bits.b.b);
        return 2;
    }

    switch (bits.n.a) {                      // check the first nibble (highest 4 bits)
        case 0:
            if(bits.n.b == 0) {
                if(bits.b.b == 0xE0) {       // Clear the screen
                    fmt::print("00E0: Clear the screen");
                    memset(pixels, 0, 64*32);
                }
                if(bits.b.b == 0xEE) {       // Return from subroutine
                    fmt::print("00EE: Return from subroutine");
                    pc = stack[stackptr];
                    stackptr--;
                    //stack[stackptr] = 0;  // correct? necessary?
                }
            } else {
                fmt::print("0{0:0>3X}: Call RCA 1802 program at address {0:0>3X}", bits.t.b);
            }
            break;
        case 1:
            fmt::print("1{0:X}: Jump to address {0:#x}", bits.t.b);
            pc = bits.t.b;
            fmt::print("\n");
            return 0;
            break;
        case 2:
            fmt::print("2{0:0>3X}: Call subroutine at address {0:0>3x}", bits.t.b);
            // Push current PC to the stack
            stackptr++;
            stack[stackptr] = pc;
            // Jump to subroutine
            pc = bits.t.b;
            fmt::print("\n");
            return 0;
            break;
        case 3:
            fmt::print("3{0:X}{1:0>2X}: Skip next instruction if V{0:X} == {1:X}", bits.n.b, bits.b.b);
            if (v[bits.n.b] == bits.b.b)
                pc += 2;
            break;
        case 4:
            fmt::print("4{0:X}{1:0>2X}: Skip next instruction if V{0:X} != {1:X}", bits.n.b, bits.b.b);
            if (v[bits.n.b] != bits.b.b)
                pc += 2;
            break;
        case 5:
            fmt::print("5{0:X}{1:X}0: Skip next instruction if V{0:X} == V{1:X}", bits.n.b, bits.n.c);
            if (v[bits.n.b] == v[bits.b.b])
                pc += 2;
            break;
        case 6:
            fmt::print("6{0:X}{1:0>2X}: V{0:X} = {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] = bits.b.b;
            break;
        case 7:
            fmt::print("7{0:X}{1:0>2X}: V{0:X} += {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] += bits.b.b;
            break;
        case 8:
            switch(bits.n.d) {
                case 0x0:
                    fmt::print("8{0:X}{1:X}0: V{0:X} = V{1:X}", bits.n.b, bits.n.c);
                    v[bits.n.b] = v[bits.n.c];
                    break; 
                case 0x1:
                    fmt::print("8{0:X}{1:X}1: V{0:X} = V{0:X} OR V{1:X} (bitwise OR)", bits.n.b, bits.n.c);
                    v[bits.n.b] |= v[bits.n.c];
                    break; 
                case 0x2:
                    fmt::print("8{0:X}{1:X}2: V{0:X} = V{0:X} AND V{1:X} (bitwise AND)", bits.n.b, bits.n.c);
                    v[bits.n.b] &= v[bits.n.c];
                    break; 
                case 0x3:
                    fmt::print("8{0:X}{1:X}3: V{0:X} = V{0:X} XOR V{1:X} (bitwise XOR)", bits.n.b, bits.n.c);
                    v[bits.n.b] ^= v[bits.n.c];
                    break; 
                case 0x4:
                    fmt::print("8{0:X}{1:X}4: V{0:X} += V{1:X} - VF set to 1 when there's a carry.", bits.n.b, bits.n.c);
                    if ((v[bits.n.b] + v[bits.n.c]) > 0xFF)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] += v[bits.n.c];
                    break; 
                case 0x5:
                    fmt::print("8{0:X}{1:X}5: V{0:X} -= V{1:X} - VF set to 0 when there's a borrow, 1 if not.", bits.n.b, bits.n.c);
                    if (v[bits.n.b] > v[bits.n.c])
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] -= v[bits.n.c];
                    break; 
                case 0x6:
                    fmt::print("8{0:X}{1:X}6: V{0:X} >>= 1. VF is set to the value of the LSB of V{0:X} before the shift.", bits.n.b, bits.n.c);
                    if (v[bits.n.b] & 1)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] >>= 1;
                    break; 
                case 0x7:
                    fmt::print("8{0:X}{1:X}7: V{0:X} = V{1:X} - V{0:X}. VF is set to 0 when there's a borrow.", bits.n.b, bits.n.c);
                    break; 
                case 0xE:
                    fmt::print("8{0:X}{1:X}6: V{0:X} <<= 1. VF is set to the value of the MSB of V{0:X} before the shift.", bits.n.b, bits.n.c);
                    VF = (v[bits.n.b] >> 7);
                    v[bits.n.b] <<= 1;
                    break; 
                default:
                    break;
            }
            break;
        case 9:
            fmt::print("9{0:X}{1:X}0: Skip next instruction if V{0:X} != V{1:X}", bits.n.b, bits.n.c);
            if (v[bits.n.b] != v[bits.b.b])
                pc += 2;
            break;
        case 0xA:
            fmt::print("A{0:0>3X}: Set I to the address {0:0>3X}", bits.t.b);
            I = bits.t.b;
            break;
        case 0xB:
            fmt::print("B{0:0>3X}: PC = V0 + {0:0>3X} (jump to address {0:0>3X} + V0)", bits.t.b);
            pc = V0 + bits.t.b;
            fmt::print("\n");
            return 0;
            break;
        case 0xC:
            fmt::print("C{0:X}{1:0>2X}: V{0:X} = rand() & {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] = rand() % bits.b.b;
            break;
        case 0xD:
            fmt::print("D{0:X}{1:X}{2:X}: Draw sprite at V{0:X},V{1:X} with height {2:d} pixels.", bits.n.b, bits.n.c, bits.n.d);
            break;
        case 0xE:
            if(bits.b.b == 0x9E) {
                fmt::print("E{0:X}9E: Skip next instruction if key stored in V{0:X} is pressed.", bits.n.b);
                if(key[v[bits.n.b]])
                    pc += 2;
            }
            if(bits.b.b == 0xA1) {
                fmt::print("E{0:X}A1: Skip next instruction if key stored in V{0:X} is not pressed.", bits.n.b);
                if(!key[v[bits.n.b]])
                    pc += 2;
            }
            break;
        case 0xF:
            switch (bits.b.b) {
                case 0x07:
                    fmt::print("F{0:X}07: Set V{0:X} to the value of the delay timer.", bits.n.b);
                    v[bits.n.b] = delaytimer;
                    break;
                case 0x0A:
                    fmt::print("F{0:X}0A: Wait for keypress and store it in V{0:X}. Blocking operation - all instruction halted until next key event.", bits.n.b);
                    break;
                case 0x15:
                    fmt::print("F{0:X}15: Set delay timer to V{0:X}", bits.n.b);
                    delaytimer = v[bits.n.b];
                    break;
                case 0x18:
                    fmt::print("F{0:X}18: Set sound timer to V{0:X}", bits.n.b);
                    soundtimer = v[bits.n.b];
                    break;
                case 0x1E:
                    fmt::print("F{0:X}1E: I += V{0:X}", bits.n.b);
                    I += v[bits.n.b];
                    break;
                case 0x29:
                    fmt::print("F{0:X}29: Set I to the location of the sprite for the character in V{0:X}", bits.n.b);
                    break;
                case 0x33:
                    fmt::print("F{0:X}33: BCD(V{0:X}) - binary coded decimal representation - see wikipedia.....", bits.n.b);
                    break;
                case 0x55:
                    fmt::print("F{0:X}55: Store V0-V{0:X} in memory starting at address in I. I += 1 for each value written.", bits.n.b);
                    for(int r = 0; r <= bits.n.b; r++) {
                        ram[I] = v[r];
                        I++;
                    }
                    break;
                case 0x66:
                    fmt::print("F{0:X}66: Load V0-V{0:X} with values from memory starting at address in I. I += 1 for each value written.", bits.n.b);
                    for(int r = 0; r <= bits.n.b; r++) {
                        v[r] = ram[I];
                        I++;
                    }
                    break;

                default:
                    break;
            }
            break;
        default:
            break;
    }
    fmt::print("\n");
    return 2;

}

void runEmulator(int programSize)
{
    pc = 0x200;
    while(1) {
        fmt::print("{0:0>4x} - ", pc);
        pc += executeOpcode();
        usleep(1000 * 100);   // microseconds!

        // these should decrement at 60Hz (60 times per second I guess?)
        if(delaytimer > 0)
            delaytimer--;
        if(soundtimer > 0)
            soundtimer--;

        if (pc > 0x200 + programSize)
            return;
    }
}

int main(int argc, char *argv[])
{
    FILE *f;
    int filesize = 0;
    std::string arg = argv[1];

    srand (time(NULL));
    
    fmt::print("\n\n     CHIPIT \n\n");

    fmt::print("[loading file...]\n\n");
    if (argc > 2) {   // simple argument parsing... TODO: improve it
        f = fopen(argv[2], "rb");
        fseek(f, 0L, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0L, SEEK_SET);
        fread(&ram[0x200], filesize, 1, f);
    } else {
        fmt::print("syntax: chipate FILENAME\n");
        return 0;
    }

    
    if(arg == "-d") {
        fmt::print("[decoding opcodes]\n\n");
        dumpProgram(0x200, filesize);
    } else if (arg == "-r") {
        runEmulator(filesize);
    }

    
    fmt::print("\n[finished]\n");
    return 0;
}




// vim: fdm=syntax
