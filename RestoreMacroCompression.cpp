#include "RestoreMacroCompression.hpp"
#include <hexrays.hpp>
#include <map>
#include <vector>

#define FCHUNK_MAXSIZE 0x50

enum asm_type
{
	at_unknown,
	at_x86,
	at_x64,
	at_arm,
	at_arm64
};

struct mba_info
{
	bool modified;
	int retn;
	mbl_array_t *mba;
	intptr_t hash;
};

extern hexdsp_t *hexdsp;
asm_type cur_asm_type;
std::map<ea_t, mba_info> microcode_cache;
bool is_preload = false;

intptr_t right_shift_loop(intptr_t num, intptr_t n)
{
	return (num << (sizeof(intptr_t) - n) | (num >> n));
}

bool is_sub(ea_t ea)
{
	qstring name;
	get_name(&name, ea);
	char *p_name = (char *)name.c_str();
	if (memcmp(p_name, "sub_", 4) == 0 || memcmp(p_name, "loc_", 4) == 0)
	{
		ea_t n = strtoull(p_name + 4, NULL, 16);
		return ea == n;
	}
	return false;
}

intptr_t get_mop_hash(mop_t *mop)
{
	intptr_t hash = mop->t;
	hash = mop->oprops ^ right_shift_loop(hash, sizeof(mop->t));
	hash = mop->valnum ^ right_shift_loop(hash, sizeof(mop->oprops));
	hash = mop->size ^ right_shift_loop(hash, sizeof(mop->valnum));
	return hash;
}

intptr_t get_minsn_hash(minsn_t *minsn)
{
	intptr_t l = get_mop_hash(&(minsn->l));
	intptr_t r = get_mop_hash(&(minsn->r));
	intptr_t d = get_mop_hash(&(minsn->d));
	intptr_t c = l ^ r ^ d;
	intptr_t n = (sizeof(intptr_t) * 8) - 8;
	intptr_t t = minsn->opcode << n;
	intptr_t hash = t ^ c;
	return hash;
}

intptr_t get_mba_hash(mbl_array_t *mba)
{
	intptr_t hash = 0;
	for (int i = 0; i < mba->qty; i++)
	{
		mblock_t *block = mba->get_mblock(i);
		minsn_t *insn = block->head;
		while (insn != NULL)
		{
			hash = get_minsn_hash(insn) ^ right_shift_loop(hash, 8);
			insn = insn->next;
		}
	}
	return hash;
}

int get_mba_retn(mbl_array_t *mba)
{
	int retn = 0;
	for (int i = 0; i < mba->qty; i++)
	{
		mblock_t *block = mba->get_mblock(i);
		minsn_t *insn = block->tail;
		if (insn != NULL && insn->opcode == m_ret)
			retn++;
	}
	return retn;
}

void blk_cpy(mblock_t *dst, mblock_t *src, ea_t ea = BADADDR)
{
	dst->flags = src->flags;
	dst->start = src->start;
	dst->end = src->end;
	dst->type = src->type;
	dst->dead_at_start = src->dead_at_start;
	dst->mustbuse = src->mustbuse;
	dst->maybuse = src->maybuse;
	dst->mustbdef = src->mustbdef;
	dst->maybdef = src->maybdef;
	dst->dnu = src->dnu;
	dst->maxbsp = src->maxbsp;
	dst->minbstkref = src->minbstkref;
	dst->minbargref = src->minbargref;
	dst->predset = src->predset;
	dst->succset = src->succset;
	minsn_t *src_insn = src->head;
	minsn_t *dst_insn = NULL;
	while (src_insn != NULL)
	{
		minsn_t *new_insn = new minsn_t(*src_insn);
		new_insn->ea = ea == BADADDR ? src_insn->ea : ea;
		dst_insn = dst->insert_into_block(new_insn, dst_insn);
		src_insn = src_insn->next;
	}
}

bool mba_cmp(mbl_array_t *mba1, mbl_array_t *mba2)
{
	return mba1 == mba2 || get_mba_hash(mba1) == get_mba_hash(mba2);
}

void mba_cpy(mbl_array_t *dst, mbl_array_t *src, ea_t ea = BADADDR)
{
	for (int i = dst->qty - 1; i >= 0; i--)
	{
		mblock_t *dst_block = dst->get_mblock(i);
		dst->remove_block(dst_block);
	}
	for (int i = 0; i < src->qty; i++)
	{
		mblock_t *dst_block = dst->insert_block(i);
		dst_block->flags |= MBL_FAKE;
	}
	for (int i = 0; i < src->qty; i++)
	{
		mblock_t *src_block = src->get_mblock(i);
		mblock_t *dst_block = dst->get_mblock(i);
		blk_cpy(dst_block, src_block, ea);
	}
}

mop_t get_mop(const char *reg_name, int s)
{
	size_t len = strlen(reg_name) + 10;
	char reg_full_name[24];
	sprintf_s(reg_full_name, "%s.%d", reg_name, s);
	mop_t mop(0, s);
	try
	{
		while (true)
		{
			if (strcmp(mop.dstr(), reg_full_name) == 0)
				break;
			mop.r++;
		}
	}
	catch (const std::exception&)
	{
		mop.zero();
	}
	return mop;
}

mba_info *PreloadMacroCompression(const mba_ranges_t &mbr);

void FixSP(mblock_t *block, ea_t ea, bool sub = false, minsn_t *position = NULL)
{
	minsn_t *insn = new minsn_t(ea);
	insn->opcode = sub ? m_sub : m_add;
	switch (cur_asm_type)
	{
	case at_unknown:
		insn->_make_nop();
		return;
	case at_x86:
		insn->l = insn->d = get_mop("esp", 4);
		insn->r.make_number(4, 4, ea);
		break;
	case at_x64:
		insn->l = insn->d = get_mop("rsp", 8);
		insn->r.make_number(8, 8, ea);
		break;
	case at_arm:
		insn->l = insn->d = get_mop("sp", 4);
		insn->r.make_number(4, 4, ea);
		break;
	case at_arm64:
		insn->l = insn->d = get_mop("sp", 8);
		insn->r.make_number(8, 8, ea);
		break;
	default:
		break;
	}
	block->insert_into_block(insn, position);
}

void FixBlockSerial(mblock_t *begin_block, mblock_t *end_block, std::map<int, int> &serial_map)
{
	for (mblock_t *block = begin_block; block != end_block; block = block->nextb)
	{
		for (minsn_t *insn = block->head; insn != NULL; insn = insn->next)
		{
			if (insn->l.t == mop_b)
				insn->l.b = serial_map[insn->l.b];
			if (insn->r.t == mop_b)
				insn->r.b = serial_map[insn->r.b];
			if (insn->d.t == mop_b)
				insn->d.b = serial_map[insn->d.b];
		}
	}
}

void RestoreMacroCompression(mbl_array_t *mba, mbl_array_t *fchunk_mba, int &index)
{
	mblock_t *block = mba->get_mblock(index);
	minsn_t *insn = block->tail;
	ea_t cur_ea = insn->ea;
	insn->_make_nop();
	mblock_t *new_block = NULL;
	std::map<int, int> serial_map;
	for (int i = 0; i < fchunk_mba->qty; i++)
	{
		mblock_t *fchunk_block = fchunk_mba->get_mblock(i);
		if (fchunk_block->head != NULL)
		{
			new_block = mba->insert_block(++index);
			serial_map[fchunk_block->serial] = new_block->serial;
			blk_cpy(new_block, fchunk_block, cur_ea);
			new_block->start = cur_ea;
			new_block->end = block->end;
		}
	}
	if (new_block != NULL)
	{
		FixBlockSerial(block->nextb, new_block->nextb, serial_map);
		minsn_t *last_insn = new_block->tail;
		if (last_insn->opcode == m_ret)
			last_insn->_make_nop();
		else if (last_insn->opcode == m_goto)
		{
			ea_t address = last_insn->l.g;
			func_t *pfn;
			mba_info *fchunk_mba;
			if (is_sub(address) && (pfn = get_fchunk(address)) != NULL && pfn->size() <= FCHUNK_MAXSIZE && (fchunk_mba = PreloadMacroCompression(pfn)) != NULL && fchunk_mba->mba != NULL && fchunk_mba->retn <= 1)
				RestoreMacroCompression(mba, fchunk_mba->mba, index);
			else
				last_insn->opcode = m_call;
		}
		FixSP(block->nextb, cur_ea, true);
		FixSP(new_block, cur_ea, false, new_block->tail);
	}
}

mba_info *PreloadMacroCompression(const mba_ranges_t &mbr)
{
	hexrays_failure_t hf;
	mbl_array_t *mba = gen_microcode(mbr, &hf, NULL, 0, MMAT_GENERATED);
	mba_info &info = microcode_cache[mbr.start()];
	if (mba == NULL)
	{
		warning("#error \"%a: %s", hf.errea, hf.desc().c_str());
		mba = info.mba;
	}
	if (info.mba != mba)
	{
		intptr_t mba_hash = get_mba_hash(mba);
		if (info.hash != mba_hash)
		{
			info.retn = get_mba_retn(mba);
			info.mba = mba;
			info.hash = mba_hash;
			for (int i = 0; i < mba->qty; i++)
			{
				mblock_t *block = mba->get_mblock(i);
				minsn_t *insn = block->tail;
				if (insn != NULL && is_mcode_call(insn->opcode))
				{
					ea_t address = insn->l.g;
					if (is_sub(address))
					{
						func_t *pfn = get_fchunk(address);
						if (pfn != NULL && pfn->size() <= FCHUNK_MAXSIZE)
						{
							mba_info *fchunk_mba = PreloadMacroCompression(pfn);
							if (fchunk_mba != NULL && fchunk_mba->mba != NULL && fchunk_mba->retn <= 1)
								RestoreMacroCompression(mba, fchunk_mba->mba, i);
						}
					}
				}
			}
			info.modified = mba_hash != get_mba_hash(mba);
		}
	}
	return &info;
}

ssize_t idaapi hexrays_callback(void *ud, hexrays_event_t event, va_list va)
{
	if (is_preload == false && event == hxe_microcode)
	{
		mbl_array_t *mba = va_arg(va, mbl_array_t *);
		auto it = microcode_cache.find(mba->entry_ea);
		if (it != microcode_cache.end())
		{
			if (it->second.modified && it->second.mba != mba && it->second.hash == get_mba_hash(mba))
				mba_cpy(mba, it->second.mba);
		}
	}
	return 0;
}

ssize_t idaapi ui_notification(void *user_data, int notification_code, va_list va)
{
	if (notification_code == ui_preprocess_action)
	{
		char *name = va_arg(va, char *);
		if (strcmp(name, "hx:GenPseudo") == 0)
		{
			func_t *pfn = get_func(get_screen_ea());
			if (pfn != NULL)
			{
				is_preload = true;
				PreloadMacroCompression(pfn);
				is_preload = false;
			}
		}
	}
	return 0;
}

void InitRestoreMacroCompression()
{
	if (strcmp(inf.procname, "ARM") == 0 || strcmp(inf.procname, "ARMB") == 0) //arm
		cur_asm_type = inf_is_64bit() ? at_arm64 : at_arm;
	else if (memcmp(inf.procname, "80386", 5) == 0 || memcmp(inf.procname, "80486", 5) == 0 || memcmp(inf.procname, "80586", 5) == 0 || memcmp(inf.procname, "80686", 5) == 0 || strcmp(inf.procname, "metapc") == 0 || strcmp(inf.procname, "p2") == 0 || strcmp(inf.procname, "p3") == 0 || strcmp(inf.procname, "p4") == 0)
		cur_asm_type = inf_is_64bit() ? at_x64 : at_x86;
	else
		cur_asm_type = at_unknown;
	install_hexrays_callback(&hexrays_callback, NULL);
	hook_to_notification_point(HT_UI, &ui_notification, NULL);
}

void UnInitRestoreMacroCompression()
{
	remove_hexrays_callback(&hexrays_callback, NULL);
	unhook_from_notification_point(HT_UI, &ui_notification, NULL);
	microcode_cache.clear();
}
