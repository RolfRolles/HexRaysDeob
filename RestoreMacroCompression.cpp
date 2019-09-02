#include "RestoreMacroCompression.hpp"
#include <hexrays.hpp>
#include <map>
#include <vector>

extern hexdsp_t *hexdsp;
std::map<ea_t, std::pair<mbl_array_t *, intptr_t>> microcode_cache;

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

bool is_embed(mbl_array_t *mba)
{
	int ret_count = 0;
	for (int i = 0; i < mba->qty; i++)
	{
		mblock_t *block = mba->get_mblock(i);
		minsn_t *insn = block->head;
		while (insn != NULL)
		{
			if (insn->opcode == m_ret)
			{
				ret_count++;
				if (ret_count > 1)
					return false;
			}
			insn = insn->next;
		}
	}
	return true;
}

void get_all_minsn(mbl_array_t *mba, std::vector<std::vector<minsn_t>> &result)
{
	for (int i = 0; i < mba->qty; i++)
	{
		std::vector<minsn_t> list;
		mblock_t *block = mba->get_mblock(i);
		minsn_t *insn = block->head;
		while (insn != NULL)
		{
			list.push_back(*insn);
			insn = insn->next;
		}
		if (list.size())
			result.push_back(list);
	}
	if (result.size())
	{
		std::vector<minsn_t> &list = result[result.size() - 1];
		minsn_t &last = list[list.size() - 1];
		if (last.opcode == m_ret)
			list.pop_back();
		else if (last.opcode == m_goto)
		{
			last.opcode = m_call;
			//get_func;
		}
	}
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

bool mba_cmp(mbl_array_t *mba1, mbl_array_t *mba2)
{
	return mba1 == mba2 || get_mba_hash(mba1) == get_mba_hash(mba2);
}

void mba_cpy(mbl_array_t *dst, mbl_array_t *src)
{
	for (int i = dst->qty - 1; i >= 0; i--)
	{
		mblock_t *dst_block = dst->get_mblock(i);
		dst->remove_block(dst_block);
	}
	for (int i = 0; i < src->qty; i++)
	{
		mblock_t *src_block = src->get_mblock(i);
		mblock_t *dst_block = dst->insert_block(i);
		dst_block->flags = src_block->flags;
		dst_block->start = src_block->start;
		dst_block->end = src_block->end;
		dst_block->type = src_block->type;
		minsn_t *src_insn = src_block->head;
		minsn_t *dst_insn = NULL;
		while (src_insn != NULL)
		{
			minsn_t *new_insn = new minsn_t(*src_insn);
			new_insn->ea = src_insn->ea;
			dst_insn = dst_block->insert_into_block(new_insn, dst_insn);
			src_insn = src_insn->next;
		}
	}
}

void RestoreMacroCompression(mbl_array_t *mba, mbl_array_t *fchunk_mba, int &index)
{
	mblock_t *block = mba->get_mblock(index);
	minsn_t *insn = block->tail;
	insn->_make_nop();
	ea_t cur_ea = insn->ea;
	mblock_t *new_block = NULL;
	for (int i = 0; i < fchunk_mba->qty; i++)
	{
		mblock_t *fchunk_block = fchunk_mba->get_mblock(i);
		minsn_t *fchunk_insn = fchunk_block->head;
		if (fchunk_insn != NULL)
		{
			new_block = mba->insert_block(++index);
			new_block->flags = block->flags;
			new_block->start = cur_ea;
			new_block->end = block->end;
			minsn_t *pre_insn = NULL;
			while (fchunk_insn != NULL)
			{
				minsn_t *new_insn = new minsn_t(*fchunk_insn);
				new_insn->ea = cur_ea;
				pre_insn = new_block->insert_into_block(new_insn, pre_insn);
				fchunk_insn = fchunk_insn->next;
			}
		}
	}
	if (new_block != NULL)
	{
		minsn_t *last_insn = new_block->tail;
		if (last_insn->opcode == m_ret)
			last_insn->_make_nop();
		else if (last_insn->opcode == m_goto)
		{
			last_insn->opcode = m_call;
		}
	}
}

mbl_array_t *PreloadMacroCompression(const mba_ranges_t &mbr)
{
	hexrays_failure_t hf;
	mbl_array_t *mba = gen_microcode(mbr, &hf, NULL, 0, MMAT_GENERATED);
	std::pair<mbl_array_t *, intptr_t> &info = microcode_cache[mbr.start()];
	if (mba == NULL)
	{
		warning("#error \"%a: %s", hf.errea, hf.desc().c_str());
		mba = info.first;
	}
	if (info.first != mba)
	{
		intptr_t mba_hash = get_mba_hash(mba);
		if (info.second != mba_hash)
		{
			info.first = mba;
			info.second = mba_hash;
			for (int i = 0; i < mba->qty; i++)
			{
				mblock_t *block = mba->get_mblock(i);
				minsn_t *insn = block->tail;
				if (insn != NULL && is_mcode_call(insn->opcode))
				{
					ea_t address = (ea_t)(insn->l.nnn);
					if (is_sub(address))
					{
						func_t *pfn = get_fchunk(address);
						if (pfn != NULL && pfn->size() <= 0x50)
						{
							mbl_array_t *fchunk_mba = PreloadMacroCompression(pfn);
							if (fchunk_mba != NULL)
								RestoreMacroCompression(mba, fchunk_mba, i);
						}
					}
				}
			}
		}
	}
	return mba;
}

ssize_t idaapi hexrays_callback(void * ud, hexrays_event_t event, va_list va)
{
	if (event == hxe_microcode)
	{
		mbl_array_t *mba = va_arg(va, mbl_array_t *);
		msg("hxe_microcode: %a", mba);
		auto it = microcode_cache.find(mba->entry_ea);
		if (it != microcode_cache.end())
		{
			if (it->second.first != mba && it->second.second == get_mba_hash(mba))
				mba_cpy(mba, it->second.first);
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
				PreloadMacroCompression(pfn);
				msg("PreloadMacroCompression End");
			}
		}
	}
	return 0;
}

void InitRestoreMacroCompression()
{
	install_hexrays_callback(&hexrays_callback, NULL);
	//register_and_attach_to_menu("Edit/Other/", "PreloadMacroCompression", "PreloadMacroCompression", NULL, SETMENU_INS, &pmc, &PLUGIN);
	hook_to_notification_point(HT_UI, &ui_notification, NULL);
}

void UnInitRestoreMacroCompression()
{
	remove_hexrays_callback(&hexrays_callback, NULL);
	//detach_action_from_menu("Edit/Other/", "PreloadMacroCompression");
	//unregister_action("PreloadMacroCompression");
	unhook_from_notification_point(HT_UI, &ui_notification, NULL);
	microcode_cache.clear();
}
