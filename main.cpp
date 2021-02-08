#include <iostream>
#include <string>
#include <fstream>
#include <assert.h>
#include <random>
#include <stdint.h>
#include <iomanip>
#include <array>

using std::cout;
using std::endl;
using std::cerr;
using std::string;

#include "c8_constants.hpp"
#include "c8_state.hpp"
#include "c8_emulator.hpp"

using namespace yac8;

int main()
{
    c8_emulator emu;
    emu.run();

    return 0;
}