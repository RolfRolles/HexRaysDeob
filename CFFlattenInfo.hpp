#pragma once
#include <hexrays.hpp>

struct JZInfo
{
	JZInfo() : op(nullptr) {};

	mop_t *op;
	int nSeen;
	std::vector<mop_t *> nums;

	bool ShouldBlacklist();
};

struct CFFlattenInfo
{
	mop_t *opAssigned, *opCompared;
	uint64 uFirst;
	int iFirst, iDispatch;
	std::map<uint64, int> m_KeyToBlock;
	std::map<int, uint64> m_BlockToKey;
	ea_t m_WhichFunc;
	array_of_bitsets *m_DomInfo;
	int *m_DominatedClusters;

	int FindBlockByKey(uint64 key);
	void Clear(bool bFree)
	{
		if (bFree && opAssigned != nullptr)
			delete opAssigned;
		opAssigned = nullptr;

		if (bFree && opCompared != nullptr)
			delete opCompared;
		opCompared = nullptr;

		iFirst = -1;
		iDispatch = -1;
		uFirst = 0LL;
		m_WhichFunc = BADADDR;
		if (bFree && m_DomInfo != nullptr)
			delete m_DomInfo;
		m_DomInfo = nullptr;

		if (bFree && m_DominatedClusters != nullptr)
			delete m_DominatedClusters;
		m_DominatedClusters = nullptr;

		m_KeyToBlock.clear();
		m_BlockToKey.clear();
	};
	CFFlattenInfo() { Clear(false); }
	~CFFlattenInfo() { Clear(true); }
	bool GetAssignedAndComparisonVariables(mblock_t *blk);
};
