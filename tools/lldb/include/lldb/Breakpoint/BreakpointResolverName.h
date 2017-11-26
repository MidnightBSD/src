//===-- BreakpointResolverName.h --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_BreakpointResolverName_h_
#define liblldb_BreakpointResolverName_h_

// C Includes
// C++ Includes
#include <vector>
#include <string>
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/BreakpointResolver.h"

namespace lldb_private {

//----------------------------------------------------------------------
/// @class BreakpointResolverName BreakpointResolverName.h "lldb/Breakpoint/BreakpointResolverName.h"
/// @brief This class sets breakpoints on a given function name, either by exact match
/// or by regular expression.
//----------------------------------------------------------------------

class BreakpointResolverName:
    public BreakpointResolver
{
public:

    BreakpointResolverName (Breakpoint *bkpt,
                            const char *name,
                            uint32_t name_type_mask,
                            Breakpoint::MatchType type,
                            bool skip_prologue);

    // This one takes an array of names.  It is always MatchType = Exact.
    BreakpointResolverName (Breakpoint *bkpt,
                            const char *names[],
                            size_t num_names,
                            uint32_t name_type_mask,
                            bool skip_prologue);

    // This one takes a C++ array of names.  It is always MatchType = Exact.
    BreakpointResolverName (Breakpoint *bkpt,
                            std::vector<std::string> names,
                            uint32_t name_type_mask,
                            bool skip_prologue);

    // Creates a function breakpoint by regular expression.  Takes over control of the lifespan of func_regex.
    BreakpointResolverName (Breakpoint *bkpt,
                            RegularExpression &func_regex,
                            bool skip_prologue);

    BreakpointResolverName (Breakpoint *bkpt,
                            const char *class_name,
                            const char *method,
                            Breakpoint::MatchType type,
                            bool skip_prologue);

    virtual
    ~BreakpointResolverName ();

    virtual Searcher::CallbackReturn
    SearchCallback (SearchFilter &filter,
                    SymbolContext &context,
                    Address *addr,
                    bool containing);

    virtual Searcher::Depth
    GetDepth ();

    virtual void
    GetDescription (Stream *s);

    virtual void
    Dump (Stream *s) const;

    /// Methods for support type inquiry through isa, cast, and dyn_cast:
    static inline bool classof(const BreakpointResolverName *) { return true; }
    static inline bool classof(const BreakpointResolver *V) {
        return V->getResolverID() == BreakpointResolver::NameResolver;
    }

protected:
    struct LookupInfo
    {
        ConstString name;
        ConstString lookup_name;
        uint32_t name_type_mask; // See FunctionNameType
        bool match_name_after_lookup;
        
        LookupInfo () :
            name(),
            lookup_name(),
            name_type_mask (0),
            match_name_after_lookup (false)
        {
        }
        
        void
        Prune (SymbolContextList &sc_list,
               size_t start_idx) const;
    };
    std::vector<LookupInfo> m_lookups;
    ConstString m_class_name;
    RegularExpression m_regex;
    Breakpoint::MatchType m_match_type;
    bool m_skip_prologue;

    void
    AddNameLookup (const ConstString &name, uint32_t name_type_mask);
private:
    DISALLOW_COPY_AND_ASSIGN(BreakpointResolverName);
};

} // namespace lldb_private

#endif  // liblldb_BreakpointResolverName_h_
