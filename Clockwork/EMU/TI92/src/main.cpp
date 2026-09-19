#include "../include.h"

static void TI92_ShowCpuError(eM68KStep result)
{
	char text[48];
	u32 pc = (result == M68K_STEP_ILLEGAL) ? TI92.cpu.fault_pc : TI92.cpu.pc;
	int i;
	DrawClear();
	DrawText2((result == M68K_STEP_ILLEGAL) ? "M68K illegal opcode" :
		"M68K address error", 8, 4, COL_RED);
	MemPrint(text, sizeof(text), "PC=%06X IR=%04X SR=%04X", pc, TI92.cpu.ir, TI92.cpu.sr);
	DrawText(text, 8, 28, COL_WHITE);
	for (i = 0; i < 4; i++)
	{
		MemPrint(text, sizeof(text), "D%d=%08X D%d=%08X", i*2, TI92.cpu.d[i*2],
			i*2 + 1, TI92.cpu.d[i*2 + 1]);
		DrawText(text, 8, 48 + i*16, COL_WHITE);
	}
	for (i = 0; i < 4; i++)
	{
		MemPrint(text, sizeof(text), "A%d=%08X A%d=%08X", i*2, TI92.cpu.a[i*2],
			i*2 + 1, TI92.cpu.a[i*2 + 1]);
		DrawText(text, 8, 116 + i*16, COL_WHITE);
	}
	MemPrint(text, sizeof(text), "USP=%08X SSP=%08X", TI92.cpu.usp, TI92.cpu.ssp);
	DrawText(text, 8, 184, COL_GRAY);
#if TI92_CPU_TRACE
	for (i = 0; i < 4; i++)
	{
		int trace = (TI92.trace_pos - 1 - i) & 7;
		MemPrint(text, sizeof(text), "T-%d PC=%06X IR=%04X", i,
			TI92.trace_pc[trace], TI92.trace_ir[trace]);
		DrawText(text, 8, 204 + i*16, COL_YELLOW);
	}
#endif
	MemPrint(text, sizeof(text), "P-8:%04X %04X %04X %04X",
		TI92_DebugRead16(pc - 8), TI92_DebugRead16(pc - 6),
		TI92_DebugRead16(pc - 4), TI92_DebugRead16(pc - 2));
	DrawText(text, 8, 272, COL_GRAY);
	MemPrint(text, sizeof(text), "P+0:%04X %04X %04X %04X",
		TI92_DebugRead16(pc), TI92_DebugRead16(pc + 2),
		TI92_DebugRead16(pc + 4), TI92_DebugRead16(pc + 6));
	DrawText(text, 8, 288, COL_GRAY);
	DrawText("Y: loader", 120, 306, COL_GRAY);
	DispUpdate();
}

int main()
{
	int batch;
	eM68KStep result;
	TI92_ShowProgress("Starting", 0);

	if (!TI92_LoadRom("/ti92.bin"))
	{
		DrawClear();
		DrawText2("ROM load failed", 40, 112, COL_RED);
		DrawText("Expected /ti92.bin (1 or 2 MiB)", 32, 152, COL_WHITE);
		DispUpdate();
		while (KeyGet() != KEY_Y) {}
		ResetToBootLoader();
	}

	// Cache the ROM from SD at the standard peripheral clock first. Set the
	// flash divider and voltage before raising RP2350 SYS to 300 MHz. Do not use
	// ClockPllSysFreqVolt() here: this emulator deliberately keeps the explicitly
	// requested, stable 1.30 V setting.
	FlashInit(GetClkDivBySysClock(300000));
	VregSetVoltage(VREG_VOLTAGE_1_30);
	VregWait();
	TI92_ShowProgress("Setting CPU to 300 MHz", 94);
	ClockPllSysFreq(300000);

	TI92_ShowProgress("Initializing calculator", 97);
	TI92_Init();
	TI92_ShowProgress("Ready", 100);
	DrawClear();
	DispUpdate();
	while (True)
	{
		// Amortize the I2C keyboard polling over longer CPU runs. Calculation
		// throughput is more important here than catching very short key presses.
		for (batch = 0; batch < 5; batch++)
		{
			result = TI92_Run(20000);
			if (TI92.exit_requested) ResetToBootLoader();
			if ((result == M68K_STEP_ADDRESS_ERROR) || (result == M68K_STEP_ILLEGAL))
			{
				TI92_ShowCpuError(result);
				while (KeyGet() != KEY_Y) {}
				ResetToBootLoader();
			}
		}
		if (TI92.lcd_dirty) TI92_DrawLCD();
	}
}
