#pragma once

#include "c8_constants.hpp"

#include <stdint.h>

struct c8_interpeter_zone {
    // 8-bit general registers V0-VF
    uint8_t v[V_REGISTERS_SIZE] = {0};
    // 16-bit register to store addresses (only 12 bits needed)
    uint16_t i = 0;
    // 8-bit delay register
    uint8_t dt = 0;
    // 8-bit sound register
    uint8_t st = 0;
    // 16-bit program counter
    uint16_t pc = PROGRAM_OFFSET;
    // 8-bit stack pointer
    uint8_t sp = 0;
    // 16x16-bit stack to store return addresses
    uint16_t stack[STACK_SIZE];
    // store typography sprites for 0-F
    uint8_t char_sprites[80] = {
        0xF0,0x90,0x90,0x90,0xF0,
        0x20,0x60,0x20,0x20,0x70,
        0xF0,0x10,0xF0,0x80,0xF0,
        0xF0,0x10,0xF0,0x10,0xF0,
        0x90,0x90,0xF0,0x10,0x10,
        0xF0,0x80,0xF0,0x10,0xF0,
        0xF0,0x80,0xF0,0x90,0xF0,
        0xF0,0x10,0x20,0x40,0x40,
        0xF0,0x90,0xF0,0x90,0xF0,
        0xF0,0x90,0xF0,0x10,0xF0,
        0xF0,0x90,0xF0,0x90,0x90,
        0xE0,0x90,0xE0,0x90,0xE0,
        0xF0,0x80,0x80,0x80,0xF0,
        0xE0,0x90,0x90,0x90,0xE0,
        0xF0,0x80,0xF0,0x80,0xF0,
        0xF0,0x80,0xF0,0x80,0x80
    };
    // "hardware" register, to be modified by c8_window
    bool keyStates[16] = {0};
    // 64x32 screen buffer would go here, if it was not made redundant by use of SDL2


};