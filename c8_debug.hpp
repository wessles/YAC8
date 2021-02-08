#pragma once

/**
 * This file simply holds a function which informatively describes an opcode. Used in the debugger.
 */

#include <sstream>
#include <iomanip>

namespace yac8 {

    #define h(N) std::setfill('0') << std::setw(N) << std::right << std::hex
    #define hx(N) "0x" << h(N)

    #define L(inst)     str << inst; break;
    #define La(inst)    str << inst << "\t" << "adr=" << h(3) << +addr; break;
    #define Lxb(inst)   str << inst << "\t" << "V" << h(1) << +x << "=" << h(2) << +vx << ", " << h(2) << +byte; break;
    #define Lxy(inst)   str << inst << "\t" << "V" << h(1) << +x << "=" << h(2) << +vx << ", " << "V" << +y << "=" << h(2) << +vy; break;
    #define Lxyn(inst)  str << inst << "\t" << "V" << h(1) << +x << "=" << h(2) << +vx << ", " << "V" << +y << "=" << h(2) << +vy << ", " << h(1) << +nibble; break;
    #define Lx(inst)    str << inst << "\t" << "V" << h(1) << +x << "=" << h(2) << +vx; break;
    #define Lxz(inst, z, vz)   str << inst << "\t" << "V" << h(1) << +x << "=" << h(2) << +vx << ", " << z << "=" << h(2) << vz; break;
    #define Lzx(inst, z, vz)   str << inst << "\t" << z << "=" << h(2) << vz << ", " << "V" << h(1) << +x << "=" << h(2) << +vx; break;
    #define L0a(inst)   str << inst << "\t" << "V0" << "adr=" << h(3) << +addr; break;

    std::string print_instruction(const int pc, const c8_state & state, const c8_quirks & quirks) {
        if(pc < PROGRAM_OFFSET || pc >= RAM_SIZE)
            return "???";

        // process instructions
        uint16_t instruction = (uint16_t)(state.ram[pc] << 8) | (uint16_t)(state.ram[pc+1]);

        // convenient aliases, used by A = {3,4,5,6,7,8,9,C,D,E}
        uint8_t x = static_cast<uint8_t>((instruction & 0x0F00) >> 8), y = static_cast<uint8_t>((instruction & 0x00F0) >> 4);
        const uint8_t &vx = state.v[x], &vy = state.v[y];

        uint16_t addr = instruction & 0x0FFF;
        uint8_t byte = instruction & 0x00FF;
        uint8_t nibble = instruction & 0x000F;

        std::ostringstream str{};

        str << hx(3) << +pc << "\t" << hx(4) << instruction << "\t";

        switch(instruction & 0xF000) {
            case 0x0000:
                switch(instruction & 0x00FF) {
                    case 0x00E0: L("CLS");
                    case 0x00EE: L("RET");
                }
                break;
            case 0x1000: La("JP  ");
            case 0x2000: La("CALL");
            case 0x3000: Lxb("SE  ");
            case 0x4000: Lxb("SNE ");
            case 0x5000: Lxy("SE  ");
            case 0x6000: Lxb("LD  ");
            case 0x7000: Lxb("ADD ");
            case 0x8000:
                switch(instruction & 0x000F) {
                    case 0x0000: Lxy("LD  ");
                    case 0x0001: Lxy("OR  ");
                    case 0x0002: Lxy("AND ");
                    case 0x0003: Lxy("XOR ");
                    case 0x0004: Lxy("ADD ");
                    case 0x0005: Lxy("SUB ");
                    case 0x0006:
                        if(quirks.shiftQuirk) {
                            Lx("SHR ");
                        } else {
                            Lxy("SHR ")
                        }
                    case 0x0007: Lxy("SUBN");
                    case 0x000E:
                        if(quirks.shiftQuirk) {
                            Lx("SHL ");
                        } else {
                            Lxy("SHL ")
                        }
                }
                break;
            case 0x9000: Lxy("SNE ");
            case 0xA000: La("LD  ");
            case 0xB000: L0a("JP  ");
                // Bnnn - JP V0, addr
            case 0xC000: Lxb("RND ");
            case 0xD000: Lxyn("DRW ");
            case 0xE000:
                switch(instruction & 0x00FF) {
                    case 0x009E: Lx("SKP ");
                    case 0x00A1: Lx("SKNP");
                    case 0x0000: Lx("PRNT");
                }
                break;
            case 0xF000:
                switch(instruction & 0x00FF) {
                    case 0x0007: Lxz("LD  ", "DT", +state.dt);
                        // Fx07 - LD Vx, DT
                    case 0x000A: Lxz("LD  ", "K", "?");
                        // Fx0A - LD Vx, K
                    case 0x0015: Lzx("LD  ", "DT", +state.dt);
                        // Fx15 - LD DT, Vx
                    case 0x0018: Lzx("LD  ", "ST", +state.st);
                        // Fx18 - LD ST, Vx
                    case 0x001E: Lzx("ADD ", "I", hx(3) << +state.I);
                        // Fx1E - ADD I, Vx
                    case 0x0029: Lzx("LD  ", "F", "?");
                        // Fx29 - LD F, Vx
                    case 0x0033: Lzx("LD  ", "B", "?");
                        // Fx33 - LD B, Vx
                    case 0x0055: Lzx("LD  ", "I", +state.I);
                        // Fx55 - LD I, Vx
                    case 0x0065: Lxz("LD  ", "I", +state.I);
                        // Fx65 - LD Vx, I
                }
                break;
        }
        return str.str();
    }

    std::string print_register(int i) {
        std::ostringstream str{};
        str << hx(2) << +i;
        return str.str();
    }
}