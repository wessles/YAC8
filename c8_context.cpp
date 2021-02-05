#include "c8_context.hpp"

#include "c8_window.hpp"

#include <thread>
#include <random>
#include <assert.h>
#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;

// helpful macros to make this code less verbose

#define hexpadnox(N) std::setfill('0') << std::setw(N) << std::right << std::hex
#define hexpad(N) "0x" << hexpadnox(N)

// handle invalid instructions
#define INVALID_INSTRUCTION \
            cout << "Unrecognized instruction." << endl; \
            pc+=2;
#ifndef NDEBUG
// log each instruction with values
// I
#define L(inst) cout << #inst"" << "\t\t";
// I addr
#define La(inst) cout << #inst" " << "addr=" << hexpad(4) << +addr << "\t\t";
// I x byte
#define Lxb(inst) cout << #inst" " << "V" << +v_reg_x << "=" << hexpad(1) << +x << ", " << hexpad(2) << +byte << "\t\t";
// I x y
#define Lxy(inst) cout << #inst" " << "V" << +v_reg_x  << "=" << hexpad(1) << +x << ", " << "V" << +v_reg_y << "=" << hexpad(1) << +y << "\t\t";
// I x y n
#define Lxyn(inst) cout << #inst" " << "V" << +v_reg_x  << "=" << hexpad(1) << +x << ", " << "V" << +v_reg_y << "=" << hexpad(1) << +y << ", " << hexpad(1) << +nibble << "\t\t";
// I x
#define Lx(inst) cout << #inst" " << "V" << +v_reg_x  << "=" << hexpad(1) << +x << "\t\t";
#else
#define L(inst)
#define La(inst)
#define Lxb(inst)
#define Lxy(inst)
#define Lxyn(inst)
#define Lx(inst)
#endif

c8_context::c8_context() {
    // zero out our memory
    std::fill((uint8_t*)(_pad), (uint8_t*)(this)+RAM_SIZE, 0);
}

void c8_context::loadROM(const uint8_t *rom, int size) {
    std::copy(rom, rom + size, program);
}

void c8_context::dump() {
    cout
        << "pc = " << hexpad(4) << +interpreter.pc << ", "
        << "sp = " << hexpad(2) <<  +interpreter.sp << ", "
        << "dt = " << hexpad(2) <<  +interpreter.dt << ", "
        << "st = " << hexpad(2) <<  +interpreter.st << ", "
        << "i = " << hexpad(4) <<  +interpreter.i
        << " V[ ";
    for(int i = 0; i < V_REGISTERS_SIZE; i++) {
        cout << hexpadnox(2) << +interpreter.v[i] << " ";
    }
    cout << "]" << endl;
    cout << "K[ ";
    for(int i = 0; i < 16; i++) {
        cout << +interpreter.keyStates[i] << " ";
    }
    cout << " ]";
}

void c8_context::run(c8_window &window) {
    bool *keyStates = interpreter.keyStates;
    uint8_t *ram = reinterpret_cast<uint8_t*>(this);

    uint16_t &pc = interpreter.pc;
    uint8_t &sp = interpreter.sp;
    uint8_t *v = interpreter.v;
    uint16_t *stack = interpreter.stack;
    uint16_t char_sprites = static_cast<uint16_t>(interpreter.char_sprites - ram);

    uint8_t &dt = interpreter.dt;
    uint8_t &st = interpreter.st;

    uint16_t &IR = interpreter.i;

    using clock = std::chrono::high_resolution_clock;

    auto last_timer_tick = clock::now();

    while(true) {
        assert(pc >= PROGRAM_OFFSET);
        assert(pc < RAM_SIZE);
        assert(IR >= 0);
        assert(IR < RAM_SIZE);
        assert(sp >= 0);
        assert(sp <= STACK_SIZE);

        auto frame_start = clock::now();

        // poll for inputs
        window.poll(interpreter);

        auto timeSinceLastTick = clock::now() - last_timer_tick;
        const auto tickInterval = std::chrono::milliseconds(1000/60);
        if(timeSinceLastTick > tickInterval) {
            if(dt != 0)
                dt -= 1;
            if(st != 0)
                st -= 1;
            last_timer_tick = clock::now();
        }

        // process instructions
        uint16_t instruction = *((uint16_t*)(ram + pc));
        uint16_t temp = instruction;
        instruction = 0;
        instruction |= temp >> 8;
        instruction |= temp << 8;

        cout << hexpad(4) << instruction << "\t";

        // convenient aliases, used by A = {3,4,5,6,7,8,9,C,D,E}
        uint8_t v_reg_x = static_cast<uint8_t>((instruction & 0x0F00) >> 8), v_reg_y = static_cast<uint8_t>((instruction & 0x00F0) >> 4);;
        uint8_t &x = v[v_reg_x], &y = v[v_reg_y];

        uint16_t addr = instruction & 0x0FFF;
        uint8_t byte = instruction & 0x00FF;
        uint8_t nibble = instruction & 0x000F;

        switch(instruction & 0xF000) {
            case 0x0000:
                switch(instruction & 0x00FF) {
                    case 0x00E0:
                        L(CLS);
                        window.clearScreen();
                        pc += 2;
                        break;
                    case 0x00EE:
                        L(RET);
                        pc = stack[--sp];
                        pc += 2;
                        break;
                    default:
                        INVALID_INSTRUCTION;
                }
                break;
            case 0x1000:
                La(JP);
                pc = addr;
                break;
            case 0x2000:
                La(CALL);
                stack[sp++] = pc;
                pc = addr;
                break;
            case 0x3000:
                // 3xkk - SE Vx, byte
                Lxb(SE);
                if(x == byte) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x4000:
                // 4xkk - SNE Vx, byte
                Lxb(SNE);
                if(x != byte) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x5000:
                // Axy0 - SE Vx, Vy
                Lxy(SE);
                if(x == y) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x6000:
                // 6xkk - LD Vx, byte
                Lxb(LD);
                x = byte;
                pc += 2;
                break;
            case 0x7000:
                // 7xkk - ADD Vx, byte
                Lxb(ADD);
                x += byte;
                pc += 2;
                break;
            case 0x8000:
                switch(instruction & 0x000F) {
                    case 0x0000:
                        // 8xy0 - LD Vx, Vy
                        Lxy(LD);
                        x = y;
                        pc += 2;
                        break;
                    case 0x0001:
                        // 8xy1 - OR Vx, Vy
                        Lxy(OR);
                        x |= y;
                        pc += 2;
                        break;
                    case 0x0002:
                        // 8xy2 - AND Vx, Vy
                        Lxy(AND);
                        x &= y;
                        pc += 2;
                        break;
                    case 0x0003:
                        // 8xy3 - XOR Vx, Vy
                        Lxy(XOR);
                        x ^= y;
                        pc += 2;
                        break;
                    case 0x0004:
                        // 8xy4 - Add Vx, Vy
                        Lxy(ADD);
                        v[0xf] = y > (0xFF - x) ? 1 : 0;
                        x += y;
                        pc += 2;
                        break;
                    case 0x0005:
                        // 8xy5 - SUB Vx, Vy
                        Lxy(SUB);
                        v[0xf] = (y <= x) ? 1 : 0;
                        x -= y;
                        pc += 2;
                        break;
                    case 0x0006:
                        // 8xy6 - SHR Vx {, Vy}
                        Lx(SHR);
                        v[0xf] = x & 0b1;
                        x = x >> 1;
                        pc += 2;
                        break;
                    case 0x0007:
                        // 8xy7 - SUBN Vx, Vy
                        Lxy(SUBN);
                        v[0xf] = (x <= y) ? 1 : 0;
                        x = y - x;
                        pc += 2;
                        break;
                    case 0x000E:
                        // 8xyE - SHL Vx {, Vy}
                        Lx(SHL);
                        v[0xf] = x >> 7;
                        x = x << 1;
                        pc += 2;
                        break;
                    default:
                    INVALID_INSTRUCTION;
                }
                break;
            case 0x9000:
                // 9xy0 - SNE Vx, Vy
                Lxy(SNE);
                if(x != y) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0xA000:
                // Annn - LD I, addr
                La(LD);
                IR = addr;
                pc += 2;
                break;
            case 0xB000:
                // Bnnn - JP V0, addr
                La(JP_V0);
                pc = addr + v[0];
                break;
            case 0xC000:
                // Cxkk - RND Vx, byte
                Lxb(RND);
                x = byte & (uint8_t)(std::rand() % 256);
                pc += 2;
                break;
            case 0xD000:
                // Dxyn - DRW Vx, Vy, nibble
                // Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
                Lxyn(DRW);
                window.drawSprite((ram + IR), x, y, nibble, v[0xf]);
                window.present();
                pc += 2;
                break;
            case 0xE000:
                switch(instruction & 0x00FF) {
                    case 0x009E:
                        // Ex9E - SKP Vx
                        Lx(SKP);
                        if(keyStates[x]) {
                            pc += 4;
                        } else {
                            pc += 2;
                        }
                        break;
                    case 0x00A1:
                        // ExA1 - SKNP Vx
                        Lx(SKNP);
                        if(!keyStates[x]) {
                            pc += 4;
                        } else {
                            pc += 2;
                        }
                        break;
                    case 0x0000:
                        // Ex00 - print Vx
                        // **NON-STANDARD DEBUG INSTRUCTION**
                        Lx(PRINT);
                        cout << "V" << +v_reg_x << " = " << hexpad(2) << +v[v_reg_x] << endl;
                        pc += 2;
                        break;
                    default:
                        INVALID_INSTRUCTION;
                }
                break;
            case 0xF000:
                switch(instruction & 0x00FF) {
                    case 0x0007:
                        // Fx07 - LD Vx, DT
                        Lx("LD Vx, DT");
                        x = dt;
                        pc += 2;
                        break;
                    case 0x000A: {
                        // Fx0A - LD Vx, K
                        Lx("LD Vx, K");
                        // wait to find key press
                        int k = -1;
                        do {
                            k = window.poll(interpreter);
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        } while (k == -1);
                        x = k;
                        pc += 2;
                        break;
                    }
                    case 0x0015:
                        // Fx15 - LD DT, Vx
                        Lx("LD DT, Vx");
                        dt = x;
                        pc += 2;
                        break;
                    case 0x0018:
                        // Fx18 - LD ST, Vx
                        Lx("LD ST, Vx");
                        st = x;
                        pc += 2;
                        break;
                    case 0x001E:
                        // Fx1E - ADD I, Vx
                        Lx("ADD I, Vx");
                        v[0xf] = (IR + x > 0xFFF) ? 1 : 0;
                        IR += x;
                        pc += 2;
                        break;
                    case 0x0029:
                        // Fx29 - LD F, Vx
                        Lx("LD F, Vx");
                        IR = char_sprites + 5 * x;
                        pc += 2;
                        break;
                    case 0x0033:
                        // Fx33 - LD B, Vx
                        // Store BCD representation of Vx in memory locations I, I+1, and I+2.
                        Lx("LD B, Vx");
                        ram[IR] = x / 100; // hundreds
                        ram[IR+1] = (x % 100) / 10; // tens
                        ram[IR+2] = x % 10; // ones
                        pc += 2;
                        break;
                    case 0x0055:
                        // Fx55 - LD [I], Vx
                        // Store registers V0 through Vx in memory starting at location I.
                        Lx("LD [I], Vx");
                        for(int i = 0; i <= v_reg_x; i++) {
                            ram[IR + i] = v[i];
                        }
                        IR += v_reg_x + 1;
                        pc += 2;
                        break;
                    case 0x0065:
                        // Fx65 - LD Vx, [I]
                        // Read registers V0 through Vx from memory starting at location I.
                        Lx("LD Vx, [I]");
                        for(int i = 0; i <= v_reg_x; i++) {
                            v[i] = ram[IR + i];
                        }
                        IR += v_reg_x + 1;
                        pc += 2;
                        break;
                    default:
                        INVALID_INSTRUCTION;
                }
                break;
            default:
                INVALID_INSTRUCTION;
        }

        cout << endl;
        dump();
        cout << endl;
        // busy loop until next frame.
        while(clock::now() - frame_start < std::chrono::milliseconds(1000 / 700)) {};
    }
}