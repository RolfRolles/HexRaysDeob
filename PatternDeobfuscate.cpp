#include <hexrays.hpp>
#include "HexRaysUtil.hpp"
#include "PatternDeobfuscateUtil.hpp"
#include "Config.hpp"

// Our pattern-based deobfuscation is implemented as an optinsn_t structure,
// which allows us to hook directly into the microcode generation phase and
// perform optimizations automatically, whenever code is decompiled.
struct ObfCompilerOptimizer : public optinsn_t
{
	// This function simplifies microinstruction patterns that look like
	// either: (x & 1) | (y & 1) ==> (x | y) & 1
	// or:     (x & 1) ^ (y & 1) ==> (x ^ y) & 1
	// Though it may not seem like much of an "obfuscation" or "deobfuscation"
	// technique on its own, getting rid of the "&1" terms helps reveal other
	// patterns so they can be deobfuscated.
	int pat_LogicAnd1(minsn_t *ins)
	{
		// Only applies to OR / XOR microinstructions
		if (ins->opcode != m_or && ins->opcode != m_xor)
			return 0;

		// Only applies when the operands are results of other 
		// microinstructions (since, after all, we are expecting them to be
		// ANDed by 1, which is represented in terms of a microinstruction
		// provider mop_d operand).
		if (ins->l.t != mop_d || ins->r.t != mop_d)
			return 0;

		minsn_t *insLeft, *insRight;
		mop_t *opLeft, *opRight;
		
		// Get rid of & 1. bLeft1 is true if there was an &1. 
		bool bLeft1 = TunnelThroughAnd1(ins->l.d, insLeft, true, &opLeft);
		if (!bLeft1)
			return 0;
		
		// Same for right-hand side
		bool bRight1 = TunnelThroughAnd1(ins->r.d, insRight, true, &opRight);
		if (!bRight1)
			return 0;

		// If we get here, then the pattern matched.
		// Move the logical operation (OR or XOR) to the left-hand side,
		// with the operands that have the &1 removed.
		ins->l.d->opcode = ins->opcode;
		ins->l.d->l.swap(*opLeft);
		ins->l.d->r.swap(*opRight);
		
		// Change the top-level instruction from OR or XOR to AND, and set the
		// right-hand side to the 1-bit constant value 1.
		ins->opcode = m_and;
		ins->r.make_number(1, 1);
		
		// msg("[I] pat_LogicAnd1\n");
		// Return 1 to indicate that we changed the instruction.
		return 1;
	}
	
	// One of the obfuscation patterns involves a subtraction by 1. In the 
	// assembly code, this is implemented by something like:
	//
	// add eax, 2
	// add eax, ecx ; or whatever
	// sub eax, 3
	//
	// Usually, Hex-Rays will automatically simplify this to (eax+ecx)-1. 
	// However, I did experience situations where Hex-Rays still represented
	// the decompiled output as 2+(eax+ecx)-3. This function, then, determines
	// when Hex-Rays has represented the subtraction as just mentioned. If so,
	// it extracts the term that is being subtracted by 1.
	bool pat_IsSubBy1(minsn_t *ins, mop_t *&op)
	{
		// We're looking for x+(y-z), where x and z are numeric
		if (ins->opcode != m_add)
			return false;

		// Extract x and (y-z)
		mop_t *opAddNum = NULL, *opAddNonNum = NULL;
		if (!ExtractNumAndNonNum(ins, opAddNum, opAddNonNum))
			return false;

		// Ensure that the purported (y-z) term actually is a subtraction
		if (opAddNonNum->t != mop_d || opAddNonNum->d->opcode != m_sub)
			return false;

		// Extract y and z. I guess technically I shouldn't use 
		// ExtractNumAndNonNum here since subtraction isn't commutative...
		// Call that a bug, but it didn't matter in practice.
		mop_t *opSubNum = NULL, *opSubNonNum = NULL;
		if (!ExtractNumAndNonNum(opAddNonNum->d, opSubNum, opSubNonNum))
			return false;

		// Pass y back to the caller
		op = opSubNonNum;
		
		// x-z must be -1, or, equivalently, z-x must be 1.
		return (opSubNum->nnn->value - opAddNum->nnn->value) == 1LL;
	}

	// This function performs the following pattern-substitution:
	// (x * (x-1)) & 1 ==> 0
	int pat_MulSub(minsn_t *andIns)
	{
		// Topmost term has to be &1. The 1 is not required to be 1-byte large.
		minsn_t *ins = andIns;
		if (!TunnelThroughAnd1(ins, ins, false))
			return 0;
		
		// Looking for multiplication terms
		if (ins->opcode != m_mul)
			return 0;

		// We have two different mechanisms for determining if there is a
		// subtraction by 1.
		bool bWasSubBy1 = false;

		// Ultimately, we need to find thse things:
		minsn_t *insSub;    // Subtraction instruction x-1
		mop_t *opMulNonSub; // Operand of multiply that isn't a subtraction
		mop_t *subNonNum;   // x from the x-1 instruction
		
		// Try first method for locating subtraction by 1, i.e., simply 
		// subtraction by the constant number 1.
		do
		{
			// Find the subtraction subterm of the multiplication
			if (!ExtractByOpcodeType(ins, m_sub, insSub, opMulNonSub))
				break;

			mop_t *subNum;
			// Find the numeric part of the subtraction. Again, I shouldn't use
			// ExtractNumAndNonNum here since subtraction isn't commutative.
			if (!ExtractNumAndNonNum(insSub, subNum, subNonNum))
				break;

			// Ensure that the subtraction amount is 1.
			if (subNum->nnn->value != 1)
				break;

			// Indicate that we successfully found the subtraction.
			bWasSubBy1 = true;
		} while (0);

		// If we didn't find the subtraction, see if we have an add/sub pair
		// instead, which totals to subtraction minus one.
		if (!bWasSubBy1)
		{
			// Find the ADD subterm of the multiplication. If this fails, both
			// methods failed to find the pattern, so return.
			if (!ExtractByOpcodeType(ins, m_add, insSub, opMulNonSub))
				return 0;
			
			// Call the previous function to determine whether the ADD 
			// implements a subtraction by 1.
			bWasSubBy1 = pat_IsSubBy1(insSub, subNonNum);
		}
		
		// If both methods failed, bail.
		if (!bWasSubBy1)
			return 0;

		// We know we're dealing with (x-1) * y. ensure x==y.
		if (!equal_mops_ignore_size(*opMulNonSub, *subNonNum))
			return 0;

		// If we get here, the pattern matched.
		// Replace the whole multiplication instruction by 0.
		ins->l.make_number(0, ins->l.size);
		andIns->optimize_flat();
		// msg("[I] pat_MulSub\n");
		return 1;
	}

	// This function looks tries to replace patterns of the form
	// either: (x&y)|(x^y)   ==> x|y
	// or:     (x&y)|(y^x)   ==> x|y
	int pat_OrViaXorAnd(minsn_t *ins)
	{
#if OPTVERBOSE
		qstring qIns;
		ins->print(&qIns);
		msg("Trying to optimize jcc cond: %s\n", qIns.c_str());
#endif
		// Looking for OR instructions...
		if (ins->opcode != m_or)
			return 0;

		// ... where one side is a compound XOR, and the other is not ...
		minsn_t *xorInsn;
		mop_t *nonXorOp;
		if (!ExtractByOpcodeType(ins, m_xor, xorInsn, nonXorOp))
			return 0;

		// .. and the other side is a compound AND ...
		if (nonXorOp->t != mop_d || nonXorOp->d->opcode != m_and)
			return 0;

		// Extract the operands for the AND and XOR terms
		mop_t *xorOp1 = &xorInsn->l, *xorOp2 = &xorInsn->r;
		mop_t *andOp1 = &nonXorOp->d->l, *andOp2 = &nonXorOp->d->r;

		// The operands must be equal
		if (!(equal_mops_ignore_size(*xorOp1, *andOp1) && equal_mops_ignore_size(*xorOp2, *andOp2)) ||
 			 (equal_mops_ignore_size(*xorOp1, *andOp2) && equal_mops_ignore_size(*xorOp2, *andOp1)))
			return 0;

		// Move the operands up to the top-level OR instruction
		ins->l.swap(*xorOp1);
		ins->r.swap(*xorOp2);
		ins->optimize_flat();
		// msg("[I] pat_OrViaXorAnd\n");
		return 1;
	}

	// This pattern replaces microcode of the form (x|!x), where x is a 
	// conditional, and !x is its syntactically-negated version, with 1.
	int pat_OrNegatedSameCondition(minsn_t *ins)
	{
#if OPTVERBOSE
		qstring qIns;
		ins->print(&qIns);
		msg("Trying to optimize jcc cond: %s\n", qIns.c_str());
#endif
		// Only applies to (x|y)
		if (ins->opcode != m_or)
			return 0;

		// Only applies when x and y are compound expressions, i.e., results
		// of other microcode instructions.
		if (ins->l.t != mop_d || ins->r.t != mop_d)
			return 0;

		// Ensure x and y are syntactically-opposite versions of the same 
		// conditional.
		if (!AreConditionsOpposite(ins->l.d, ins->r.d))
			return 0;
			
		// If we get here, the pattern matched. Replace both sides of OR with
		// 1, and then call optimize_flat to fold the constants.
		ins->l.make_number(1, 1);
		ins->r.make_number(1, 1);
		ins->optimize_flat();
		// msg("[I] pat_OrNegatedSameCondition\n");
		return 1;
	}

	// Replace patterns of the form (x&c)|(~x&d) (when c and d are numbers such 
	// that c == ~d) with x^d.
	int pat_OrAndNot(minsn_t *ins)
	{
		// Looking for OR instructions...
		if(ins->opcode != m_or)
			return 0;
		
		// ... with compound operands ...
		if (ins->l.t != mop_d || ins->r.t != mop_d)
			return 0;

		minsn_t *lhs1 = ins->l.d;
		minsn_t *rhs1 = ins->r.d;

		// ... where each operand is an AND ...
		if (lhs1->opcode != m_and || rhs1->opcode != m_and)
			return 0;

		// Extract the numeric and non-numeric operands from both AND terms
		mop_t *lhsNum = NULL, *rhsNum = NULL;
		mop_t *lhsNonNum = NULL, *rhsNonNum = NULL;
		bool bLhsSucc = ExtractNumAndNonNum(lhs1, lhsNum, lhsNonNum);
		bool bRhsSucc = ExtractNumAndNonNum(rhs1, rhsNum, rhsNonNum);

		// ... both AND terms must have one constant ...
		if (!bLhsSucc || !bRhsSucc)
			return 0;

		// .. both constants have a size, and are the same size ...
		if (lhsNum->size == NOSIZE || lhsNum->size != rhsNum->size)
			return 0;
		
		// ... and the constants are bitwise inverses of one another ...		
		if ((lhsNum->nnn->value & rhsNum->nnn->value) != 0)
			return 0;

		// One of the non-numeric parts must have a binary not (i.e., ~) on it
		minsn_t *sourceOfResult = NULL;
		mop_t *nonNottedInsn = NULL, *nottedNum = NULL, *nottedInsn = NULL;

		// Check the left-hand size for binary not
		if (lhsNonNum->t == mop_d && lhsNonNum->d->opcode == m_bnot)
		{
			// Extract the NOTed term
			nottedInsn = &lhsNonNum->d->l;
			// Make note of the corresponding constant value
			nottedNum = lhsNum;
		}
		else
			nonNottedInsn = lhsNonNum;

		// Check the left-hand size for binary not
		if (rhsNonNum->t == mop_d && rhsNonNum->d->opcode == m_bnot)
		{
			// Both sides NOT? Not what we want, return 0
			if (nottedInsn != NULL)
				return 0;

			// Extract the NOTed term
			nottedInsn = &rhsNonNum->d->l;
			// Make note of the corresponding constant value
			nottedNum = rhsNum;
		}
		else
		{
			// Neither side has a NOT? Bail
			if (nonNottedInsn != NULL)
				return 0;
			nonNottedInsn = rhsNonNum;
		}
		
		// The expression that was NOTed must match the non-NOTed operand
		if (!equal_mops_ignore_size(*nottedInsn, *nonNottedInsn))
			return 0;

		// Okay, all of our conditions matched. Make an XOR(x,d) instruction
		ins->opcode = m_xor;
		ins->l.swap(*nonNottedInsn);
		ins->r.swap(*nottedNum);
		// msg("[I] pat_OrAndNot\n");
		return 1;
	}

	// Remove XOR chains with common terms. E.g. x^5^y^6^5^x ==> y^6.
	// This uses the XorSimplifier class from PatternDeobfuscateUtil.
	int pat_XorChain(minsn_t *ins)
	{
		if (ins->opcode != m_xor)
			return 0;

#if OPTVERBOSE
		qstring qInsBefore, qInsAfter;
		ins->print(&qInsBefore);
#endif

		// Automagically find duplicated expressions and erase them
		XorSimplifier xs;
		if (!xs.Simplify(ins))
			return 0;

#if OPTVERBOSE
		ins->print(&qInsAfter);
		msg("[I] Optimized XOR from:\n\t%s\nto:\t%s\n", qInsBefore.c_str(), qInsAfter.c_str());
#endif
		// msg("[I] pat_XorChain\n");
		return 1;

	}

	// Compare two sets of mop_t * element-by-element. Return true if they match.
	bool NonConstSetsMatch(std::set<mop_t *> *s1, std::set<mop_t *> *s2)
	{
		// Iterate over one set
		for (auto eL : *s1)
		{
			bool bFound = false;
			// Iterate over the other set
			for (auto eR : *s2)
			{
				// Compare the element from the first set against the ones in
				// the other set.
				if (equal_mops_ignore_size(*eL, *eR))
				{
					bFound = true;
					break;
				}
			}
			// If we can't find some element from the first set in the other, we're done
			if (!bFound)
				return false;
		}
		// All elements matched
		return true;
	}

	// Compare two sets of mop_t * (number values) element-by-element. There 
	// should be one value in the larger set that's not in the smaller set. 
	// Find and return it if that's the case.
	mop_t *FindNonCommonConstant(std::set<mop_t *> *smaller, std::set<mop_t *> *bigger)
	{
		mop_t *noMatch = NULL;
		// Iterate through the larger set
		for (auto eL : *bigger)
		{
			bool bFound = false;
			// Find each element in the smaller set
			for (auto eR : *smaller)
			{
				if (equal_mops_ignore_size(*eL, *eR))
				{
					bFound = true;
					break;
				}
			}
			// We're looking for one constant in the larger set that isn't 
			// present in the smaller set.
			if (!bFound)
			{
				// If noMatch was not NULL, then there was more than one 
				// constant in the larger set that wasn't in the smaller one,
				// so return NULL on failure.
				if (noMatch != NULL)
					return 0;

				noMatch = eL;
			}
		}
		// Return the constant from the larger set that wasn't in the smaller
		return noMatch;
	}

	// Matches patterns of the form:
	// (a^b^c^d) & (a^b^c^d^e) => (a^b^c^d) & ~e, where e is numeric
	// The terms don't necessarily have to be in the same order; we extract the
	// XOR subterms from both sides and find the missing value from the smaller
	// XOR chain.
	int pat_AndXor(minsn_t *ins)
	{
		// Instruction must be AND ...
		if (ins->opcode != m_and)
			return 0;

		// ... at least one side must be XOR ...
		bool bLeftIsNotXor  = ins->l.t != mop_d || ins->l.d->opcode == m_xor;
		bool bRightIsNotXor = ins->r.t != mop_d || ins->r.d->opcode == m_xor;
		if (!bLeftIsNotXor && !bRightIsNotXor)
			return 0;
		
		// Collect the constant and non-constant parts of the XOR chains. We 
		// use the XorSimplifier class, but we don't actually simplify the 
		// instruction; we just make use of the existing functionality to 
		// collect the operands that are XORed together.
		XorSimplifier xsL, xsR;
		xsL.Insert(&ins->l);
		xsR.Insert(&ins->r);
		
		// There must be the same number of non-constant terms on both sides
		if (xsL.m_NonConst.size() != xsR.m_NonConst.size())
			return 0;

		bool bLeftIsSmaller;
		std::set<mop_t *> *smaller, *bigger;

		// Either the left is one bigger than the right...
		if (xsL.m_Const.size() == xsR.m_Const.size() + 1)
			smaller = &xsR.m_Const, bigger = &xsL.m_Const, bLeftIsSmaller = false;

		// Or the right is one bigger than the left...
		else
		if (xsR.m_Const.size() == xsL.m_Const.size() + 1)
			smaller = &xsL.m_Const, bigger = &xsR.m_Const, bLeftIsSmaller = true;

		// Or, the pattern doesn't match, so return 0.
		else
			return 0;

		// The sets of non-constant operands must match
		if (!(NonConstSetsMatch(&xsL.m_NonConst, &xsR.m_NonConst)))
			return 0;

		// Find the one constant value that wasn't common to both sides
		mop_t *noMatch = FindNonCommonConstant(smaller, bigger);
		
		// If there wasn't one, the pattern failed, so return 0
		if (noMatch == NULL)
			return 0;

		// Invert the non-common number and truncate it down to its proper size
		noMatch->nnn->update_value(~noMatch->nnn->value & ((1ULL << (noMatch->size * 8)) - 1));
		
		// Replace the larger XOR construct with the now-inverted value
		if (bLeftIsSmaller)
			ins->r.swap(*noMatch);
		else
			ins->l.swap(*noMatch);

		// msg("[I] pat_AndXor\n");
		return 1;
	}

	// Replaces conditionals of the form !(!c1 || !c2) with (c1 && c2).
	int pat_LnotOrLnotLnot(minsn_t *ins)
	{
		// The whole expression must be logically negated.
		minsn_t *inner;
		if (!ExtractLogicallyNegatedTerm(ins, inner) || inner == NULL)
			return 0;

		// The thing that was negated must be an OR with compound operands.
		if (inner->opcode != m_or || inner->l.t != mop_d || inner->r.t != mop_d)
			return 0;

		// The two compound operands must also be negated
		minsn_t *insLeft = inner->l.d;
		minsn_t *insRight = inner->r.d;
		mop_t *opLeft, *opRight;
		if (!ExtractLogicallyNegatedTerm(inner->l.d, insLeft, &opLeft) || !ExtractLogicallyNegatedTerm(inner->r.d, insRight, &opRight))
			return 0;

		// If we're here, the pattern matched. Make the AND.
		ins->opcode = m_and;
		ins->l.swap(*opLeft);
		ins->r.swap(*opRight);
		// msg("[I] pat_LnotOrLnotLnot\n");
		return 1;
	}

	// Replaces terms of the form ~(~x | n), where n is a number, with x & ~n.
	int pat_BnotOrBnotConst(minsn_t *ins)
	{
		// We're looking for BNOT instructions (~y)...
		if (ins->opcode != m_bnot || ins->l.t != mop_d)
			return 0;

		// ... where x is an OR instruction ...
		minsn_t *inner = ins->l.d;
		if (inner->opcode != m_or)
			return 0;

		// ... and one side is constant, where the other one isn't ...
		mop_t *orNum, *orNonNum;
		if (!ExtractNumAndNonNum(inner, orNum, orNonNum))
			return 0;

		// ... and the non-constant part is itself a BNOT instruction (~x)
		if (orNonNum->t != mop_d || orNonNum->d->opcode != m_bnot)
			return 0;

		// Once we found it, rewrite the top-level BNOT with an AND
		ins->opcode = m_and;
		ins->l.swap(orNonNum->d->l);

		// Invert the numeric part
		uint64 notNum = ~(orNum->nnn->value) & ((1ULL << (orNum->size * 8)) - 1);
		ins->r.make_number(notNum, orNum->size);

		return 1;
	}
	
	// This function just inspects the instruction and calls the 
	// pattern-replacement functions above to perform deobfuscation.
	int Optimize(minsn_t *ins)
	{
		int iLocalRetVal = 0;

		switch (ins->opcode)
		{
		case m_bnot:
			iLocalRetVal = pat_BnotOrBnotConst(ins);
			break;
		case m_or:
			iLocalRetVal = pat_OrAndNot(ins);
			if (!iLocalRetVal)
				iLocalRetVal = pat_OrViaXorAnd(ins);
			if (!iLocalRetVal)
				iLocalRetVal = pat_OrNegatedSameCondition(ins);
			if (!iLocalRetVal)
				iLocalRetVal = pat_LogicAnd1(ins);

			break;
		case m_and:
			iLocalRetVal = pat_AndXor(ins);
			if (!iLocalRetVal)
				iLocalRetVal = pat_MulSub(ins);
			break;
		case m_xor:
			iLocalRetVal = pat_XorChain(ins);
			if(!iLocalRetVal)
				iLocalRetVal = pat_LnotOrLnotLnot(ins);
			if (!iLocalRetVal)
				iLocalRetVal = pat_LogicAnd1(ins);
			break;
		case m_lnot:
			iLocalRetVal = pat_LnotOrLnotLnot(ins);
			break;
		}
		return iLocalRetVal;
	}

	// This is the virtual function dictated by the optinsn_t interface. This
	// function gets called by the Hex-Rays kernel; we optimize the microcode.
	int func(mblock_t *blk, minsn_t *ins);
};

// Callback function. Do pattern-deobfuscation.
int ObfCompilerOptimizer::func(mblock_t *blk, minsn_t *ins)
{
#if OPTVERBOSE
	char buf[1000];
	mcode_t_to_string(ins, buf, sizeof(buf));
	msg("ObfCompilerOptimizer: %a %s\n", ins->ea, buf);
#endif

	int retVal = Optimize(ins);
	int iLocalRetVal = 0;
	
	// This callback doesn't seem to get called for subinstructions of 
	// conditional branches. So, if we're dealing with a conditional branch,
	// manually optimize the condition expression
	if ((is_mcode_jcond(ins->opcode) || is_mcode_set(ins->opcode)) && ins->l.t == mop_d)
	{
		// In order to optimize the jcc condition, we actually need a different
		// structure than optinsn_t: in particular, we need a minsn_visitor_t.
		// This local structure declaration just passes the calls to
		// minsn_visitor_t::visit_minsn onto the Optimize function in this 
		// optinsn_t object.
		struct Blah : minsn_visitor_t
		{
			int visit_minsn()
			{
				return othis->Optimize(this->curins);
			}
			ObfCompilerOptimizer *othis;
			Blah(ObfCompilerOptimizer *o) : othis(o) { };
		};
		
		Blah b(this);
		
		// Optimize all subinstructions of the JCC conditional
		iLocalRetVal += ins->for_all_insns(b);
		// For good measure, optimize the top-level instruction again. I don't
		// know if this is necessary or important, but whatever.
		// iLocalRetVal += Optimize(ins);		
	}
	retVal += iLocalRetVal;
	
	// If any optimizations were performed...
	if (retVal)
	{
#if OPTVERBOSE
		// ... inform the user ...
		mcode_t_to_string(ins, buf, sizeof(buf));
		msg("ObfCompilerOptimizer: replaced by %s\n", buf);
#endif
		ins->optimize_flat();
		// I got an INTERR if I optimized jcc conditionals without marking the lists dirty.
		blk->mark_lists_dirty();
		blk->mba->verify(true);
		//blk->mba->optimize_local(0);
		// ... verify we haven't corrupted anything 
		//blk->mba->verify(true);
	}
	return retVal;
}

