#pragma once
#include <map>
#include <hexrays.hpp>
#include "CFFlattenInfo.hpp"
#include "DefUtil.hpp"

struct CFUnflattener : public optblock_t
{
	CFFlattenInfo cfi;
	MovChain m_DeferredErasuresLocal;
	MovChain m_PerformedErasuresGlobal;

	void Clear(bool bFree)
	{
		cfi.Clear(bFree);
		m_DeferredErasuresLocal.clear();
		m_PerformedErasuresGlobal.clear();
	}

	CFUnflattener() { Clear(false); };
	~CFUnflattener() { Clear(true); }
	int idaapi func(mblock_t *blk);
	mblock_t *GetDominatedClusterHead(mbl_array_t *mba, int iDispPred, int &iClusterHead);
	int FindBlockTargetOrLastCopy(mblock_t *mb, mblock_t *mbClusterHead, mop_t *what, bool bAllowMultiSuccs);
	bool HandleTwoPreds(mblock_t *mb, mblock_t *mbClusterHead, mop_t *opCopy, mblock_t *&endsWithJcc, int &actualGotoTarget, int &actualJccTarget);
	void ProcessErasures(mbl_array_t *mba);
};