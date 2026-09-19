#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned char Bool;

#define True 1
#define False 0
#define B0 (1u << 0)
#define B1 (1u << 1)
#define B2 (1u << 2)
#define B3 (1u << 3)
#define B4 (1u << 4)
#define B7 (1u << 7)
#define B8 (1u << 8)
#define B11 (1u << 11)
#define B13 (1u << 13)
#define B15 (1u << 15)
#define B31 (1u << 31)
#define INLINE static inline
#define USE_EMU_M68K 1

#include "../../_lib/emu/emu_m68k.h"
#include "../../_lib/emu/emu_m68k.c"

#define MEM_SIZE 0x2000
static u8 Memory[MEM_SIZE];

static u8 Read8(u32 addr)
{
	assert(addr < MEM_SIZE);
	return Memory[addr];
}

static void Write8(u32 addr, u8 data)
{
	assert(addr < MEM_SIZE);
	Memory[addr] = data;
}

static void Put16(u32 addr, u16 data)
{
	Memory[addr] = (u8)(data >> 8);
	Memory[addr + 1] = (u8)data;
}

static void Put32(u32 addr, u32 data)
{
	Put16(addr, (u16)(data >> 16));
	Put16(addr + 2, (u16)data);
}

static void Prepare(sM68K* cpu)
{
	memset(Memory, 0, sizeof(Memory));
	Put32(0, 0x1800);
	Put32(4, 0x0100);
	M68K_Init(cpu, Read8, Write8);
	M68K_Reset(cpu);
}

static void TestMoveq(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x70ff);
	Put16(0x102, 0x7200);
	assert(cpu.a[7] == 0x1800 && cpu.pc == 0x100 && cpu.sr == 0x2700);
	assert(M68K_Step(&cpu) == M68K_STEP_OK);
	assert(cpu.d[0] == 0xffffffff && (cpu.sr & M68K_SR_N));
	assert(M68K_Step(&cpu) == M68K_STEP_OK);
	assert(cpu.d[1] == 0 && (cpu.sr & M68K_SR_Z));
}

static void TestSubroutine(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x6104);
	Put16(0x106, 0x4e75);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x106);
	assert(cpu.a[7] == 0x17fc);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x102);
	assert(cpu.a[7] == 0x1800);
}

static void TestWordBranchBase(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x6c00); // bge.w: displacement is relative to extension word
	Put16(0x102, 0x0020);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x122);

	Prepare(&cpu);
	Put16(0x100, 0x6100); // bsr.w $112, return address is after extension
	Put16(0x102, 0x0010);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x112);
	assert(cpu.a[7] == 0x17fc);
	assert(Memory[0x17fc] == 0x00 && Memory[0x17ff] == 0x04);
}

static void TestPcRelativeJsr(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x4eba); Put16(0x102, 0x0010); // jsr ($10,pc) -> $112
	Put16(0x112, 0x4e75);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x112);
	assert(cpu.a[7] == 0x17fc && Memory[0x17ff] == 0x04);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x104);
}

static void TestIllegal(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put32(M68K_VECT_ILLEGAL * 4, 0x0400);
	Put16(0x100, 0xffff);
	assert(M68K_Step(&cpu) == M68K_STEP_ILLEGAL);
	assert(cpu.pc == 0x400 && cpu.a[7] == 0x17fa);
	assert(cpu.fault_pc == 0x100 && cpu.ir == 0xffff);
	assert(Memory[0x17fa] == 0x27 && Memory[0x17fb] == 0x00);
	assert(Memory[0x17fc] == 0x00 && Memory[0x17fe] == 0x01);
}

static void TestStopInterruptAndRte(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put32((M68K_VECT_AUTOVECT1 + 4) * 4, 0x0500);
	Put16(0x100, 0x4e72);
	Put16(0x102, 0x0000);
	Put16(0x500, 0x4e73);
	assert(M68K_Step(&cpu) == M68K_STEP_STOPPED && cpu.stopped);
	assert((cpu.sr & M68K_SR_S) == 0 && cpu.a[7] == 0);
	assert(M68K_Interrupt(&cpu, 5));
	assert(!cpu.stopped && cpu.pc == 0x500 && cpu.a[7] == 0x17fa);
	assert(M68K_Step(&cpu) == M68K_STEP_OK);
	assert(cpu.pc == 0x104 && (cpu.sr & M68K_SR_S) == 0 && cpu.a[7] == 0);
}

static void TestTrapAndRte(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put32((M68K_VECT_TRAP0 + 1) * 4, 0x0400);
	Put16(0x100, 0x4e41);
	Put16(0x400, 0x4e73);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x400);
	assert(cpu.a[7] == 0x17fa);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x102);
	assert(cpu.a[7] == 0x1800);
}

static void TestAddressErrorSignal(void)
{
	sM68K cpu;
	Prepare(&cpu);
	cpu.pc = 0x101;
	assert(M68K_Step(&cpu) == M68K_STEP_ADDRESS_ERROR);
	assert(cpu.address_error);
}

static void TestLevel7IsNonMaskable(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put32((M68K_VECT_AUTOVECT1 + 6) * 4, 0x0600);
	assert((cpu.sr & 0x0700) == 0x0700);
	assert(M68K_Interrupt(&cpu, 7));
	assert(cpu.pc == 0x0600);
}

static void TestMoveEffectiveAddresses(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x203c);
	Put32(0x102, 0x12345678);
	Put16(0x106, 0x327c);
	Put16(0x108, 0xff80);
	Put16(0x10a, 0x20c0);
	cpu.a[0] = 0x0800;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 0x12345678);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[1] == 0xffffff80);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[0] == 0x0804);
	assert(Memory[0x800] == 0x12 && Memory[0x803] == 0x78);
}

static void TestQuickAndControlEA(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x5280);
	Put16(0x102, 0x5188);
	Put16(0x104, 0x43e8);
	Put16(0x106, 0x0010);
	cpu.d[0] = 0xffffffff;
	cpu.a[0] = 0x0900;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 0);
	assert((cpu.sr & (M68K_SR_Z | M68K_SR_C | M68K_SR_X)) ==
		(M68K_SR_Z | M68K_SR_C | M68K_SR_X));
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[0] == 0x08f8);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[1] == 0x0908);
}

static void TestBitOperations(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x0839); Put16(0x102, 0x0002); Put32(0x104, 0x00000800);
	Put16(0x108, 0x08b9); Put16(0x10a, 0x0002); Put32(0x10c, 0x00000800);
	Put16(0x110, 0x08f9); Put16(0x112, 0x0007); Put32(0x114, 0x00000800);
	Put16(0x118, 0x03c0); // bset d1,d0
	Memory[0x800] = 0x04;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && !(cpu.sr & M68K_SR_Z));
	assert(Memory[0x800] == 0x04);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && !(cpu.sr & M68K_SR_Z));
	assert(Memory[0x800] == 0x00);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.sr & M68K_SR_Z));
	assert(Memory[0x800] == 0x80);
	cpu.d[0] = 0;
	cpu.d[1] = 31;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.sr & M68K_SR_Z));
	assert(cpu.d[0] == 0x80000000);
}

static void TestSetCondition(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x57c0); // seq d0
	Put16(0x102, 0x56f9); Put32(0x104, 0x00000800); // sne $800
	cpu.d[0] = 0x12345678;
	cpu.sr |= M68K_SR_Z;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 0x123456ff);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && Memory[0x800] == 0x00);
}

static void TestRegisterShifts(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0xe18c); // lsl.l #8,d4
	Put16(0x102, 0xe240); // asr.w #1,d0
	Put16(0x104, 0xe338); // rol.b d1,d0
	cpu.d[4] = 0x00123456;
	cpu.d[0] = 0x00008103;
	cpu.d[1] = 1;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[4] == 0x12345600);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xffff) == 0xc081);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0x03);
}

static void TestMultiplyDivide(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x82c0); // divu.w d0,d1
	Put16(0x102, 0x85c0); // divs.w d0,d2
	Put16(0x104, 0xc6c0); // mulu.w d0,d3
	Put16(0x106, 0xc9c0); // muls.w d0,d4
	cpu.d[0] = 3;
	cpu.d[1] = 0x00010000;
	cpu.d[2] = 0xfffffff6;
	cpu.d[3] = 4;
	cpu.d[4] = 0xfffffffe;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[1] == 0x00015555);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[2] == 0xfffffffd);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[3] == 12);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[4] == 0xfffffffa);
}

static void TestCmpm(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0xb109); // cmpm.b (a1)+,(a0)+
	cpu.a[0] = 0x0800;
	cpu.a[1] = 0x0900;
	Memory[0x800] = 0x42;
	Memory[0x900] = 0x42;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.sr & M68K_SR_Z));
	assert(cpu.a[0] == 0x0801 && cpu.a[1] == 0x0901);
}

static void TestBcd(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0xc101); // abcd d1,d0
	Put16(0x102, 0x8101); // sbcd d1,d0
	Put16(0x104, 0x4800); // nbcd d0
	cpu.d[0] = 0x45;
	cpu.d[1] = 0x38;
	cpu.sr |= M68K_SR_Z;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0x83);
	cpu.d[1] = 0x18;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0x65);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0x35);
}

static void TestExg(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0xc141); // exg d0,d1
	Put16(0x102, 0xc34c); // exg a1,a4
	Put16(0x104, 0xc189); // exg d0,a1
	cpu.d[0] = 1; cpu.d[1] = 2;
	cpu.a[1] = 3; cpu.a[4] = 4;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 2 && cpu.d[1] == 1);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[1] == 4 && cpu.a[4] == 3);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 4 && cpu.a[1] == 2);
}

static void TestImmediateMovemAndDbra(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0x0000); Put16(0x102, 0x0080);
	Put16(0x104, 0x0640); Put16(0x106, 0x0001);
	Put16(0x108, 0x48e7); Put16(0x10a, 0xc080);
	Put16(0x10c, 0x4cdf); Put16(0x10e, 0x0103);
	Put16(0x110, 0x51c9); Put16(0x112, 0xfffe);
	cpu.d[0] = 0x12340000;
	cpu.d[1] = 1;
	cpu.a[0] = 0xabcdef00;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 0x12340080);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 0x12340081);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[7] == 0x17f4);
	cpu.d[0] = cpu.d[1] = cpu.a[0] = 0;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.a[7] == 0x1800);
	assert(cpu.d[0] == 0x12340081 && cpu.d[1] == 1 && cpu.a[0] == 0xabcdef00);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x110 && (u16)cpu.d[1] == 0);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.pc == 0x114 && (u16)cpu.d[1] == 0xffff);
}

static void TestMainAluFamilies(void)
{
	sM68K cpu;
	Prepare(&cpu);
	Put16(0x100, 0xd081);
	Put16(0x102, 0x9240);
	Put16(0x104, 0xb081);
	Put16(0x106, 0xc001);
	Put16(0x108, 0x8001);
	Put16(0x10a, 0xb300);
	cpu.d[0] = 5;
	cpu.d[1] = 3;
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[0] == 8);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && cpu.d[1] == 0x0000fffb);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.sr & M68K_SR_C));
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 8);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0xfb);
	assert(M68K_Step(&cpu) == M68K_STEP_OK && (cpu.d[0] & 0xff) == 0x00);
}

int main(void)
{
	TestMoveq();
	TestSubroutine();
	TestWordBranchBase();
	TestPcRelativeJsr();
	TestIllegal();
	TestStopInterruptAndRte();
	TestTrapAndRte();
	TestAddressErrorSignal();
	TestLevel7IsNonMaskable();
	TestMoveEffectiveAddresses();
	TestQuickAndControlEA();
	TestBitOperations();
	TestSetCondition();
	TestRegisterShifts();
	TestMultiplyDivide();
	TestCmpm();
	TestBcd();
	TestExg();
	TestImmediateMovemAndDbra();
	TestMainAluFamilies();
	puts("M68K tests: OK");
	return 0;
}
