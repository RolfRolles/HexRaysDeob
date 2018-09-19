#pragma once
#include <hexrays.hpp>

int RemoveSingleGotos(mbl_array_t *mba);
bool SplitMblocksByJccEnding(mblock_t *pred1, mblock_t *pred2, mblock_t *&endsWithJcc, mblock_t *&nonJcc, int &jccDest, int &jccFallthrough);
int PruneUnreachable(mbl_array_t *mba);

// The "deferred graph modifier" records changes that the client wishes to make
// to a given graph, but does not apply them immediately. Weird things could
// happen if we were to modify a graph while we were iterating over it, so save
// the modifications until we're done iterating over the graph.
struct DeferredGraphModifier
{
	std::vector<std::pair<int, int> > m_RemoveEdges;
	std::vector<std::pair<int, int> > m_AddEdges;
	void Remove(int src, int dest);
	void Add(int src, int dest);
	void Replace(int src, int oldDest, int newDest);
	int Apply(mbl_array_t *mba);
	bool ChangeGoto(mblock_t *blk, int iOld, int iNew);
	void Clear() { m_RemoveEdges.clear(); m_AddEdges.clear(); }
};