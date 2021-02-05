#pragma once

#include <SDL.h>

#include "c8_window.hpp"
#include "c8_interpreter_zone.hpp"

class c8_window_sdl2 : public c8_window {
    SDL_Window *window;
    SDL_Renderer *renderer;
    uint8_t pixels[WINDOW_WIDTH * WINDOW_HEIGHT];

public:
    c8_window_sdl2(int scale);

    void clearScreen();

    void present();

    int SDL_to_c8_key(int key);

    int poll(c8_interpeter_zone & interpreter);
    
    void drawSprite(uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF);

    ~c8_window_sdl2();
};