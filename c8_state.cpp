#include "c8_state.hpp"

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

#define NDEBUG

#ifndef NDEBUG
#define L(inst) cout << #inst"" << "\t\t";
#define La(inst) cout << #inst" " << "addr=" << hexpad(4) << +addr << "\t\t";
#define Lxb(inst) cout << #inst" " << "V" << +x << "=" << hexpad(1) << +vx << ", " << hexpad(2) << +byte << "\t\t";
#define Lxy(inst) cout << #inst" " << "V" << +x  << "=" << hexpad(1) << +vx << ", " << "V" << +y << "=" << hexpad(1) << +vy << "\t\t";
#define Lxyn(inst) cout << #inst" " << "V" << +x  << "=" << hexpad(1) << +vx << ", " << "V" << +y << "=" << hexpad(1) << +vy << ", " << hexpad(1) << +nibble << "\t\t";
#define Lx(inst) cout << #inst" " << "V" << +x  << "=" << hexpad(1) << +vx << "\t\t";
#else
#define L(inst)
#define La(inst)
#define Lxb(inst)
#define Lxy(inst)
#define Lxyn(inst)
#define Lx(inst)
#endif

namespace yac8 {
    c8_state::c8_state() {
        // assume default typography
        loadTypography(yac8::default_typography_buffer);
    }

    void c8_state::loadROM(const uint8_t *rom, int size) {
        // load ROM at program start (0x200)
        std::copy(rom, rom + size, ram + PROGRAM_OFFSET);
    }

    void c8_state::loadTypography(const uint16_t *typography) {
        // copy typography buffer (16 characters * 5 bytes per character)
        std::copy(typography, typography + 16 * 5, ram);
    }

    void c8_state::dump() {
        cout
            << "pc = " << hexpad(4) << +pc << ", "
            << "sp = " << hexpad(2) <<  +sp << ", "
            << "dt = " << hexpad(2) <<  +dt << ", "
            << "st = " << hexpad(2) <<  +st << ", "
            << "I = " << hexpad(4) <<  +I
            << " V[ ";
        for(int i = 0; i < V_REGISTERS_SIZE; i++) {
            cout << hexpadnox(2) << +v[i] << " ";
        }
        cout << "]" << endl;
        cout << "K[ ";
        for(int i = 0; i < 16; i++) {
            cout << +keyStates[i] << " ";
        }
        cout << " ]";
    }

    void c8_state::step(c8_hardware_api &hardware_api, c8_quirks quirks) {
        assert(pc >= PROGRAM_OFFSET);
        assert(pc < RAM_SIZE);
        assert(I >= 0);
        assert(I < RAM_SIZE);
        assert(sp >= 0);
        assert(sp <= STACK_SIZE);

        // process instructions
        uint16_t instruction = (uint16_t)(ram[pc] << 8) | (uint16_t)(ram[pc+1]);

//        cout << hexpad(4) << instruction << "\t";

        // convenient aliases, used by A = {3,4,5,6,7,8,9,C,D,E}
        uint8_t x = static_cast<uint8_t>((instruction & 0x0F00) >> 8), y = static_cast<uint8_t>((instruction & 0x00F0) >> 4);
        uint8_t &vx = v[x], &vy = v[y];

        uint16_t addr = instruction & 0x0FFF;
        uint8_t byte = instruction & 0x00FF;
        uint8_t nibble = instruction & 0x000F;

        switch(instruction & 0xF000) {
            case 0x0000:
                switch(instruction & 0x00FF) {
                    case 0x00E0:
                        L(CLS);
                        hardware_api.clear_screen();
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
                if(vx == byte) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x4000:
                // 4xkk - SNE Vx, byte
                Lxb(SNE);
                if(vx != byte) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x5000:
                // Axy0 - SE Vx, Vy
                Lxy(SE);
                if(vx == vy) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0x6000:
                // 6xkk - LD Vx, byte
                Lxb(LD);
                vx = byte;
                pc += 2;
                break;
            case 0x7000:
                // 7xkk - ADD Vx, byte
                Lxb(ADD);
                vx += byte;
                pc += 2;
                break;
            case 0x8000:
                switch(instruction & 0x000F) {
                    case 0x0000:
                        // 8xy0 - LD Vx, Vy
                        Lxy(LD);
                        vx = vy;
                        pc += 2;
                        break;
                    case 0x0001:
                        // 8xy1 - OR Vx, Vy
                        Lxy(OR);
                        vx |= vy;
                        pc += 2;
                        break;
                    case 0x0002:
                        // 8xy2 - AND Vx, Vy
                        Lxy(AND);
                        vx &= vy;
                        pc += 2;
                        break;
                    case 0x0003:
                        // 8xy3 - XOR Vx, Vy
                        Lxy(XOR);
                        vx ^= vy;
                        pc += 2;
                        break;
                    case 0x0004:
                        // 8xy4 - Add Vx, Vy
                        Lxy(ADD);
                        v[0xf] = vy > (0xFF - vx) ? 1 : 0;
                        vx += vy;
                        pc += 2;
                        break;
                    case 0x0005:
                        // 8xy5 - SUB Vx, Vy
                        Lxy(SUB);
                        v[0xf] = (vy <= vx) ? 1 : 0;
                        vx -= vy;
                        pc += 2;
                        break;
                    case 0x0006:
                        // 8xy6 - SHR Vx {, Vy}
                        Lx(SHR);
                        v[0xf] = vx & 0b1;
                        if(quirks.shiftQuirk)
                            vx = vx >> vx;
                        else
                            vx = vx >> vy;
                        pc += 2;
                        break;
                    case 0x0007:
                        // 8xy7 - SUBN Vx, Vy
                        Lxy(SUBN);
                        v[0xf] = (vx <= vy) ? 1 : 0;
                        vx = vy - vx;
                        pc += 2;
                        break;
                    case 0x000E:
                        // 8xyE - SHL Vx {, Vy}
                        Lx(SHL);
                        v[0xf] = vx >> 7;
                        if(quirks.shiftQuirk)
                            vx = vx << vx;
                        else
                            vx = vx << vy;
                        pc += 2;
                        break;
                    default:
                    INVALID_INSTRUCTION;
                }
                break;
            case 0x9000:
                // 9xy0 - SNE Vx, Vy
                Lxy(SNE);
                if(vx != vy) {
                    pc += 4;
                } else {
                    pc += 2;
                }
                break;
            case 0xA000:
                // Annn - LD I, addr
                La(LD);
                I = addr;
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
                vx = byte & hardware_api.random_byte();
                pc += 2;
                break;
            case 0xD000:
                // Dxyn - DRW Vx, Vy, nibble
                // Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
                Lxyn(DRW);
                hardware_api.draw_sprite(ram + I, vx, vy, nibble, v[0xf]);
                pc += 2;
                break;
            case 0xE000:
                switch(instruction & 0x00FF) {
                    case 0x009E:
                        // Ex9E - SKP Vx
                        Lx(SKP);
                        if(keyStates[vx]) {
                            pc += 4;
                        } else {
                            pc += 2;
                        }
                        break;
                    case 0x00A1:
                        // ExA1 - SKNP Vx
                        Lx(SKNP);
                        if(!keyStates[vx]) {
                            pc += 4;
                        } else {
                            pc += 2;
                        }
                        break;
                    case 0x0000:
                        // Ex00 - print Vx
                        // **NON-STANDARD DEBUG INSTRUCTION**
                        Lx(PRINT);
                        cout << "V" << +x << " = " << hexpad(2) << +vx << endl;
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
                        vx = dt;
                        pc += 2;
                        break;
                    case 0x000A: {
                        // Fx0A - LD Vx, K
                        Lx("LD Vx, K");
                        // wait for hardware to set last_key flag
                        if(lastKey == NO_LAST_KEY)
                            break;
                        assert(lastKey < 16);
                        vx = lastKey;
                        pc += 2;
                        break;
                    }
                    case 0x0015:
                        // Fx15 - LD DT, Vx
                        Lx("LD DT, Vx");
                        dt = vx;
                        pc += 2;
                        break;
                    case 0x0018:
                        // Fx18 - LD ST, Vx
                        Lx("LD ST, Vx");
                        st = vx;
                        pc += 2;
                        break;
                    case 0x001E:
                        // Fx1E - ADD I, Vx
                        Lx("ADD I, Vx");
                        v[0xf] = (I + vx > 0xFFF) ? 1 : 0;
                        I += vx;
                        pc += 2;
                        break;
                    case 0x0029:
                        // Fx29 - LD F, Vx
                        Lx("LD F, Vx");
                        I = 5 * vx;
                        pc += 2;
                        break;
                    case 0x0033:
                        // Fx33 - LD B, Vx
                        // Store BCD representation of Vx in memory locations I, I+1, and I+2.
                        Lx("LD B, Vx");
                        ram[I] = vx / 100; // hundreds
                        ram[I+1] = (vx % 100) / 10; // tens
                        ram[I+2] = vx % 10; // ones
                        pc += 2;
                        break;
                    case 0x0055:
                        // Fx55 - LD [I], Vx
                        // Store registers V0 through Vx in memory starting at location I.
                        Lx("LD [I], Vx");
                        for(int i = 0; i <= x; i++) {
                            ram[I + i] = v[i];
                        }
                        if(!quirks.loadStoreQuirk)
                            I += x + 1;
                        pc += 2;
                        break;
                    case 0x0065:
                        // Fx65 - LD Vx, [I]
                        // Read registers V0 through Vx from memory starting at location I.
                        Lx("LD Vx, [I]");
                        for(int i = 0; i <= x; i++) {
                            v[i] = ram[I + i];
                        }
                        if(!quirks.loadStoreQuirk)
                            I += x + 1;
                        pc += 2;
                        break;
                    default:
                        INVALID_INSTRUCTION;
                }
                break;
            default:
                INVALID_INSTRUCTION;
        }

//        cout << endl;
//        dump();
//        cout << endl;
    }
}