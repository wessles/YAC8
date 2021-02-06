#pragma once

namespace yac8 {
    const int
        V_REGISTERS_SIZE = 0x10,
        RAM_SIZE = 0x1000, // 4096
        PROGRAM_OFFSET = 0x200, // 512
        STACK_SIZE = 0x10, // 16
        WINDOW_WIDTH = 64,
        WINDOW_HEIGHT = 32,
        SPRITE_BYTE_WIDTH = 1,
        SPRITE_HEIGHT = 5;

    const uint16_t default_typography_buffer[80] = {
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
}