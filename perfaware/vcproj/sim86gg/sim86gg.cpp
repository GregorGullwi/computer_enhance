// sim86gg.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <cstring>
#include <intrin.h> // include header for __popcnt()

#include "sim86_shared.h"
#include "../sim86_memory.h"
#include "../sim86_memory.cpp"
#include "../sim86_instruction_table.cpp"
#include "../sim86_instruction.cpp"
#include "../sim86_decode.cpp"
#include "../sim86_text.cpp"

#include "cpu_registers.h"

#pragma comment (lib, "sim86_shared_debug.lib")

static u32 LoadMemoryFromFile(char* FileName, segmented_access SegMem, u32 AtOffset)
{
	u32 Result = 0;

	// NOTE(casey): Because we are simulating a machine, we only attempt to load as
	// much of a file as will fit into that machine's memory. 
	// Any additional bytes are discarded.
	u32 BaseAddress = GetAbsoluteAddressOf(SegMem, AtOffset);
	u32 HighAddress = GetHighestAddress(SegMem);
	u32 MaxBytes = (HighAddress - BaseAddress) + 1;

	FILE* File = nullptr;
	fopen_s(&File, FileName, "rb");
	if (File)
	{
		Result = static_cast<u32>(fread(SegMem.Memory + BaseAddress, 1, MaxBytes, File));
		fclose(File);
	}
	else
	{
		fprintf(stderr, "ERROR: Unable to open %s.\n", FileName);
	}

	return Result;
}

static segmented_access AllocateMemoryPow2(u32 SizePow2)
{
	static u8 FailedAllocationByte;

	u8* Memory = (u8*)malloc(static_cast<u32>(1 << SizePow2));
	if (!Memory)
	{
		SizePow2 = 0;
		Memory = &FailedAllocationByte;
	}

	segmented_access Result = FixedMemoryPow2(SizePow2, Memory);
	return Result;
}

static constexpr u16 NumRegisters = 15;
static constexpr u16 IpRegIndex = 13;
static constexpr u16 FlagsRegIndex = 14;
static SingleRegister Registers[NumRegisters];
static FlagsReg& Flags = Registers[FlagsRegIndex].Flags;

static void PrintFlags(FlagsReg FlagsToPrint)
{
	if (FlagsToPrint.Parity != 0) printf("P");
	if (FlagsToPrint.Zero != 0) printf("Z");
	if (FlagsToPrint.Sign != 0) printf("S");
}

static u8* GetAbsoluteAddressFromOperand(instruction Instruction, u8 OperandIndex)
{
	instruction_operand Operand = Instruction.Operands[OperandIndex];
	u32 Mask = 0xffff >> ((2 - Operand.Register.Count) * 0xff);

	switch (Operand.Type)
	{
	case Operand_None: break;
	case Operand_Register:
	{
		return &Registers[Operand.Register.Index].mem[Operand.Register.Offset];
	}
	case Operand_Memory:
	{
		break;
	}
	case Operand_Immediate:
	{
		static SingleRegister ImmediateRegister;
		ImmediateRegister.wide = Operand.Immediate.Value;
		return &ImmediateRegister.mem[0];
	}
	default:
		break;
	}

	return nullptr;
}

static void ExecuteInstruction(instruction Instruction)
{
	u32 InstructionFlags = Instruction.Flags;
	u32 W = InstructionFlags & Inst_Wide;
	size_t ByteSize = W ? 2 : 1;

	switch (Instruction.Op)
	{
	case Op_mov:
	{
		u8* DstAddr = GetAbsoluteAddressFromOperand(Instruction, 0);
		u8* SrcAddr = GetAbsoluteAddressFromOperand(Instruction, 1);
		std::memcpy(DstAddr, SrcAddr, ByteSize);
	}
	break;
	
	case Op_add:
	case Op_sub:
	case Op_cmp:
	{
		const FlagsReg PrevFlags = Flags;
		u8* DstAddr = GetAbsoluteAddressFromOperand(Instruction, 0);
		u8* SrcAddr = GetAbsoluteAddressFromOperand(Instruction, 1);
		u32 WorkingMemoryDst = 0;
		std::memcpy(&WorkingMemoryDst, DstAddr, ByteSize);
		u32 WorkingMemorySrc = 0;
		std::memcpy(&WorkingMemorySrc, SrcAddr, ByteSize);

		switch (Instruction.Op)
		{
		case Op_add:
			WorkingMemoryDst += WorkingMemorySrc;
			break;
		
		case Op_sub:
		case Op_cmp:
			WorkingMemoryDst -= WorkingMemorySrc;
			break;
		}

		Flags.Parity = ((__popcnt(WorkingMemoryDst & 0xff) & 1) == 0);
		Flags.Zero = (WorkingMemoryDst == 0);
		Flags.Sign = ((WorkingMemoryDst & 0x8000) != 0);

		if (memcmp(&PrevFlags, &Flags, sizeof(FlagsReg)) != 0)
		{
			printf(" flags:");
			PrintFlags(PrevFlags);
			printf("->");
			PrintFlags(Flags);
		}
		
		if (Instruction.Op != Op_cmp)
			std::memcpy(DstAddr, &WorkingMemoryDst, ByteSize);		
	}
	break;

	case Op_jne:
	{
		if (!Flags.Zero)
		{
			assert(Instruction.Operands[0].Type == Operand_Immediate);
			Registers[IpRegIndex].wide += Instruction.Operands[0].Immediate.Value;
		}
	}
	break;

	default:
		printf("Unhandled op code");
		break;
	}
}

static void DisAsm8086(u32 DisAsmByteCount, segmented_access DisAsmStart)
{
	instruction_table Table = Get8086InstructionTable();
	segmented_access IpReg = DisAsmStart;

	do
	{
		IpReg.SegmentOffset = Registers[IpRegIndex].wide;

		instruction Instruction = DecodeInstruction(Table, IpReg);
		if (Instruction.Op)
		{
			Registers[IpRegIndex].wide += Instruction.Size;

			PrintInstruction(Instruction, stdout);
			ExecuteInstruction(Instruction);
			printf("\n");
		}
		else
		{
			fprintf(stderr, "ERROR: Unrecognized binary in instruction stream.\n");
			break;
		}
	} while (Registers[IpRegIndex].wide < DisAsmByteCount);
}

int main(int ArgCount, char** Args)
{
	segmented_access MainMemory = AllocateMemoryPow2(20);
	if (IsValid(MainMemory))
	{
		if (ArgCount > 1)
		{
			for (int ArgIndex = 1; ArgIndex < ArgCount; ++ArgIndex)
			{
				char* FileName = Args[ArgIndex];
				u32 BytesRead = LoadMemoryFromFile(FileName, MainMemory, 0);

				printf("; %s disassembly:\n", FileName);
				printf("bits 16\n");
				DisAsm8086(BytesRead, MainMemory);
			}

			printf("\nFinal registers:\n");
			for (u32 RegIndex = 1; RegIndex < NumRegisters - 1; ++RegIndex)
			{
				const char* RegName = GetRegName({ RegIndex, 0, 2 });
				if (Registers[RegIndex].wide == 0)
					continue;

				printf("%8s: 0x%04x (%d)\n", RegName, Registers[RegIndex].wide, Registers[RegIndex].wide);
			}

			printf("%8s: ", "flags");
			PrintFlags(Flags);
			printf("\n");

		}
		else
		{
			fprintf(stderr, "USAGE: %s [8086 machine code file] ...\n", Args[0]);
		}
	}
	else
	{
		fprintf(stderr, "ERROR: Unable to allow main memory for 8086.\n");
	}
}
