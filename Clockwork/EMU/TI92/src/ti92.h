#ifndef _TI92_H
#define _TI92_H

// Keep this header self-contained. Some PicoLibSDK device include sets do not
// expose the optional CPU headers through INCLUDES_H.
#include "../../../../_lib/emu/emu_m68k.h"

#define TI92_RAM_SIZE_I     (128*1024)
#define TI92_RAM_SIZE_II    (256*1024)
#define TI92_RAM_SIZE       TI92_RAM_SIZE_II
#define TI92_ROM_SIZE_I     (1024*1024)
#define TI92_ROM_SIZE_II    (2*1024*1024)
#define TI92_LCD_WIDTH      240
#define TI92_LCD_HEIGHT     128

// Per-instruction history is useful in special diagnostic builds, but costs
// measurable time in this interpreter's hottest loop.
#define TI92_CPU_TRACE      0

typedef struct {
	sM68K cpu;
	u8 ram[TI92_RAM_SIZE];
	u32 ram_size;
	u8 io[32];
	u8 keys[10];		// pressed columns per TI-92 keyboard row
	const u8* rom;		// XIP address of cached ROM
	u32 rom_size;
	u32 rom_base;		// emulated address (usually 0x200000 or 0x400000)
	u32 lcd_addr;
	u16 lcd_width;
	u16 lcd_height;
	volatile Bool lcd_dirty;
	u64 hw_cycles;
	u32 timer_ticks;
	u8 timer_value;
	u8 pending_irqs;
	volatile Bool key_irq;
	volatile Bool exit_requested;
	u32 trace_pc[8];
	u16 trace_ir[8];
	u8 trace_pos;
} sTI92;

extern sTI92 TI92;

Bool TI92_LoadRom(const char* path);
void TI92_ShowProgress(const char* status, int percent);
void TI92_Init();
eM68KStep TI92_Run(int instructions);
u16 TI92_DebugRead16(u32 addr);
void TI92_DrawLCD();
void TI92_KeyEvent(u8 status, char ch);

#endif // _TI92_H
