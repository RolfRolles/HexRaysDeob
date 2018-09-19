#define USE_DANGEROUS_FUNCTIONS 
#include <hexrays.hpp>

// Produce a string for an operand type
const char *mopt_t_to_string(mopt_t t)
{
	switch (t)
	{
	case mop_z: return "mop_z";
	case mop_r: return "mop_r";
	case mop_n: return "mop_n";
	case mop_str: return "mop_str";
	case mop_d: return "mop_d";
	case mop_S: return "mop_S";
	case mop_v: return "mop_v";
	case mop_b: return "mop_b";
	case mop_f: return "mop_f";
	case mop_l: return "mop_l";
	case mop_a: return "mop_a";
	case mop_h: return "mop_h";
	case mop_c: return "mop_c";
	case mop_fn: return "mop_fn";
	case mop_p: return "mop_p";
	case mop_sc: return "mop_sc";
	};
	return "???";
}

// Produce a brief representation of a microinstruction, including the types
// of its operands.
void mcode_t_to_string(minsn_t *o, char *outBuf, size_t n)
{
	switch (o->opcode)
	{
	case m_nop: snprintf(outBuf, n, "m_nop"); break;
	case m_stx: snprintf(outBuf, n, "m_stx(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_ldx: snprintf(outBuf, n, "m_ldx(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_ldc: snprintf(outBuf, n, "m_ldc(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_mov: snprintf(outBuf, n, "m_mov(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_neg: snprintf(outBuf, n, "m_neg(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_lnot: snprintf(outBuf, n, "m_lnot(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_bnot: snprintf(outBuf, n, "m_bnot(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_xds: snprintf(outBuf, n, "m_xds(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_xdu: snprintf(outBuf, n, "m_xdu(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_low: snprintf(outBuf, n, "m_low(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_high: snprintf(outBuf, n, "m_high(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_add: snprintf(outBuf, n, "m_add(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_sub: snprintf(outBuf, n, "m_sub(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_mul: snprintf(outBuf, n, "m_mul(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_udiv: snprintf(outBuf, n, "m_udiv(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_sdiv: snprintf(outBuf, n, "m_sdiv(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_umod: snprintf(outBuf, n, "m_umod(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_smod: snprintf(outBuf, n, "m_smod(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_or: snprintf(outBuf, n, "m_or(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_and: snprintf(outBuf, n, "m_and(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_xor: snprintf(outBuf, n, "m_xor(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_shl: snprintf(outBuf, n, "m_shl(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_shr: snprintf(outBuf, n, "m_shr(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_sar: snprintf(outBuf, n, "m_sar(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_cfadd: snprintf(outBuf, n, "m_cfadd(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_ofadd: snprintf(outBuf, n, "m_ofadd(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_cfshl: snprintf(outBuf, n, "m_cfshl(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_cfshr: snprintf(outBuf, n, "m_cfshr(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_sets: snprintf(outBuf, n, "m_sets(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_seto: snprintf(outBuf, n, "m_seto(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setp: snprintf(outBuf, n, "m_setp(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setnz: snprintf(outBuf, n, "m_setnz(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setz: snprintf(outBuf, n, "m_setz(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setae: snprintf(outBuf, n, "m_setae(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setb: snprintf(outBuf, n, "m_setb(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_seta: snprintf(outBuf, n, "m_seta(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setbe: snprintf(outBuf, n, "m_setbe(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setg: snprintf(outBuf, n, "m_setg(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setge: snprintf(outBuf, n, "m_setge(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setl: snprintf(outBuf, n, "m_setl(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_setle: snprintf(outBuf, n, "m_setle(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jcnd: snprintf(outBuf, n, "m_jcnd(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_jnz: snprintf(outBuf, n, "m_jnz(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jz: snprintf(outBuf, n, "m_jz(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jae: snprintf(outBuf, n, "m_jae(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jb: snprintf(outBuf, n, "m_jb(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_ja: snprintf(outBuf, n, "m_ja(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jbe: snprintf(outBuf, n, "m_jbe(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jg: snprintf(outBuf, n, "m_jg(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jge: snprintf(outBuf, n, "m_jge(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jl: snprintf(outBuf, n, "m_jl(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jle: snprintf(outBuf, n, "m_jle(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_jtbl: snprintf(outBuf, n, "m_jtbl(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t)); break;
	case m_ijmp: snprintf(outBuf, n, "m_ijmp(%s,%s)", mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_goto: snprintf(outBuf, n, "m_goto(%s)", mopt_t_to_string(o->l.t)); break;
	case m_call: snprintf(outBuf, n, "m_call(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_icall: snprintf(outBuf, n, "m_icall(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_ret: snprintf(outBuf, n, "m_ret"); break;
	case m_push: snprintf(outBuf, n, "m_push(%s)", mopt_t_to_string(o->l.t)); break;
	case m_pop: snprintf(outBuf, n, "m_pop(%s)", mopt_t_to_string(o->d.t)); break;
	case m_und: snprintf(outBuf, n, "m_und(%s)", mopt_t_to_string(o->d.t)); break;
	case m_ext: snprintf(outBuf, n, "m_ext(???)"); break;
	case m_f2i: snprintf(outBuf, n, "m_f2i(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_f2u: snprintf(outBuf, n, "m_f2u(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_i2f: snprintf(outBuf, n, "m_i2f(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_u2f: snprintf(outBuf, n, "m_u2f(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_f2f: snprintf(outBuf, n, "m_f2f(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_fneg: snprintf(outBuf, n, "m_fneg(%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->d.t)); break;
	case m_fadd: snprintf(outBuf, n, "m_fadd(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_fsub: snprintf(outBuf, n, "m_fsub(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_fmul: snprintf(outBuf, n, "m_fmul(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	case m_fdiv: snprintf(outBuf, n, "m_fdiv(%s,%s,%s)", mopt_t_to_string(o->l.t), mopt_t_to_string(o->r.t), mopt_t_to_string(o->d.t)); break;
	}
}

// Produce a string describing the microcode maturity level.
const char *MicroMaturityToString(mba_maturity_t mmt)
{
	switch (mmt)
	{
	case MMAT_ZERO: return "MMAT_ZERO";
	case MMAT_GENERATED: return "MMAT_GENERATED";
	case MMAT_PREOPTIMIZED: return "MMAT_PREOPTIMIZED";
	case MMAT_LOCOPT: return "MMAT_LOCOPT";
	case MMAT_CALLS: return "MMAT_CALLS";
	case MMAT_GLBOPT1: return "MMAT_GLBOPT1";
	case MMAT_GLBOPT2: return "MMAT_GLBOPT2";
	case MMAT_GLBOPT3: return "MMAT_GLBOPT3";
	case MMAT_LVARS: return "MMAT_LVARS";
	default: return "???";
	}
}

// Copied from http://www.hexblog.com/?p=1198
// I did add code for the mop_d case; it used to return false

//--------------------------------------------------------------------------
// compare operands but ignore the sizes
bool equal_mops_ignore_size(const mop_t &lo, const mop_t &ro)
{
	if (lo.t != ro.t)
		return false;

	switch (lo.t)
	{
	case mop_z:         // none
		return true;
	case mop_fn:        // floating point
		return *ro.fpc == *lo.fpc;
	case mop_n:         // immediate
	{
		int minsize = qmin(lo.size, ro.size);
		uint64 v1 = extend_sign(ro.nnn->value, minsize, false);
		uint64 v2 = extend_sign(lo.nnn->value, minsize, false);
		return v1 == v2;
	}
	case mop_S:         // stack variable
		return *ro.s == *lo.s;
	case mop_v:         // global variable
		return ro.g == lo.g;
	case mop_d:         // result of another instruction
		// I added this
		return ro.d->equal_insns(*lo.d, EQ_IGNSIZE | EQ_IGNCODE);
	case mop_b:         // micro basic block (mblock_t)
		return ro.b == lo.b;
	case mop_r:         // register
		return ro.r == lo.r;
	case mop_f:
		break;            // not implemented
	case mop_l:
		return *ro.l == *lo.l;
	case mop_a:
		return lo.a->insize == ro.a->insize
			&& lo.a->outsize == ro.a->outsize
			&& equal_mops_ignore_size(*lo.a, *ro.a);
	case mop_h:
		return streq(ro.helper, lo.helper);
	case mop_str:
		return streq(ro.cstr, lo.cstr);
	case mop_c:
		return *ro.c == *lo.c;
	case mop_p:
		return equal_mops_ignore_size(lo.pair->lop, ro.pair->lop)
			&& equal_mops_ignore_size(lo.pair->hop, ro.pair->hop);
	case mop_sc: // not implemented
		break;
	}
	return false;
}
