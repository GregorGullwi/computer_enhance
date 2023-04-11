#pragma once

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
	};
};
