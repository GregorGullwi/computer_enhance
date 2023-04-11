#pragma once

struct FlagsReg
{
	u8 Parity : 1;
	u8 Zero : 1;
	u8 Sign : 1;
};

struct SingleRegister
{
	union
	{
		u16 wide;
		struct
		{			
			u8 low;
			u8 high;
		};
		u8 mem[2];
		FlagsReg Flags;
	};
};
