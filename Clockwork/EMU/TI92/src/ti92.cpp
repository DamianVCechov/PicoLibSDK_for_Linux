#include "../include.h"

sTI92 TI92;
u8 ALIGNED TI92RomPage[4096];

#define TI92_HOST_ALT       0x01
#define TI92_HOST_SHIFT_L   0x02
#define TI92_HOST_SHIFT_R   0x04
#define TI92_HOST_SYM       0x08
#define TI92_HOST_CTRL      0x10
#define TI92_HOST_CAPS      0x20

static u8 TI92HostModifiers;
static Bool TI92TempShift;
static Bool TI92Temp2nd;
static Bool TI92SuppressShift;
static Bool TI92SqrtChord;
static u32 TI92OpcodeCacheAddr = 0x1000000;
static u16 TI92OpcodeCache;

static u32 TI92_RomLong(u32 off)
{
	return ((u32)TI92.rom[off] << 24) | ((u32)TI92.rom[off + 1] << 16) |
		((u32)TI92.rom[off + 2] << 8) | TI92.rom[off + 3];
}

void TI92_ShowProgress(const char* status, int percent)
{
	char value[8];
	int fill;
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	fill = 264 * percent / 100;

	DrawClear();
	DrawText2("TI-92 Emulator", 48, 88, COL_WHITE);
	DrawText(status, (WIDTH - StrLen(status)*8)/2, 140, COL_GRAY);
	DrawFrameW(22, 166, 276, 32, COL_WHITE, 2);
	DrawRect(28, 172, 264, 20, COL_BLACK);
	if (fill > 0) DrawRect(28, 172, fill, 20, COL_GREEN);
	MemPrint(value, sizeof(value), "%d%%", percent);
	DrawText2(value, (WIDTH - StrLen(value)*16)/2, 214, COL_YELLOW);
	DispUpdate();
}

// Compare the SD image with the cached XIP image to avoid wearing Flash.
static Bool TI92_RomIsCached(sFile* file, u32 size)
{
	u32 off;
	int percent, shown = -1;
	for (off = 0; off < size; off += sizeof(TI92RomPage))
	{
		percent = 5 + (int)(((u64)off * 35) / size);
		if (percent != shown)
		{
			TI92_ShowProgress("Checking ROM cache", percent);
			shown = percent;
		}
		if (FileRead(file, TI92RomPage, sizeof(TI92RomPage)) != sizeof(TI92RomPage))
			return False;
		if (memcmp(TI92RomPage, TI92_ROM_XIP_BASE + off, sizeof(TI92RomPage)) != 0)
			return False;
	}
	TI92_ShowProgress("Checking ROM cache", 40);
	return True;
}

Bool TI92_LoadRom(const char* path)
{
	sFile file;
	u32 size, off, flashoff;
	int percent, shown;
	Bool cached;

	TI92_ShowProgress("Mounting SD card", 1);
	if (!DiskAutoMount()) return False;
	TI92_ShowProgress("Opening ti92.bin", 3);
	if (!FileOpen(&file, path)) return False;
	size = FileSize(&file);
	if ((size != TI92_ROM_SIZE_I) && (size != TI92_ROM_SIZE_II))
	{
		FileClose(&file);
		return False;
	}
	if ((u32)TI92_ROM_XIP_BASE + size > XIP_BASE + FLASHSIZE - FLASH_SECTOR_SIZE)
	{
		FileClose(&file);
		return False;
	}

	cached = TI92_RomIsCached(&file, size);
	if (!cached)
	{
		TI92_ShowProgress("Erasing ROM cache", 42);
		FileSeek(&file, 0);
		flashoff = (u32)TI92_ROM_XIP_BASE - XIP_BASE;
		FlashErase(flashoff, size);
		shown = -1;
		for (off = 0; off < size; off += sizeof(TI92RomPage))
		{
			if (FileRead(&file, TI92RomPage, sizeof(TI92RomPage)) != sizeof(TI92RomPage))
			{
				FileClose(&file);
				return False;
			}
			FlashProgram(flashoff + off, TI92RomPage, sizeof(TI92RomPage));
			percent = 45 +
				(int)(((u64)(off + sizeof(TI92RomPage)) * 45) / size);
			if (percent != shown)
			{
				TI92_ShowProgress("Caching ti92.bin", percent);
				shown = percent;
			}
		}
	}
	else
		TI92_ShowProgress("ROM cache ready", 90);

	FileClose(&file);
	TI92.rom = TI92_ROM_XIP_BASE;
	TI92.rom_size = size;
	TI92.rom_base = ((u32)TI92.rom[5] & 0xf0) << 16;
	if ((TI92.rom_base != 0x200000) && (TI92.rom_base != 0x400000)) return False;
	return True;
}

static u8 TI92_ReadIO(u32 addr)
{
	u8 reg = addr & 31;
	u8 value = TI92.io[reg];
	if ((reg >= 6) && (reg <= 11)) return 0x14;
	if ((reg == 0x10) || (reg == 0x11) || (reg == 0x12) || (reg == 0x13) ||
		(reg == 0x1e) || (reg == 0x1f)) return 0x14;
	if (reg == 0x00) return (value & 0x20) | 4; // contrast bit and good battery
	if (reg == 0x17) return TI92.timer_value;
	if (reg == 0x1a) return value | 2;          // ON key released
	if (reg == 0x1b)
	{
		u16 mask = ((u16)TI92.io[0x18] << 8) | TI92.io[0x19];
		u8 pressed = 0;
		int row;
		for (row = 0; row < 10; row++)
			if ((mask & (1 << row)) == 0) pressed |= TI92.keys[row];
		return ~pressed;
	}
	return value;
}

static void TI92_WriteIO(u32 addr, u8 data)
{
	u8 reg = addr & 31;
	TI92.io[reg] = data;
	switch (reg)
	{
	case 0x01:
		// HW1 bit 0 selects normal 128 KiB or interleaved 256 KiB RAM.
		TI92.ram_size = (data & 1) ? TI92_RAM_SIZE_I : TI92_RAM_SIZE_II;
		break;
	case 0x10:
	case 0x11:
		TI92.lcd_addr = (((u32)TI92.io[0x10] << 8) | TI92.io[0x11]) << 3;
		TI92.lcd_dirty = True;
		break;
	case 0x12:
		TI92.lcd_width = (64 - data) * 16;
		TI92.lcd_dirty = True;
		break;
	case 0x13:
		TI92.lcd_height = 0x100 - data;
		TI92.lcd_dirty = True;
		break;
	case 0x17:
		TI92.timer_value = data;
		break;
	default:
		break;
	}
}

static u8 TI92_Read8(u32 addr)
{
	u32 delta;
	addr &= 0xffffff;
	delta = addr - TI92OpcodeCacheAddr;
	if (delta < 2)
		return (delta == 0) ? (u8)(TI92OpcodeCache >> 8) : (u8)TI92OpcodeCache;
	if (addr < 0x200000) return TI92.ram[addr & (TI92.ram_size - 1)];
	if ((addr >= TI92.rom_base) && (addr < TI92.rom_base + 0x200000))
		return TI92.rom[(addr - TI92.rom_base) & (TI92.rom_size - 1)];
	if ((addr >= 0x600000) && (addr < 0x700000)) return TI92_ReadIO(addr);
	return 0x14;
}

static void TI92_Write8(u32 addr, u8 data)
{
	addr &= 0xffffff;
	if (addr - TI92OpcodeCacheAddr < 2) TI92OpcodeCacheAddr = 0x1000000;
	if (addr < 0x200000)
	{
		u32 physical = addr & (TI92.ram_size - 1);
		u32 lcd = TI92.lcd_addr & (TI92.ram_size - 1);
		TI92.ram[physical] = data;
		// The TI-92 LCD is 240*128/8 = 3840 bytes. The normal framebuffer
		// does not wrap at the end of RAM, but retain a wrap-safe check.
		if (((physical - lcd) & (TI92.ram_size - 1)) <
			(TI92_LCD_WIDTH * TI92_LCD_HEIGHT / 8)) TI92.lcd_dirty = True;
	}
	else if ((addr >= 0x600000) && (addr < 0x700000))
		TI92_WriteIO(addr, data);
}

static u32 TI92_Read32(u32 addr)
{
	return ((u32)TI92_Read8(addr) << 24) | ((u32)TI92_Read8(addr + 1) << 16) |
		((u32)TI92_Read8(addr + 2) << 8) | TI92_Read8(addr + 3);
}

static void TI92_Write16(u32 addr, u16 data)
{
	TI92_Write8(addr, (u8)(data >> 8));
	TI92_Write8(addr + 1, (u8)data);
}

static void TI92_Write32(u32 addr, u32 data)
{
	TI92_Write16(addr, (u16)(data >> 16));
	TI92_Write16(addr + 2, (u16)data);
}

static u32 TI92_ReadSized(u32 addr, int size)
{
	if (size == 1) return TI92_Read8(addr);
	if (size == 2) return TI92_DebugRead16(addr);
	return TI92_Read32(addr);
}

static void TI92_WriteSized(u32 addr, int size, u32 data)
{
	if (size == 1) TI92_Write8(addr, (u8)data);
	else if (size == 2) TI92_Write16(addr, (u16)data);
	else TI92_Write32(addr, data);
}

// The shared compact 68000 core does not decode ADDX/SUBX. Without handling
// them here their encodings fall through to ordinary ADD/SUB, silently losing
// the extend bit and cumulative Z flag. AMS uses these instructions for its
// multi-precision arithmetic, most visibly while evaluating and graphing sin().
static Bool TI92_AddSubX(u16 op)
{
	sM68K* cpu = &TI92.cpu;
	int sizecode, size, src_reg, dst_reg;
	u32 mask, sign, src, dst, result;
	u64 full;
	Bool subtract, carry, overflow;

	if (((op & 0xf130) != 0x9100) && ((op & 0xf130) != 0xd100)) return False;
	sizecode = (op >> 6) & 3;
	if (sizecode == 3) return False;
	size = (sizecode == 0) ? 1 : ((sizecode == 1) ? 2 : 4);
	src_reg = op & 7;
	dst_reg = (op >> 9) & 7;
	subtract = (op & 0x4000) == 0;
	cpu->ir = op;
	mask = (size == 1) ? 0xff : ((size == 2) ? 0xffff : 0xffffffff);
	sign = (size == 1) ? 0x80 : ((size == 2) ? 0x8000 : 0x80000000);

	if (op & 8) // -(Ay),-(Ax); A7 byte accesses still move by two bytes
	{
		u32 dec = (size == 1 && src_reg == 7) ? 2 : size;
		cpu->a[src_reg] -= dec;
		src = TI92_ReadSized(cpu->a[src_reg], size);
		dec = (size == 1 && dst_reg == 7) ? 2 : size;
		cpu->a[dst_reg] -= dec;
		dst = TI92_ReadSized(cpu->a[dst_reg], size);
	}
	else
	{
		src = cpu->d[src_reg] & mask;
		dst = cpu->d[dst_reg] & mask;
	}

	if (subtract)
	{
		u64 subtrahend = (u64)src + ((cpu->sr & M68K_SR_X) ? 1 : 0);
		result = (dst - (u32)subtrahend) & mask;
		carry = subtrahend > (u64)dst;
		overflow = (((dst ^ src) & (dst ^ result) & sign) != 0);
	}
	else
	{
		full = (u64)dst + (u64)src + ((cpu->sr & M68K_SR_X) ? 1 : 0);
		result = (u32)full & mask;
		carry = full > (u64)mask;
		overflow = (((~(dst ^ src)) & (dst ^ result) & sign) != 0);
	}

	// ADDX/SUBX leave Z set across a zero limb and clear it on any nonzero limb.
	cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C | M68K_SR_X);
	if (result != 0) cpu->sr &= ~M68K_SR_Z;
	if (result & sign) cpu->sr |= M68K_SR_N;
	if (overflow) cpu->sr |= M68K_SR_V;
	if (carry) cpu->sr |= M68K_SR_C | M68K_SR_X;

	if (op & 8)
		TI92_WriteSized(cpu->a[dst_reg], size, result);
	else
		cpu->d[dst_reg] = (cpu->d[dst_reg] & ~mask) | result;
	cpu->pc += 2;
	cpu->cycles += (op & 8) ? ((size == 4) ? 30 : 18) : ((size == 4) ? 8 : 4);
	return True;
}

// Standard six-byte 68000 exception frame. Line-A/Line-F instructions are
// exceptions, not illegal opcodes; their saved PC points at the opcode so the
// TI-92 handler can decode its low 12 bits.
static eM68KStep TI92_LineException(int vector)
{
	sM68K* cpu = &TI92.cpu;
	u16 old_sr = cpu->sr;
	if ((old_sr & M68K_SR_S) == 0)
	{
		cpu->usp = cpu->a[7];
		cpu->a[7] = cpu->ssp;
	}
	cpu->sr = (old_sr | M68K_SR_S) & ~M68K_SR_T;
	cpu->stopped = False;
	cpu->a[7] -= 4;
	TI92_Write32(cpu->a[7], cpu->pc);
	cpu->a[7] -= 2;
	TI92_Write16(cpu->a[7], old_sr);
	cpu->pc = TI92_Read32((u32)vector * 4);
	cpu->cycles += 34;
	return M68K_STEP_OK;
}

void TI92_Init()
{
	u32 initial_ssp, initial_pc;
	MemSet(TI92.ram, 0, sizeof(TI92.ram));
	MemSet(TI92.io, 0, sizeof(TI92.io));
	MemSet(TI92.keys, 0, sizeof(TI92.keys));
	TI92.io[0x0d] = 0x40;
	TI92.lcd_addr = 0;
	TI92.lcd_width = TI92_LCD_WIDTH;
	TI92.lcd_height = TI92_LCD_HEIGHT;
	TI92.lcd_dirty = True;
	TI92.hw_cycles = 0;
	TI92.timer_ticks = 0;
	TI92.timer_value = 0;
	TI92.pending_irqs = 0;
	TI92.ram_size = (TI92.rom_size == TI92_ROM_SIZE_II) ?
		TI92_RAM_SIZE_II : TI92_RAM_SIZE_I;
	TI92.key_irq = False;
	TI92.exit_requested = False;
	TI92HostModifiers = 0;
	TI92TempShift = False;
	TI92Temp2nd = False;
	TI92SuppressShift = False;
	TI92SqrtChord = False;
	TI92OpcodeCacheAddr = 0x1000000;
#if TI92_CPU_TRACE
	MemSet(TI92.trace_pc, 0, sizeof(TI92.trace_pc));
	MemSet(TI92.trace_ir, 0, sizeof(TI92.trace_ir));
	TI92.trace_pos = 0;
#endif
	M68K_Init(&TI92.cpu, TI92_Read8, TI92_Write8);
	initial_ssp = TI92_RomLong(0);
	initial_pc = TI92_RomLong(4);
	// Older revisions of the standalone M68K core only expose M68K_Reset(),
	// which obtains both reset vectors through the bus callbacks. Temporarily
	// mirror the ROM vectors at address zero, reset the CPU, then restore RAM.
	TI92.ram[0] = (u8)(initial_ssp >> 24);
	TI92.ram[1] = (u8)(initial_ssp >> 16);
	TI92.ram[2] = (u8)(initial_ssp >> 8);
	TI92.ram[3] = (u8)initial_ssp;
	TI92.ram[4] = (u8)(initial_pc >> 24);
	TI92.ram[5] = (u8)(initial_pc >> 16);
	TI92.ram[6] = (u8)(initial_pc >> 8);
	TI92.ram[7] = (u8)initial_pc;
	M68K_Reset(&TI92.cpu);
	MemSet(TI92.ram, 0, 8);
	KeySetRawCallback(TI92_KeyEvent);
}

// Advance the HW1 OSC2 domain. TiEmu models one hardware tick per 427 CPU
// cycles for a 10 MHz TI-92. Ports $600015/$600017 control the timer.
static void TI92_UpdateHardware()
{
	static const u16 timer_masks[4] = { 0, 15, 127, 8191 };
	u64 cycles = TI92.cpu.cycles;
	while (cycles - TI92.hw_cycles >= 427)
	{
		u16 mask;
		TI92.hw_cycles += 427;
		TI92.timer_ticks++;
		if ((TI92.io[0x15] & 2) == 0) continue; // OSC2 stopped
		mask = timer_masks[(TI92.io[0x15] >> 4) & 3];
		if (((TI92.timer_ticks & 63) == 0) && ((TI92.io[0x15] & 0x80) == 0))
			TI92.pending_irqs |= 1 << 1;
		if (((TI92.timer_ticks & 16383) == 0) && (TI92.io[0x15] & 4) &&
			((TI92.io[0x15] & 0x80) == 0))
			TI92.pending_irqs |= 1 << 3;
		if (((TI92.timer_ticks & mask) == 0) && (TI92.io[0x15] & 8))
		{
			if (TI92.timer_value == 0)
				TI92.timer_value = TI92.io[0x17];
			else
				TI92.timer_value++;
			if ((TI92.timer_value == 0) && ((TI92.io[0x15] & 0x80) == 0))
				TI92.pending_irqs |= 1 << 5;
		}
	}
}

static void TI92_ProcessInterrupts()
{
	int level;
	for (level = 7; level >= 1; level--)
		if ((TI92.pending_irqs & (1 << level)) && M68K_Interrupt(&TI92.cpu, level))
		{
			TI92.pending_irqs &= ~(1 << level);
			break;
		}
}

// The dispatcher must inspect opcodes that the shared core does not implement
// (Line-A/Line-F and ADDX/SUBX). Avoid running the full byte-oriented bus
// decoder twice for the overwhelmingly common RAM and ROM fetches.
static inline __attribute__((always_inline)) u16 TI92_FastRead16(u32 addr)
{
	u32 off;
	addr &= 0xffffff;
	if (addr < 0x200000)
	{
		off = addr & (TI92.ram_size - 1);
		if (off != TI92.ram_size - 1)
			return ((u16)TI92.ram[off] << 8) | TI92.ram[off + 1];
	}
	else if ((addr >= TI92.rom_base) && (addr < TI92.rom_base + 0x200000))
	{
		off = (addr - TI92.rom_base) & (TI92.rom_size - 1);
		if (off != TI92.rom_size - 1)
			return ((u16)TI92.rom[off] << 8) | TI92.rom[off + 1];
	}
	return TI92_DebugRead16(addr);
}

eM68KStep TI92_Run(int instructions)
{
	int i;
	(void)KeyGetRel(); // pump the PicoCalc I2C keyboard and raw callback
	if (TI92.exit_requested) return M68K_STEP_OK;
	if (TI92.key_irq)
	{
		TI92.key_irq = False;
		TI92.pending_irqs |= 1 << 2;
	}
	for (i = 0; i < instructions; i++)
	{
		u16 op;
		eM68KStep result;
		if (TI92.cpu.cycles - TI92.hw_cycles >= 427) TI92_UpdateHardware();
		if (TI92.pending_irqs != 0) TI92_ProcessInterrupts();
#if TI92_CPU_TRACE
		u32 pc = TI92.cpu.pc;
#endif
		if (TI92.cpu.stopped)
		{
			TI92.cpu.cycles += 4;
			result = M68K_STEP_STOPPED;
		}
		else
		{
			op = TI92_FastRead16(TI92.cpu.pc);
			if (TI92_AddSubX(op))
				result = M68K_STEP_OK;
			else if ((op & 0xf000) == 0xa000)
			{
				TI92.cpu.ir = op;
				result = TI92_LineException(10);
			}
			else if ((op & 0xf000) == 0xf000)
			{
				TI92.cpu.ir = op;
				result = TI92_LineException(11);
			}
			else
			{
				// Reuse the opcode already inspected by the local dispatcher when
				// the shared core performs its byte-wise instruction fetch.
				TI92OpcodeCache = op;
				TI92OpcodeCacheAddr = TI92.cpu.pc & 0xffffff;
				result = M68K_Step(&TI92.cpu);
				TI92OpcodeCacheAddr = 0x1000000;
			}
		}
#if TI92_CPU_TRACE
		TI92.trace_pc[TI92.trace_pos] = pc;
		TI92.trace_ir[TI92.trace_pos] = TI92.cpu.ir;
		TI92.trace_pos = (TI92.trace_pos + 1) & 7;
#endif
		if ((result == M68K_STEP_ADDRESS_ERROR) || (result == M68K_STEP_ILLEGAL))
			return result;
	}
	return M68K_STEP_OK;
}

u16 TI92_DebugRead16(u32 addr)
{
	return ((u16)TI92_Read8(addr) << 8) | TI92_Read8(addr + 1);
}

void TI92_DrawLCD()
{
	int x, y;
	int width = TI92.lcd_width;
	int height = TI92.lcd_height;
	int pitch;
	u32 addr;
	u16 black = COL_BLACK;
	u16 white = COL_WHITE;
	if ((width <= 0) || (width > 1024)) width = TI92_LCD_WIDTH;
	if ((height <= 0) || (height > 256)) height = TI92_LCD_HEIGHT;
	pitch = width >> 3;
	addr = TI92.lcd_addr;
	for (y = 0; y < TI92_LCD_HEIGHT; y++)
	{
		u16* dst = &FrameBuf[(96 + y)*WIDTH + 40];
		for (x = 0; x < TI92_LCD_WIDTH; x += 8)
		{
			u8 pixels = 0;
			int bit;
			if ((y < height) && (x < width))
				pixels = TI92.ram[(addr + y*pitch + (x >> 3)) & (TI92.ram_size - 1)];
			for (bit = 0; bit < 8; bit++)
				dst[x + bit] = (pixels & (0x80 >> bit)) ? black : white;
		}
	}
	// Direct FrameBuf writes bypass the drawing API. Explicitly mark the LCD
	// rectangle dirty or DispUpdate() only transfers regions touched by Draw*.
	DispDirtyRect(40, 96, TI92_LCD_WIDTH, TI92_LCD_HEIGHT);
	DispUpdate();
	TI92.lcd_dirty = False;
}

static void TI92_SetKey(int row, int col, Bool down)
{
	u8 mask = 0x80 >> col;
	if (down)
	{
		if ((TI92.keys[row] & mask) == 0) TI92.key_irq = True;
		TI92.keys[row] |= mask;
	}
	else
		TI92.keys[row] &= ~mask;
}

static void TI92_UpdateModifiers()
{
	Bool shift = ((TI92HostModifiers & (TI92_HOST_SHIFT_L | TI92_HOST_SHIFT_R)) != 0) ||
		TI92TempShift;
	Bool second = ((TI92HostModifiers & (TI92_HOST_ALT | TI92_HOST_SYM)) != 0) ||
		TI92Temp2nd;
	TI92_SetKey(0, 5, shift && !TI92SuppressShift); // Shift
	TI92_SetKey(0, 6, (TI92HostModifiers & TI92_HOST_CTRL) != 0); // Diamond
	TI92_SetKey(0, 7, second); // 2nd
	TI92_SetKey(0, 4, (TI92HostModifiers & TI92_HOST_CAPS) != 0); // Hand
}

static Bool TI92_SetHostModifier(u8 ch, Bool down)
{
	u8 flag;
	switch (ch)
	{
	case 0xa1: flag = TI92_HOST_ALT; break;
	case 0xa2: flag = TI92_HOST_SHIFT_L; break;
	case 0xa3: flag = TI92_HOST_SHIFT_R; break;
	case 0xa4: flag = TI92_HOST_SYM; break;
	case 0xa5: flag = TI92_HOST_CTRL; break;
	case 0xc1: flag = TI92_HOST_CAPS; break;
	default: return False;
	}
	if (down) TI92HostModifiers |= flag;
	else TI92HostModifiers &= ~flag;
	TI92_UpdateModifiers();
	return True;
}

void TI92_KeyEvent(u8 status, char rawch)
{
	u8 ch = (u8)rawch;
	Bool down;
	Bool shifted_ascii = False;
	Bool second_ascii = False;
	Bool suppress_shift = False;
	int row = -1, col = -1;

	// 1=press, 2=hold, 3=release. A hold report must not release a key.
	if ((status != 0x01) && (status != 0x03)) return;
	down = status == 0x01;
	if (TI92_SetHostModifier(ch, down)) return;

	// Deliberately require three keys so this cannot collide with TI input.
	if (down && ((ch == 0xb1) || (ch == 0x1b)) &&
		((TI92HostModifiers & (TI92_HOST_CTRL | TI92_HOST_ALT)) ==
		 (TI92_HOST_CTRL | TI92_HOST_ALT)))
	{
		TI92.exit_requested = True;
		return;
	}

	if ((ch >= 'a') && (ch <= 'z')) ch -= 'a' - 'A';
	else if ((ch >= 'A') && (ch <= 'Z')) shifted_ascii = True;

	// PicoCalc can report Shift, 8 and 2nd as three independent raw keys.
	// Translate that physical chord to the TI-92's 2nd+multiply (sqrt), while
	// remembering the translation until key-up even if modifiers are released
	// in a different order.
	if (ch == '8')
	{
		if (down &&
			(TI92HostModifiers & (TI92_HOST_ALT | TI92_HOST_SYM)) &&
			(TI92HostModifiers & (TI92_HOST_SHIFT_L | TI92_HOST_SHIFT_R)))
		{
			TI92SqrtChord = True;
			ch = '*';
		}
		else if (!down && TI92SqrtChord)
		{
			TI92SqrtChord = False;
			ch = '*';
		}
	}

	switch (ch)
	{
	case 0x81: row=6; col=3; break; // F1
	case 0x82: row=4; col=3; break; // F2
	case 0x83: row=2; col=3; break; // F3
	case 0x84: row=9; col=3; break; // F4
	case 0x85: row=7; col=3; break; // F5
	case 0x86: row=5; col=3; break; // F6
	case 0x87: row=3; col=3; break; // F7
	case 0x88: row=1; col=3; break; // F8
	case 0x89: row=7; col=1; break; // F9 -> Apps
	case 0xb6: row=0; col=0; break; // down
	case 0xb7: row=0; col=1; break; // right
	case 0xb5: row=0; col=2; break; // up
	case 0xb4: row=0; col=3; break; // left
	case 0xb1: row=8; col=1; break; // Escape
	case 0xd1: row=5; col=2; break; // Insert -> sin
	case 0xd2: row=5; col=1; break; // Home -> cos
	case 0xd4: row=7; col=2; break; // Delete -> Clear
	case 0xd5: row=6; col=1; break; // End -> secondary Enter
	case 0xd6: row=5; col=0; break; // Page Up -> tan
	case 0xd7: row=6; col=7; break; // Page Down -> power
	case '1': row=1; col=2; break; case '2': row=1; col=1; break; case '3': row=1; col=0; break;
	case '4': row=2; col=2; break; case '5': row=2; col=1; break; case '6': row=2; col=0; break;
	case '7': row=3; col=2; break; case '8': row=3; col=1; break; case '9': row=3; col=0; break;
	case '0': row=9; col=2; break;
	case '!': row=1; col=2; break; case '@': row=1; col=1; break;
	case '#': row=1; col=0; break; case '$': row=2; col=2; break;
	case '%': row=2; col=1; break; case '&': row=3; col=2; break;
	case 'A': row=9; col=5; break; case 'B': row=5; col=6; break; case 'C': row=3; col=6; break;
	case 'D': row=2; col=5; break; case 'E': row=2; col=4; break; case 'F': row=3; col=5; break;
	case 'G': row=4; col=5; break; case 'H': row=5; col=5; break; case 'I': row=7; col=4; break;
	case 'J': row=6; col=5; break; case 'K': row=7; col=5; break; case 'L': row=8; col=5; break;
	case 'M': row=7; col=6; break; case 'N': row=6; col=6; break; case 'O': row=8; col=4; break;
	case 'P': row=6; col=0; break; case 'Q': row=9; col=4; break; case 'R': row=3; col=4; break;
	case 'S': row=1; col=5; break; case 'T': row=4; col=4; break; case 'U': row=6; col=4; break;
	case 'V': row=4; col=6; break; case 'W': row=1; col=4; break; case 'X': row=2; col=6; break;
	case 'Y': row=5; col=4; break; case 'Z': row=1; col=6; break;
	case ' ': row=4; col=7; break;
	case ',': row=4; col=0; suppress_shift=True; break;
	case '.': row=9; col=1; suppress_shift=True; break;
	case '+': row=8; col=3; suppress_shift=True; break;
	case '-': row=9; col=7; suppress_shift=True; break;
	case '*': row=7; col=0; suppress_shift=True; break;
	case '/': row=5; col=7; suppress_shift=True; break;
	case '=': row=7; col=7; suppress_shift=True; break;
	case '(': row=4; col=2; suppress_shift=True; break;
	case ')': row=4; col=1; suppress_shift=True; break;
	case '^': row=6; col=7; suppress_shift=True; break;
	case '_': row=9; col=0; suppress_shift=True; break;
	case ':': row=8; col=6; second_ascii=True; suppress_shift=True; break;
	case ';': row=7; col=6; second_ascii=True; suppress_shift=True; break;
	case '<': row=9; col=2; second_ascii=True; suppress_shift=True; break;
	case '>': row=9; col=1; second_ascii=True; suppress_shift=True; break;
	case '[': row=4; col=0; second_ascii=True; suppress_shift=True; break;
	case '\\': row=7; col=7; second_ascii=True; suppress_shift=True; break;
	case ']': row=5; col=7; second_ascii=True; suppress_shift=True; break;
	case '{': row=4; col=2; second_ascii=True; suppress_shift=True; break;
	case '}': row=4; col=1; second_ascii=True; suppress_shift=True; break;
	case '`': row=8; col=2; suppress_shift=True; break; // Mode
	case 0x09: row=3; col=7; break; // Tab -> Store
	case 0x08: case 0x7f: row=8; col=7; break;
	case 0x0d: case 0x0a: row=9; col=6; break;
	case 0x1b: row=8; col=1; break;
	default: break;
	}
	if (row < 0) return;

	if (down)
	{
		TI92TempShift = shifted_ascii &&
			((TI92HostModifiers & (TI92_HOST_SHIFT_L | TI92_HOST_SHIFT_R)) == 0);
		TI92Temp2nd = second_ascii;
		TI92SuppressShift = suppress_shift;
		TI92_UpdateModifiers();
		TI92_SetKey(row, col, True);
	}
	else
	{
		TI92_SetKey(row, col, False);
		TI92TempShift = False;
		TI92Temp2nd = False;
		TI92SuppressShift = False;
		TI92_UpdateModifiers();
	}
}
