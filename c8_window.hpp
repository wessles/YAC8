#pragma once

#include "c8_interpreter_zone.hpp"

class c8_window {
public:
    virtual void clearScreen() = 0;
    virtual void drawSprite(uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) = 0;
    virtual void present() = 0;
    virtual int poll(c8_interpeter_zone & interpreter) = 0;
    virtual ~c8_window() {};
};