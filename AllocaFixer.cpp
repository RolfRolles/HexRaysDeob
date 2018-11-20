// This file tries to fix the stack pointer differentials at call sites for
// alloca(). Basically, this binary uses a GCC-like argument-passing schema
// like sub esp, 4 / mov [esp], eax, except that the "sub esp, 4" is 
// implemented as a call to __alloca_probe. IDA usually handles these calls
// remarkably well for ordinary compiled binaries, but this obfuscator doesn't
// produce ordinary binaries. Thus, IDA's typical analysis fails to determine
// the integer values passed to __alloca_probe, and hence also does not change
// the stack pointer accordingly. (Note also that the binary also creates stack
// buffers with this technique, not just function arguments.)

// However, the decompiler is able to determine the integer parameters to
// __alloca_probe. Thus, we examine all cross-references to __alloca_probe,
// decompile the referring functions, extract the arguments, and use them
// to set stack pointer differentials on the addresses after the calls.

#include <vector>

#include <hexrays.hpp>
#include "HexRaysUtil.hpp"
#include <frame.hpp>
#include "Config.hpp"

// Finds calls to alloca in a function's decompilation microcode, and
// records the integer parameter from each call site.
struct AllocaFixer : minsn_visitor_t
{
	// Results are stored here
	std::vector<std::pair<ea_t, int> > m_FixupLocations;

	int visit_minsn(void)
	{
		// Only process calls to alloca
		if (curins->opcode != m_call || curins->l.t != mop_h || qstrcmp(curins->l.helper, "alloca") != 0)
			return 0;

		// Sanity check that the microinstruction's operand is a list of arguments
		if (curins->d.t != mop_f)
		{
			msg("[E] %a: Call to alloca()'s d operand was unexpectedly %s\n", curins->ea, mopt_t_to_string(curins->r.t));
			return 0;
		}

		// Sanity check that the microinstruction's argument list is not null
#if IDA_SDK_VERSION == 710
		mfuncinfo_t *func = curins->d.f;
#elif IDA_SDK_VERSION >= 720
		mcallinfo_t *func = curins->d.f;
#endif
if (func == NULL)
		{
			msg("[E] %a: curins->d.f was NULL?", curins->ea);
			return 0;
		}

		// Sanity check that the call to alloca passes one argument
#if IDA_SDK_VERSION == 710
		mfuncargs_t &args = func->args;
#elif IDA_SDK_VERSION >= 720
		mcallargs_t &args = func->args;
#endif
		if (args.size() != 1)
		{
			msg("[E] Call to alloca had %d arguments instead of 1?\n", args.size());
			return 0;
		}
		
		// We can only fix the call site if its parameter is a constant number
		if (args[0].t != mop_n)
		{
			msg("[E] Call to alloca did not have a constant number; type was %s\n", mopt_t_to_string(args[0].t));
			return 0;
		}

		// Everything went according to plan. Save the call to alloca's address and integer parameter.
		m_FixupLocations.push_back(std::pair<ea_t, int>(curins->ea, args[0].nnn->value));
		return 0;
	}
};

// Find all calls to __alloca_probe, extract the parameters, and update the 
// stack pointer differentials.
void FixCallsToAllocaProbe()
{
	ea_t eaAlloca = get_name_ea(BADADDR, "__alloca_probe");
	if (eaAlloca == BADADDR)
	{
		msg("[E] Couldn't find __alloca_probe\n");
		return;
	}

	// Collect up all functions (as func_t * objects) that call __alloca_probe.
	std::set<func_t *> funcsCallingAlloca;
	xrefblk_t xr;
	bool bFirst = true;
	
	// Examine all addresses that reference __alloca_probe, and collect their
	// func_t * containing function objects.
	while (bFirst ? xr.first_to(eaAlloca, XREF_FAR) : xr.next_to())
	{
		bFirst = false;
		if (xr.type != fl_CN)
			continue;

		func_t *f = get_func(xr.from);
		if (f == NULL)
		{
			msg("[E] Call to alloca from %a is not within a function; will not be processed\n", xr.from);
			continue;
		}

		funcsCallingAlloca.insert(f);
	}

	// For each function that calls __alloca_probe(), extract the address of 
	// each such call and its integer argument. Set a stack pointer delta at
	// that address with that value.
	for (auto f : funcsCallingAlloca)
	{
		// Decompile the function
		mba_ranges_t mbr(f);
		hexrays_failure_t hf;
		mbl_array_t *mba = gen_microcode(mbr, &hf);
		if (mba == NULL)
		{
			msg("[E] FixCallsToAllocaProbe(%a): decompilation failed (%s)\n", f->start_ea, hf.desc().c_str());
			continue;
		}

		// Extract the alloca information by visiting its top-level instructions
		AllocaFixer af;
		mba->for_all_insns(af);
		
		// We own the mbl_array_t produced by gen_microcode, so we have to delete it.
		delete mba;

		// For each location that references alloca...
		for (auto g : af.m_FixupLocations)
		{
			// Set the stack point on the *subsequent* EA (thanks, Hex-Rays!)
			ea_t eaNext = get_item_end(g.first);
			msg("[I] Adding auto stack point at %a: %d\n", eaNext, -g.second);
			// ... fix its stack pointer differential
			if (!add_auto_stkpnt(f, eaNext, -g.second))
			{
				msg("[E] Couldn't change stack delta to %d at %a\n", -g.second, g.first);
				// YOLO
				add_user_stkpnt(eaNext, -g.second);
			}
		}

		// Force re-analysis of the function
		reanalyze_function(f);
	}
}

