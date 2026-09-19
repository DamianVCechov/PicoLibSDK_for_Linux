// ****************************************************************************
//
//                         Motorola 68000 CPU Emulator
//
// ****************************************************************************
// PicoLibSDK - Alternative SDK library for Raspberry Pico and RP2040/RP2350
// License: free to use and modify, matching the rest of PicoLibSDK.

#if USE_EMU_M68K

#ifndef _EMU_M68K_H
#define _EMU_M68K_H

#ifdef __cplusplus
extern "C" {
#endif

// Status register bits
#define M68K_SR_C       B0
#define M68K_SR_V       B1
#define M68K_SR_Z       B2
#define M68K_SR_N       B3
#define M68K_SR_X       B4
#define M68K_SR_S       B13
#define M68K_SR_T       B15

// Exception vectors
#define M68K_VECT_BUS_ERROR      2
#define M68K_VECT_ADDRESS_ERROR  3
#define M68K_VECT_ILLEGAL        4
#define M68K_VECT_ZERO_DIVIDE    5
#define M68K_VECT_PRIVILEGE      8
#define M68K_VECT_TRAP0          32
#define M68K_VECT_AUTOVECT1      25

typedef u8 (*pM68KRead8)(u32 addr);
typedef void (*pM68KWrite8)(u32 addr, u8 data);

typedef enum {
	M68K_STEP_OK = 0,
	M68K_STEP_STOPPED,
	M68K_STEP_ADDRESS_ERROR,
	M68K_STEP_ILLEGAL
} eM68KStep;

// CPU descriptor. Addresses are byte addresses; all bus words are big-endian.
typedef struct {
	u32 d[8];		// data registers D0..D7
	u32 a[8];		// address registers A0..A7 (A7 is active SP)
	u32 pc;		// program counter
	u32 usp;		// saved user stack pointer
	u32 ssp;		// saved supervisor stack pointer
	u16 sr;		// status register
	u16 ir;		// last fetched instruction
	u32 fault_pc;		// address of the last unsupported/illegal opcode
	u64 cycles;		// total emulated clock cycles
	pM68KRead8 read8;	// byte read callback
	pM68KWrite8 write8;	// byte write callback
	Bool stopped;		// STOP instruction is waiting for an interrupt
	Bool address_error;	// odd word/long access was attempted
} sM68K;

// Initialize descriptor and install bus callbacks.
void M68K_Init(sM68K* cpu, pM68KRead8 read8, pM68KWrite8 write8);

// Load SSP and PC from reset vectors 0 and 1.
void M68K_Reset(sM68K* cpu);

// Reset directly to supplied SSP and PC values.
void M68K_ResetTo(sM68K* cpu, u32 initial_ssp, u32 initial_pc);

// Execute one instruction.
eM68KStep M68K_Step(sM68K* cpu);

// Raise an autovectored interrupt, level 1..7. False means invalid or masked.
Bool M68K_Interrupt(sM68K* cpu, int level);

#ifdef __cplusplus
}
#endif

#endif // _EMU_M68K_H

#endif // USE_EMU_M68K
