#pragma once
#include <hexrays.hpp>

struct ObfCompilerOptimizer : public optinsn_t
{
	int func(mblock_t *blk, minsn_t *ins);
};
