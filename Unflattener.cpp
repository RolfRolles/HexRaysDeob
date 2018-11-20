// ROLF TODO: Change control flow in func so that PruneUnreachable gets called even if CFI info fails?
// Probably doesn't matter at that point.

#define USE_DANGEROUS_FUNCTIONS
#include <hexrays.hpp>
#include "HexRaysUtil.hpp"
#include "Unflattener.hpp"
#include "CFFlattenInfo.hpp"
#include "TargetUtil.hpp"
#include "DefUtil.hpp"
#include "Config.hpp"

std::set<ea_t> g_BlackList;
std::set<ea_t> g_WhiteList;

static int debugmsg(const char *fmt, ...)
{
#if UNFLATTENVERBOSE
	va_list va;
	va_start(va, fmt);
	return vmsg(fmt, va);
#endif
	return 0;
}

void DumpMBAToFile(mbl_array_t *mba, const char *fpath)
{
	FILE *fp = qfopen(fpath, "w");
	file_printer_t fpt(fp);
	mba->print(fpt);
	qfclose(fp);
}

mba_maturity_t g_Last = MMAT_ZERO;
int g_NumGotosRemoved = 0;
int atThisMaturity = 0;

// Find the block that dominates iDispPred, and which is one of the targets of
// the control flow flattening switch.
mblock_t *CFUnflattener::GetDominatedClusterHead(mbl_array_t *mba, int iDispPred, int &iClusterHead)
{
	mblock_t *mbClusterHead = NULL;
	// Find the block that is targeted by the dispatcher, and that 
	// dominates the block we're currently looking at. This logic won't
	// work for the first block (since it wasn't targeted by the control 
	// flow dispatch switch, so it doesn't have an entry in the dominated
	// cluster information), so we special-case it.
	if (iDispPred == cfi.iFirst)
		iClusterHead = cfi.iFirst, mbClusterHead = mba->get_mblock(cfi.iFirst);
	
	else
	{
		// If it wasn't the first block, look up its cluster head block 
		iClusterHead = cfi.m_DominatedClusters[iDispPred];
		if (iClusterHead < 0)
		{
			debugmsg("[I] Block %d was not part of a dominated cluster\n", iDispPred);
			return NULL;
		}
		mbClusterHead = mba->get_mblock(iClusterHead);
#if UNFLATTENVERBOSE
		debugmsg("[I] Block %d was part of dominated cluster %d\n", iDispPred, iClusterHead);
#endif
	}
	return mbClusterHead;

}

// This function attempts to locate the numeric assignment to a given variable
// "what" starting from the end of the block "mb". It follows definitions 
// backwards, even across blocks, until it either reaches the block 
// "mbClusterHead", or, if the boolean "bAllowMultiSuccs" is false, it will 
// stop the first time it reaches a block with more than one successor.
// If it finds an assignment whose source is a stack variable, then it will not
// be able to continue in the backwards direction, because intervening memory
// writes will make the definition information useless. In that case, it 
// switches to a strategy of searching in the forward direction from 
// mbClusterHead, looking for assignments to that stack variable.
// Information about the chain of assignment instructions along the way are
// stored in the vector called m_DeferredErasuresLocal, a member variable of
// the CFUnflattener class.
int CFUnflattener::FindBlockTargetOrLastCopy(mblock_t *mb, mblock_t *mbClusterHead, mop_t *what, bool bAllowMultiSuccs)
{
	mbl_array_t *mba = mb->mba;
	int iClusterHead = mbClusterHead->serial;

	MovChain local;

	mop_t *opNum = NULL, *opCopy;
	// Search backwards looking for a numeric assignment to "what". We may or 
	// may not find a numeric assignment, but we might find intervening 
	// assignments where "what" is copied from other variables.
	bool bFound = FindNumericDefBackwards(mb, what, opNum, local, true, bAllowMultiSuccs, iClusterHead);
	
	// If we found no intervening assignments to "what", that's bad.
	if (local.empty())
		return -1;
	
	// opCopy now contains the last non-numeric assignment that we saw before
	// FindNumericDefBackwards terminated (either due to not being able to 
	// follow definitions, or, if bAllowMultiSuccs is true, because it recursed
	// into a block with more than one successor.
	opCopy = local.back().opCopy;
	
	// Copy the assignment chain into the erasures vector, so we can later 
	// remove them if our analysis succeeds.
	m_DeferredErasuresLocal.insert(m_DeferredErasuresLocal.end(), local.begin(), local.end());

	// If we didn't find a numeric definition, but we did find an assignment 
	// from a stack variable, switch to a forward analysis from the beginning
	// of the cluster. If we don't find it, this is not necessarily an 
	// indication that the analysis failed; for blocks with two successors,
	// we do further analysis.
	if (!bFound && opCopy != NULL && opCopy->t == mop_S)
	{
		mop_t *num = FindForwardStackVarDef(mbClusterHead, opCopy, local);
		if (num)
			opNum = num, bFound = true;
		else
		{
#if UNFLATTENVERBOSE
			debugmsg("[EEE] Forward method also failed\n");
#endif
		}

	}
	
	// If we found a numeric assignment...
	if (bFound)
	{
		// Look up the integer number of the block corresponding to that value.
		int iDestNo = cfi.FindBlockByKey(opNum->nnn->value);
		
		// If we couldn't find the block, that's bad news. 
		if (iDestNo < 0)
			msg("[E] Block %d assigned unknown key %llx to assigned var\n", mb->serial, opNum->nnn->value);
		
		// Otherwise, we win! Return the block number.
		else
			return iDestNo;
	}
	
	// Negative return code indicates failure.
	return -1;
}

// This function is used for unflattening constructs that have two successors,
// such as if statements. Given a block that assigns to the assignment variable
// that has two predecessors, analyze each of the predecessors looking for 
// numeric assignments by calling the previous function.
bool CFUnflattener::HandleTwoPreds(mblock_t *mb, mblock_t *mbClusterHead, mop_t *opCopy, mblock_t *&nonJcc, int &actualGotoTarget, int &actualJccTarget)
{
	char buf[1000];
	mbl_array_t *mba = mb->mba;
	int iDispPred = mb->serial;
	int iClusterHead = mbClusterHead->serial;

	// No really, don't call this function on a block that doesn't have two 
	// predecessors. I was kind enough to warn you in the documentation; now
	// you get an assertion failure.
	assert(mb->npred() == 2);

	mblock_t *pred1 = mba->get_mblock(mb->pred(0));
	mblock_t *pred2 = mba->get_mblock(mb->pred(1));

	mblock_t *endsWithJcc = NULL;
	nonJcc = NULL;
	int jccDest = -1, jccFallthrough = -1;

	// Given the two predecessors, find the block with the conditional jump at
	// the end of it (store the block in "endsWithJcc") and the one without
	// (store it in nonJcc). Also find the block number of the jcc target, and
	// the block number of the jcc fallthrough (i.e., the block number of 
	// nonJcc).
	if (!SplitMblocksByJccEnding(pred1, pred2, endsWithJcc, nonJcc, jccDest, jccFallthrough))
	{
		debugmsg("[I] Block %d w/preds %d, %d did not have one predecessor ending in jcc, one without\n", iDispPred, pred1->serial, pred2->serial);
		return false;
	}

	// Sanity checking the structure of the graph. The nonJcc block should only
	// have one incoming edge...
	if (nonJcc->npred() != 1)
	{
		debugmsg("[I] Block %d w/preds %d, %d, non-jcc pred %d had %d predecessors (not 1)\n", iDispPred, pred1->serial, pred2->serial, nonJcc->serial, nonJcc->npred());
		return false;
	}
	
	// ... namely, from the block ending with the jcc.
	if (nonJcc->pred(0) != endsWithJcc->serial)
	{
		debugmsg("[I] Block %d w/preds %d, %d, non-jcc pred %d did not have the other as its predecessor\n", iDispPred, pred1->serial, pred2->serial, nonJcc->serial);
		return false;
	}

	// Call the previous function to locate the numeric definition of the 
	// variable that is used to update the assignment variable if the jcc is
	// not taken.
	actualGotoTarget = FindBlockTargetOrLastCopy(endsWithJcc, mbClusterHead, opCopy, false);
	
	// If that succeeded...
	if (actualGotoTarget >= 0)
	{
		// ... then do the same thing when the jcc is not taken.
		actualJccTarget = FindBlockTargetOrLastCopy(nonJcc, mbClusterHead, opCopy, true);
		
		// If that succeeded, great! We can unflatten this two-way block.
		if (actualJccTarget >= 0)
			return true;
	}
	return false;
}

// Erase the now-superfluous chain of instructions that were used to copy a
// numeric value into the assignment variable.
void CFUnflattener::ProcessErasures(mbl_array_t *mba)
{
	m_PerformedErasuresGlobal.insert(m_PerformedErasuresGlobal.end(), m_DeferredErasuresLocal.begin(), m_DeferredErasuresLocal.end());
	for (auto erase : m_DeferredErasuresLocal)
	{
#if UNFLATTENVERBOSE
		qstring qs;
		erase.insMov->print(&qs);
		tag_remove(&qs);
		msg("[I] Erasing %a: %s\n", erase.insMov->ea, qs.c_str());
#endif
		// Be gone, sucker
		mba->get_mblock(erase.iBlock)->make_nop(erase.insMov);
	}

	m_DeferredErasuresLocal.clear();
}

/*
// This method was suggested by Hex-Rays to force block recombination, as 
// opposed to my own function PruneUnreachable. At present, it does not do what
// it's supposed to, so I'm continuing to use my own code for now.

#define MBA_CMBBLK   0x00000400 // request to combine blocks
void RequestBlockCombination(mbl_array_t *mba)
{
	uint32 *flags = reinterpret_cast<uint32 *>(mba);
	*flags |= MBA_CMBBLK;
}
*/

// This is the top-level un-flattening function for an entire graph. Hex-Rays
// calls this function since we register our CFUnflattener class as a block
// optimizer.
int idaapi CFUnflattener::func(mblock_t *blk)
{
	char buf[1000];
	vd_printer_t vd;

	// Was this function blacklisted? Skip it if so
	mbl_array_t *mba = blk->mba;
	if (g_BlackList.find(mba->entry_ea) != g_BlackList.end())
		return 0;

#if UNFLATTENVERBOSE || UNFLATTENDEBUG
	const char *matStr = MicroMaturityToString(mba->maturity);
#endif
#if UNFLATTENVERBOSE
	debugmsg("[I] Block optimization called at maturity level %s\n", matStr);
#endif

	// Only operate once per maturity level
	if (g_Last == mba->maturity)
		return 0;

	// Update the maturity level
	g_Last = mba->maturity;

#if UNFLATTENDEBUG
	// If we're debugging, save a copy of the graph on disk
	snprintf(buf, sizeof(buf), "c:\\temp\\dumpBefore-%s-%d.txt", matStr, atThisMaturity);
	DumpMBAToFile(mba, buf);
#endif

	// We only operate at MMAT_LOCOPT
	if (mba->maturity != MMAT_LOCOPT)
		return 0;

	int iChanged = 0;
	
	// If local optimization has just been completed, remove transfer-to-gotos
	iChanged = RemoveSingleGotos(mba);
	//return iChanged;

#if UNFLATTENVERBOSE
	debugmsg("\tRemoved %d vacuous GOTOs\n", iChanged);
#endif

#if UNFLATTENDEBUG
	snprintf(buf, sizeof(buf), "c:\\temp\\dumpAfter-%s-%d.txt", matStr, atThisMaturity);
	DumpMBAToFile(mba, buf);
#endif

	// Might as well verify we haven't broken anything
	if (iChanged)
		mba->verify(true);

#if UNFLATTENVERBOSE
		mba->print(vd);
#endif

	// Get the preliminary information needed for control flow flattening, such
	// as the assignment/comparison variables.
	if (!cfi.GetAssignedAndComparisonVariables(blk))
	{
		debugmsg("[E] Couldn't get control-flow flattening information\n");
		return iChanged;
	}

	// Create an object that allows us to modify the graph at a future point.
	DeferredGraphModifier dgm;
	bool bDirtyChains = false;

	// Iterate through the predecessors of the top-level control flow switch
	for (auto iDispPred : mba->get_mblock(cfi.iDispatch)->predset)
	{
		mblock_t *mb = mba->get_mblock(iDispPred);
		
		// The predecessors should only have one successor, i.e., they should 
		// directly branch to the dispatcher, not in a conditional fashion
		if (mb->nsucc() != 1)
		{
			debugmsg("[I] Block %d had %d successors, not 1\n", iDispPred, mb->nsucc());
			continue;
		}
		
		// Find the block that dominates this cluster, or skip this block if
		// we can't. This ensures that we only try to unflatten parts of the
		// control flow graph that were actually flattened. Also, we need the
		// cluster head so we know where to bound our searches for numeric 
		// definitions.
		int iClusterHead;
		mblock_t *mbClusterHead = GetDominatedClusterHead(mba, iDispPred, iClusterHead);
		if (mbClusterHead == NULL)
			continue;

		// It's best to process erasures for every block we unflatten 
		// immediately, so we don't end up duplicating instructions that we 
		// want to eliminate
		m_DeferredErasuresLocal.clear();

		// Try to find a numeric assignment to the assignment variable, but 
		// pass false for the last parameter so that the search stops if it 
		// reaches a block with more than one successor. This ought to succeed
		// if the flattened control flow region only has one destination, 
		// rather than two destinations for flattening of if-statements.
		int iDestNo = FindBlockTargetOrLastCopy(mb, mbClusterHead, cfi.opAssigned, false);
		
		// Couldn't find any assignments at all to the assignment variable?
		// That's bad, don't continue.
		if (m_DeferredErasuresLocal.empty())
			continue;

		// Did we find a block target? Great; just update the CFG to point the
		// destination directly to its target, rather than back to the 
		// dispatcher.
		if (iDestNo >= 0)
		{
			// Make a note to ourselves to modify the graph structure later
			dgm.ChangeGoto(mb, cfi.iDispatch, iDestNo);
			
			// Erase the intermediary assignments to the assignment variable
			ProcessErasures(mba);

#if UNFLATTENVERBOSE
			msg("[I] Changed goto on %d to %d\n", iDispPred, iDestNo);
#endif

			++iChanged;
			continue;
		}

		// Stash off a copy of the last variable in the chain of assignments
		// to the assignment variable, as well as the assignment instruction 
		// (the latter only for debug-printing purposes).
		mop_t *opCopy = m_DeferredErasuresLocal.back().opCopy;
		minsn_t *m = m_DeferredErasuresLocal.back().insMov;

#if UNFLATTENVERBOSE
		debugmsg("[I] Block %d did not define assign a number to assigned var; assigned %s instead\n", iDispPred, mopt_t_to_string(m->l.t));
#endif

		// If the block we're currently examining has more than two 
		// predecessors, that's unexpected, so stop.
		if (mb->npred() != 2)
		{
#if UNFLATTENVERBOSE
			debugmsg("[I] Block %d that assigned non-numeric value had %d predecessors, not 2\n", iDispPred, mb->npred());
#endif
			continue;
		}

		mblock_t *nonJcc;
		int actualGotoTarget, actualJccTarget;
		
		// Call the function that handles the case of a conditional assignment
		// to the assignment variable (i.e., the flattened version of an 
		// if-statement).
		if (HandleTwoPreds(mb, mbClusterHead, opCopy, nonJcc, actualGotoTarget, actualJccTarget))
		{
			// If it succeeded...
			
			// Get rid of the superfluous assignments
			ProcessErasures(mba);
			
			// Make a note to ourselves to modify the graph structure later,
			// for the non-taken side of the conditional. Change the goto 
			// target.
			dgm.Replace(mb->serial, cfi.iDispatch, actualGotoTarget);
			mb->tail->l.b = actualGotoTarget;

			// Mark that the def-use information will need re-analyzing
			bDirtyChains = true;
			
			// Copy the instructions from the block that targets the dispatcher
			// onto the end of the jcc taken block.
			minsn_t *mbHead = mb->head;
			minsn_t *mbCurr = mbHead;
			do
			{
				minsn_t *mCopy = new minsn_t(*mbCurr);
				nonJcc->insert_into_block(mCopy, nonJcc->tail);
				mbCurr = mbCurr->next;

#if UNFLATTENVERBOSE
				mcode_t_to_string(nonJcc->tail, buf, sizeof(buf));
				debugmsg("[I] %d: tail is %s\n", nonJcc->serial, buf);
#endif

			} while (mbCurr != NULL);

			
			// Make a note to ourselves to modify the graph structure later,
			// for the taken side of the conditional. Change the goto target.
			dgm.Replace(nonJcc->serial, mb->serial, actualJccTarget);
			nonJcc->tail->l.b = actualJccTarget;
			
			// We added instructions to the nonJcc block, so its def-use lists
			// are now spoiled. Mark it dirty.
			nonJcc->mark_lists_dirty();
		}
	} // end for loop that unflattens all blocks
	
	// After we've processed every block, apply the deferred modifications to
	// the graph structure.
	iChanged += dgm.Apply(mba);

	// If we modified the graph structure, hopefully some blocks (especially 
	// those making up the control flow dispatch switch, but also perhaps
	// intermediary goto-to-goto blocks) will now be unreachable. Prune them,
	// so that later optimization phases don't have to consider their contents
	// anymore and can do a better job.
	if (iChanged != 0)
	{
		int nRemoved = PruneUnreachable(mba);
		iChanged += nRemoved;
#if UNFLATTENVERBOSE
		msg("[I] Removed %d blocks\n", nRemoved);
#endif
	}

	// If there were any two-way conditionals, that means we copied 
	// instructions onto the jcc taken blocks, which means the def-use info is
	// stale. Mark them dirty, and perform local optimization for the lulz too.
	if (bDirtyChains)
	{
#if IDA_SDK_VERSION == 710
		mba->make_chains_dirty();
#elif IDA_SDK_VERSION >= 720
		mba->mark_chains_dirty();
#endif
		mba->optimize_local(0);
	}

	// If we changed the graph, verify that we did so legally.
	if (iChanged != 0)
		mba->verify(true);

	return iChanged;
}
