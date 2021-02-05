#include "c8_window_sdl2.hpp"

#include "c8_window.hpp"

#include <algorithm>

c8_window_sdl2::c8_window_sdl2(int scale) {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("SDL2Test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    // need a smaller resolution to fit c8 standard
    SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

    clearScreen();
    present();
}

void c8_window_sdl2::clearScreen() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    std::fill(pixels, pixels + WINDOW_WIDTH * WINDOW_HEIGHT, 0);
}

void c8_window_sdl2::present() {
    SDL_RenderPresent(renderer);
}

void c8_window_sdl2::drawSprite(uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) {
    VF = 0;
    for(int j = 0; j < n; j++) {
        for(int i = 0; i <= 8; i++) {
            uint8_t value = (sprite[j] >> (8-i)) & 0b1;
            if(value > 0) {
                int px = x+i;
                int py = y+j;
                if(px < 0) px += WINDOW_WIDTH;
                px = px % WINDOW_WIDTH;
                if(py < 0) py += WINDOW_HEIGHT;
                py = py % WINDOW_HEIGHT;

                 uint8_t & savedPixel = pixels[py*WINDOW_WIDTH+px];
                 uint8_t previous = savedPixel;
                 savedPixel ^= value;
                 if(savedPixel == 0 && previous == 1)
                     VF = 1;
                 SDL_SetRenderDrawColor(renderer, 255*savedPixel, 255*savedPixel, 255*savedPixel, SDL_ALPHA_OPAQUE);
                 SDL_RenderDrawPoint(renderer, px, py);
            }
        }
    }
}

int c8_window_sdl2::SDL_to_c8_key(int key) {
    switch(key) {
        case SDLK_1: return 1;
        case SDLK_2: return 2;
        case SDLK_3: return 3;
        case SDLK_4: return 0xC;
        case SDLK_q: return 4;
        case SDLK_w: return 5;
        case SDLK_e: return 6;
        case SDLK_r: return 0xD;
        case SDLK_a: return 7;
        case SDLK_s: return 8;
        case SDLK_d: return 9;
        case SDLK_f: return 0xE;
        case SDLK_z: return 0xA;
        case SDLK_x: return 0x0;
        case SDLK_c: return 0xB;
        case SDLK_v: return 0xF;
        default: return 0x0;
    }
}

int c8_window_sdl2::poll(c8_interpeter_zone & interpreter) {
    SDL_Event event;
    int firstKeyDown = -1;
    while (SDL_PollEvent(&event)) {
        // check for messages
        int k = SDL_to_c8_key(event.key.keysym.sym);
        switch (event.type) {
            case SDL_QUIT:
            std::exit(1);
            break;
            // check for keypresses
            case SDL_KEYDOWN:
            if(firstKeyDown == -1)
                firstKeyDown = k;
            interpreter.keyStates[k] = true;
            break;
            case SDL_KEYUP:
            interpreter.keyStates[k] = false;
            break;
            default: break;
        }
    }
    return firstKeyDown;
}

c8_window_sdl2::~c8_window_sdl2() {
    SDL_DestroyWindow(window);
    SDL_Quit();
}