#include <memory>
#define USE_DANGEROUS_FUNCTIONS 
#include <hexrays.hpp>
#include "HexRaysUtil.hpp"

typedef std::shared_ptr<mbl_array_t *> shared_mbl_array_t;

struct mblock_virtual_dumper_t : public vd_printer_t
{
	int nline;
	int serial;
	mblock_virtual_dumper_t() : nline(0), serial(0) {};
	virtual void AddLine(qstring &qs) = 0;
	AS_PRINTF(3, 4) int print(int indent, const char *format, ...)
	{
		qstring buf;
		if (indent > 0)
			buf.fill(0, ' ', indent);
		va_list va;
		va_start(va, format);
		buf.cat_vsprnt(format, va);
		va_end(va);

		// ida 7.1 apparently has a problem with line prefixes, remove this color
		static const char pfx_on[] = { COLOR_ON, COLOR_PREFIX };
		static const char pfx_off[] = { COLOR_OFF, COLOR_PREFIX };
		buf.replace(pfx_on, "");
		buf.replace(pfx_off, "");

		AddLine(buf);
		return buf.length();
	}
};

struct mblock_qstring_dumper_t : public mblock_virtual_dumper_t
{
	qstring qStr;
	mblock_qstring_dumper_t() : mblock_virtual_dumper_t() {};
	virtual void AddLine(qstring &qs)
	{
		qStr.append(qs);
	}
};

struct mblock_dumper_t : public mblock_virtual_dumper_t
{
	strvec_t lines;
	mblock_dumper_t() : mblock_virtual_dumper_t() {};
	virtual void AddLine(qstring &qs)
	{
		lines.push_back(simpleline_t(qs));
	}
};

struct sample_info_t
{
	TWidget *cv;
	mblock_dumper_t md;
	shared_mbl_array_t mba;
	mba_maturity_t mat;
	sample_info_t() : cv(NULL), mba(NULL) {}
};

#include <graph.hpp>

class MicrocodeInstructionGraph
{
public:
	qstring tmp;            // temporary buffer for grcode_user_text
	qstrvec_t m_ShortText;
	qstrvec_t m_BlockText;
	intvec_t m_EdgeColors;
	edgevec_t m_Edges;
	int m_NumBlocks;

	void Clear()
	{
		m_ShortText.clear();
		m_BlockText.clear();
		m_EdgeColors.clear();
		m_Edges.clear();
		m_NumBlocks = 0;
	}

	void Build(minsn_t *top)
	{
		Clear();
		Insert(top, -1);
	}

protected:
	void AddEdge(int iSrc, int iDest, int iPos)
	{
		if (iSrc < 0 || iDest < 0)
			return;

		m_Edges.push_back(edge_t(iSrc, iDest));
		m_EdgeColors.push_back(iPos);
	}

	int GetIncrBlockNum()
	{
		return m_NumBlocks++;
	}

	int Insert(minsn_t *ins, int iParent)
	{
		char l_Buf[MAXSTR];
		mcode_t_to_string(ins, l_Buf, sizeof(l_Buf));
		m_ShortText.push_back(l_Buf);

		qstring qStr;
		ins->print(&qStr);
		m_BlockText.push_back(qStr);

		int iThisBlock = GetIncrBlockNum();

		Insert(ins->l, iThisBlock, 0);
		Insert(ins->r, iThisBlock, 1);
		Insert(ins->d, iThisBlock, 2);

		return iThisBlock;
	}
	int Insert(mop_t &op, int iParent, int iPos)
	{
		if (op.t == mop_z)
			return -1;

		m_ShortText.push_back(mopt_t_to_string(op.t));

		qstring qStr;
		op.print(&qStr);
		m_BlockText.push_back(qStr);

		int iThisBlock = GetIncrBlockNum();
		AddEdge(iParent, iThisBlock, iPos);

		switch (op.t)
		{
		case mop_d: // result of another instruction
		{
			int iDestBlock = Insert(op.d, iThisBlock);
			AddEdge(iThisBlock, iDestBlock, 0);
			break;
		}
		case mop_f: // list of arguments
			for (int i = 0; i < op.f->args.size(); ++i)
				Insert(op.f->args[i], iThisBlock, i);
			break;
		case mop_p: // operand pair
		{
			Insert(op.pair->lop, iThisBlock, 0);
			Insert(op.pair->hop, iThisBlock, 1);
			break;
		}
		case mop_a: // result of another instruction
		{
			int iDestBlock = Insert(*op.a, iThisBlock, 0);
			break;
		}
		}
		return iThisBlock;
	}
};

class MicrocodeInstructionGraphContainer;

static ssize_t idaapi migr_callback(void *ud, int code, va_list va);

class MicrocodeInstructionGraphContainer
{
protected:
	TWidget * m_TW;
	graph_viewer_t *m_GV;
	qstring m_Title;
	qstring m_GVName;

public:
	MicrocodeInstructionGraph m_MG;
	MicrocodeInstructionGraphContainer() : m_TW(NULL), m_GV(NULL) {};

	bool Display(minsn_t *top, sample_info_t *si, int nBlock, int nSerial)
	{
		mbl_array_t *mba = *si->mba;
		m_MG.Build(top);
		
		m_Title.cat_sprnt("Microinstruction Graph - %a[%s]/%d:%d", mba->entry_ea, MicroMaturityToString(si->mat), nBlock, nSerial);
		m_TW = create_empty_widget(m_Title.c_str());
		netnode id;
		id.create();
		
		m_GVName.cat_sprnt("microins_%a_%s_%d_%d", mba->entry_ea, MicroMaturityToString(si->mat), nBlock, nSerial);
		m_GV = create_graph_viewer(m_GVName.c_str(), id, migr_callback, this, 0, m_TW);
		activate_widget(m_TW, true);
#if IDA_SDK_VERSION == 710
		display_widget(m_TW, WOPN_TAB | WOPN_MENU);
#elif IDA_SDK_VERSION >= 720
		display_widget(m_TW, WOPN_TAB);
#endif
		viewer_fit_window(m_GV);
		return true;
	}
};

static ssize_t idaapi migr_callback(void *ud, int code, va_list va)
{
	MicrocodeInstructionGraphContainer *gcont = (MicrocodeInstructionGraphContainer *)ud;
	MicrocodeInstructionGraph *microg = &gcont->m_MG;
	bool result = false;

	switch (code)
	{
	case grcode_user_gentext:
		result = true;
		break;

		// refresh user-defined graph nodes and edges
	case grcode_user_refresh:
		// in:  mutable_graph_t *g
		// out: success
	{
		mutable_graph_t *mg = va_arg(va, mutable_graph_t *);

		// we have to resize
		mg->resize(microg->m_NumBlocks);

		for (auto &it : microg->m_Edges)
			mg->add_edge(it.src, it.dst, NULL);

		result = true;
	}
	break;

	// retrieve text for user-defined graph node
	case grcode_user_text:
		//mutable_graph_t *g
		//      int node
		//      const char **result
		//      bgcolor_t *bg_color (maybe NULL)
		// out: must return 0, result must be filled
		// NB: do not use anything calling GDI!
	{
		va_arg(va, mutable_graph_t *);
		int node = va_arg(va, int);
		const char **text = va_arg(va, const char **);

		microg->tmp = microg->m_ShortText[node];
		microg->tmp.append('\n');
		microg->tmp.append(microg->m_BlockText[node]);
		*text = microg->tmp.begin();
		result = true;
	}
	break;
	}
	return (int)result;
}

static ssize_t idaapi mgr_callback(void *ud, int code, va_list va);

class MicrocodeGraphContainer
{
public:
	shared_mbl_array_t m_MBA;
	mblock_qstring_dumper_t m_MQD;
	qstring m_Title;
	qstring m_GVName;
	qstring tmp;
	MicrocodeGraphContainer(shared_mbl_array_t mba) : m_MBA(mba) {};
	bool Display(sample_info_t *si)
	{
		mbl_array_t *mba = *si->mba;
		m_Title.cat_sprnt("Microcode Graph - %a[%s]", mba->entry_ea, MicroMaturityToString(si->mat));

		TWidget *tw = create_empty_widget(m_Title.c_str());
		netnode id;
		id.create();
		
		m_GVName.cat_sprnt("microblkgraph_%a_%s", mba->entry_ea, MicroMaturityToString(si->mat));
		graph_viewer_t *gv = create_graph_viewer(m_GVName.c_str(), id, mgr_callback, this, 0, tw);
		activate_widget(tw, true);
#if IDA_SDK_VERSION == 710
		display_widget(tw, WOPN_TAB | WOPN_MENU);
#elif IDA_SDK_VERSION >= 720
		display_widget(tw, WOPN_TAB);
#endif
		viewer_fit_window(gv);
		return true;
	}

};

static ssize_t idaapi mgr_callback(void *ud, int code, va_list va)
{
	MicrocodeGraphContainer *gcont = (MicrocodeGraphContainer *)ud;
	mbl_array_t *mba = *gcont->m_MBA;
	bool result = false;

	switch (code)
	{
	case grcode_user_gentext:
		result = true;
		break;

		// refresh user-defined graph nodes and edges
	case grcode_user_refresh:
		// in:  mutable_graph_t *g
		// out: success
	{
		mutable_graph_t *mg = va_arg(va, mutable_graph_t *);

		// we have to resize
		mg->resize(mba->qty);

		for (int i = 0; i < mba->qty; ++i)
			for (auto dst : mba->get_mblock(i)->succset)
				mg->add_edge(i, dst, NULL);

		result = true;
	}
	break;

	// retrieve text for user-defined graph node
	case grcode_user_text:
		//mutable_graph_t *g
		//      int node
		//      const char **result
		//      bgcolor_t *bg_color (maybe NULL)
		// out: must return 0, result must be filled
		// NB: do not use anything calling GDI!
	{
		va_arg(va, mutable_graph_t *);
		int node = va_arg(va, int);
		const char **text = va_arg(va, const char **);

		gcont->m_MQD.qStr.clear();
		mba->get_mblock(node)->print(gcont->m_MQD);
		*text = gcont->m_MQD.qStr.begin();
		result = true;
	}
	break;
	}
	return (int)result;
}

static bool idaapi ct_keyboard(TWidget * /*v*/, int key, int shift, void *ud)
{
	if (shift == 0)
	{
		sample_info_t *si = (sample_info_t *)ud;
		switch (key)
		{
		case 'G':
		{
			MicrocodeGraphContainer *mgc = new MicrocodeGraphContainer(si->mba);
			return mgc->Display(si);
		}


		// User wants to show a graph of the current instruction
		case 'I':
		{
			qstring buf;
			tag_remove(&buf, get_custom_viewer_curline(si->cv, false));
			const char *pLine = buf.c_str();
			const char *pDot = strchr(pLine, '.');
			if (pDot == NULL)
			{
				warning(
					"Couldn't find the block number on the current line; was the block empty?\n"
					"If it was not empty, and you don't see [int].[int] at the beginning of the lines\n"
					"please run the plugin again to generate a new microcode listing.\n"
					"That should fix it.");
				return true; // reacted to the keypress
			}
			int nBlock = atoi(pLine);
			int nSerial = atoi(pDot + 1);
			mbl_array_t *mba = *si->mba;

			if (nBlock > mba->qty)
			{
				warning("Plugin error: line prefix was %d:%d, but block only has %d basic blocks.", nBlock, nSerial, mba->qty);
				return true;
			}

			mblock_t *blk = mba->get_mblock(nBlock);
			minsn_t *minsn = blk->head;
			int i;
			for (i = 0; i < nSerial; ++i)
			{
				minsn = minsn->next;
				if (minsn == NULL)
					break;
			}

			if (minsn == NULL)
			{
				if (i == 0)
					warning(
						"Couldn't get first minsn_t from %d:%d; was the block empty?\n"
						"If it was not empty, and you don't see [int].[int] at the beginning of the lines\n"
						"please run the plugin again to generate a new microcode listing.\n"
						"That should fix it.", nBlock, nSerial);
				else
					warning("Couldn't get first minsn_t from %d:%d; last valid instruction was %d", nBlock, nSerial, i - 1);
				return true;
			}

			char repr[MAXSTR];
			mcode_t_to_string(minsn, repr, sizeof(repr));
			MicrocodeInstructionGraphContainer *mcg = new MicrocodeInstructionGraphContainer;
			return mcg->Display(minsn, si, nBlock, nSerial);
		}
		case IK_ESCAPE:
			close_widget(si->cv, WCLS_SAVE | WCLS_CLOSE_LATER);
			return true;
		}
	}
	return false;
}

static const custom_viewer_handlers_t handlers(
	ct_keyboard,
	NULL, // popup
	NULL, // mouse_moved
	NULL, // click
	NULL, // dblclick
	NULL,
	NULL, // close
	NULL, // help
	NULL);// adjust_place

ssize_t idaapi ui_callback(void *ud, int code, va_list va)
{
	sample_info_t *si = (sample_info_t *)ud;
	switch (code)
	{
	case ui_widget_invisible:
	{
		TWidget *f = va_arg(va, TWidget *);
		if (f == si->cv)
		{
			delete si;
			unhook_from_notification_point(HT_UI, ui_callback);
		}
	}
	break;
	}
	return 0;
}

const char *matLevels[] =
{
	"MMAT_GENERATED",
	"MMAT_PREOPTIMIZED",
	"MMAT_LOCOPT",
	"MMAT_CALLS",
	"MMAT_GLBOPT1",
	"MMAT_GLBOPT2",
	"MMAT_GLBOPT3",
	"MMAT_LVARS"
};

mba_maturity_t AskDesiredMaturity()
{
	const char dlgText[] =
		"Select maturity level\n"
		"<Desired ~m~aturity level:b:0:::>\n";

	qstrvec_t opts;
	for (int i = 0; i < qnumber(matLevels); ++i)
		opts.push_back(matLevels[i]);

	int sel = 0;
	int ret = ask_form(dlgText, &opts, &sel);

	if (ret > 0)
		return (mba_maturity_t)((int)MMAT_GENERATED + sel);
	return MMAT_ZERO;
}

void ShowMicrocodeExplorer()
{
	func_t *pfn = get_func(get_screen_ea());
	if (pfn == NULL)
	{
		warning("Please position the cursor within a function");
		return;
	}

	mba_maturity_t mmat = AskDesiredMaturity();
	if (mmat == MMAT_ZERO)
		return;

	hexrays_failure_t hf;
	mbl_array_t *mba = gen_microcode(pfn, &hf, NULL, 0, mmat);
	if (mba == NULL)
	{
		warning("#error \"%a: %s", hf.errea, hf.desc().c_str());
		return;
	}

	sample_info_t *si = new sample_info_t;
	si->mba = std::make_shared<mbl_array_t *>(mba);
	si->mat = mmat;
	// Dump the microcode to the output window
	mba->print(si->md);

	simpleline_place_t s1;
	simpleline_place_t s2(si->md.lines.size() - 1);

	qstring title;
	title.cat_sprnt("Microcode Explorer - %a - %s", pfn->start_ea, MicroMaturityToString(mmat));

	si->cv = create_custom_viewer(
		title.c_str(), // title
		&s1, // minplace
		&s2, // maxplace
		&s1, // curplace
		NULL, // renderer_info_t *rinfo
		&si->md.lines, // ud
		&handlers, // cvhandlers
		si, // cvhandlers_ud
		NULL); // parent

	hook_to_notification_point(HT_UI, ui_callback, si);
	display_widget(si->cv, WOPN_TAB | WOPN_RESTORE);
}

