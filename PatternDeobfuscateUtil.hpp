#pragma once

#include <hexrays.hpp>

bool ExtractNumAndNonNum(minsn_t *insn, mop_t *&numOp, mop_t *&otherOp);
bool ExtractByOpcodeType(minsn_t *ins, mcode_t mc, minsn_t *&match, mop_t*& noMatch);
bool TunnelThroughAnd1(minsn_t *ins, minsn_t *&inner, bool bRequireSize1 = true, mop_t **opInner = NULL);
bool AreConditionCodesOpposite(mcode_t c1, mcode_t c2);
bool ExtractLogicallyNegatedTerm(minsn_t *ins, minsn_t *&insNegated, mop_t **opNegated = NULL);
bool AreConditionsOpposite(minsn_t *lhsCond, minsn_t *rhsCond);

class XorSimplifier
{
public:
	XorSimplifier() : m_InsertedConst(0), m_InsertedNonConst(0) {};

	// The set of terms in the XOR chain that aren't constant numbers.
	std::set<mop_t *> m_NonConst;
	// A counter for number of insertions of non-constant terms.
	int m_InsertedNonConst;
	
	// The set of constant number terms, and an insertion counter.
	std::set<mop_t *> m_Const;
	int m_InsertedConst;

	// This contains pointers to the operands that can be zeroed out. I.e.,
	// the terms that were cancelled out, before we actually erase them from
	// the microcode itself.
	std::vector<mop_t *> m_ZeroOut;

	bool Insert(std::set<mop_t *> &whichSet, mop_t *op);
	bool InsertNonConst(mop_t *op);
	bool InsertConst(mop_t *op);
	void Insert(mop_t *op);
	void Insert(minsn_t *insn);
	bool DidSimplify();
	bool Simplify(minsn_t *insn);
};