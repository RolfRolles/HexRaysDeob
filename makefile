PROC=HexRaysDeob
include ../plugin.mak

__CFLAGS=-std=c++14

$(F)AllocaFixer$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    AllocaFixer.hpp AllocaFixer.cpp

$(F)CFFlattenInfo$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    CFFlattenInfo.hpp CFFlattenInfo.cpp

$(F)DefUtil$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    DefUtil.hpp DefUtil.cpp

$(F)HexRaysUtil$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    HexRaysUtil.hpp HexRaysUtil.cpp

$(F)MicrocodeExplorer$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    MicrocodeExplorer.hpp MicrocodeExplorer.cpp

$(F)PatternDeobfuscate$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    PatternDeobfuscate.hpp PatternDeobfuscate.cpp

$(F)PatternDeobfuscateUtil$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    PatternDeobfuscateUtil.hpp PatternDeobfuscateUtil.cpp

$(F)TargetUtil$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    TargetUtil.hpp TargetUtil.cpp

$(F)Unflattener$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    Unflattener.hpp Unflattener.cpp

$(F)main$(O): $(I)bitrange.hpp $(I)bytes.hpp $(I)config.hpp     \
    $(I)fpro.h $(I)funcs.hpp $(I)gdl.hpp $(I)hexrays.hpp      \
    $(I)ida.hpp $(I)idp.hpp $(I)ieee.h $(I)kernwin.hpp        \
    $(I)lines.hpp $(I)llong.hpp $(I)loader.hpp $(I)nalt.hpp   \
    $(I)name.hpp $(I)netnode.hpp $(I)pro.h $(I)range.hpp      \
    $(I)segment.hpp $(I)typeinf.hpp $(I)ua.hpp $(I)xref.hpp   \
    main.cpp

$(F)HexRaysDeob$(O): $(F)AllocaFixer$(O) $(F)CFFlattenInfo$(O) $(F)DefUtil$(O) 				\
	$(F)HexRaysUtil$(O) $(F)MicrocodeExplorer$(O) $(F)PatternDeobfuscate$(O) 				\
	$(F)PatternDeobfuscateUtil$(O) $(F)TargetUtil$(O) $(F)Unflattener$(O) $(F)main$(O)
	$(CCL) $(STDLIBS) $(IDALIB) -shared -o $@ $^ 
