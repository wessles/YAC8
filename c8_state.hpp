#pragma once

#include <stdint.h>
#include <algorithm>

#include "c8_constants.hpp"
#include "c8_hardware_api.hpp"
#include "c8_quirks.hpp"

namespace yac8 {
    const uint8_t NO_LAST_KEY = 0xFF;

    class c8_state {
    public:
        // 16-bit program counter
        uint16_t pc = PROGRAM_OFFSET;
        // 8-bit stack pointer
        uint8_t sp = 0;
        // 16x16-bit stack to store return addresses
        uint16_t stack[STACK_SIZE] = {0};
        // 8-bit general registers V0-VF
        uint8_t v[V_REGISTERS_SIZE] = {0};
        // 16-bit register to store addresses for special ops
        uint16_t I = 0;
        // 8-bit delay register
        uint8_t dt = 0;
        // 8-bit sound register
        uint8_t st = 0;
        // set by c8_hardware
        bool keyStates[16] = {false};
        // set by c8_hardware, equals value of last key pressed
        uint8_t lastKey = NO_LAST_KEY;

        // RAM, contains program memory, typography, etc.
        uint8_t ram[RAM_SIZE] = {0};

        c8_state();
        void loadTypography(const uint16_t *typography);
        void loadROM(const uint8_t *rom, int size);
        void step(c8_hardware_api &window, c8_quirks quirks);
        void dump();
    };
}