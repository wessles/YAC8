#pragma once

#include <functional>

namespace yac8 {
    /**
     * A set of functions that define every hardware operation a Chip-8 instruction could kick off.
     */
    struct c8_hardware_api {
        std::function<void(const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF)> draw_sprite;
        std::function<void()> clear_screen;
        std::function<uint8_t()> random_byte;
    };
}