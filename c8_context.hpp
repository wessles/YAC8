#pragma once

#include <stdint.h>
#include <algorithm>

#include "c8_constants.hpp"
#include "c8_window.hpp"

class c8_context {
public:
    c8_interpeter_zone interpreter;
    uint8_t _pad[PROGRAM_OFFSET - sizeof(c8_interpeter_zone)];
    // start program at 0x200
    uint8_t program[RAM_SIZE - PROGRAM_OFFSET];

    c8_context();
    void loadROM(const uint8_t *rom, int size);
    void dump();
    void run(c8_window &window);
};