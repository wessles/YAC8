#pragma once

#include <SDL.h>
#include <string>
#include <unordered_map>

#include "c8_constants.hpp"
#include "c8_state.hpp"

namespace yac8 {
    // accounts for the size of the menu bar
    const int VIEWPORT_Y_OFFSET = 19;
    const int MAX_SPEED = 1000;

    // egomaniacal bootup sequence
    const uint16_t DEMO_ROM[] = {
            0x0b12,0x7962,0x5720,0x5345,0x4c20,
            0x602e,0x6105,0x6216,0xa21e,0xd150,
            0xa20f,0xd25f,0x630f,0xa210,0xd36e,
            0x7315,0xa206,0xd373,0x7315,0xa206,
            0xd378,0x7315,0xa206,0xd37d,0x7315,
            0xa206,0xd382,0x6015,0x6100,0x6200,
            0x633f,0xa21f,0xd04f,0x7011,0xd202,
            0x7231,0x12fe,0x2841,0x4482,0x8038,
            0x6300,0x3e77,0x1c1c,0x001c,0x7f3e,
            0x6063,0x7f63,0x003e,0x773e,0x7f63,
            0x6363,0x3e00,0x6363,0x633e,0x3e63,
            0x88f0,0x88f0,0x88f0,0x2050,0x2020,
            0x8888,0xa888,0xf8d8,0xf080,0xf880,
            0x8078,0x0870,0x00f0,
    };

    class c8_emulator {
        SDL_Renderer *renderer;
        SDL_Texture *screenBuffer;
        bool pixels[WINDOW_WIDTH * WINDOW_HEIGHT] = {0};

        float bgColor[4] = {15.0f/255.0f, 35.0f/255.0f, 17.0f/255.0f};
        float fgColor[4] = {141.0f/255.0f, 255.0f/255.0f, 128.0f/255.0f};
        c8_quirks quirks{};
        int processorSpeed = MAX_SPEED;

    public:
        void run();
        void clearScreen();
        void redrawScreen();
        void drawSprite(const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF);
    };
}