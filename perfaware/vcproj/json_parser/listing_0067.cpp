/* ========================================================================

   (C) Copyright 2023 by Molly Rocket, Inc., All Rights Reserved.
   
   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any damages
   arising from the use of this software.
   
   Please see https://computerenhance.com for more information
   
   ======================================================================== */

/* ========================================================================
   LISTING 67
   ======================================================================== */

/* NOTE(casey): _CRT_SECURE_NO_WARNINGS is here because otherwise we cannot
   call fopen(). If we replace fopen() with fopen_s() to avoid the warning,
   then the code doesn't compile on Linux anymore, since fopen_s() does not
   exist there.
   
   What exactly the CRT maintainers were thinking when they made this choice,
   I have no idea. */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t b32;

typedef float f32;
typedef double f64;

struct haversine_pair
{
    f64 X0, Y0;
    f64 X1, Y1;
};

#include "listing_0065_haversine_formula.cpp"
#include "listing_0068_buffer.cpp"
#include "listing_0069_lookup_json_parser.cpp"
#include "ScopedTimer.h"

static buffer ReadEntireFile(char *FileName)
{
    buffer Result = {};
        
    FILE *File = fopen(FileName, "rb");
    if(File)
    {
#if _WIN32
        struct __stat64 Stat;
        _stat64(FileName, &Stat);
#else
        struct stat Stat;
        stat(FileName, &Stat);
#endif
        
        Result = AllocateBuffer(Stat.st_size);
        if(Result.Data)
        {
            if(fread(Result.Data, Result.Count, 1, File) != 1)
            {
                fprintf(stderr, "ERROR: Unable to read \"%s\".\n", FileName);
                FreeBuffer(&Result);
            }
        }
    }
    else
    {
        fprintf(stderr, "ERROR: Unable to open \"%s\".\n", FileName);
    }
    
    return Result;
}

static f64 SumHaversineDistances(u64 PairCount, haversine_pair *Pairs)
{
    f64 Sum = 0;
    
    f64 SumCoef = 1 / (f64)PairCount;
    for(u64 PairIndex = 0; PairIndex < PairCount; ++PairIndex)
    {
        haversine_pair Pair = Pairs[PairIndex];
        f64 EarthRadius = 6372.8;
        f64 Dist = ReferenceHaversine(Pair.X0, Pair.Y0, Pair.X1, Pair.Y1, EarthRadius);
        Sum += SumCoef*Dist;
    }
    
    return Sum;
}

int main(int ArgCount, char **Args)
{
    int Result = 1;
    
    enum class Timers
	{
        Total,
		ReadJSON,
        AllocateBuffer,
		ParsePairs,
		SumDistances,
        Validation,
        Count
	};

    ScopedTimer timers[static_cast<int>(Timers::Count)];

    if((ArgCount == 2) || (ArgCount == 3))
    {
        timers[static_cast<int>(Timers::Total)].Start();
        
        timers[static_cast<int>(Timers::ReadJSON)].Start();
        buffer InputJSON = ReadEntireFile(Args[1]);
        timers[static_cast<int>(Timers::ReadJSON)].Stop();
        
        u32 MinimumJSONPairEncoding = 6*4;
        u64 MaxPairCount = InputJSON.Count / MinimumJSONPairEncoding;
        if(MaxPairCount)
        {
            timers[static_cast<int>(Timers::AllocateBuffer)].Start();
            buffer ParsedValues = AllocateBuffer(MaxPairCount * sizeof(haversine_pair));
            timers[static_cast<int>(Timers::AllocateBuffer)].Stop();

            if(ParsedValues.Count)
            {
				timers[static_cast<int>(Timers::ParsePairs)].Start();
                haversine_pair *Pairs = (haversine_pair *)ParsedValues.Data;
                u64 PairCount = ParseHaversinePairs(InputJSON, MaxPairCount, Pairs);
                timers[static_cast<int>(Timers::ParsePairs)].Stop();

                timers[static_cast<int>(Timers::SumDistances)].Start();
                f64 Sum = SumHaversineDistances(PairCount, Pairs);
                timers[static_cast<int>(Timers::SumDistances)].Stop();
                
                fprintf(stdout, "Input size: %llu\n", InputJSON.Count);
                fprintf(stdout, "Pair count: %llu\n", PairCount);
                fprintf(stdout, "Haversine sum: %.16f\n", Sum);
                
                if(ArgCount == 3)
                {
                    timers[static_cast<int>(Timers::Validation)].Start();
                    buffer AnswersF64 = ReadEntireFile(Args[2]);
                    if(AnswersF64.Count >= sizeof(f64))
                    {
                        f64 *AnswerValues = (f64 *)AnswersF64.Data;
                        
                        fprintf(stdout, "\nValidation:\n");
                        
                        u64 RefAnswerCount = (AnswersF64.Count - sizeof(f64)) / sizeof(f64);
                        if(PairCount != RefAnswerCount)
                        {
                            fprintf(stdout, "FAILED - pair count doesn't match %llu.\n", RefAnswerCount);
                        }
                        
                        f64 RefSum = AnswerValues[RefAnswerCount];
                        fprintf(stdout, "Reference sum: %.16f\n", RefSum);
                        fprintf(stdout, "Difference: %.16f\n", Sum - RefSum);
                        
                        fprintf(stdout, "\n");
                    }
                    timers[static_cast<int>(Timers::Validation)].Stop();
                }
            }
            
            FreeBuffer(&ParsedValues);
        }
        else
        {
            fprintf(stderr, "ERROR: Malformed input JSON\n");
        }

        FreeBuffer(&InputJSON);
        
        Result = 0;

        timers[static_cast<int>(Timers::Total)].Stop();

        static const char* timer_names[] = {
			"Total",
			"Read JSON",
			"Allocate Buffer",
			"Parse Pairs",
			"Sum Distances",
			"Validation",
		};

        f64 total_cycles_inv_f = 100.f / timers[static_cast<int>(Timers::Total)].GetElapsedCycles();
        for (int i = 0; i < static_cast<int>(Timers::Count); ++i)
        {
            const u64 cycles = timers[i].GetElapsedCycles();
            const f64 percent = (f64)cycles * total_cycles_inv_f;
            fprintf(stdout, "%s: %.2f%% (%llu cycles, %f s)\n", timer_names[i], percent, cycles, timers[i].GetElapsedSeconds());
        }

        fprintf(stdout, "Estimated CPU frequency: %f MHz", ScopedTimer::EstimatedCpuFrequency / 1000000.0);
    }
    else
    {
        fprintf(stderr, "Usage: %s [haversine_input.json]\n", Args[0]);
        fprintf(stderr, "       %s [haversine_input.json] [answers.f64]\n", Args[0]);
    }
    
    return Result;
}
