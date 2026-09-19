CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror

.PHONY: test clean

test: test_m68k
	./test_m68k

test_m68k: test_m68k.c ../../_lib/emu/emu_m68k.h ../../_lib/emu/emu_m68k.c
	$(CC) $(CFLAGS) test_m68k.c -o $@

clean:
	rm -f test_m68k
