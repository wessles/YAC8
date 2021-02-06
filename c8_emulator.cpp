#include "c8_emulator.hpp"

#include "c8_state.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_sdl.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <windows.h>
#include <string>
#include <map>
#include <unordered_map>
#include <fstream>
#include <iostream>

using std::string;

namespace yac8 {

    string openROM() {
        OPENFILENAME ofn{};

        char fileName[MAX_PATH] = "";

        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = NULL;
        ofn.lpstrFilter = "Chip8 ROMs (*)\0*\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        ofn.lpstrDefExt = "";

        string fileNameStr;
        if ( GetOpenFileName(&ofn) )
            fileNameStr = fileName;

        return fileNameStr;
    }

    int SDL_to_c8_key(int key) {
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
            default: return -1;
        }
    }

    void c8_emulator::run() {
        int scale = 20;

        SDL_Init(SDL_INIT_EVERYTHING);
        SDL_Window *window = SDL_CreateWindow("YAC8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale + VIEWPORT_Y_OFFSET, 0);
        this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        ImGui::CreateContext();
        ImGuiSDL::Initialize(renderer, WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale);
        this->screenBuffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
        {
            SDL_SetRenderTarget(renderer, screenBuffer);
            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
            SDL_RenderClear(renderer);
            SDL_SetRenderTarget(renderer, nullptr);
        }

        // setup hardware API hooks
        c8_hardware_api hardware_api{};
        hardware_api.draw_sprite = [&](const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) { drawSprite(sprite, x, y, n, VF); };
        hardware_api.clear_screen = [&]() { clearScreen(); };
        std::random_device engine;
        std::uniform_int_distribution<int> dist(0, 0xff);
        hardware_api.random_byte = [&]()->uint8_t { return (uint8_t)dist(engine); };

        // setup timers
        using clock = std::chrono::high_resolution_clock;
        auto last_timer_tick = clock::now();
        auto frame_start = clock::now();

        // load initial demo rom
        std::vector<char> romData(sizeof(DEMO_ROM));
        std::copy((const char*)DEMO_ROM,(const char*)DEMO_ROM+sizeof(DEMO_ROM), &romData[0]);
        c8_state state = {};
        clearScreen();
        state.loadROM((const uint8_t*)romData.data(), romData.size());
        state.loadTypography(yac8::default_typography_buffer);

        bool run = true;
        while(run) {
            ImGuiIO& io = ImGui::GetIO();

            // handle timer ticks
            auto timeSinceLastTick = clock::now() - last_timer_tick;
            const auto tickInterval = std::chrono::milliseconds(1000/60);
            if(timeSinceLastTick > tickInterval) {
                if(state.dt != 0)
                    state.dt -= 1;
                if(state.st != 0)
                    state.st -= 1;
                last_timer_tick = clock::now();
            }

            bool reset = false;

            // handle inputs
            state.lastKey = yac8::NO_LAST_KEY;
            SDL_Event e;
            int wheel = 0;
            while (SDL_PollEvent(&e))
            {
                int k = SDL_to_c8_key(e.key.keysym.sym);

                if (e.type == SDL_QUIT) run = false;
                else if (e.type == SDL_WINDOWEVENT)
                {
                    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        io.DisplaySize.x = static_cast<float>(e.window.data1);
                        io.DisplaySize.y = static_cast<float>(e.window.data2);
                    }
                }
                else if (e.type == SDL_MOUSEWHEEL)
                {
                    wheel = e.wheel.y;
                } else if(e.type == SDL_KEYUP) {
                    if(k > 0) {
                        state.keyStates[k] = false;
                    }
                } else if(e.type == SDL_KEYDOWN) {
                    if(e.key.keysym.sym == SDLK_BACKSPACE) {
                        reset = true;
                    } else if(k > 0) {
                        if(state.lastKey == yac8::NO_LAST_KEY) {
                            state.lastKey = k;
                        }
                        state.keyStates[k] = true;
                    }
                }
            }

            // step chip8 simulation if it's time
            if(clock::now() - frame_start >= std::chrono::microseconds(1000000 / (processorSpeed))) {
                state.step(hardware_api, quirks);
                frame_start = clock::now();
            }

            // handle imgui inputs
            int mouseX, mouseY;
            const int buttons = SDL_GetMouseState(&mouseX, &mouseY);
            io.DeltaTime = 1.0f / 60.0f;
            io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
            io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
            io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
            io.MouseWheel = static_cast<float>(wheel);

            // handle menus
            ImGui::NewFrame();
            if(ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if(ImGui::MenuItem("Open"))
                    {
                        string romFileName = openROM();

                        std::ifstream is(romFileName, std::ios::in|std::ios::binary|std::ios::ate);
                        if(is.is_open()) {
                            int size = is.tellg();
                            romData.resize(size);
                            is.seekg(0, std::ios::beg);
                            is.read(&romData[0], size);
                            is.close();

                            state = {};
                            state.loadROM((const uint8_t *) romData.data(), romData.size());
                            state.loadTypography(yac8::default_typography_buffer);

                            clearScreen();
                        }
                    }
                    if(ImGui::MenuItem("Reset"))
                    {
                        reset = true;
                    }
                    ImGui::EndMenu();
                }

                if(ImGui::BeginMenu("Emulation")) {
                    ImGui::SliderInt("Processor Cycles / Sec", &processorSpeed, 1, MAX_SPEED);
                    ImGui::Checkbox("Load/Store Quirk", &quirks.loadStoreQuirk);
                    ImGui::Checkbox("Shift Quirk", &quirks.shiftQuirk);
                    ImGui::Checkbox("Wrapping", &quirks.wrap);
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Colors"))
                {
                    if(ImGui::ColorPicker3("Background Color", bgColor)) {
                        redrawScreen();
                    }
                    if(ImGui::ColorPicker3("Foreground Color", fgColor)) {
                        redrawScreen();
                    }
                    ImGui::EndMenu();
                }
                if(ImGui::BeginMenu("Help")) {
                    ImGui::Text("Game Controls:\n\t1234\n\tqwer\n\tasdf\n\tzxcv\nEmulator Controls:\n\t[Backspace] Reset");
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            SDL_Rect rect;
            rect.x = 0;
            rect.y = VIEWPORT_Y_OFFSET;
            rect.w = WINDOW_WIDTH * scale;
            rect.h = WINDOW_HEIGHT * scale;
            SDL_RenderCopy(renderer, screenBuffer, NULL, &rect);

            ImGui::Render();
            ImGuiSDL::Render(ImGui::GetDrawData());

            SDL_RenderPresent(renderer);

            if(reset) {
                state = {};
                clearScreen();
                state.loadROM((const uint8_t *) romData.data(), romData.size());
                state.loadTypography(yac8::default_typography_buffer);
            }
        }

        ImGuiSDL::Deinitialize();

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        ImGui::DestroyContext();
    }

    void c8_emulator::redrawScreen() {
        SDL_SetRenderTarget(renderer, screenBuffer);
        SDL_SetRenderDrawColor(renderer, 255*bgColor[0], 255*bgColor[1], 255*bgColor[2], SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        for(int i = 0; i < WINDOW_WIDTH; i++) {
            for(int j = 0; j < WINDOW_HEIGHT; j++) {
                SDL_SetRenderDrawColor(renderer, 255*fgColor[0], 255*fgColor[1], 255*fgColor[2], SDL_ALPHA_OPAQUE);
                if(pixels[j*WINDOW_WIDTH + i]) {
                    SDL_RenderDrawPoint(renderer, i, j);
                }
            }
        }
        SDL_SetRenderTarget(renderer, nullptr);
    }

    void c8_emulator::clearScreen() {
        SDL_SetRenderTarget(renderer, screenBuffer);
        SDL_SetRenderDrawColor(renderer, 255*bgColor[0], 255*bgColor[1], 255*bgColor[2], SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);
        std::fill(pixels, pixels + WINDOW_WIDTH * WINDOW_HEIGHT, 0);
        SDL_SetRenderTarget(renderer, nullptr);
    }

    void c8_emulator::drawSprite(const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) {
        SDL_SetRenderTarget(renderer, screenBuffer);
        VF = 0;
        x = x % WINDOW_WIDTH;
        y = y % WINDOW_HEIGHT;

        for(int j = 0; j < n; j++) {
            for(int i = 1; i <= 8; i++) {
                uint8_t value = (sprite[j] >> (8-i)) & 0b1;
                if(value > 0) {
                    int px = x+i - 1;
                    int py = y+j;

                    if(quirks.wrap) {
                        px = px % WINDOW_WIDTH;
                        py = py % WINDOW_HEIGHT;
                    } else {
                        if(px > WINDOW_WIDTH || py > WINDOW_HEIGHT)
                            continue;
                    }

                    bool & savedPixel = pixels[py*WINDOW_WIDTH+px];
                    bool previous = savedPixel;
                    savedPixel ^= value;
                    if(savedPixel == 0 && previous == 1)
                        VF = 1;

                    SDL_SetRenderDrawColor(renderer,
                                           255*fgColor[0]*savedPixel + 255*bgColor[0]*(!savedPixel),
                                           255*fgColor[1]*savedPixel + 255*bgColor[1]*(!savedPixel),
                                           255*fgColor[2]*savedPixel + 255*bgColor[2]*(!savedPixel),
                                           SDL_ALPHA_OPAQUE);
                    SDL_RenderDrawPoint(renderer, px, py);
                }
            }
        }
        SDL_SetRenderTarget(renderer, nullptr);
    }
}