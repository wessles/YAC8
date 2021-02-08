#include "c8_emulator.hpp"

#include <SDL.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <thread>
#include <string>
#include <fstream>
#include <iostream>
#include <Windows.h>
#include <mutex>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_sdl.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "c8_debug.hpp"
#include "c8_noisemaker.hpp"

using std::string;

namespace yac8 {

    // get filename of ROM using windows explorer api
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

    // mapping from SDL to chip8 input keypad
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

    // Shader sources for CRT screen
    const GLchar* vertexSource =
        "#version 150\n"
        "attribute vec4 position;\n"
        "out vec2 texCoord;"
        "void main()\n"
        "{\n"
        "float x = -1.0 + float((gl_VertexID & 1) << 2);\n"
        "float y = -1.0 + float((gl_VertexID & 2) << 1);\n"
        "texCoord.x = (x+1.0)*0.5;\n"
        "texCoord.y = (y+1.0)*0.5;\n"
        "gl_Position = vec4(x, y, 0, 1);\n"
        "}\n";
    const GLchar* fragmentSource =
        "#version 150\n"
        "\n"
        "\n"
        "uniform sampler2D tex;\n"
        "uniform vec4 background;\n"
        "uniform vec4 foreground;\n"
        "\n"
        "in vec2 texCoord;\n"
        "\n"
        "out vec4 fragColor;\n"
        "\n"
        "uniform float CRT_CURVE_AMNTx;\n"
        "uniform float CRT_CURVE_AMNTy;\n"
        "uniform float SCAN_LINE_MULT;\n"
        "uniform float e;\n"
        "\n"
        "void main() {\n"
        "\tfor(int x = -3; x <= 3; x++) {\n"
        "\t\tfor(int y = -3; y <= 3; y++) {\n"
        "\t\t\tvec2 tc = texCoord + e*vec2(x,y);\n"
        "\t\t\tfloat dx = abs(0.5-tc.x);\n"
        "\t\t\tfloat dy = abs(0.5-tc.y);\n"
        "\t\t\tdx *= dx;\n"
        "\t\t\tdy *= dy;\n"
        "\t\t\ttc.x -= 0.5;\n"
        "\t\t\ttc.x *= 1.0 + (dy * CRT_CURVE_AMNTx);\n"
        "\t\t\ttc.x += 0.5;\n"
        "\t\t\ttc.y -= 0.5;\n"
        "\t\t\ttc.y *= 1.0 + (dx * CRT_CURVE_AMNTy);\n"
        "\t\t\ttc.y += 0.5;\n"
        "\t\t\tfloat a = texture(tex, vec2(tc.x, 1.0-tc.y)).r;\n"
        "\t\t\tbool b = tc.y > 1.0 || tc.x < 0.0 || tc.x > 1.0 || tc.y < 0.0;\n"
        "\t\t\tfragColor += float(!b)*(background * float(1.0-a) + foreground * float(a) + sin(tc.y * SCAN_LINE_MULT) * 0.02) / 49.0;\n"
        "\t\t}\n"
        "\t}\n"
        "}";

    // emulation to be run on a seperate thread
    void emulationThread(
            std::mutex &state_mutex,
             const bool *running,
             bool *incompatible_flag,
             c8_emulator & emu,
             c8_hardware_api & hardware_api,
             c8_state & state)
             {
        using clock = std::chrono::high_resolution_clock;
        auto frame_start = clock::now();

        while(*running) {
            // step chip8 simulation if it's time
            if(clock::now() - frame_start >= std::chrono::microseconds(1000000 / emu.processorSpeed)) {
                state_mutex.lock();

                // step emulation if it's time. If we encounter a bad instruction, mark the incompatible_flag
                if(!emu.debug_state.paused || emu.debug_state.step) {
                    emu.debug_state.step = false;
                    if(!state.step(hardware_api, emu.quirks)) {
                        *incompatible_flag = true;
                    }
                }

                // if not paused, evaluate any breakpoints
                if(!emu.debug_state.paused) {
                    // evaluate regular breakpoints
                    if(emu.debug_state.breakPoints[state.pc - PROGRAM_OFFSET]) {
                        emu.debug_state.paused = true;
                    }

                    uint16_t inst = (uint16_t) (state.ram[state.pc] << 8) | (uint16_t) (state.ram[state.pc + 1]);
                    uint8_t A = (inst & 0xF000) >> 12;

                    // break on any draw call
                    if (emu.debug_state.breakDRW && A == 0xD) {
                        emu.debug_state.breakDRW = false;
                        emu.debug_state.paused = true;
                    }

                    // break on any JP,CALL,RET, or skip instruction
                    if (emu.debug_state.breakJP && (
                            inst == 0x00EE
                            || A == 0x1
                            || A == 0x2
                            || A == 0x3
                            || A == 0x4
                            || A == 0x5
                            || A == 0x9
                            || A == 0xB
                    )) {
                        emu.debug_state.breakJP = false;
                        emu.debug_state.paused = true;
                    }

                    // break on wait for keypress
                    if(emu.debug_state.breakLDK && (inst&0xF0FF) == 0xF00A) {
                        emu.debug_state.breakLDK = false;
                        emu.debug_state.paused = true;
                    }

                    // break on key skip
                    if(emu.debug_state.breakSKP && (inst&0xF000) == 0xE000) {
                        emu.debug_state.breakSKP = false;
                        emu.debug_state.paused = true;
                    }
                }
                state_mutex.unlock();

                frame_start = clock::now();

                // now rest
                std::this_thread::yield();
                if(emu.slowedProcessorSpeed) {
                    SDL_Delay(1);
                }
            }
        }
    }

    void c8_emulator::run() {
        int scale = 20;

        // initialize SDL, OpenGL, ImGui
        SDL_Window *window;
        {
            SDL_Init(SDL_INIT_EVERYTHING);
            window = SDL_CreateWindow("YAC8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH * scale, WINDOW_HEIGHT * scale + VIEWPORT_Y_OFFSET, SDL_WINDOW_OPENGL);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetSwapInterval(1);
            SDL_GLContext context = SDL_GL_CreateContext(window);
            gladLoadGL();
            ImGui::CreateContext();
            ImGui_ImplSDL2_InitForOpenGL(window, context);
            ImGui_ImplOpenGL3_Init();
        }

        // Create dummy Vertex Array Object
        {
            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
        }

        // compile CRT shaders
        GLuint crtShaderProgram;
        {
            // Create and compile the vertex shader
            GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertexShader, 1, &vertexSource, NULL);
            glCompileShader(vertexShader);
            GLint isCompiled = 0;
            glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE) {
                GLint maxLength = 0;
                glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

                // The maxLength includes the NULL character
                std::vector<GLchar> errorLog(maxLength);
                glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &errorLog[0]);
                std::cerr << errorLog.data() << std::endl;
            }

            // Create and compile the fragment shader
            GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
            glCompileShader(fragmentShader);
            isCompiled = 0;
            glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE) {
                GLint maxLength = 0;
                glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

                // The maxLength includes the NULL character
                std::vector<GLchar> errorLog(maxLength);
                glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &errorLog[0]);
                std::cerr << errorLog.data() << std::endl;
            }

            // Link the vertex and fragment shader into a shader program
            crtShaderProgram = glCreateProgram();
            glAttachShader(crtShaderProgram, vertexShader);
            glAttachShader(crtShaderProgram, fragmentShader);
            glLinkProgram(crtShaderProgram);
        }

        // create screenbuffer texture
        {
            glGenTextures(1, &screenBufferTex);
            glBindTexture(GL_TEXTURE_2D, screenBufferTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(
                    GL_TEXTURE_2D, 0, GL_R8,
                    WINDOW_WIDTH, WINDOW_HEIGHT,
                    0, GL_RED, GL_UNSIGNED_BYTE, decayingPixelBuffer);
        }

        // initialize the buzzer
        c8_noisemaker noisemaker{};

        // setup hardware API hooks
        c8_hardware_api hardware_api{};
        hardware_api.draw_sprite = [&](const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) { drawSprite(sprite, x, y, n, VF); };
        hardware_api.clear_screen = [&]() { clearScreen(); };
        std::random_device engine;
        std::uniform_int_distribution<int> dist(0, 0xff);
        hardware_api.random_byte = [&]()->uint8_t { return (uint8_t)dist(engine); };

        // load initial demo rom
        std::vector<char> romData(sizeof(DEMO_ROM));
        std::copy((const char*)DEMO_ROM,(const char*)DEMO_ROM+sizeof(DEMO_ROM), &romData[0]);
        c8_state state = {};
        clearScreen();
        state.loadROM((const uint8_t*)romData.data(), romData.size());
        state.loadTypography(yac8::default_typography_buffer);

        // initialize debugging state
        bool is_noisemaker_testing = false;
        bool incompatible_flag = false;

        // setup timers
        using clock = std::chrono::high_resolution_clock;
        auto last_timer_tick = clock::now();

        // kick off emulation thread and start gameloop
        bool run = true;
        std::mutex state_mutex{};
        std::thread emuThread([&](){
                    emulationThread(state_mutex, &run, &incompatible_flag, *this, hardware_api, state);
                });

        while(run) {
            // start/stop audio as needed, depending on state.st
            if(!is_noisemaker_testing) {
                if (state.st != 0) {
                    noisemaker.play();
                } else {
                    noisemaker.stop();
                }
            }

            // handle timer register ticks on separate interval
            if(!debug_state.enabled || !debug_state.freezeTimers) {
                auto timeSinceLastTick = clock::now() - last_timer_tick;
                const auto tickInterval = std::chrono::milliseconds(1000 / 60);
                if (timeSinceLastTick > tickInterval) {
                    if (state.dt != 0)
                        state.dt -= 1;
                    if (state.st != 0)
                        state.st -= 1;
                    last_timer_tick = clock::now();
                }
            }

            // state that will determine whether to load/reset ROM at the end of this loop
            bool reset = false;
            bool loadRom = false;
            string romFilename;

            // handle SDL events for the emulation and ImGui
            ImGuiIO& io = ImGui::GetIO();
            if(!debug_state.paused)
                state.lastKey = yac8::NO_LAST_KEY;
            int wheel = 0;
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                int k = SDL_to_c8_key(e.key.keysym.sym);

                if (e.type == SDL_QUIT) {
                    run = false;
                } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    io.DisplaySize.x = static_cast<float>(e.window.data1);
                    io.DisplaySize.y = static_cast<float>(e.window.data2);
                } else if (e.type == SDL_MOUSEWHEEL) {
                    wheel = e.wheel.y;
                } else if(e.type == SDL_DROPFILE) {
                    loadRom = true;
                    romFilename = e.drop.file;
                } else if(e.type == SDL_KEYUP) {
                    if(k > 0) {
                        state.keyStates[k] = false;
                    }
                } else if(e.type == SDL_KEYDOWN) {
                    if(e.key.keysym.sym == SDLK_BACKSPACE) {
                        reset = true;
                    } else if(e.key.keysym.sym == SDLK_n) {
                        debug_state.step = true;
                    } else if(e.key.keysym.sym == SDLK_SPACE) {
                        debug_state.paused = !debug_state.paused;
                    } else if(k > 0) {
                        if(state.lastKey == yac8::NO_LAST_KEY) {
                            state.lastKey = k;
                        }
                        state.keyStates[k] = true;
                    }
                }
            }

            // Begin ImGui code
            {
                // handle imgui inputs
                int mouseX, mouseY;
                const int buttons = SDL_GetMouseState(&mouseX, &mouseY);
                io.DeltaTime = 1.0f / 60.0f;
                io.MousePos = ImVec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
                io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
                io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
                io.MouseWheel = static_cast<float>(wheel);

                // begin rendering menus / screen
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplSDL2_NewFrame(window);
                ImGui::NewFrame();

                if (ImGui::BeginMainMenuBar()) {
                    if (ImGui::BeginMenu("File")) {
                        if (ImGui::MenuItem("Open")) {
                            loadRom = true;
                            romFilename = openROM();
                        }
                        if (ImGui::MenuItem("Reset")) {
                            reset = true;
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Emulation")) {
                        ImGui::SliderInt("Processor Cycles / Sec", &processorSpeed, 1, MAX_SPEED);
                        ImGui::Checkbox("Load/Store Quirk", &quirks.loadStoreQuirk);
                        ImGui::Checkbox("Shift Quirk", &quirks.shiftQuirk);
                        ImGui::Checkbox("Wrapping", &quirks.wrap);
                        ImGui::Checkbox("Lock Processor Speed", &slowedProcessorSpeed);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip(
                                    "Use thread sleep to limit CPU usage.\nTurning this off will make the emulation thread run faster, but makes CPU usage go nuts.");
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Colors")) {
                        ImGui::ColorPicker3("Background Color", bgColor);
                        ImGui::ColorPicker3("Foreground Color", fgColor);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("CRT Screen")) {
                        ImGui::SliderFloat("Screen Curve X", &screenCurveX, 0.0f, 2.0f);
                        ImGui::SliderFloat("Screen Curve Y", &screenCurveY, 0.0f, 2.0f);
                        ImGui::SliderInt("Scan Line Frequency", &scanLineMult, 0, 1250);
                        ImGui::SliderFloat("Softness", &softness, 0.0f, 5.0f);
                        ImGui::SliderFloat("Screen Decay Factor", &screenDecayFactor, 0.0f, 0.9f);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Buzzer")) {
                        ImGui::SliderFloat("Volume", &noisemaker.volume, 0.0f, 3.0f);
                        ImGui::SliderFloat("Pitch", &noisemaker.pitch, 0.1f, 3.0f);
                        ImGui::Button("Test");
                        if (ImGui::IsItemActive()) {
                            is_noisemaker_testing = true;
                            noisemaker.play();
                        } else if (is_noisemaker_testing) {
                            is_noisemaker_testing = false;
                            noisemaker.stop();
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Debugger")) {
                        ImGui::Text("Debugger Controls:\n\t[N] Step\n\t[Spacebar] Pause/unpause");
                        if (ImGui::Button("Toggle Debugger")) {
                            debug_state.enabled = !debug_state.enabled;
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Help")) {
                        ImGui::Text(
                                "Game Controls:\n\t1234\n\tqwer\n\tasdf\n\tzxcv\nEmulator Controls:\n\t[Backspace] Reset\n\nYou can drag ROMs onto this window to load");
                        ImGui::Separator();
                        ImGui::Text("Created by Wes L, 2021");
                        ImGui::Text("systemvoidgames.com");
                        ImGui::EndMenu();
                    }
                    if (debug_state.paused) {
                        ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.5f, 1.0f}, " | Paused");
                    }
                    if (incompatible_flag) {
                        ImGui::TextColored(ImVec4{1.0f, 0.5f, 0.5f, 1.0f}, " | Possibly Incompatible ROM");
                    }
                    ImGui::EndMainMenuBar();
                }

                if (debug_state.enabled) {
                    if (ImGui::Begin("Debugger", &debug_state.enabled)) {
                        if (incompatible_flag) {
                            ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f},
                                               "Unrecognized instructions detected.\nThis emulator only supports the Cowgod-spec Chip8\nSuper-Chip8, XO-Chip, etc. are not supported.");
                        }
                        ImGui::Columns(2);
                        ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f}, "%s",
                                           (string("PC=") + print_register(state.pc)).c_str());
                        ImGui::TextColored(ImVec4{0.0f, 1.0f, 0.0f, 1.0f}, "%s",
                                           (string("SP=") + print_register(state.sp)).c_str());
                        ImGui::TextColored(ImVec4{1.0f, 0.0f, 0.0f, 1.0f}, "%s",
                                           (string("I =") + print_register(state.I)).c_str());
                        ImGui::TextColored(ImVec4{0.5f, 0.5f, 1.0f, 1.0f}, "%s",
                                           (string("DT=") + print_register(state.dt)).c_str());
                        ImGui::TextColored(ImVec4{0.5f, 0.5f, 1.0f, 1.0f}, "%s",
                                           (string("ST=") + print_register(state.st)).c_str());
                        ImGui::TextColored(ImVec4{1.0f, 1.0f, 0.0f, 1.0f}, "%s",
                                           (string("K =") + print_register(state.lastKey)).c_str());
                        ImGui::NextColumn();
                        for (int r = 0; r < V_REGISTERS_SIZE; r++) {
                            ImGui::Text("%s",
                                        (string("V") + std::to_string(r) + "=" + print_register(state.v[r])).c_str());
                        }

                        ImGui::Separator();
                        ImGui::Columns(1);

                        ImGui::Checkbox("Paused", &debug_state.paused);
                        ImGui::Checkbox("Freeze Timers", &debug_state.freezeTimers);
                        if (ImGui::Button("Break on next (JP, CALL, RET)")) {
                            debug_state.breakJP = true;
                            debug_state.breakDRW = false;
                            debug_state.breakLDK = false;
                            debug_state.breakSKP = false;
                            debug_state.paused = false;
                        }
                        if (ImGui::Button("Break on next Draw Call (DRW)")) {
                            debug_state.breakJP = false;
                            debug_state.breakDRW = true;
                            debug_state.breakLDK = false;
                            debug_state.breakSKP = false;
                            debug_state.paused = false;
                        }
                        if (ImGui::Button("Break on next Keypress Wait (LD Vx, K)")) {
                            debug_state.breakJP = false;
                            debug_state.breakDRW = false;
                            debug_state.breakLDK = true;
                            debug_state.breakSKP = false;
                            debug_state.paused = false;
                        }
                        if (ImGui::Button("Break on next Key Skip (SKP,SKNP)")) {
                            debug_state.breakJP = false;
                            debug_state.breakDRW = false;
                            debug_state.breakLDK = false;
                            debug_state.breakSKP = true;
                            debug_state.paused = false;
                        }
                        if (ImGui::Button("Step")) {
                            debug_state.step = true;
                        }
                        if (ImGui::Button("Clear Breakpoints")) {
                            std::fill(debug_state.breakPoints,
                                      debug_state.breakPoints + sizeof(debug_state.breakPoints), false);
                        }
                    }
                    ImGui::End();

                    ImGui::Begin("Instruction View");
                    for (int i = -10; i <= 11; ++i) {
                        int instNum = state.pc + i * 2;
                        if (instNum < PROGRAM_OFFSET || instNum >= RAM_SIZE) {
                            ImGui::Text("???");
                        } else {
                            bool isPC = instNum == state.pc;
                            if (isPC) {
                                ImGui::PushStyleColor(0, ImVec4{0.0f, 1.0f, 0.0f, 1.0f});
                            }
                            string instruction = print_instruction(instNum, state, quirks);
                            ImGui::Selectable(instruction.c_str(), &debug_state.breakPoints[instNum - PROGRAM_OFFSET]);
                            if (isPC) {
                                ImGui::PopStyleColor();
                            }
                        }
                    }
                    ImGui::End();
                }
            }

            // rendering
            {
                glClearColor(1.0f,0.0f,1.0f,1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                glBindTexture(GL_TEXTURE_2D, screenBufferTex);

                // simulate phosphorescent display
                for(int i = 0; i < sizeof(decayingPixelBuffer); i++) {
                    if(pixels[i]) decayingPixelBuffer[i] = 255;
                    else {
                        decayingPixelBuffer[i] *= screenDecayFactor;
                    }
                }

                // expensive operation: copying pixel buffer from cpu to gpu
                glTexSubImage2D(
                        GL_TEXTURE_2D, 0, 0, 0,
                        WINDOW_WIDTH, WINDOW_HEIGHT,
                        GL_RED, GL_UNSIGNED_BYTE, decayingPixelBuffer);

                glBindTexture(GL_TEXTURE_2D, screenBufferTex);
                glActiveTexture(GL_TEXTURE0);

                glUniform4f(glGetUniformLocation(crtShaderProgram, "background"),bgColor[0],bgColor[1],bgColor[2],1.0f);
                glUniform4f(glGetUniformLocation(crtShaderProgram, "foreground"),fgColor[0],fgColor[1],fgColor[2],1.0f);
                glUniform1f(glGetUniformLocation(crtShaderProgram, "CRT_CURVE_AMNTx"),screenCurveX);
                glUniform1f(glGetUniformLocation(crtShaderProgram, "CRT_CURVE_AMNTy"),screenCurveY);
                glUniform1f(glGetUniformLocation(crtShaderProgram, "SCAN_LINE_MULT"),(float)scanLineMult);
                glUniform1f(glGetUniformLocation(crtShaderProgram, "e"), softness / 10000.0f);

                glViewport(0,0,WINDOW_WIDTH*scale,WINDOW_HEIGHT*scale);

                glUseProgram(crtShaderProgram);
                glDrawArrays(GL_TRIANGLES, 0, 3);

                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                SDL_GL_SwapWindow(window);
                SDL_Delay(1000/120);
            }

            if(loadRom) {
                std::ifstream is(romFilename, std::ios::in|std::ios::binary|std::ios::ate);
                if(is.is_open()) {
                    int size = is.tellg();
                    romData.resize(size);
                    is.seekg(0, std::ios::beg);
                    is.read(&romData[0], size);
                    is.close();

                    // remove breakpoints
                    std::fill(debug_state.breakPoints, debug_state.breakPoints+sizeof(debug_state.breakPoints), false);

                    // remove incompatibility flag
                    incompatible_flag = false;

                    reset = true;
                }
            }

            if(reset) {
                state_mutex.lock();
                state = {};
                clearScreen();
                state.loadROM((const uint8_t *) romData.data(), romData.size());
                state.loadTypography(yac8::default_typography_buffer);
                state_mutex.unlock();
            }
        }

        // wait for emu thread to get the memo
        emuThread.join();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        SDL_DestroyWindow(window);
    }

    void c8_emulator::clearScreen() {
        std::fill(pixels, pixels + WINDOW_WIDTH * WINDOW_HEIGHT, false);
    }

    void c8_emulator::drawSprite(const uint8_t *sprite, uint8_t x, uint8_t y, uint8_t n, uint8_t &VF) {
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

                    bool &bufferPix = pixels[py*WINDOW_WIDTH+px];
                    uint8_t previous = bufferPix;
                    bufferPix ^= value;
                    if(bufferPix == 0 && previous != 0)
                        VF = 1;
                }
            }
        }
    }
}