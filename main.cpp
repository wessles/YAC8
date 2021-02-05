#include <iostream>
#include <string>
#include <fstream>
#include <assert.h>
#include <random>
#include <stdint.h>
#include <array>

using std::cout;
using std::endl;
using std::cerr;
using std::string;

#include "c8_context.hpp"
#include "c8_window_sdl2.hpp"

int main(int argc, char *argv[])
{
    if(argc != 2) {
        cerr << "No ROM specified." << endl;
        return 1;
    }

    std::vector<uint16_t> program = {
         0xF10A, // 0200 v1 = keypress
         0xE100, // 0204 print v2
         0xf129, // store I = location of character @V1
//         0x00e0,
         0xD345, // draw sprite
         0x7301,
         0x7402,
         0xF115,
         0x1200, // 0206 jump back to key query
    };

    // endianize it
    for(uint16_t & k : program) {
        uint16_t temp = k;
        k = (temp >> 8) | (temp << 8);
    }

    const char* filename = argv[1];
    std::ifstream instream(filename, std::ios::in | std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(instream)), std::istreambuf_iterator<char>());

    c8_window_sdl2 window(8);

    c8_context c8{};

    c8.loadROM((const uint8_t*)data.data(), data.size());
//    c8.loadROM((const uint8_t*)program.data(), program.size() * sizeof(uint16_t));
    c8.run(window);
    return 0;
}