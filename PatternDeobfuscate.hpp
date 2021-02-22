#pragma once
#include <hexrays.hpp>

struct ObfCompilerOptimizer : optinsn_t
{
// sfink - i'm not actually sure when the signature changed, but i'm assuming later than 730
#if IDA_SDK_VERSION <= 730
	int func(mblock_t *blk, minsn_t *ins);
#else
    int func(mblock_t* blk, minsn_t* ins, int optflags) override;
#endif
};
