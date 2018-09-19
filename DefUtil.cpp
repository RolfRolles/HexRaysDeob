#define USE_DANGEROUS_FUNCTIONS
#include <hexrays.hpp>
#include "HexRaysUtil.hpp"
#include "DefUtil.hpp"
#include "Config.hpp"

static int debugmsg(const char *fmt, ...)
{
#if UNFLATTENVERBOSE
	va_list va;
	va_start(va, fmt);
	return vmsg(fmt, va);
#endif
	return 0;
}

// Put an mop_t into an mlist_t. The op must be either a register or a stack
// variable.
bool InsertOp(mblock_t *mb, mlist_t &ml, mop_t *op)
{
	if (op->t != mop_r && op->t != mop_S)
		return false;

	// I needed help from Hex-Rays with this line. Some of the example plugins
	// showed how to insert a register into an mlist_t. None of them showed
	// how to insert a stack variable. I figured out a way to do it by reverse
	// engineering Hex-Rays, but it seemed really janky. This is The Official
	// Method (TM).
	mb->append_use_list(&ml, *op, MUST_ACCESS);
	return true;
	
	// For posterity, here was what I came up with on my own for inserting a
	// stack variable into an mlist_t:
/*
		ivl_t ivl(op->s->off | MAX_SUPPORTED_STACK_SIZE, op->size);
		ml.mem.add(ivl);
*/
}

// Ilfak sent me this function in response to a similar support request. It 
// walks backwards through a block, instruction-by-instruction, looking at
// what each instruction defines. It stops when it finds definitions for
// everything in the mlist_t, or when it hits the beginning of the block.
minsn_t *my_find_def_backwards(mblock_t *mb, mlist_t &ml, minsn_t *start)
{
	minsn_t *mend = mb->head;
	for (minsn_t *p = start != NULL ? start : mb->tail; p != NULL; p = p->prev)
	{
		mlist_t def = mb->build_def_list(*p, MAY_ACCESS | FULL_XDSU);
		if (def.includes(ml))
			return p;
	}
	return NULL;
}

// This is a nearly identical version of the function above, except it works
// in the forward direction rather than backwards.
minsn_t *my_find_def_forwards(mblock_t *mb, mlist_t &ml, minsn_t *start)
{
	minsn_t *mend = mb->head;
	for (minsn_t *p = start != NULL ? start : mb->head; p != NULL; p = p->next)
	{
		mlist_t def = mb->build_def_list(*p, MAY_ACCESS | FULL_XDSU);
		if (def.includes(ml))
			return p;
	}
	return NULL;

}

// This function has way too many arguments. Basically, it's a wrapper around
// my_find_def_backwards from above. It is extended in the following ways:
// * If my_find_def_backwards identifies a definition of the variable "op"
//   which is an assignment from another variable, this function then continues
//   looking for numeric assignments to that variable (and recursively so, if
//   that variable is in turn assigned from another variable).
// * It keeps a list of all the assignment instructions it finds along the way,
//   storing them in the vector passed as the "chain" argument.
// * It has support for traversing more than one basic block in a graph, if
//   the bRecursive argument is true. It won't traverse into blocks with more 
//   than one successor if bAllowMultiSuccs is false. In any case, it will 
//   never traverse past the block numbered iBlockStop, if that parameter is
//   non-negative.
bool FindNumericDefBackwards(mblock_t *blk, mop_t *op, mop_t *&opNum, MovChain &chain, bool bRecursive, bool bAllowMultiSuccs, int iBlockStop)
{
	mbl_array_t *mba = blk->mba;

	char buf[1000];
	mlist_t ml;

	if (!InsertOp(blk, ml, op))
		return false;

	// Start from the end of the block. This variable gets updated when a copy
	// is encountered, so that subsequent searches start from the right place.
	minsn_t *mStart = NULL;
	do
	{
		// Told you this function was just a wrapper around 
		// my_find_def_backwards.
		minsn_t *mDef = my_find_def_backwards(blk, ml, mStart);

		// If we did find a definition...
		if (mDef != NULL)
		{
			// Ensure that it's a mov instruction. We don't want, for example,
			// an "stx" instruction, which is assumed to redefine everything
			// until its aliasing information is refined.
			if (mDef->opcode != m_mov)
			{
				mcode_t_to_string(mDef, buf, sizeof(buf));
#if UNFLATTENVERBOSE
				debugmsg("[E] FindNumericDef: found %s\n", buf);
#endif
				return false;
			}

			// Now that we found a mov, add it to the chain.
			chain.emplace_back();
			MovInfo &mi = chain.back();
			mi.opCopy = &mDef->l;
			mi.iBlock = blk->serial;
			mi.insMov = mDef;

			// Was it a numeric assignment?
			if (mDef->l.t == mop_n)
			{
				// Great! We're done.
				opNum = &mDef->l;
				return true;
			}

			// Otherwise, if it was not a numeric assignment, then try to track
			// whatever was assigned to it. This can only succeed if the thing
			// that was assigned was a register or stack variable.
#if UNFLATTENVERBOSE
			qstring qs;
			mDef->l.print(&qs);
			tag_remove(&qs);
			debugmsg("[III] Now tracking %s\n", qs.c_str());
#endif

			// Try to start tracking the other thing...
			ml.clear();
			if (!InsertOp(blk, ml, &mDef->l))
				return false;
			
			// Resume the search from the assignment instruction we just 
			// processed.
			mStart = mDef;
		}

		// Otherwise, we did not find a definition of the currently-tracked 
		// variable on this block. Try to continue if the parameters allow.
		else
		{
			// If recursion was disallowed, or we reached the topmost legal 
			// block, then quit.
			if (!bRecursive || blk->serial == iBlockStop)
				return false;
			
			// If there is more than one predecessor for this block, we don't
			// know which one to follow, so stop.
			if (blk->npred() != 1)
				return false;
			
			// Recurse into sole predecessor block
			int iPred = blk->pred(0);
			blk = mba->get_mblock(iPred);
			
			// If the predecessor has more than one successor, check to see
			// whether the arguments allow that.
			if (!bAllowMultiSuccs && blk->nsucc() != 1)
				return false;
			
			// Resume the search at the end of the new block.
			mStart = NULL;
		}
	} while (true);
	return false;
}

// This function finds a numeric definition by searching in the forward 
// direction.
mop_t *FindForwardNumericDef(mblock_t *blk, mop_t *mop, minsn_t *&assign_insn)
{
	mlist_t ml;
	if (!InsertOp(blk, ml, mop))
		return NULL;

	// Find a forward definition
	assign_insn = my_find_def_forwards(blk, ml, NULL);
	if (assign_insn != NULL)
	{

#if UNFLATTENVERBOSE
		qstring qs;
		assign_insn->print(&qs);
		tag_remove(&qs);
		debugmsg("[III] Forward search found %s\n", qs.c_str());
#endif
		
		// We only want MOV instructions with numeric left-hand sides
		if (assign_insn->opcode != m_mov || assign_insn->l.t != mop_n)
			return NULL;
		
		// Return the numeric operand if we found it
		return &assign_insn->l;
	}
	return NULL;
}

// This function is just a thin wrapper around FindForwardNumericDef, which 
// also inserts the mov into the "chain" argument.
mop_t *FindForwardStackVarDef(mblock_t *mbClusterHead, mop_t *opCopy, MovChain &chain)
{
	// Must be a non-NULL stack variable
	if (opCopy == NULL || opCopy->t != mop_S)
		return NULL;

	minsn_t *ins;
		
	// Find the definition
	mop_t *num = FindForwardNumericDef(mbClusterHead, opCopy, ins);
	if (num == NULL)
		return NULL;

#if UNFLATTENVERBOSE
		qstring qs;
		num->print(&qs);
		tag_remove(&qs);
		debugmsg("[III] Forward method found %s!\n", qs.c_str());
#endif

	// If the found definition was suitable, add the assignment to the chain
	chain.emplace_back();
	MovInfo &mi = chain.back();
	mi.opCopy = num;
	mi.iBlock = mbClusterHead->serial;
	mi.insMov = ins;

	// Return the number
	return num;
}

