#pragma once

#include <hexrays.hpp>

struct MovInfo
{
	mop_t *opCopy;
	minsn_t *insMov;
	int iBlock;
};

typedef std::vector<MovInfo> MovChain;

bool FindNumericDefBackwards(mblock_t *blk, mop_t *op, mop_t *&opNum, MovChain &chain, bool bRecursive, bool bAllowMultiSuccs, int iBlockStop = -1);
mop_t *FindForwardStackVarDef(mblock_t *mbClusterHead, mop_t *opCopy, MovChain &chain);