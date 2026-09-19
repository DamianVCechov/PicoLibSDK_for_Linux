# ASM source files
ASRC +=

# C source files
CSRC +=

# C++ source files (PicoLibSDK device entry point is main.cpp)
SRC += src/main.cpp
SRC += src/ti92.cpp

PICO_ROOT_PATH = ../../..

include ${PICO_ROOT_PATH}/Makefile.inc

# This emulator is CPU-bound. Override the SDK's size-oriented -Os locally;
# LTO also lets the compiler optimize across the CPU core and TI-92 bus code.
# These flags affect only objects in this project's build directory.
CFLAGS += -O3 -flto
CPPFLAGS += -O3 -flto
LDFLAGS += -flto
