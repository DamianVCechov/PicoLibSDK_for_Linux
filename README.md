# PicoLibSDK for Linux 

## Description

Effort for better integration under the Linux system (debugged under Debian/Ubuntu distributions) PicoLibSDK - Alternative SDK library for Raspberry Pico, RP2040 and RP2350
A fork of the full version of PicoLibSDK is here in a branch called Original-fork. That is, with compilation scripts for Windows and ready-made binaries for SD cards
or direct upload to Rpi Pico. And of course, you can find the latest version of this SDK with complete documentation from the author of the entire Panda38 project: 
https://github.com/Panda381/PicoLibSDK

## Usage

1) git clone https://github.com/DamianVCechov/PicoLibSDK_for_Linux.git

You can also insert the necessary aliases manually at the end of the file ~/.bashrc or ~/.profilerc.
These aliases will make your translation work easier.

```
alias c='bash c.sh'
alias d='bash d.sh'
alias e='bash e.sh'
alias ce='bash ce.sh'
```

## My project for PicoLibSDK

Implementation of Picocalc Clockwork. 320×320 display driver and ST7365p controller, I₂C keyboard. Most of my test programs are primarily tuned for Pico2 RP2350,
but many of them will work on Pico RP2040 as well.
I have modified the vast majority of the original programs for Picopad. My programs and tests and experimentation are in the DAMNGAME directory.

DAMNGAME/CHESS    - Modified and Extended Version of Chess Game by Panda38
DAMNGAME/CONWAY   - Conway's Game of Life. I tried to optimize for the hardware as much as possible.
DAMNGANE/GBCHEAT  - Enhanced Panda38's GameBoy emulator about entering GameGenie cheats
DAMNGAME/IMPACT   - Simple Space Impact Pattern Game for Nokia. (Unfinished)
DAMNGAME/PAINT    - My first attempt at the program. Very simple painting.
DAMNGAME/QR-CODES - Program generating a QR code from the entered text
DAMNGAME/SINC     - 3D projection of the cardinal sinus graph
EMU/A2600         - My attempt at an Atari 2600 emulator. (Incompleted, Unfinished)
EMU/TI92          - Texas Instruments TI-92 calculator emulator (not Plus!) with M68000 processor. 
                    Fully functional. (not tested on RP2040) Upload the ROM of the calculator to 
                    the root of the SD card with the name ti92.bin. I'm not allowed to share a ROM.
ZXSPEC/ZXSPEC     - Modification and optimization of the ZX Spectra emulator from https://github.com/tmilata/ZXSpect

The screen backlight control does not work in the loader, because Picocalc Clockwork does not control it from the Rpi Pico pin, but from the Atmel processor controlling the keyboard.

## Contributing

Information on how to contribute to the project.

## License

Project PicoLibSDK_for_Linux follows the same licensing terms as the original PicoLibSDK.

Damian V. Čechov
admin@damianvcechov.cz
