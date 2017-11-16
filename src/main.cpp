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

#include <fmt/format.h>
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

const int pixelWidth = 16;
const int pixelHeight = 16;
const int screenWidth = 64 * pixelWidth;
const int screenHeight = 32 * pixelHeight;

// Some flags
bool verbose = false;
bool dirtyDisplay = true;

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
    opcode = 0;
    I = 0;
    stackptr = 0;
}

void runCPU()
{
    if (verbose) fmt::print("{0:0>4x} - ", pc);

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
    rect.setPosition(x*pixelWidth, y*pixelHeight);
    tex.draw(rect);
}

// this is still slow.
// migrate to SDL? not use rect?
void updateDisplay()
{
    //tex.clear(sf::Color::Black);

    for (auto it : pixels) {
        if (it.pixel) {
            rect.setFillColor(sf::Color::White);
            rect.setPosition(it.x, it.y);
            tex.draw(rect);
        } else {
            rect.setFillColor(sf::Color::Black);
            rect.setPosition(it.x, it.y);
            tex.draw(rect);
        }
    }

    dirtyDisplay = false;
}

void initSFML()
{
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

    window.create(sf::VideoMode(screenWidth, screenHeight), "chipit");
    sf::Vector2i windowPosition;
    windowPosition.x = (desktop.width / 4) - (screenWidth / 2);
    windowPosition.y = (desktop.height / 2) - (screenHeight / 2);
    window.setPosition(windowPosition);
    window.setVerticalSyncEnabled(true);
    window.clear(sf::Color::Black);

    tex.create(screenWidth, screenHeight);

    rect.setSize(sf::Vector2f(pixelWidth, pixelHeight));
    rect.setFillColor(sf::Color::White);
}

// The slowness was caused by calling window.display all the time in the main loop!!!
// Changed it to only be called when we update the display - now the emulator is really fast!
void mainLoop()
{
    bool done = false;

    while (window.isOpen() && !done) {

        runCPU();

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

int main(int argc, char *argv[])
{
    FILE *f;
    int filesize = 0;
    std::string arg;
    
    if(argc < 2) {
        fmt::print("syntax: chipate [-d | -r] FILENAME\n");
        return 0;
    }
        
    arg = argv[1];

    srand (time(NULL));
    
    initSFML();
    initPixelData();

    fmt::print("\n\n     CHIPIT v1.0\n\n");

    fmt::print("[loading font sprites...]\n");
    loadFont();

    fmt::print("[loading file...]\n");

    f = fopen(argv[1], "rb");
    fseek(f, 0L, SEEK_END);
    filesize = ftell(f);
    fseek(f, 0L, SEEK_SET);
    fread(&ram[0x200], filesize, 1, f);

    //} else {
    //    fmt::print("syntax: chipate [-d | -r] FILENAME\n");
    //    return 0;
    //}

    
    /*if(arg == "-d") {
        fmt::print("[decoding opcodes...]\n");
        dumpProgram(0x200, filesize);
    } else if (arg == "-r") {*/
        fmt::print("[running emulator...]\n");
        initEmulator();
        mainLoop();
        //runEmulator(filesize);
    //}

    
    fmt::print("[finished]\n\n");
    return 0;
}


int executeOpcode()
{
    opcodeBits bits;
    bits.opcode = (ram[pc] << 8) | ram[pc+1];

    if(bits.b.a == 0 && bits.b.b == 0) {
        if (verbose) fmt::print("{0:0>2X}{1:0>2X}\n", bits.b.a, bits.b.b);
        return 2;
    }

    switch (bits.n.a) {                      // check the first nibble (highest 4 bits)
        case 0:
            if(bits.n.b == 0) {
                if(bits.b.b == 0xE0) {       // Clear the screen
                    if (verbose) fmt::print("00E0: Clear the screen");
                    for (auto it = pixels.begin(); it != pixels.end(); it++)
                        it->pixel = false;
                    //window.clear(sf::Color::Black);
                    dirtyDisplay = true;
                }
                if(bits.b.b == 0xEE) {       // Return from subroutine
                    if (verbose) fmt::print("00EE: Return from subroutine");
                    stackptr--;
                    pc = stack[stackptr];
                }
            } else {
                fmt::print("0{0:0>3X}: Call RCA 1802 program at address {0:0>3X} NOT IMPLEMENTED\n", bits.t.b);
            }
            break;
        case 1:
            if (verbose) fmt::print("1{0:X}: Jump to address {0:#x}", bits.t.b);
            pc = bits.t.b;
            if (verbose) fmt::print("\n");
            return 0;
            break;
        case 2:
            if (verbose) fmt::print("2{0:0>3X}: Call subroutine at address {0:0>3x}", bits.t.b);
            // Push current PC to the stack
            stack[stackptr] = pc;
            stackptr++;
            // Jump to subroutine
            pc = bits.t.b;
            if (verbose) fmt::print("\n");
            return 0;
            break;
        case 3:
            if (verbose) fmt::print("3{0:X}{1:0>2X}: Skip next instruction if V{0:X} == {1:X}", bits.n.b, bits.b.b);
            if (v[bits.n.b] == bits.b.b)
                pc += 2;
            break;
        case 4:
            if (verbose) fmt::print("4{0:X}{1:0>2X}: Skip next instruction if V{0:X} != {1:X}", bits.n.b, bits.b.b);
            if (v[bits.n.b] != bits.b.b)
                pc += 2;
            break;
        case 5:
            if (verbose) fmt::print("5{0:X}{1:X}0: Skip next instruction if V{0:X} == V{1:X}", bits.n.b, bits.n.c);
            if (v[bits.n.b] == v[bits.n.c])
                pc += 2;
            break;
        case 6:
            if (verbose) fmt::print("6{0:X}{1:0>2X}: V{0:X} = {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] = bits.b.b;
            break;
        case 7:
            if (verbose) fmt::print("7{0:X}{1:0>2X}: V{0:X} += {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] += bits.b.b;
            break;
        case 8:
            switch(bits.n.d) {
                case 0x0:
                    if (verbose) fmt::print("8{0:X}{1:X}0: V{0:X} = V{1:X}", bits.n.b, bits.n.c);
                    v[bits.n.b] = v[bits.n.c];
                    break; 
                case 0x1:
                    if (verbose) fmt::print("8{0:X}{1:X}1: V{0:X} = V{0:X} OR V{1:X} (bitwise OR)", bits.n.b, bits.n.c);
                    v[bits.n.b] |= v[bits.n.c];
                    break; 
                case 0x2:
                    if (verbose) fmt::print("8{0:X}{1:X}2: V{0:X} = V{0:X} AND V{1:X} (bitwise AND)", bits.n.b, bits.n.c);
                    v[bits.n.b] &= v[bits.n.c];
                    break; 
                case 0x3:
                    if (verbose) fmt::print("8{0:X}{1:X}3: V{0:X} = V{0:X} XOR V{1:X} (bitwise XOR)", bits.n.b, bits.n.c);
                    v[bits.n.b] ^= v[bits.n.c];
                    break; 
                case 0x4:
                    if (verbose) fmt::print("8{0:X}{1:X}4: V{0:X} += V{1:X} - VF set to 1 when there's a carry.", bits.n.b, bits.n.c);
                    if ((v[bits.n.b] + v[bits.n.c]) > 0xFF)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] += v[bits.n.c];
                    break; 
                case 0x5:
                    if (verbose) fmt::print("8{0:X}{1:X}5: V{0:X} -= V{1:X} - VF set to 0 when there's a borrow, 1 if not.", bits.n.b, bits.n.c);
                    if (v[bits.n.b] > v[bits.n.c])
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] -= v[bits.n.c];
                    break; 
                case 0x6:
                    if (verbose) fmt::print("8{0:X}{1:X}6: V{0:X} >>= 1. VF is set to the value of the LSB of V{0:X} before the shift.", bits.n.b, bits.n.c);
                    if (v[bits.n.b] & 1)
                        VF = 1;
                    else
                        VF = 0;
                    v[bits.n.b] >>= 1;
                    break; 
                case 0x7:
                    if (verbose) fmt::print("8{0:X}{1:X}7: V{0:X} = V{1:X} - V{0:X}. VF is set to 0 when there's a borrow.", bits.n.b, bits.n.c);
                    if (v[bits.n.b] > v[bits.n.c])
                        VF = 0;
                    else
                        VF = 1;
                    v[bits.n.b] = v[bits.n.c] - v[bits.n.b];
                    break; 
                case 0xE:
                    if (verbose) fmt::print("8{0:X}{1:X}6: V{0:X} <<= 1. VF is set to the value of the MSB of V{0:X} before the shift.", bits.n.b, bits.n.c);
                    VF = (v[bits.n.b] >> 7);
                    v[bits.n.b] <<= 1;
                    break; 
                default:
                    break;
            }
            break;
        case 9:
            if (verbose) fmt::print("9{0:X}{1:X}0: Skip next instruction if V{0:X} != V{1:X}", bits.n.b, bits.n.c);
            if (v[bits.n.b] != v[bits.b.b])
                pc += 2;
            break;
        case 0xA:
            if (verbose) fmt::print("A{0:0>3X}: Set I to the address {0:0>3X}", bits.t.b);
            I = bits.t.b;
            break;
        case 0xB:
            if (verbose) fmt::print("B{0:0>3X}: PC = V0 + {0:0>3X} (jump to address {0:0>3X} + V0)\n", bits.t.b);
            pc = V0 + bits.t.b;
            return 0;
            break;
        case 0xC:
            if (verbose) fmt::print("C{0:X}{1:0>2X}: V{0:X} = rand() & {1:X}", bits.n.b, bits.b.b);
            v[bits.n.b] = rand() % (bits.b.b + 1);
            break;
        case 0xD: // TODO
            if (verbose) fmt::print("D{0:X}{1:X}{2:X}: Draw sprite at V{0:X},V{1:X} with height {2:d} pixels.", bits.n.b, bits.n.c, bits.n.d);
            drawSprite(bits.n.b, bits.n.c, bits.n.d);
            dirtyDisplay = true;
            break;
        case 0xE:
            if(bits.b.b == 0x9E) {
                if (verbose) fmt::print("E{0:X}9E: Skip next instruction if key stored in V{0:X} ({1:X}) is pressed.", bits.n.b, v[bits.n.b]);
                if(key[v[bits.n.b]])
                    pc += 2;
            }
            if(bits.b.b == 0xA1) {
                if (verbose) fmt::print("E{0:X}A1: Skip next instruction if key stored in V{0:X} is not pressed.", bits.n.b);
                if(!key[v[bits.n.b]])
                    pc += 2;
            }
            break;
        case 0xF:
            switch (bits.b.b) {
                case 0x07:
                    if (verbose) fmt::print("F{0:X}07: Set V{0:X} to the value of the delay timer.", bits.n.b);
                    v[bits.n.b] = delaytimer;
                    break;
                case 0x0A: {
                               if (verbose) fmt::print("F{0:X}0A: Wait for keypress and store it in V{0:X}. Blocking operation - all instruction halted until next key event.\n", bits.n.b);
                               bool keyPressed = false;
                               for (int i = 0; i < 16; i++) {
                                   if (key[i]) {
                                       V0 = i;
                                       keyPressed = true;
                                       if (verbose) fmt::print("KEY PRESSED: {0:X} V0 is now: {1:X}\n", i, V0);
                                       return 2;
                                   }
                               }
                               if(!keyPressed)
                                   return 0;
                           }
                    break;
                case 0x15:
                    if (verbose) fmt::print("F{0:X}15: Set delay timer to V{0:X}", bits.n.b);
                    delaytimer = v[bits.n.b];
                    break;
                case 0x18:
                    if (verbose) fmt::print("F{0:X}18: Set sound timer to V{0:X}", bits.n.b);
                    soundtimer = v[bits.n.b];
                    break;
                case 0x1E:
                    if (verbose) fmt::print("F{0:X}1E: I += V{0:X}", bits.n.b);
                    I += v[bits.n.b];
                    break;
                case 0x29:
                    if (verbose) fmt::print("F{0:X}29: Set I to the location of the sprite for the character in V{0:X}", bits.n.b);
                    I = v[bits.n.b] * 5;
                    break;
                case 0x33:
                    if (verbose) fmt::print("F{0:X}33: BCD(V{0:X}) - store binary coded decimal representation of V{0:X} at address I ({1:X})", bits.n.b, I);
                    ram[I+0] =  v[bits.n.b] / 100;
                    ram[I+1] = (v[bits.n.b] /  10) % 10;
                    ram[I+2] = (v[bits.n.b] % 100) % 10;
                    break;
                case 0x55:
                    if (verbose) fmt::print("F{0:X}55: Store V0-V{0:X} in memory starting at address in I. I += 1 for each value written.", bits.n.b);
                    for(int r = 0; r <= bits.n.b; r++) {
                        ram[I] = v[r];
                        I++;
                    }
                    break;
                case 0x65:
                    if (verbose) fmt::print("F{0:X}66: Load V0-V{0:X} with values from memory starting at address in I. I += 1 for each value read.", bits.n.b);
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
    if (verbose) fmt::print("\n");


    return 2;
}


