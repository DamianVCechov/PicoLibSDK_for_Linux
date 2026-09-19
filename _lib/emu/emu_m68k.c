// ****************************************************************************
//
//                         Motorola 68000 CPU Emulator
//
// ****************************************************************************

#if USE_EMU_M68K

INLINE u8 M68K_Read8(sM68K* cpu, u32 addr)
{
	return cpu->read8(addr);
}

INLINE u16 M68K_Read16(sM68K* cpu, u32 addr)
{
	if ((addr & 1) != 0)
	{
		cpu->address_error = True;
		return 0;
	}
	return ((u16)cpu->read8(addr) << 8) | cpu->read8(addr + 1);
}

INLINE u32 M68K_Read32(sM68K* cpu, u32 addr)
{
	u32 hi = M68K_Read16(cpu, addr);
	if (cpu->address_error) return 0;
	return (hi << 16) | M68K_Read16(cpu, addr + 2);
}

INLINE void M68K_Write8(sM68K* cpu, u32 addr, u8 data)
{
	cpu->write8(addr, data);
}

INLINE void M68K_Write16(sM68K* cpu, u32 addr, u16 data)
{
	if ((addr & 1) != 0)
	{
		cpu->address_error = True;
		return;
	}
	cpu->write8(addr, (u8)(data >> 8));
	cpu->write8(addr + 1, (u8)data);
}

INLINE void M68K_Write32(sM68K* cpu, u32 addr, u32 data)
{
	M68K_Write16(cpu, addr, (u16)(data >> 16));
	if (!cpu->address_error) M68K_Write16(cpu, addr + 2, (u16)data);
}

INLINE u16 M68K_Fetch16(sM68K* cpu)
{
	u16 data = M68K_Read16(cpu, cpu->pc);
	if (!cpu->address_error) cpu->pc += 2;
	return data;
}

INLINE u32 M68K_Fetch32(sM68K* cpu)
{
	u32 data = M68K_Read32(cpu, cpu->pc);
	if (!cpu->address_error) cpu->pc += 4;
	return data;
}

INLINE void M68K_Push16(sM68K* cpu, u16 data)
{
	cpu->a[7] -= 2;
	M68K_Write16(cpu, cpu->a[7], data);
}

INLINE void M68K_Push32(sM68K* cpu, u32 data)
{
	cpu->a[7] -= 4;
	M68K_Write32(cpu, cpu->a[7], data);
}

INLINE u16 M68K_Pop16(sM68K* cpu)
{
	u16 data = M68K_Read16(cpu, cpu->a[7]);
	if (!cpu->address_error) cpu->a[7] += 2;
	return data;
}

INLINE u32 M68K_Pop32(sM68K* cpu)
{
	u32 data = M68K_Read32(cpu, cpu->a[7]);
	if (!cpu->address_error) cpu->a[7] += 4;
	return data;
}

// Change supervisor state and exchange the active A7 when necessary.
INLINE void M68K_SetSR(sM68K* cpu, u16 sr)
{
	if (((cpu->sr ^ sr) & M68K_SR_S) != 0)
	{
		if ((cpu->sr & M68K_SR_S) != 0)
		{
			cpu->ssp = cpu->a[7];
			cpu->a[7] = cpu->usp;
		}
		else
		{
			cpu->usp = cpu->a[7];
			cpu->a[7] = cpu->ssp;
		}
	}
	cpu->sr = sr;
}

INLINE void M68K_SetNZ32(sM68K* cpu, u32 data)
{
	cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
	if (data == 0) cpu->sr |= M68K_SR_Z;
	if ((data & B31) != 0) cpu->sr |= M68K_SR_N;
}

INLINE u32 M68K_SizeMask(int size)
{
	return (size == 1) ? 0xff : ((size == 2) ? 0xffff : 0xffffffff);
}

INLINE u32 M68K_SizeSign(int size)
{
	return (size == 1) ? B7 : ((size == 2) ? B15 : B31);
}

INLINE s32 M68K_SignExtend(u32 data, int size)
{
	if (size == 1) return (s8)data;
	if (size == 2) return (s16)data;
	return (s32)data;
}

INLINE void M68K_SetNZ(sM68K* cpu, u32 data, int size)
{
	u32 mask = M68K_SizeMask(size);
	cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
	data &= mask;
	if (data == 0) cpu->sr |= M68K_SR_Z;
	if ((data & M68K_SizeSign(size)) != 0) cpu->sr |= M68K_SR_N;
}

INLINE void M68K_SetAddFlags(sM68K* cpu, u32 src, u32 dst, u32 result, int size)
{
	u32 mask = M68K_SizeMask(size);
	u32 sign = M68K_SizeSign(size);
	src &= mask;
	dst &= mask;
	result &= mask;
	cpu->sr &= ~(M68K_SR_X | M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
	if (result == 0) cpu->sr |= M68K_SR_Z;
	if ((result & sign) != 0) cpu->sr |= M68K_SR_N;
	if (result < dst) cpu->sr |= M68K_SR_X | M68K_SR_C;
	if (((~(dst ^ src)) & (dst ^ result) & sign) != 0) cpu->sr |= M68K_SR_V;
}

INLINE void M68K_SetSubFlags(sM68K* cpu, u32 src, u32 dst, u32 result,
	int size, Bool set_x)
{
	u32 mask = M68K_SizeMask(size);
	u32 sign = M68K_SizeSign(size);
	u16 clear = M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C;
	if (set_x) clear |= M68K_SR_X;
	src &= mask;
	dst &= mask;
	result &= mask;
	cpu->sr &= ~clear;
	if (result == 0) cpu->sr |= M68K_SR_Z;
	if ((result & sign) != 0) cpu->sr |= M68K_SR_N;
	if (src > dst) cpu->sr |= M68K_SR_C | (set_x ? M68K_SR_X : 0);
	if (((dst ^ src) & (dst ^ result) & sign) != 0) cpu->sr |= M68K_SR_V;
}

static u8 M68K_Bcd(sM68K* cpu, u8 src, u8 dst, Bool subtract)
{
	int src10 = ((src >> 4) & 15)*10 + (src & 15);
	int dst10 = ((dst >> 4) & 15)*10 + (dst & 15);
	int value = subtract ? dst10 - src10 - ((cpu->sr & M68K_SR_X) ? 1 : 0) :
		dst10 + src10 + ((cpu->sr & M68K_SR_X) ? 1 : 0);
	Bool carry = subtract ? (value < 0) : (value > 99);
	Bool old_z = (cpu->sr & M68K_SR_Z) != 0;
	u8 result;
	if (subtract && value < 0) value += 100;
	if (!subtract && value > 99) value -= 100;
	result = (u8)(((value / 10) << 4) | (value % 10));
	cpu->sr &= ~(M68K_SR_X | M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
	if (carry) cpu->sr |= M68K_SR_X | M68K_SR_C;
	if ((result & 0x80) != 0) cpu->sr |= M68K_SR_N;
	if (old_z && result == 0) cpu->sr |= M68K_SR_Z;
	if (subtract)
	{
		if (((dst ^ src) & (dst ^ result) & 0x80) != 0) cpu->sr |= M68K_SR_V;
	}
	else if (((~(dst ^ src)) & (dst ^ result) & 0x80) != 0)
		cpu->sr |= M68K_SR_V;
	return result;
}

INLINE u32 M68K_GetReg(const sM68K* cpu, int reg)
{
	return (reg < 8) ? cpu->d[reg] : cpu->a[reg - 8];
}

INLINE void M68K_SetReg(sM68K* cpu, int reg, u32 data)
{
	if (reg < 8) cpu->d[reg] = data;
	else cpu->a[reg - 8] = data;
}

typedef enum {
	M68K_EA_NONE = 0,
	M68K_EA_DREG,
	M68K_EA_AREG,
	M68K_EA_MEMORY,
	M68K_EA_IMMEDIATE
} eM68KEAKind;

typedef struct {
	eM68KEAKind kind;
	int reg;
	u32 addr;
	u32 value;
} sM68KEA;

// Resolve one original-68000 effective address. Size is 1, 2 or 4 bytes.
static Bool M68K_EAResolve(sM68K* cpu, int mode, int reg, int size,
	Bool immediate, sM68KEA* ea)
{
	u16 ext;
	s32 index;
	ea->kind = M68K_EA_NONE;
	ea->reg = reg;
	ea->addr = 0;
	ea->value = 0;

	switch (mode)
	{
	case 0: // Dn
		ea->kind = M68K_EA_DREG;
		return True;
	case 1: // An
		ea->kind = M68K_EA_AREG;
		return size != 1;
	case 2: // (An)
		ea->kind = M68K_EA_MEMORY;
		ea->addr = cpu->a[reg];
		return True;
	case 3: // (An)+
		ea->kind = M68K_EA_MEMORY;
		ea->addr = cpu->a[reg];
		cpu->a[reg] += ((size == 1) && (reg == 7)) ? 2 : size;
		return True;
	case 4: // -(An)
		cpu->a[reg] -= ((size == 1) && (reg == 7)) ? 2 : size;
		ea->kind = M68K_EA_MEMORY;
		ea->addr = cpu->a[reg];
		return True;
	case 5: // (d16,An)
		ea->kind = M68K_EA_MEMORY;
		ea->addr = cpu->a[reg] + (s16)M68K_Fetch16(cpu);
		return !cpu->address_error;
	case 6: // (d8,An,Xn)
		ext = M68K_Fetch16(cpu);
		if (cpu->address_error) return False;
		index = ((ext & B15) != 0) ? (s32)cpu->a[(ext >> 12) & 7] :
			(s32)cpu->d[(ext >> 12) & 7];
		if ((ext & B11) == 0) index = (s16)index;
		ea->kind = M68K_EA_MEMORY;
		ea->addr = cpu->a[reg] + (s8)ext + index;
		return True;
	case 7:
		switch (reg)
		{
		case 0: // absolute word
			ea->kind = M68K_EA_MEMORY;
			ea->addr = (u32)(s32)(s16)M68K_Fetch16(cpu);
			return !cpu->address_error;
		case 1: // absolute long
			ea->kind = M68K_EA_MEMORY;
			ea->addr = M68K_Fetch32(cpu);
			return !cpu->address_error;
		case 2: // (d16,PC), PC is address of extension word
		{
			u32 base = cpu->pc;
			ext = M68K_Fetch16(cpu);
			ea->kind = M68K_EA_MEMORY;
			ea->addr = base + (s16)ext;
			return !cpu->address_error;
		}
		case 3: // (d8,PC,Xn)
			ext = M68K_Fetch16(cpu);
			if (cpu->address_error) return False;
			index = ((ext & B15) != 0) ? (s32)cpu->a[(ext >> 12) & 7] :
				(s32)cpu->d[(ext >> 12) & 7];
			if ((ext & B11) == 0) index = (s16)index;
			ea->kind = M68K_EA_MEMORY;
			ea->addr = cpu->pc - 2 + (s8)ext + index;
			return True;
		case 4: // immediate
			if (!immediate) return False;
			ea->kind = M68K_EA_IMMEDIATE;
			ea->value = (size == 4) ? M68K_Fetch32(cpu) : M68K_Fetch16(cpu);
			if (size == 1) ea->value &= 0xff;
			return !cpu->address_error;
		default:
			return False;
		}
	default:
		return False;
	}
}

INLINE u32 M68K_EARead(sM68K* cpu, const sM68KEA* ea, int size)
{
	u32 data;
	switch (ea->kind)
	{
	case M68K_EA_DREG: data = cpu->d[ea->reg]; break;
	case M68K_EA_AREG: data = cpu->a[ea->reg]; break;
	case M68K_EA_MEMORY:
		if (size == 1) return M68K_Read8(cpu, ea->addr);
		if (size == 2) return M68K_Read16(cpu, ea->addr);
		return M68K_Read32(cpu, ea->addr);
	case M68K_EA_IMMEDIATE: data = ea->value; break;
	default: return 0;
	}
	return data & M68K_SizeMask(size);
}

static Bool M68K_EAWrite(sM68K* cpu, const sM68KEA* ea, int size, u32 data)
{
	u32 mask = M68K_SizeMask(size);
	data &= mask;
	switch (ea->kind)
	{
	case M68K_EA_DREG:
		cpu->d[ea->reg] = (cpu->d[ea->reg] & ~mask) | data;
		return True;
	case M68K_EA_AREG:
		cpu->a[ea->reg] = data;
		return True;
	case M68K_EA_MEMORY:
		if (size == 1) M68K_Write8(cpu, ea->addr, (u8)data);
		else if (size == 2) M68K_Write16(cpu, ea->addr, (u16)data);
		else M68K_Write32(cpu, ea->addr, data);
		return !cpu->address_error;
	default:
		return False;
	}
}

static Bool M68K_ControlEA(sM68K* cpu, int mode, int reg, u32* addr)
{
	sM68KEA ea;
	if ((mode == 0) || (mode == 1) || (mode == 3) || (mode == 4) ||
		((mode == 7) && (reg > 3))) return False;
	if (!M68K_EAResolve(cpu, mode, reg, 4, False, &ea) ||
		(ea.kind != M68K_EA_MEMORY)) return False;
	*addr = ea.addr;
	return True;
}

INLINE Bool M68K_DataAlterableEA(int mode, int reg)
{
	return (mode == 0) || ((mode >= 2) && (mode <= 6)) ||
		((mode == 7) && (reg <= 1));
}

INLINE Bool M68K_Cond(const sM68K* cpu, int cond)
{
	Bool c = (cpu->sr & M68K_SR_C) != 0;
	Bool v = (cpu->sr & M68K_SR_V) != 0;
	Bool z = (cpu->sr & M68K_SR_Z) != 0;
	Bool n = (cpu->sr & M68K_SR_N) != 0;

	switch (cond & 15)
	{
	case 0: return True;		// T / BRA
	case 1: return False;		// F (branch group handles BSR separately)
	case 2: return !c && !z;	// HI
	case 3: return c || z;		// LS
	case 4: return !c;		// CC
	case 5: return c;		// CS
	case 6: return !z;		// NE
	case 7: return z;		// EQ
	case 8: return !v;		// VC
	case 9: return v;		// VS
	case 10: return !n;		// PL
	case 11: return n;		// MI
	case 12: return n == v;	// GE
	case 13: return n != v;	// LT
	case 14: return !z && (n == v); // GT
	default: return z || (n != v);  // LE
	}
}

// Create the standard 6-byte 68000 exception frame.
static void M68K_Exception(sM68K* cpu, int vector, u16 new_sr)
{
	u16 old_sr = cpu->sr;
	if ((old_sr & M68K_SR_S) == 0)
	{
		cpu->usp = cpu->a[7];
		cpu->a[7] = cpu->ssp;
	}
	cpu->sr = (new_sr | M68K_SR_S) & ~M68K_SR_T;
	cpu->stopped = False;
	M68K_Push32(cpu, cpu->pc);
	M68K_Push16(cpu, old_sr);
	if (!cpu->address_error) cpu->pc = M68K_Read32(cpu, (u32)vector * 4);
}

static eM68KStep M68K_Illegal(sM68K* cpu)
{
	M68K_Exception(cpu, M68K_VECT_ILLEGAL, cpu->sr);
	cpu->cycles += 34;
	return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_ILLEGAL;
}

void M68K_Init(sM68K* cpu, pM68KRead8 read8, pM68KWrite8 write8)
{
	int i;
	for (i = 0; i < 8; i++)
	{
		cpu->d[i] = 0;
		cpu->a[i] = 0;
	}
	cpu->pc = 0;
	cpu->usp = 0;
	cpu->ssp = 0;
	cpu->sr = 0x2700;
	cpu->ir = 0;
	cpu->fault_pc = 0;
	cpu->cycles = 0;
	cpu->read8 = read8;
	cpu->write8 = write8;
	cpu->stopped = False;
	cpu->address_error = False;
}

void M68K_Reset(sM68K* cpu)
{
	u32 initial_ssp, initial_pc;
	cpu->address_error = False;
	initial_ssp = M68K_Read32(cpu, 0);
	initial_pc = M68K_Read32(cpu, 4);
	M68K_ResetTo(cpu, initial_ssp, initial_pc);
}

void M68K_ResetTo(sM68K* cpu, u32 initial_ssp, u32 initial_pc)
{
	cpu->address_error = False;
	cpu->stopped = False;
	cpu->sr = 0x2700;
	cpu->ssp = initial_ssp;
	cpu->a[7] = initial_ssp;
	cpu->pc = initial_pc;
	cpu->fault_pc = 0;
	cpu->cycles = 0;
}

Bool M68K_Interrupt(sM68K* cpu, int level)
{
	u16 sr;
	// Level 7 is non-maskable on the 68000; levels 1..6 obey the SR mask.
	if ((level < 1) || (level > 7) ||
		((level != 7) && (level <= ((cpu->sr >> 8) & 7)))) return False;
	sr = (cpu->sr & ~0x0700) | ((u16)level << 8);
	M68K_Exception(cpu, M68K_VECT_AUTOVECT1 + level - 1, sr);
	cpu->cycles += 44;
	return !cpu->address_error;
}

eM68KStep M68K_Step(sM68K* cpu)
{
	u16 op;
	u32 op_pc = cpu->pc;
	cpu->address_error = False;
	if (cpu->stopped) return M68K_STEP_STOPPED;
	op = M68K_Fetch16(cpu);
	if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
	cpu->ir = op;

	// MOVE.B, MOVE.W, MOVE.L and MOVEA
	if (((op >> 12) >= 1) && ((op >> 12) <= 3))
	{
		int top = op >> 12;
		int size = (top == 1) ? 1 : ((top == 3) ? 2 : 4);
		int srcmode = (op >> 3) & 7;
		int srcreg = op & 7;
		int dstmode = (op >> 6) & 7;
		int dstreg = (op >> 9) & 7;
		sM68KEA src, dst;
		u32 data;
		if (!M68K_EAResolve(cpu, srcmode, srcreg, size, True, &src)) goto illegal;
		data = M68K_EARead(cpu, &src, size);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		if (dstmode == 1) // MOVEA.W/L
		{
			if (size == 1) goto illegal;
			cpu->a[dstreg] = (size == 2) ? (u32)(s32)(s16)data : data;
		}
		else
		{
			if (!M68K_EAResolve(cpu, dstmode, dstreg, size, False, &dst) ||
				!M68K_EAWrite(cpu, &dst, size, data)) goto illegal;
			M68K_SetNZ(cpu, data, size);
		}
		cpu->cycles += 8;
		return M68K_STEP_OK;
	}

	if (op == 0x4e71) // NOP
	{
		cpu->cycles += 4;
		return M68K_STEP_OK;
	}

	if ((op & 0xfff0) == 0x4e40) // TRAP #0..#15
	{
		M68K_Exception(cpu, M68K_VECT_TRAP0 + (op & 15), cpu->sr);
		cpu->cycles += 34;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	if (op == 0x4e75) // RTS
	{
		cpu->pc = M68K_Pop32(cpu);
		cpu->cycles += 16;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	if ((op & 0xfff8) == 0x4e50) // LINK An,#disp
	{
		int reg = op & 7;
		s16 disp = (s16)M68K_Fetch16(cpu);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		M68K_Push32(cpu, cpu->a[reg]);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		cpu->a[reg] = cpu->a[7];
		cpu->a[7] += disp;
		cpu->cycles += 16;
		return M68K_STEP_OK;
	}

	if ((op & 0xfff8) == 0x4e58) // UNLK An
	{
		int reg = op & 7;
		cpu->a[7] = cpu->a[reg];
		cpu->a[reg] = M68K_Pop32(cpu);
		cpu->cycles += 12;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	if ((op & 0xfff0) == 0x4e60) // MOVE An,USP / MOVE USP,An
	{
		if ((cpu->sr & M68K_SR_S) == 0)
		{
			cpu->pc = op_pc;
			M68K_Exception(cpu, M68K_VECT_PRIVILEGE, cpu->sr);
			cpu->cycles += 34;
			return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
		}
		if ((op & 8) == 0) cpu->usp = cpu->a[op & 7];
		else cpu->a[op & 7] = cpu->usp;
		cpu->cycles += 4;
		return M68K_STEP_OK;
	}

	if (op == 0x4e73) // RTE
	{
		u16 sr;
		u32 pc;
		if ((cpu->sr & M68K_SR_S) == 0)
		{
			cpu->pc -= 2;
			M68K_Exception(cpu, M68K_VECT_PRIVILEGE, cpu->sr);
			cpu->cycles += 34;
			return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
		}
		sr = M68K_Pop16(cpu);
		pc = M68K_Pop32(cpu);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		M68K_SetSR(cpu, sr);
		cpu->pc = pc;
		cpu->cycles += 20;
		return M68K_STEP_OK;
	}

	if (op == 0x4e72) // STOP #<sr>
	{
		u16 sr;
		if ((cpu->sr & M68K_SR_S) == 0)
		{
			cpu->pc -= 2;
			M68K_Exception(cpu, M68K_VECT_PRIVILEGE, cpu->sr);
			cpu->cycles += 34;
			return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
		}
		sr = M68K_Fetch16(cpu);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		M68K_SetSR(cpu, sr);
		cpu->stopped = True;
		cpu->cycles += 4;
		return M68K_STEP_STOPPED;
	}

	if ((op & 0xf100) == 0x7000) // MOVEQ #imm,Dn
	{
		int reg = (op >> 9) & 7;
		cpu->d[reg] = (u32)(s32)(s8)op;
		M68K_SetNZ32(cpu, cpu->d[reg]);
		cpu->cycles += 4;
		return M68K_STEP_OK;
	}

	// Immediate and register bit operations: BTST, BCHG, BCLR, BSET.
	if (((op & 0xff00) == 0x0800) || ((op & 0xf100) == 0x0100))
	{
		Bool immediate_bit = (op & 0xff00) == 0x0800;
		int operation = (op >> 6) & 3;
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		int size = (mode == 0) ? 4 : 1;
		u32 bit;
		u32 data;
		u32 mask;
		sM68KEA ea;
		if (immediate_bit)
		{
			bit = M68K_Fetch16(cpu) & 0xff;
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		}
		else
			bit = cpu->d[(op >> 9) & 7];
		if ((mode == 1) || ((operation != 0) && !M68K_DataAlterableEA(mode, reg)) ||
			!M68K_EAResolve(cpu, mode, reg, size, False, &ea)) goto illegal;
		data = M68K_EARead(cpu, &ea, size);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		bit &= (size == 4) ? 31 : 7;
		mask = (u32)1 << bit;
		cpu->sr &= ~M68K_SR_Z;
		if ((data & mask) == 0) cpu->sr |= M68K_SR_Z;
		if (operation != 0)
		{
			if (operation == 1) data ^= mask;
			else if (operation == 2) data &= ~mask;
			else data |= mask;
			if (!M68K_EAWrite(cpu, &ea, size, data)) goto illegal;
		}
		cpu->cycles += (mode == 0) ? 8 : 12;
		return M68K_STEP_OK;
	}

	// ORI/ANDI/SUBI/ADDI/EORI/CMPI
	if (((op & 0xff00) == 0x0000) || ((op & 0xff00) == 0x0200) ||
		((op & 0xff00) == 0x0400) || ((op & 0xff00) == 0x0600) ||
		((op & 0xff00) == 0x0a00) || ((op & 0xff00) == 0x0c00))
	{
		int kind = op & 0x0f00;
		int sizecode = (op >> 6) & 3;
		int size = (sizecode == 0) ? 1 : ((sizecode == 1) ? 2 : 4);
		u32 src, dst, result;
		sM68KEA ea;
		if (sizecode == 3) goto illegal;
		// Logical immediates to CCR/SR use dedicated encodings.
		if (((op & 0x003f) == 0x003c) &&
			((kind == 0x0000) || (kind == 0x0200) || (kind == 0x0a00)))
		{
			u16 imm = M68K_Fetch16(cpu);
			Bool to_sr = (op & 0x0040) != 0;
			if (to_sr && ((cpu->sr & M68K_SR_S) == 0))
			{
				cpu->pc = op_pc;
				M68K_Exception(cpu, M68K_VECT_PRIVILEGE, cpu->sr);
				cpu->cycles += 34;
				return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
			}
			u16 value = to_sr ? cpu->sr : (cpu->sr & 0x1f);
			if (kind == 0x0000) value |= imm;
			else if (kind == 0x0200) value &= imm;
			else value ^= imm;
			if (to_sr) M68K_SetSR(cpu, value);
			else cpu->sr = (cpu->sr & 0xff00) | (value & 0x1f);
			cpu->cycles += 20;
			return M68K_STEP_OK;
		}
		src = (size == 4) ? M68K_Fetch32(cpu) : M68K_Fetch16(cpu);
		if (size == 1) src &= 0xff;
		if (cpu->address_error ||
			!M68K_EAResolve(cpu, (op >> 3) & 7, op & 7, size, False, &ea) ||
			(ea.kind == M68K_EA_AREG)) goto illegal;
		dst = M68K_EARead(cpu, &ea, size);
		if (kind == 0x0000) result = dst | src;
		else if (kind == 0x0200) result = dst & src;
		else if (kind == 0x0400) result = dst - src;
		else if (kind == 0x0600) result = dst + src;
		else if (kind == 0x0a00) result = dst ^ src;
		else result = dst - src;
		if (kind == 0x0400) M68K_SetSubFlags(cpu, src, dst, result, size, True);
		else if (kind == 0x0600) M68K_SetAddFlags(cpu, src, dst, result, size);
		else if (kind == 0x0c00) M68K_SetSubFlags(cpu, src, dst, result, size, False);
		else M68K_SetNZ(cpu, result, size);
		if ((kind != 0x0c00) && !M68K_EAWrite(cpu, &ea, size, result)) goto illegal;
		cpu->cycles += 8;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	// MOVE from SR, MOVE to CCR, MOVE to SR
	if (((op & 0xffc0) == 0x40c0) || ((op & 0xffc0) == 0x44c0) ||
		((op & 0xffc0) == 0x46c0))
	{
		sM68KEA ea;
		u32 data;
		if ((op & 0xffc0) == 0x40c0)
		{
			if (!M68K_EAResolve(cpu, (op >> 3) & 7, op & 7, 2, False, &ea) ||
				!M68K_EAWrite(cpu, &ea, 2, cpu->sr)) goto illegal;
		}
		else
		{
			if (((op & 0xffc0) == 0x46c0) && ((cpu->sr & M68K_SR_S) == 0))
			{
				cpu->pc = op_pc;
				M68K_Exception(cpu, M68K_VECT_PRIVILEGE, cpu->sr);
				cpu->cycles += 34;
				return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
			}
			if (!M68K_EAResolve(cpu, (op >> 3) & 7, op & 7, 2, True, &ea)) goto illegal;
			data = M68K_EARead(cpu, &ea, 2);
			if ((op & 0xffc0) == 0x44c0)
				cpu->sr = (cpu->sr & 0xff00) | (data & 0x1f);
			else
				M68K_SetSR(cpu, (u16)data);
		}
		cpu->cycles += 12;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	// CLR.B/W/L and TST.B/W/L
	if (((op & 0xff00) == 0x4200) || ((op & 0xff00) == 0x4a00))
	{
		int sc = (op >> 6) & 3;
		int size = (sc == 0) ? 1 : ((sc == 1) ? 2 : 4);
		sM68KEA ea;
		u32 data;
		if (sc == 3) goto illegal;
		if (!M68K_EAResolve(cpu, (op >> 3) & 7, op & 7, size, False, &ea) ||
			(ea.kind == M68K_EA_AREG)) goto illegal;
		if ((op & 0xff00) == 0x4200)
		{
			if (!M68K_EAWrite(cpu, &ea, size, 0)) goto illegal;
			data = 0;
		}
		else
			data = M68K_EARead(cpu, &ea, size);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		M68K_SetNZ(cpu, data, size);
		cpu->cycles += 8;
		return M68K_STEP_OK;
	}

	if ((op & 0xffc0) == 0x4800) // NBCD <ea>
	{
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		sM68KEA ea;
		u8 src, result;
		if (!M68K_DataAlterableEA(mode, reg) ||
			!M68K_EAResolve(cpu, mode, reg, 1, False, &ea)) goto illegal;
		src = (u8)M68K_EARead(cpu, &ea, 1);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		result = M68K_Bcd(cpu, src, 0, True);
		if (!M68K_EAWrite(cpu, &ea, 1, result)) goto illegal;
		cpu->cycles += (mode == 0) ? 6 : 8;
		return M68K_STEP_OK;
	}

	// NEG.B/W/L and NOT.B/W/L
	if (((op & 0xff00) == 0x4400) || ((op & 0xff00) == 0x4600))
	{
		int sc = (op >> 6) & 3;
		int size = (sc == 0) ? 1 : ((sc == 1) ? 2 : 4);
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		sM68KEA ea;
		u32 old, result;
		Bool neg = (op & 0xff00) == 0x4400;
		if ((sc == 3) || !M68K_DataAlterableEA(mode, reg) ||
			!M68K_EAResolve(cpu, mode, reg, size, False, &ea)) goto illegal;
		old = M68K_EARead(cpu, &ea, size);
		result = neg ? 0 - old : ~old;
		if (!M68K_EAWrite(cpu, &ea, size, result)) goto illegal;
		if (neg) M68K_SetSubFlags(cpu, old, 0, result, size, True);
		else M68K_SetNZ(cpu, result, size);
		cpu->cycles += 8;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	// SWAP Dn, EXT.W Dn and EXT.L Dn
	if ((op & 0xfff8) == 0x4840)
	{
		int reg = op & 7;
		cpu->d[reg] = (cpu->d[reg] << 16) | (cpu->d[reg] >> 16);
		M68K_SetNZ32(cpu, cpu->d[reg]);
		cpu->cycles += 4;
		return M68K_STEP_OK;
	}
	if (((op & 0xfff8) == 0x4880) || ((op & 0xfff8) == 0x48c0))
	{
		int reg = op & 7;
		if ((op & 0x0040) == 0)
		{
			cpu->d[reg] = (cpu->d[reg] & 0xffff0000) | (u16)(s16)(s8)cpu->d[reg];
			M68K_SetNZ(cpu, cpu->d[reg], 2);
		}
		else
		{
			cpu->d[reg] = (u32)(s32)(s16)cpu->d[reg];
			M68K_SetNZ32(cpu, cpu->d[reg]);
		}
		cpu->cycles += 4;
		return M68K_STEP_OK;
	}

	if ((op & 0xffc0) == 0x4840) // PEA <ea>
	{
		u32 addr;
		if (!M68K_ControlEA(cpu, (op >> 3) & 7, op & 7, &addr)) goto illegal;
		M68K_Push32(cpu, addr);
		cpu->cycles += 12;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	// MOVEM.W/L register list to/from memory.
	if ((op & 0xfb80) == 0x4880)
	{
		Bool mem_to_reg = (op & 0x0400) != 0;
		int size = (op & 0x0040) ? 4 : 2;
		int mode = (op >> 3) & 7;
		int areg = op & 7;
		u16 mask = M68K_Fetch16(cpu);
		u32 addr;
		int bit;
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		if ((!mem_to_reg && (mode == 4)))
		{
			addr = cpu->a[areg];
			for (bit = 0; bit < 16; bit++)
				if ((mask & (1 << bit)) != 0)
				{
					addr -= size;
					if (size == 2) M68K_Write16(cpu, addr, (u16)M68K_GetReg(cpu, 15 - bit));
					else M68K_Write32(cpu, addr, M68K_GetReg(cpu, 15 - bit));
				}
			cpu->a[areg] = addr;
		}
		else
		{
			sM68KEA ea;
			if ((mem_to_reg && (mode == 4)) || (!mem_to_reg && (mode == 3)) ||
				!M68K_EAResolve(cpu, mode, areg, size, False, &ea) ||
				(ea.kind != M68K_EA_MEMORY)) goto illegal;
			addr = ea.addr;
			for (bit = 0; bit < 16; bit++)
				if ((mask & (1 << bit)) != 0)
				{
					if (mem_to_reg)
					{
						u32 data = (size == 2) ? (u32)(s32)(s16)M68K_Read16(cpu, addr) :
							M68K_Read32(cpu, addr);
						M68K_SetReg(cpu, bit, data);
					}
					else if (size == 2) M68K_Write16(cpu, addr, (u16)M68K_GetReg(cpu, bit));
					else M68K_Write32(cpu, addr, M68K_GetReg(cpu, bit));
					addr += size;
				}
			if (mode == 3) cpu->a[areg] = addr;
		}
		cpu->cycles += 12;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	// DBcc Dn,disp16
	if ((op & 0xf0f8) == 0x50c8)
	{
		int cond = (op >> 8) & 15;
		u32 branch_base = cpu->pc;
		s16 disp = (s16)M68K_Fetch16(cpu);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		if (!M68K_Cond(cpu, cond))
		{
			u16 count = (u16)cpu->d[op & 7] - 1;
			cpu->d[op & 7] = (cpu->d[op & 7] & 0xffff0000) | count;
			if (count != 0xffff)
			{
				cpu->pc = branch_base + disp;
				cpu->cycles += 10;
				return M68K_STEP_OK;
			}
		}
		cpu->cycles += 14;
		return M68K_STEP_OK;
	}

	// Scc <ea>: store $ff when the condition is true, otherwise $00.
	if ((op & 0xf0c0) == 0x50c0)
	{
		int cond = (op >> 8) & 15;
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		sM68KEA ea;
		if (!M68K_DataAlterableEA(mode, reg) ||
			!M68K_EAResolve(cpu, mode, reg, 1, False, &ea) ||
			!M68K_EAWrite(cpu, &ea, 1, M68K_Cond(cpu, cond) ? 0xff : 0))
			goto illegal;
		cpu->cycles += (mode == 0) ? 6 : 12;
		return M68K_STEP_OK;
	}

	// ADDQ/SUBQ. Address registers are always modified as 32-bit, without flags.
	if ((op & 0xf000) == 0x5000 && ((op & 0x00c0) != 0x00c0))
	{
		int sizecode = (op >> 6) & 3;
		int size = (sizecode == 0) ? 1 : ((sizecode == 1) ? 2 : 4);
		u32 quick = (op >> 9) & 7;
		sM68KEA ea;
		u32 old, result, mask;
		Bool sub = (op & B8) != 0;
		if (quick == 0) quick = 8;
		if (!M68K_EAResolve(cpu, (op >> 3) & 7, op & 7,
			(((op >> 3) & 7) == 1) ? 4 : size, False, &ea)) goto illegal;
		if (ea.kind == M68K_EA_AREG)
		{
			cpu->a[ea.reg] = sub ? cpu->a[ea.reg] - quick : cpu->a[ea.reg] + quick;
			cpu->cycles += 8;
			return M68K_STEP_OK;
		}
		old = M68K_EARead(cpu, &ea, size);
		mask = M68K_SizeMask(size);
		result = sub ? (old - quick) & mask : (old + quick) & mask;
		if (!M68K_EAWrite(cpu, &ea, size, result)) goto illegal;
		if (sub) M68K_SetSubFlags(cpu, quick, old, result, size, True);
		else M68K_SetAddFlags(cpu, quick, old, result, size);
		cpu->cycles += 8;
		return M68K_STEP_OK;
	}

	// EXG Dn,Dm / An,Am / Dn,Am.
	if (((op & 0xf1f8) == 0xc140) || ((op & 0xf1f8) == 0xc148) ||
		((op & 0xf1f8) == 0xc188))
	{
		int rx = (op >> 9) & 7;
		int ry = op & 7;
		u32 value;
		if ((op & 0xf1f8) == 0xc140)
		{
			value = cpu->d[rx]; cpu->d[rx] = cpu->d[ry]; cpu->d[ry] = value;
		}
		else if ((op & 0xf1f8) == 0xc148)
		{
			value = cpu->a[rx]; cpu->a[rx] = cpu->a[ry]; cpu->a[ry] = value;
		}
		else
		{
			value = cpu->d[rx]; cpu->d[rx] = cpu->a[ry]; cpu->a[ry] = value;
		}
		cpu->cycles += 6;
		return M68K_STEP_OK;
	}

	// ABCD/SBCD between data registers or predecrement memory operands.
	if (((op & 0xf1f0) == 0xc100) || ((op & 0xf1f0) == 0x8100))
	{
		Bool subtract = (op & 0x4000) == 0;
		int src_reg = op & 7;
		int dst_reg = (op >> 9) & 7;
		u8 src, dst, result;
		if ((op & 8) != 0)
		{
			sM68KEA src_ea, dst_ea;
			if (!M68K_EAResolve(cpu, 4, src_reg, 1, False, &src_ea)) goto illegal;
			src = (u8)M68K_EARead(cpu, &src_ea, 1);
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
			if (!M68K_EAResolve(cpu, 4, dst_reg, 1, False, &dst_ea)) goto illegal;
			dst = (u8)M68K_EARead(cpu, &dst_ea, 1);
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
			result = M68K_Bcd(cpu, src, dst, subtract);
			if (!M68K_EAWrite(cpu, &dst_ea, 1, result)) goto illegal;
		}
		else
		{
			src = (u8)cpu->d[src_reg];
			dst = (u8)cpu->d[dst_reg];
			result = M68K_Bcd(cpu, src, dst, subtract);
			cpu->d[dst_reg] = (cpu->d[dst_reg] & 0xffffff00) | result;
		}
		cpu->cycles += (op & 8) ? 18 : 6;
		return M68K_STEP_OK;
	}

	// CMPM.B/W/L (Ay)+,(Ax)+.
	if (((op & 0xf138) == 0xb108) && (((op >> 6) & 3) != 3))
	{
		int sizecode = (op >> 6) & 3;
		int size = (sizecode == 0) ? 1 : ((sizecode == 1) ? 2 : 4);
		sM68KEA src_ea, dst_ea;
		u32 src, dst;
		if (!M68K_EAResolve(cpu, 3, op & 7, size, False, &src_ea)) goto illegal;
		src = M68K_EARead(cpu, &src_ea, size);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		if (!M68K_EAResolve(cpu, 3, (op >> 9) & 7, size, False, &dst_ea)) goto illegal;
		dst = M68K_EARead(cpu, &dst_ea, size);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		M68K_SetSubFlags(cpu, src, dst, dst - src, size, False);
		cpu->cycles += 12;
		return M68K_STEP_OK;
	}

	// Unsigned/signed word multiply and divide.
	if (((op & 0xf1c0) == 0x80c0) || ((op & 0xf1c0) == 0x81c0) ||
		((op & 0xf1c0) == 0xc0c0) || ((op & 0xf1c0) == 0xc1c0))
	{
		int dn = (op >> 9) & 7;
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		Bool divide = (op & 0x4000) == 0;
		Bool signed_op = (op & B8) != 0;
		sM68KEA ea;
		u32 src;
		if (!M68K_EAResolve(cpu, mode, reg, 2, True, &ea) ||
			(ea.kind == M68K_EA_AREG)) goto illegal;
		src = M68K_EARead(cpu, &ea, 2);
		if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		if (!divide)
		{
			if (signed_op)
				cpu->d[dn] = (u32)((s32)(s16)src * (s32)(s16)cpu->d[dn]);
			else
				cpu->d[dn] = (u32)(u16)src * (u32)(u16)cpu->d[dn];
			M68K_SetNZ32(cpu, cpu->d[dn]);
			cpu->cycles += 70;
			return M68K_STEP_OK;
		}
		if ((u16)src == 0)
		{
			M68K_Exception(cpu, M68K_VECT_ZERO_DIVIDE, cpu->sr);
			cpu->cycles += 38;
			return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
		}
		cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
		if (signed_op)
		{
			signed long long dividend = (s32)cpu->d[dn];
			signed long long divisor = (s16)src;
			signed long long quotient = dividend / divisor;
			signed long long remainder = dividend % divisor;
			if ((quotient < -32768) || (quotient > 32767))
				cpu->sr |= M68K_SR_V;
			else
			{
				cpu->d[dn] = ((u32)(u16)remainder << 16) | (u16)quotient;
				if ((u16)quotient == 0) cpu->sr |= M68K_SR_Z;
				if ((quotient & 0x8000) != 0) cpu->sr |= M68K_SR_N;
			}
		}
		else
		{
			u32 dividend = cpu->d[dn];
			u32 divisor = (u16)src;
			u32 quotient = dividend / divisor;
			u32 remainder = dividend % divisor;
			if (quotient > 0xffff)
				cpu->sr |= M68K_SR_V;
			else
			{
				cpu->d[dn] = (remainder << 16) | quotient;
				if ((u16)quotient == 0) cpu->sr |= M68K_SR_Z;
				if ((quotient & 0x8000) != 0) cpu->sr |= M68K_SR_N;
			}
		}
		cpu->cycles += 140;
		return M68K_STEP_OK;
	}

	// ADD/SUB/CMP/AND/OR/EOR and ADDA/SUBA/CMPA.
	if (((op & 0xf000) == 0x8000) || ((op & 0xf000) == 0x9000) ||
		((op & 0xf000) == 0xb000) || ((op & 0xf000) == 0xc000) ||
		((op & 0xf000) == 0xd000))
	{
		int family = op >> 12;
		int dn = (op >> 9) & 7;
		int opmode = (op >> 6) & 7;
		int mode = (op >> 3) & 7;
		int reg = op & 7;
		int size;
		sM68KEA ea;
		u32 src, dst, result;

		// Address arithmetic/compare (opmodes 3 and 7). OR/AND use these for DIV/MUL.
		if ((opmode == 3) || (opmode == 7))
		{
			if ((family == 8) || (family == 12)) goto illegal;
			size = (opmode == 3) ? 2 : 4;
			if (!M68K_EAResolve(cpu, mode, reg, size, True, &ea)) goto illegal;
			src = M68K_EARead(cpu, &ea, size);
			if (size == 2) src = (u32)(s32)(s16)src;
			dst = cpu->a[dn];
			result = dst - src;
			if (family == 9) cpu->a[dn] = result;       // SUBA
			else if (family == 13) cpu->a[dn] = dst + src; // ADDA
			else if (family == 11) M68K_SetSubFlags(cpu, src, dst, result, 4, False); // CMPA
			else goto illegal;
			cpu->cycles += 8;
			return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
		}

		size = (opmode % 4 == 0) ? 1 : ((opmode % 4 == 1) ? 2 : 4);
		if ((opmode <= 2) || ((family == 11) && (opmode <= 2)))
		{
			if (!M68K_EAResolve(cpu, mode, reg, size, True, &ea)) goto illegal;
			src = M68K_EARead(cpu, &ea, size);
			dst = cpu->d[dn] & M68K_SizeMask(size);
			if (family == 8) result = dst | src;
			else if (family == 9) result = dst - src;
			else if (family == 11) result = dst - src;
			else if (family == 12) result = dst & src;
			else result = dst + src;
			if (family == 9) M68K_SetSubFlags(cpu, src, dst, result, size, True);
			else if (family == 11) M68K_SetSubFlags(cpu, src, dst, result, size, False);
			else if (family == 13) M68K_SetAddFlags(cpu, src, dst, result, size);
			else M68K_SetNZ(cpu, result, size);
			if (family != 11)
			{
				sM68KEA dstea = { M68K_EA_DREG, dn, 0, 0 };
				M68K_EAWrite(cpu, &dstea, size, result);
			}
		}
		else
		{
			// CMP family opmodes 4..6 are EOR Dn,<ea>.
			if (!M68K_DataAlterableEA(mode, reg) ||
				!M68K_EAResolve(cpu, mode, reg, size, False, &ea)) goto illegal;
			src = cpu->d[dn] & M68K_SizeMask(size);
			dst = M68K_EARead(cpu, &ea, size);
			if (family == 8) result = dst | src;
			else if (family == 9) result = dst - src;
			else if (family == 11) result = dst ^ src;
			else if (family == 12) result = dst & src;
			else result = dst + src;
			if (family == 9) M68K_SetSubFlags(cpu, src, dst, result, size, True);
			else if (family == 13) M68K_SetAddFlags(cpu, src, dst, result, size);
			else M68K_SetNZ(cpu, result, size);
			if (!M68K_EAWrite(cpu, &ea, size, result)) goto illegal;
		}
		cpu->cycles += 8;
		return cpu->address_error ? M68K_STEP_ADDRESS_ERROR : M68K_STEP_OK;
	}

	if ((op & 0xf000) == 0x6000) // BRA, BSR, Bcc
	{
		int cond = (op >> 8) & 15;
		s32 disp = (s8)op;
		u32 branch_base = cpu->pc;
		if ((op & 0xff) == 0)
		{
			disp = (s16)M68K_Fetch16(cpu);
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		}
		if (cond == 1)
		{
			M68K_Push32(cpu, cpu->pc);
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
			cpu->pc = branch_base + disp;
			cpu->cycles += 18;
		}
		else if (M68K_Cond(cpu, cond))
		{
			cpu->pc = branch_base + disp;
			cpu->cycles += 10;
		}
		else
			cpu->cycles += ((op & 0xff) == 0) ? 12 : 8;
		return M68K_STEP_OK;
	}

	// Register shifts and rotates: ASx, LSx, ROXx and ROx.
	if (((op & 0xf000) == 0xe000) && (((op >> 6) & 3) != 3))
	{
		int count_reg = (op >> 9) & 7;
		Bool left = (op & B8) != 0;
		int sizecode = (op >> 6) & 3;
		int size = (sizecode == 0) ? 1 : ((sizecode == 1) ? 2 : 4);
		Bool register_count = (op & 0x20) != 0;
		int kind = (op >> 3) & 3;
		int reg = op & 7;
		int count = register_count ? (int)(cpu->d[count_reg] & 63) :
			(count_reg == 0 ? 8 : count_reg);
		u32 mask = M68K_SizeMask(size);
		u32 sign = M68K_SizeSign(size);
		u32 result = cpu->d[reg] & mask;
		Bool x = (cpu->sr & M68K_SR_X) != 0;
		Bool carry = False;
		Bool overflow = False;
		int n;
		cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
		for (n = 0; n < count; n++)
		{
			Bool old_sign = (result & sign) != 0;
			if (left)
			{
				carry = old_sign;
				if (kind == 2) result = ((result << 1) | (x ? 1 : 0)) & mask;
				else if (kind == 3) result = ((result << 1) | (carry ? 1 : 0)) & mask;
				else result = (result << 1) & mask;
				if ((kind == 0) && (((result & sign) != 0) != old_sign)) overflow = True;
			}
			else
			{
				carry = (result & 1) != 0;
				if (kind == 0) result = (result >> 1) | (old_sign ? sign : 0);
				else if (kind == 2) result = (result >> 1) | (x ? sign : 0);
				else if (kind == 3) result = (result >> 1) | (carry ? sign : 0);
				else result >>= 1;
			}
			if (kind == 2) x = carry;
		}
		if (count != 0)
		{
			if (carry) cpu->sr |= M68K_SR_C;
			if (kind != 3)
			{
				cpu->sr &= ~M68K_SR_X;
				if (carry) cpu->sr |= M68K_SR_X;
			}
		}
		if (overflow) cpu->sr |= M68K_SR_V;
		if ((result & mask) == 0) cpu->sr |= M68K_SR_Z;
		if ((result & sign) != 0) cpu->sr |= M68K_SR_N;
		cpu->d[reg] = (cpu->d[reg] & ~mask) | (result & mask);
		cpu->cycles += 6 + 2*count;
		return M68K_STEP_OK;
	}

	if ((op & 0xffc0) == 0x4ec0 || (op & 0xffc0) == 0x4e80) // JMP/JSR <ea>
	{
		u32 target;
		Bool jsr = (op & 0xffc0) == 0x4e80;
		if (!M68K_ControlEA(cpu, (op >> 3) & 7, op & 7, &target)) goto illegal;
		if (jsr)
		{
			M68K_Push32(cpu, cpu->pc);
			if (cpu->address_error) return M68K_STEP_ADDRESS_ERROR;
		}
		cpu->pc = target;
		cpu->cycles += jsr ? 20 : 12;
		return M68K_STEP_OK;
	}

	if ((op & 0xf1c0) == 0x41c0) // LEA <ea>,An
	{
		u32 addr;
		if (!M68K_ControlEA(cpu, (op >> 3) & 7, op & 7, &addr)) goto illegal;
		cpu->a[(op >> 9) & 7] = addr;
		cpu->cycles += 12;
		return M68K_STEP_OK;
	}

	illegal:
	cpu->fault_pc = op_pc;
	cpu->pc = op_pc;
	return M68K_Illegal(cpu);
}

#endif // USE_EMU_M68K
