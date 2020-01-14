/*
 *
 * CHIPIT
 *
 * A CHIP-8 emulator.
 *
 * TODO: Keypresses, drawing, sprites, BCD instruction.
 */

#include <unistd.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>

//#include <fmt/format.h>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

// Typedefs
typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef int64_t  i64;

// SFML
sf::RenderWindow window;
sf::RenderTexture tex;
sf::Clock clck;
sf::Font sfmlFont;
sf::Text t;

const int pixelWidth = 16;
const int pixelHeight = 16;
const int c8Width = 64 * pixelWidth;
const int c8Height = 32 * pixelHeight;
const int screenWidth = c8Width + 500;
const int screenHeight = c8Height + 500;
// CHIP-8 output offset
const int c8X = (screenWidth / 2) - (c8Width / 2) + 20, c8Y = 20;
// Registers output offset
const int regX = 32;
const int regY = c8Height + 32;

// Some flags
bool verbose = false;
bool dirtyDisplay = true;
std::map<uint16_t, std::string> disasm;


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
u8 ram[4096] = {0};

// Registers. The CHIP-8 has 16 8-bit registers, named V0 - VF.
// VF doubles as a flag for some instructions. VF is also carry flag.
// While in subtraction, it is the "not borrow" flag. In the draw instruction, VF is set upon pixel collision.
u8 v[16];
u8 &V0 = v[0x0];
u8 &V1 = v[0x1];
u8 &V2 = v[0x2];
u8 &V3 = v[0x3];
u8 &V4 = v[0x4];
u8 &V5 = v[0x5];
u8 &V6 = v[0x6];
u8 &V7 = v[0x7];
u8 &V8 = v[0x8];
u8 &V9 = v[0x9];
u8 &VA = v[0xa];
u8 &VB = v[0xb];
u8 &VC = v[0xc];
u8 &VD = v[0xd];
u8 &VE = v[0xe];
u8 &VF = v[0xf];

// An opcode is 2 bytes / 16 bits
//u16 opcode;

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

typedef struct {
    int x, y;
    bool pixel;
} pixelData;

// The display is 64x32 pixels. Color is monochrome.
//u8 pixels[64*32];
std::array<pixelData, 64*32> pixels;
// vector is maaaybe slower than array, but if averaged probably very similar.
// Note: compile with optimization! in debug mode vector is noticably slower than array.
// std::vector<pixelData> pixels;

// The font sprites
u8 font[16][5] = {
    { 0xF0, 0x90, 0x90, 0x90, 0xF0 },  // 0
    { 0x20, 0x60, 0x20, 0x20, 0x70 },  // 1
    { 0xF0, 0x10, 0xF0, 0x80, 0xF0 },  // etc..
    { 0xF0, 0x10, 0xF0, 0x10, 0xF0 },
    { 0x90, 0x90, 0xF0, 0x10, 0x10 },
    { 0xF0, 0x80, 0xF0, 0x10, 0xF0 },
    { 0xF0, 0x80, 0xF0, 0x90, 0xF0 },
    { 0xF0, 0x10, 0x20, 0x40, 0x40 },
    { 0xF0, 0x90, 0xF0, 0x90, 0xF0 },
    { 0xF0, 0x90, 0xF0, 0x10, 0xF0 },
    { 0xF0, 0x90, 0xF0, 0x90, 0x90 },
    { 0xE0, 0x90, 0xE0, 0x90, 0xE0 },
    { 0xF0, 0x80, 0x80, 0x80, 0xF0 },
    { 0xE0, 0x90, 0x90, 0x90, 0xE0 },
    { 0xF0, 0x80, 0xF0, 0x80, 0xF0 },
    { 0xF0, 0x80, 0xF0, 0x80, 0x80 }
};

// Forward declarations
void drawSprite(u8 vx, u8 vy, u8 h);
int executeOpcode();
std::map<uint16_t, std::string> disassemble(uint16_t start, uint16_t end);

//void dumpProgram(int start, int length)
//{
//    u16 opcode = 0;
//    for(int i = 0; i < length; i+=2) {
//        opcode = (ram[start + i] << 8) | ram[start + i + 1];
//        fmt::print("{0:0>4x} - ", start + i);
//        decodeOpcode(opcode);
//    }
//}

// simple way to measure execution time - could be better...
std::string measureTask;
sf::Time start;
sf::Time end;
sf::Time elapsed;
void measureStart(std::string task)
{
    measureTask = task;
    start = clck.getElapsedTime();
}

void measureEnd()
{
    end = clck.getElapsedTime();
    sf::Time elapsed = end - start;
    std::cout << measureTask << " took " << elapsed.asMicroseconds() << " microseconds" << std::endl;
}

void initPixelData()
{
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 64; x++) {
            pixels[x + (y*64)].x = x * pixelWidth;
            pixels[x + (y*64)].y = y * pixelHeight;
            pixels[x + (y*64)].pixel = false;
        }
    }
}



void initEmulator()
{
    // TODO: set everything to zero / other initial value
    pc = 0x200;
    //opcode = 0;
    I = 0;
    stackptr = 0;
}

void runCPU()
{
    //if (verbose) fmt::print("{0:0>4x} - ", pc);

    pc += executeOpcode();

    // these should decrement at 60Hz (60 times per second) TODO: implement correct timing!
    if(delaytimer > 0)
        delaytimer--;
    if(soundtimer > 0)
        soundtimer--;
}

void loadFont()
{
    for (int i = 0; i < 16; i++) {
        std::memcpy(&ram[i*5], font[i], 5*sizeof(u8));
    }
}

// fmt::print("D{0:X}{1:X}{2:X}: Draw sprite at V{0:X},V{1:X} with height {2:d} pixels.", bits.n.b, bits.n.c, bits.n.d);
// Dxyn - DRW Vx, Vy, nibble
// Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
// The interpreter reads n bytes from memory, starting at the address stored in I. These bytes are then displayed as sprites on
// screen at coordinates (Vx, Vy). Sprites are XORed onto the existing screen. If this causes any pixels to be erased,
// VF is set to 1, otherwise it is set to 0. If the sprite is positioned so part of it is outside the coordinates of the display,
// it wraps around to the opposite side of the screen. 
//
// parts of drawSprite code borrowed from https://github.com/JamesGriffin/CHIP-8-Emulator/blob/master/src/chip8.cpp
void drawSprite(u8 vx, u8 vy, u8 h)
{
    u8 &x = v[vx];
    u8 &y = v[vy];

    // TODO: Deal with coordinates out of bounds!
    VF = 0;
    for (int yl = 0; yl < h; yl++) {
        u8 &pixel = ram[I + yl];
        for (int xl = 0; xl < 8; xl++) {
            if ((pixel & (0x80 >> xl))) {
                int pos = (x+xl) + ((y+yl)*64);
                if (pixels[pos].pixel) {
                    VF = 1;
                }
                pixels[pos].pixel ^= 1;
            }
        }
    }
}

/*
 * Render a pixel from Chip-8 memory to the actual screen
 */
sf::RectangleShape rect;
void renderPixel(int x, int y)
{
    rect.setPosition(x * pixelWidth, y * pixelHeight);
    tex.draw(rect);
}

void drawString(int x, int y, std::string s)
{
    t.setString(s);
    t.setPosition(x, y);
    tex.draw(t);
}

void drawDisassembly(int x,  int y, int lines)
{
    auto it = disasm.find(pc);
    auto next = disassemble(pc, pc);
    int liney = (lines >> 1) * 10 + y;

    // Draw "live" (it's not really live yet) disassembly of next instruction
    t.setFillColor(sf::Color::Cyan);
    drawString(x, liney, next[pc]);
    t.setFillColor(sf::Color::White);

    // Draw the rest
    if (it != disasm.end()) {
        while (liney < (lines * 10) + y) {
            liney += 16;
            if (++it != disasm.end()) {
                drawString(x, liney, (*it).second);
            }
        }
    }

    it = disasm.find(pc);
    liney = (lines >> 1) * 10 + y;
    if (it != disasm.end()) {
        while (liney > y) {
            liney -= 16;
            if (--it != disasm.end()) {
                drawString(x, liney, (*it).second);
            }
        }
    }
}

// this is still slow.
// migrate to SDL? not use rect?
void updateDisplay()
{
    const int fontsize = 20;
    char out[100];

    tex.clear(sf::Color::Black);

    // Draw debug / disasm info
    t.setFont(sfmlFont);
    t.setCharacterSize(fontsize);
    t.setFillColor(sf::Color::White);

    // - Draw registers
    for (int reg = 0; reg < 16; reg++) {
        sprintf(out, "V%01X: %02X", reg, v[reg]);
        drawString(regX, regY + (reg * (fontsize + 4)), std::string(out));
    }

    // PC
    sprintf(out, "PC: %04X", pc);
    drawString(regX + (6 * fontsize) + 12, regY, std::string(out));

    // I
    sprintf(out, " I: %04X", I);
    drawString(regX + (6 * fontsize) + 12, regY + (fontsize + 2), std::string(out));

    // SP
    sprintf(out, "SP: %04X", stackptr);
    drawString(regX + (6 * fontsize) + 12, regY + 2 * (fontsize + 2), std::string(out));

    // Disassembly
    drawDisassembly(regX + (6 * fontsize) + 150, regY, 16);

    // TODO: timers
    // TODO: keys
    // TODO: RAM
    
    // Draw Chip-8 output
    for (auto it : pixels) {
        if (it.pixel) {
            rect.setFillColor(sf::Color::White);
            rect.setPosition(c8X + it.x, c8Y + it.y);
            tex.draw(rect);
        } else {
            rect.setFillColor(sf::Color::Black);
            rect.setPosition(c8X + it.x, c8Y + it.y);
            tex.draw(rect);
        }
    }


    dirtyDisplay = false;
}

void initSFML()
{
    //sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

    window.create(sf::VideoMode(screenWidth, screenHeight), "chipit");
    sf::Vector2i windowPosition;
    windowPosition.x = 1700; //(desktop.width / 4) - (screenWidth / 2);
    windowPosition.y = 50; //(desktop.height / 2) - (screenHeight / 2);
    window.setPosition(windowPosition);
    window.setVerticalSyncEnabled(true);
    window.clear(sf::Color::Black);

    tex.create(screenWidth, screenHeight);

    rect.setSize(sf::Vector2f(pixelWidth, pixelHeight));
    rect.setFillColor(sf::Color::White);

    if(!sfmlFont.loadFromFile("Courier Prime Code.ttf")) {
        printf("ERROR: couldn't load font file!\n");
        exit(1);
    }
}

// The slowness was caused by calling window.display all the time in the main loop!!!
// Changed it to only be called when we update the display - now the emulator is really fast!
//
// TODO: flag to set if we are to do debug output (disasm / cpu monitor / etc)
void mainLoop()
{
    bool done = false;
    bool run = false, runOnce = false;;

    while (window.isOpen() && !done) {

        if (runOnce) {
            runCPU();
            runOnce = false;
            dirtyDisplay = true;
        }

        if (run) {
            runCPU();
            dirtyDisplay = true;
        }

        if (dirtyDisplay) {
            updateDisplay();
            tex.display();
            sf::Sprite spr(tex.getTexture());
            spr.move(0, 0);
            window.draw(spr);
            window.display();
        }


        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                done = true;
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape)
                done = true;
            else if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F1) {
            }

            if (event.type == sf::Event::KeyReleased) {
                switch (event.key.code) {
                    case sf::Keyboard::M:
                        run = false;
                        break;
                    case sf::Keyboard::Num1:
                        key[0x1] = 0;
                        break;
                    case sf::Keyboard::Num2:
                        key[0x2] = 0;
                        break;
                    case sf::Keyboard::Num3:
                        key[0x3] = 0;
                        break;
                    case sf::Keyboard::Num4:
                        key[0xC] = 0;
                        break;
                    case sf::Keyboard::Q:
                        key[0x4] = 0;
                        break;
                    case sf::Keyboard::W:
                        key[0x5] = 0;
                        break;
                    case sf::Keyboard::E:
                        key[0x6] = 0;
                        break;
                    case sf::Keyboard::R:
                        key[0xD] = 0;
                        break;
                    case sf::Keyboard::A:
                        key[0x7] = 0;
                        break;
                    case sf::Keyboard::S:
                        key[0x8] = 0;
                        break;
                    case sf::Keyboard::D:
                        key[0x9] = 0;
                        break;
                    case sf::Keyboard::F:
                        key[0xE] = 0;
                        break;
                    case sf::Keyboard::Z:
                        key[0xA] = 0;
                        break;
                    case sf::Keyboard::X:
                        key[0x0] = 0;
                        break;
                    case sf::Keyboard::C:
                        key[0xB] = 0;
                        break;
                    case sf::Keyboard::V:
                        key[0xF] = 0;
                        break;

                    default:
                        break;
                }
            }

            if (event.type == sf::Event::KeyPressed) {
                switch (event.key.code) {
                    case sf::Keyboard::Space:
                        run = !run;
                        break;
                    case sf::Keyboard::M:
                        run = true;
                        break;
                    case sf::Keyboard::Enter:
                        runOnce = true;
                        break;
                    case sf::Keyboard::Num1:
                        key[0x1] = 1;
                        break;
                    case sf::Keyboard::Num2:
                        key[0x2] = 1;
                        break;
                    case sf::Keyboard::Num3:
                        key[0x3] = 1;
                        break;
                    case sf::Keyboard::Num4:
                        key[0xC] = 1;
                        break;
                    case sf::Keyboard::Q:
                        key[0x4] = 1;
                        break;
                    case sf::Keyboard::W:
                        key[0x5] = 1;
                        break;
                    case sf::Keyboard::E:
                        key[0x6] = 1;
                        break;
                    case sf::Keyboard::R:
                        key[0xD] = 1;
                        break;
                    case sf::Keyboard::A:
                        key[0x7] = 1;
                        break;
                    case sf::Keyboard::S:
                        key[0x8] = 1;
                        break;
                    case sf::Keyboard::D:
                        key[0x9] = 1;
                        break;
                    case sf::Keyboard::F:
                        key[0xE] = 1;
                        break;
                    case sf::Keyboard::Z:
                        key[0xA] = 1;
                        break;
                    case sf::Keyboard::X:
                        key[0x0] = 1;
                        break;
                    case sf::Keyboard::C:
                        key[0xB] = 1;
                        break;
                    case sf::Keyboard::V:
                        key[0xF] = 1;
                        break;

                    default:
                        break;
                }
            }
        }

        usleep(1200);
    }
    //window.clear();

    window.close();
}


int executeOpcode()
{
    //uint16_t opcode = (ram[pc] << 8) | ram[pc+1];
    opcodeBits bits;
    bits.opcode = (ram[pc] << 8) | ram[pc+1];
    //const uint16_t &opcode = bits.opcode;

    if(bits.b.a == 0 && bits.b.b == 0) {
        //if (verbose) fmt::print("{0:0>2X}{1:0>2X}\n", BA(opcode), BB(opcode));
        return 2;
    }

    switch (bits.n.a) {                      // check the first nibble (highest 4 bits)
        case 0:
            if(bits.n.b == 0) {
                if(bits.b.b == 0xE0) {       // Clear the screen
                    //if (verbose) fmt::print("00E0: Clear the screen");
                    for (auto it = pixels.begin(); it != pixels.end(); it++)
                        it->pixel = false;
                    //window.clear(sf::Color::Black);
                    dirtyDisplay = true;
                }
                if(bits.b.b == 0xEE) {       // Return from subroutine
                    //if (verbose) fmt::print("00EE: Return from subroutine");
                    stackptr--;
                    pc = stack[stackptr];
                }
            } else {
                //fmt::print("0{0:0>3X}: Call RCA 1802 program at address {0:0>3X} NOT IMPLEMENTED\n", L3(opcode));
            }
            break;
        case 1:
            //if (verbose) fmt::print("1{0:X}: Jump to address {0:#x}", L3(opcode));
            pc = bits.t.b;
            //if (verbose) fmt::print("\n");
            return 0;
            break;
        case 2:
            //if (verbose) fmt::print("2{0:0>3X}: Call subroutine at address {0:0>3x}", L3(opcode));
            // Push current PC to the stack
            stack[stackptr] = pc;
            stackptr++;
            // Jump to subroutine
            pc = bits.t.b;
            //if (verbose) fmt::print("\n");
            return 0;
            break;
        case 3:
            //if (verbose) fmt::print("3{0:X}{1:0>2X}: Skip next instruction if V{0:X} == {1:X}", NB(opcode), BB(opcode));
            if (v[bits.n.b] == bits.b.b)
                pc += 2;
            break;
        case 4:
            //if (verbose) fmt::print("4{0:X}{1:0>2X}: Skip next instruction if V{0:X} != {1:X}", NB(opcode), BB(opcode));
            if (v[bits.n.b] != bits.b.b)
                pc += 2;
            break;
        case 5:
            //if (verbose) fmt::print("5{0:X}{1:X}0: Skip next instruction if V{0:X} == V{1:X}", NB(opcode), NC(opcode));
            if (v[bits.n.b] == v[bits.n.c])
                pc += 2;
            break;
        case 6:
            //if (verbose) fmt::print("6{0:X}{1:0>2X}: V{0:X} = {1:X}", NB(opcode), BB(opcode));
            v[bits.n.b] = bits.b.b;
            break;
        case 7:
            //if (verbose) fmt::print("7{0:X}{1:0>2X}: V{0:X} += {1:X}", NB(opcode), BB(opcode));
            v[bits.n.b] += bits.b.b;
            break;
        case 8:
            switch(bits.n.d) {
                case 0x0:
                    //if (verbose) fmt::print("8{0:X}{1:X}0: V{0:X} = V{1:X}", NB(opcode), NC(opcode));
                    v[bits.n.b] = v[bits.n.c];
                    break; 
                case 0x1:
                    //if (verbose) fmt::print("8{0:X}{1:X}1: V{0:X} = V{0:X} OR V{1:X} (bitwise OR)", NB(opcode), NC(opcode));
                    v[bits.n.b] |= v[bits.n.c];
                    break; 
                case 0x2:
                    //if (verbose) fmt::print("8{0:X}{1:X}2: V{0:X} = V{0:X} AND V{1:X} (bitwise AND)", NB(opcode), NC(opcode));
                    v[bits.n.b] &= v[bits.n.c];
                    break; 
                case 0x3:
                    //if (verbose) fmt::print("8{0:X}{1:X}3: V{0:X} = V{0:X} XOR V{1:X} (bitwise XOR)", NB(opcode), NC(opcode));
                    v[bits.n.b] ^= v[bits.n.c];
                    break; 
                case 0x4:
                    //if (verbose) fmt::print("8{0:X}{1:X}4: V{0:X} += V{1:X} - VF set to 1 when there's a carry.", NB(opcode), NC(opcode));
                    if ((v[bits.n.b] + v[bits.n.c]) > 0xFF)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] += v[bits.n.c];
                    break; 
                case 0x5:
                    //if (verbose) fmt::print("8{0:X}{1:X}5: V{0:X} -= V{1:X} - VF set to 0 when there's a borrow, 1 if not.", NB(opcode), NC(opcode));
                    if (v[bits.n.b] > v[bits.n.c])
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] -= v[bits.n.c];
                    break; 
                case 0x6:
                    //if (verbose) fmt::print("8{0:X}{1:X}6: V{0:X} >>= 1. VF is set to the value of the LSB of V{0:X} before the shift.", NB(opcode), NC(opcode));
                    if (v[bits.n.b] & 1)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] >>= 1;
                    break; 
                case 0x7:
                    //if (verbose) fmt::print("8{0:X}{1:X}7: V{0:X} = V{1:X} - V{0:X}. VF is set to 0 when there's a borrow.", NB(opcode), NC(opcode));
                    if (v[bits.n.b] > v[bits.n.c])
                        VF = 0;
                    else
                        VF = 1;
                    v[bits.n.b] = v[bits.n.c] - v[bits.n.b];
                    break; 
                case 0xE:
                    //if (verbose) fmt::print("8{0:X}{1:X}6: V{0:X} <<= 1. VF is set to the value of the MSB of V{0:X} before the shift.", NB(opcode), NC(opcode));
                    VF = (v[bits.n.b] >> 7);
                    v[bits.n.b] <<= 1;
                    break; 
                default:
                    break;
            }
            break;
        case 9:
            //if (verbose) fmt::print("9{0:X}{1:X}0: Skip next instruction if V{0:X} != V{1:X}", NB(opcode), NC(opcode));
            if (v[bits.n.b] != v[bits.b.b])
                pc += 2;
            break;
        case 0xA:
            //if (verbose) fmt::print("A{0:0>3X}: Set I to the address {0:0>3X}", L3(opcode));
            I = bits.t.b;
            break;
        case 0xB:
            //if (verbose) fmt::print("B{0:0>3X}: PC = V0 + {0:0>3X} (jump to address {0:0>3X} + V0)\n", L3(opcode));
            pc = V0 + bits.t.b;
            return 0;
            break;
        case 0xC:
            //if (verbose) fmt::print("C{0:X}{1:0>2X}: V{0:X} = rand() & {1:X}", NB(opcode), L3(opcode));
            v[bits.n.b] = rand() % (bits.b.b + 1);
            break;
        case 0xD: // TODO
            //if (verbose) fmt::print("D{0:X}{1:X}{2:X}: Draw sprite at V{0:X},V{1:X} with height {2:d} pixels.", NB(opcode), NC(opcode), ND(opcode));
            drawSprite(bits.n.b, bits.n.c, bits.n.d);
            dirtyDisplay = true;
            break;
        case 0xE:
            if(bits.b.b == 0x9E) {
                //if (verbose) fmt::print("E{0:X}9E: Skip next instruction if key stored in V{0:X} ({1:X}) is pressed.", NB(opcode), v[bits.n.b]);
                if(key[v[bits.n.b]])
                    pc += 2;
            }
            if(bits.b.b == 0xA1) {
                //if (verbose) fmt::print("E{0:X}A1: Skip next instruction if key stored in V{0:X} is not pressed.", NB(opcode));
                if(!key[v[bits.n.b]])
                    pc += 2;
            }
            break;
        case 0xF:
            switch (bits.b.b) {
                case 0x07:
                    //if (verbose) fmt::print("F{0:X}07: Set V{0:X} to the value of the delay timer.", NB(opcode));
                    v[bits.n.b] = delaytimer;
                    break;
                case 0x0A: {
                               //if (verbose) fmt::print("F{0:X}0A: Wait for keypress and store it in V{0:X}. Blocking operation - all instruction halted until next key event.\n", NB(opcode));
                               bool keyPressed = false;
                               for (int i = 0; i < 16; i++) {
                                   if (key[i]) {
                                       V0 = i;
                                       keyPressed = true;
                                       //if (verbose) fmt::print("KEY PRESSED: {0:X} V0 is now: {1:X}\n", i, V0);
                                       return 2;
                                   }
                               }
                               if(!keyPressed)
                                   return 0;
                           }
                    break;
                case 0x15:
                    //if (verbose) fmt::print("F{0:X}15: Set delay timer to V{0:X}", NB(opcode));
                    delaytimer = v[bits.n.b];
                    break;
                case 0x18:
                    //if (verbose) fmt::print("F{0:X}18: Set sound timer to V{0:X}", NB(opcode));
                    soundtimer = v[bits.n.b];
                    break;
                case 0x1E:
                    //if (verbose) fmt::print("F{0:X}1E: I += V{0:X}", NB(opcode));
                    I += v[bits.n.b];
                    break;
                case 0x29:
                    //if (verbose) fmt::print("F{0:X}29: Set I to the location of the sprite for the character in V{0:X}", NB(opcode));
                    I = v[bits.n.b] * 5;
                    break;
                case 0x33:
                    //if (verbose) fmt::print("F{0:X}33: BCD(V{0:X}) - store binary coded decimal representation of V{0:X} at address I ({1:X})", NB(opcode), I);
                    ram[I+0] =  v[bits.n.b] / 100;
                    ram[I+1] = (v[bits.n.b] /  10) % 10;
                    ram[I+2] = (v[bits.n.b] % 100) % 10;
                    break;
                case 0x55:
                    //if (verbose) fmt::print("F{0:X}55: Store V0-V{0:X} in memory starting at address in I. I += 1 for each value written.", NB(opcode));
                    for(int r = 0; r <= bits.n.b; r++) {
                        ram[I] = v[r];
                        I++;
                    }
                    break;
                case 0x65:
                    //if (verbose) fmt::print("F{0:X}66: Load V0-V{0:X} with values from memory starting at address in I. I += 1 for each value read.", NB(opcode));
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
    //if (verbose) fmt::print("\n");


    return 2;
}

std::map<uint16_t, std::string> disassemble(uint16_t start, uint16_t end)
{
    std::map<uint16_t, std::string> output;
    uint16_t addr = start;
    uint16_t line;
    opcodeBits bits;

    auto hex = [](uint32_t n, uint8_t d)
    {
        std::string s(d, '0');
		for (int i = d - 1; i >= 0; i--, n >>= 4)
			s[i] = "0123456789ABCDEF"[n & 0xF];
		return s;
    };

    while (addr <= end) {
        line = addr;
        bits.opcode = (ram[addr] << 8) | ram[addr+1];
        std::string text = "0x" + hex(addr, 4) + ": " + hex(bits.opcode, 4) + " - ";

        addr += 2;
        int what = bits.n.a;
        switch (what) {
            case 0:
                if (bits.b.b == 0x00E0) {
                    text += "CLS";
                } else if (bits.b.b == 0x00EE) {
                    text += "RTS";
                } else {
                    text += "CALL RCA1802 0x" + hex(bits.t.b, 4);
                }
                break;
            case 1:
                text += "JMP  0x" + hex(bits.t.b, 4); break;
            case 2:
                text += "CALL 0x" + hex(bits.t.b, 4); break;
            case 3:
                text += "SKIP if V" + hex(bits.n.b, 1) + " == " + hex(bits.b.b, 2); break;
            case 4:
                text += "SKIP if V" + hex(bits.n.b, 1) + " != " + hex(bits.b.b, 2); break;
            case 5:
                text += "SKIP if V" + hex(bits.n.b, 1) + " == " + "V" + hex(bits.n.c, 1); break;
            case 6:
                text += "LOAD V" + hex(bits.n.b, 1) + ", " + hex(bits.b.b, 2); break;
            case 7:
                text += "ADD  V" + hex(bits.n.b, 1) + ", " + hex(bits.b.b, 2); break;
            case 8:
                switch (bits.n.d) {
                    case 0:
                        text += "LOAD V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 1:
                        text += "OR   V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 2:
                        text += "AND  V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 3:
                        text += "XOR  V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 4:
                        text += "ADD  V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 5:
                        text += "SUB  V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1); break;
                    case 6:
                        text += "RSH  V" + hex(bits.n.b, 1); break;
                    case 7:
                        text += "SUBX V" + hex(bits.n.c, 1) + ", V" + hex(bits.n.b, 1); break;
                    case 0xE:
                        text += "LSH  V" + hex(bits.n.b, 1); break;
                    default:
                        text += "???"; break;
                }
                break;
            case 9:
                text += "SKIP if V" + hex(bits.n.b, 1) + " != " + "V" + hex(bits.n.c, 1); break;
            case 0xA:
                text += "LOAD I, " + hex(bits.t.b, 3); break;
            case 0xB:
                text += "JMP  " + hex(bits.t.b, 3) + ", V0"; break;
            case 0xC:
                text += "LOAD V" + hex(bits.n.b, 1) + ", RND(" + hex(bits.b.b, 2) + ")"; break;
            case 0xD:
                text += "DRAW V" + hex(bits.n.b, 1) + ", V" + hex(bits.n.c, 1) + ", " + hex(bits.n.d, 1); break;
            case 0xE:
                switch(bits.b.b) {
                    case 0x9E: text += "KEYP V" + hex(bits.n.b, 1); break;
                    case 0xA1: text += "KEYR V" + hex(bits.n.b, 1); break;
                    default: text += "???"; break;
                }
                break;
            case 0xF:
                switch (bits.b.b) {
                    case 0x07: text += "LOAD V" + hex(bits.n.b, 1) + ", dTIM"; break;
                    case 0x0A: text += "LOAD V" + hex(bits.n.b, 1) + ", KEY"; break;
                    case 0x15: text += "LOAD dTIM, V" + hex(bits.n.b, 1); break;
                    case 0x18: text += "LOAD sTIM, V" + hex(bits.n.b, 1); break;
                    case 0x1E: text += "ADD  I, V" + hex(bits.n.b, 1); break;
                    case 0x29: text += "LOAD I, SPR(V" + hex(bits.n.b, 1) + ")"; break;
                    case 0x33: text += "LOAD I, BCD(V" + hex(bits.n.b, 1) + ")"; break;
                    case 0x55: text += "DUMP V0 - V" + hex(bits.n.b, 1); break;
                    case 0x66: text += "LOAD V0 - V" + hex(bits.n.b, 1); break;
                    default:   text += "???"; break;
                }
                break;
            default:
                text += "???";
                break;
        }

        output[line] = text;
    }

    return output;
}

int main(int argc, char *argv[])
{
    FILE *f;
    int filesize = 0;
    std::string arg;
    char *filename;
    bool disasmOnly = false;
    
    if(argc < 2) {
        printf("syntax: chipit [-d | -r] FILENAME\n");
        return 0;
    }
        
    arg = argv[1];

    srand (time(NULL));
    
    printf("\n\n     CHIPIT v1.0\n\n");

    printf("[loading font sprites...]\n");
    loadFont();

    printf("[loading file...]\n");

    if(arg == "-d") {
        filename = argv[2];
        disasmOnly = true;
    } else {
        filename = argv[1];
    }

    f = fopen(filename, "rb");
    fseek(f, 0L, SEEK_END);
    filesize = ftell(f);
    fseek(f, 0L, SEEK_SET);
    fread(&ram[0x200], filesize, 1, f);


    disasm = disassemble(0x200, filesize+0x200);

    if (disasmOnly) {
        printf("[decoding opcodes...]\n\n");
        for (auto it = disasm.begin(); it != disasm.end(); it++) {
            std::cout << it->second << std::endl;
        }
    } else {
        printf("[running emulator...]\n");
        initSFML();
        initPixelData();
        initEmulator();
        mainLoop();
    }
    
    
    printf("\n[finished]\n\n");
    return 0;
}
