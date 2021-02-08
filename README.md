# YAC8
**YAC8** stands for *"Yet Another Chip-8" Emulator"*. It is an **SDL2** + **OpenGL 3.2** CHIP-8 emulator / debugger, written in **C++11**. It is based on [Cowgod's Chip-8 Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM), and does not support SCHIP-8 instructions.

![Vanity Demo ROM](https://i.imgur.com/vWzYZOY.png)

## Debugger
YAC8 features a full debugging suite, which allows for setting breakpoints on individual instruction addresses, as well as on certain instruction types.

![YAC8 Debugger](https://i.imgur.com/OJivsR8.png)
## CRT Simulation
This emulator includes a CRT screen shader, with customizable warping, scan-lines and ghosting.

![CRT Settings](https://i.imgur.com/oIeHIH2.png)

## Multithreaded Emulation
The simulated Chip-8 processor runs on a separate thread, allowing for high speed emulation. A low-CPU usage mode is enabled by default, which sleeps the thread periodically to avoid hogging CPU time, but this can be disabled for truly ludicrous speeds.

![Emulation Settings](https://i.imgur.com/mL4ecxj.png)

## Quirks
A definitive specification for Chip8 was never really made, so many Chip-8 implementations over the years have made different assumptions about certain instructions. These "quirks" are toggleable through emulation settings:

* **Load/Store Quirk** - Originally the instructions `Fx55` and `Fx65` increment the `I` register by `x+1` when they are done. Some ROMs think that this doesn't happen, so it is a toggleable quirk. For example, `TICTACTOE` or `INVADERS`.
* **Shift Quirk** - Originally the instructions `8xy6` and `8xyE` set `Vx` to the value of `Vy << 1` or `Vy >> 1`. Some ROMS think that it's instead `Vx = Vx << 1` or `Vx = Vx >> 1`. For example, `TICTACTOE` or `INVADERS`.
* **Partial Wrapping Quirk** - Originally the draw instruction only wrapped sprites when their rendering position passed the screen boundary. Cowgod's reference says that the sprites will also partially wrap around; this quirk breaks the game `BLITZ`.

I got most of this info from [Faizilham](https://faizilham.github.io/revisiting-chip8); big thanks.
