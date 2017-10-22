//===-- SearchFilter.cpp ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes

#include "lldb/lldb-private.h"
#include "lldb/Core/SearchFilter.h"
#include "lldb/Core/Module.h"
#include "lldb/Symbol/CompileUnit.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// SearchFilter constructor
//----------------------------------------------------------------------
Searcher::Searcher ()
{

}

Searcher::~Searcher ()
{

}

void
Searcher::GetDescription (Stream *s)
{
}

//----------------------------------------------------------------------
// SearchFilter constructor
//----------------------------------------------------------------------
SearchFilter::SearchFilter(const TargetSP &target_sp) :
    m_target_sp (target_sp)
{
}

//----------------------------------------------------------------------
// SearchFilter copy constructor
//----------------------------------------------------------------------
SearchFilter::SearchFilter(const SearchFilter& rhs) :
    m_target_sp (rhs.m_target_sp)
{
}

//----------------------------------------------------------------------
// SearchFilter assignment operator
//----------------------------------------------------------------------
const SearchFilter&
SearchFilter::operator=(const SearchFilter& rhs)
{
    m_target_sp = rhs.m_target_sp;
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
SearchFilter::~SearchFilter()
{
}

bool
SearchFilter::ModulePasses (const FileSpec &spec)
{
    return true;
}

bool
SearchFilter::ModulePasses (const ModuleSP &module_sp)
{
    return true;
}

bool
SearchFilter::AddressPasses (Address &address)
{
    return true;
}

bool
SearchFilter::CompUnitPasses (FileSpec &fileSpec)
{
    return true;
}

bool
SearchFilter::CompUnitPasses (CompileUnit &compUnit)
{
    return true;
}

uint32_t
SearchFilter::GetFilterRequiredItems()
{
    return (lldb::SymbolContextItem) 0;
}

void
SearchFilter::GetDescription (Stream *s)
{
}

void
SearchFilter::Dump (Stream *s) const
{

}

//----------------------------------------------------------------------
// UTILITY Functions to help iterate down through the elements of the
// SymbolContext.
//----------------------------------------------------------------------

void
SearchFilter::Search (Searcher &searcher)
{
    SymbolContext empty_sc;

    if (!m_target_sp)
        return;
    empty_sc.target_sp = m_target_sp;

    if (searcher.GetDepth() == Searcher::eDepthTarget)
        searcher.SearchCallback (*this, empty_sc, NULL, false);
    else
        DoModuleIteration(empty_sc, searcher);
}

void
SearchFilter::SearchInModuleList (Searcher &searcher, ModuleList &modules)
{
    SymbolContext empty_sc;

    if (!m_target_sp)
        return;
    empty_sc.target_sp = m_target_sp;

    if (searcher.GetDepth() == Searcher::eDepthTarget)
        searcher.SearchCallback (*this, empty_sc, NULL, false);
    else
    {
        Mutex::Locker modules_locker(modules.GetMutex());
        const size_t numModules = modules.GetSize();

        for (size_t i = 0; i < numModules; i++)
        {
            ModuleSP module_sp(modules.GetModuleAtIndexUnlocked(i));
            if (ModulePasses(module_sp))
            {
                if (DoModuleIteration(module_sp, searcher) == Searcher::eCallbackReturnStop)
                    return;
            }
        }
    }
}


Searcher::CallbackReturn
SearchFilter::DoModuleIteration (const lldb::ModuleSP& module_sp, Searcher &searcher)
{
    SymbolContext matchingContext (m_target_sp, module_sp);
    return DoModuleIteration(matchingContext, searcher);
}

Searcher::CallbackReturn
SearchFilter::DoModuleIteration (const SymbolContext &context, Searcher &searcher)
{
    if (searcher.GetDepth () >= Searcher::eDepthModule)
    {
        if (context.module_sp)
        {
            if (searcher.GetDepth () == Searcher::eDepthModule)
            {
                SymbolContext matchingContext(context.module_sp.get());
                searcher.SearchCallback (*this, matchingContext, NULL, false);
            }
            else
            {
                return DoCUIteration(context.module_sp, context, searcher);
            }
        }
        else
        {
            const ModuleList &target_images = m_target_sp->GetImages();
            Mutex::Locker modules_locker(target_images.GetMutex());
            
            size_t n_modules = target_images.GetSize();
            for (size_t i = 0; i < n_modules; i++)
            {
                // If this is the last level supplied, then call the callback directly,
                // otherwise descend.
                ModuleSP module_sp(target_images.GetModuleAtIndexUnlocked (i));
                if (!ModulePasses (module_sp))
                    continue;

                if (searcher.GetDepth () == Searcher::eDepthModule)
                {
                    SymbolContext matchingContext(m_target_sp, module_sp);

                    Searcher::CallbackReturn shouldContinue = searcher.SearchCallback (*this, matchingContext, NULL, false);
                    if (shouldContinue == Searcher::eCallbackReturnStop
                        || shouldContinue == Searcher::eCallbackReturnPop)
                        return shouldContinue;
                }
                else
                {
                    Searcher::CallbackReturn shouldContinue = DoCUIteration(module_sp, context, searcher);
                    if (shouldContinue == Searcher::eCallbackReturnStop)
                        return shouldContinue;
                    else if (shouldContinue == Searcher::eCallbackReturnPop)
                        continue;
                }
            }
        }
    }
    return Searcher::eCallbackReturnContinue;
}

Searcher::CallbackReturn
SearchFilter::DoCUIteration (const ModuleSP &module_sp, const SymbolContext &context, Searcher &searcher)
{
    Searcher::CallbackReturn shouldContinue;
    if (context.comp_unit == NULL)
    {
        const size_t num_comp_units = module_sp->GetNumCompileUnits();
        for (size_t i = 0; i < num_comp_units; i++)
        {
            CompUnitSP cu_sp (module_sp->GetCompileUnitAtIndex (i));
            if (cu_sp)
            {
                if (!CompUnitPasses (*(cu_sp.get())))
                    continue;

                if (searcher.GetDepth () == Searcher::eDepthCompUnit)
                {
                    SymbolContext matchingContext(m_target_sp, module_sp, cu_sp.get());

                    shouldContinue = searcher.SearchCallback (*this, matchingContext, NULL, false);

                    if (shouldContinue == Searcher::eCallbackReturnPop)
                        return Searcher::eCallbackReturnContinue;
                    else if (shouldContinue == Searcher::eCallbackReturnStop)
                        return shouldContinue;
                }
                else
                {
                    // FIXME Descend to block.
                }
            }
        }
    }
    else
    {
        if (CompUnitPasses(*context.comp_unit))
        {
            SymbolContext matchingContext (m_target_sp, module_sp, context.comp_unit);
            return searcher.SearchCallback (*this, matchingContext, NULL, false);
        }
    }
    return Searcher::eCallbackReturnContinue;
}

Searcher::CallbackReturn
SearchFilter::DoFunctionIteration (Function *function, const SymbolContext &context, Searcher &searcher)
{
    // FIXME: Implement...
    return Searcher::eCallbackReturnContinue;
}

//----------------------------------------------------------------------
//  SearchFilterForNonModuleSpecificSearches:
//  Selects a shared library matching a given file spec, consulting the targets "black list".
//----------------------------------------------------------------------

    bool 
    SearchFilterForNonModuleSpecificSearches::ModulePasses (const FileSpec &module_spec)
    {
        if (m_target_sp->ModuleIsExcludedForNonModuleSpecificSearches (module_spec))
            return false;
        else
            return true;
    }
    
    bool
    SearchFilterForNonModuleSpecificSearches::ModulePasses (const lldb::ModuleSP &module_sp)
    {
        if (!module_sp)
            return true;
        else if (m_target_sp->ModuleIsExcludedForNonModuleSpecificSearches (module_sp))
            return false;
        else
            return true;
    }

//----------------------------------------------------------------------
//  SearchFilterByModule:
//  Selects a shared library matching a given file spec
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// SearchFilterByModule constructors
//----------------------------------------------------------------------

SearchFilterByModule::SearchFilterByModule (const lldb::TargetSP &target_sp, const FileSpec &module) :
    SearchFilter (target_sp),
    m_module_spec (module)
{
}


//----------------------------------------------------------------------
// SearchFilterByModule copy constructor
//----------------------------------------------------------------------
SearchFilterByModule::SearchFilterByModule(const SearchFilterByModule& rhs) :
    SearchFilter (rhs),
    m_module_spec (rhs.m_module_spec)
{
}

//----------------------------------------------------------------------
// SearchFilterByModule assignment operator
//----------------------------------------------------------------------
const SearchFilterByModule&
SearchFilterByModule::operator=(const SearchFilterByModule& rhs)
{
    m_target_sp = rhs.m_target_sp;
    m_module_spec = rhs.m_module_spec;
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
SearchFilterByModule::~SearchFilterByModule()
{
}

bool
SearchFilterByModule::ModulePasses (const ModuleSP &module_sp)
{
    if (module_sp && FileSpec::Equal(module_sp->GetFileSpec(), m_module_spec, false))
        return true;
    else
        return false;
}

bool
SearchFilterByModule::ModulePasses (const FileSpec &spec)
{
    // Do a full match only if "spec" has a directory
    const bool full_match = spec.GetDirectory();
    return FileSpec::Equal(spec, m_module_spec, full_match);
}

bool
SearchFilterByModule::AddressPasses (Address &address)
{
    // FIXME: Not yet implemented
    return true;
}


bool
SearchFilterByModule::CompUnitPasses (FileSpec &fileSpec)
{
    return true;
}

bool
SearchFilterByModule::CompUnitPasses (CompileUnit &compUnit)
{
    return true;
}

void
SearchFilterByModule::Search (Searcher &searcher)
{
    if (!m_target_sp)
        return;

    if (searcher.GetDepth() == Searcher::eDepthTarget)
    {
        SymbolContext empty_sc;
        empty_sc.target_sp = m_target_sp;
        searcher.SearchCallback (*this, empty_sc, NULL, false);
    }

    // If the module file spec is a full path, then we can just find the one
    // filespec that passes.  Otherwise, we need to go through all modules and
    // find the ones that match the file name.

    const ModuleList &target_modules = m_target_sp->GetImages();
    Mutex::Locker modules_locker (target_modules.GetMutex());
    
    const size_t num_modules = target_modules.GetSize ();
    for (size_t i = 0; i < num_modules; i++)
    {
        Module* module = target_modules.GetModulePointerAtIndexUnlocked(i);
        const bool full_match = m_module_spec.GetDirectory();
        if (FileSpec::Equal (m_module_spec, module->GetFileSpec(), full_match))
        {
            SymbolContext matchingContext(m_target_sp, module->shared_from_this());
            Searcher::CallbackReturn shouldContinue;

            shouldContinue = DoModuleIteration(matchingContext, searcher);
            if (shouldContinue == Searcher::eCallbackReturnStop)
                return;
        }
    }
}

void
SearchFilterByModule::GetDescription (Stream *s)
{
    s->PutCString(", module = ");
    if (s->GetVerbose())
    {
        char buffer[2048];
        m_module_spec.GetPath(buffer, 2047);
        s->PutCString(buffer);
    }
    else
    {
        s->PutCString(m_module_spec.GetFilename().AsCString("<unknown>"));
    }
}

uint32_t
SearchFilterByModule::GetFilterRequiredItems()
{
    return eSymbolContextModule;
}

void
SearchFilterByModule::Dump (Stream *s) const
{

}
//----------------------------------------------------------------------
//  SearchFilterByModuleList:
//  Selects a shared library matching a given file spec
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// SearchFilterByModuleList constructors
//----------------------------------------------------------------------

SearchFilterByModuleList::SearchFilterByModuleList (const lldb::TargetSP &target_sp, const FileSpecList &module_list) :
    SearchFilter (target_sp),
    m_module_spec_list (module_list)
{
}


//----------------------------------------------------------------------
// SearchFilterByModuleList copy constructor
//----------------------------------------------------------------------
SearchFilterByModuleList::SearchFilterByModuleList(const SearchFilterByModuleList& rhs) :
    SearchFilter (rhs),
    m_module_spec_list (rhs.m_module_spec_list)
{
}

//----------------------------------------------------------------------
// SearchFilterByModuleList assignment operator
//----------------------------------------------------------------------
const SearchFilterByModuleList&
SearchFilterByModuleList::operator=(const SearchFilterByModuleList& rhs)
{
    m_target_sp = rhs.m_target_sp;
    m_module_spec_list = rhs.m_module_spec_list;
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
SearchFilterByModuleList::~SearchFilterByModuleList()
{
}

bool
SearchFilterByModuleList::ModulePasses (const ModuleSP &module_sp)
{
    if (m_module_spec_list.GetSize() == 0)
        return true;
        
    if (module_sp && m_module_spec_list.FindFileIndex(0, module_sp->GetFileSpec(), false) != UINT32_MAX)
        return true;
    else
        return false;
}

bool
SearchFilterByModuleList::ModulePasses (const FileSpec &spec)
{
    if (m_module_spec_list.GetSize() == 0)
        return true;
        
    if (m_module_spec_list.FindFileIndex(0, spec, true) != UINT32_MAX)
        return true;
    else
        return false;
}

bool
SearchFilterByModuleList::AddressPasses (Address &address)
{
    // FIXME: Not yet implemented
    return true;
}


bool
SearchFilterByModuleList::CompUnitPasses (FileSpec &fileSpec)
{
    return true;
}

bool
SearchFilterByModuleList::CompUnitPasses (CompileUnit &compUnit)
{
    return true;
}

void
SearchFilterByModuleList::Search (Searcher &searcher)
{
    if (!m_target_sp)
        return;

    if (searcher.GetDepth() == Searcher::eDepthTarget)
    {
        SymbolContext empty_sc;
        empty_sc.target_sp = m_target_sp;
        searcher.SearchCallback (*this, empty_sc, NULL, false);
    }

    // If the module file spec is a full path, then we can just find the one
    // filespec that passes.  Otherwise, we need to go through all modules and
    // find the ones that match the file name.

    const ModuleList &target_modules = m_target_sp->GetImages();
    Mutex::Locker modules_locker (target_modules.GetMutex());
    
    const size_t num_modules = target_modules.GetSize ();
    for (size_t i = 0; i < num_modules; i++)
    {
        Module* module = target_modules.GetModulePointerAtIndexUnlocked(i);
        if (m_module_spec_list.FindFileIndex(0, module->GetFileSpec(), false) != UINT32_MAX)
        {
            SymbolContext matchingContext(m_target_sp, module->shared_from_this());
            Searcher::CallbackReturn shouldContinue;

            shouldContinue = DoModuleIteration(matchingContext, searcher);
            if (shouldContinue == Searcher::eCallbackReturnStop)
                return;
        }
    }
}

void
SearchFilterByModuleList::GetDescription (Stream *s)
{
    size_t num_modules = m_module_spec_list.GetSize();
    if (num_modules == 1)
    {
        s->Printf (", module = ");
        if (s->GetVerbose())
        {
            char buffer[2048];
            m_module_spec_list.GetFileSpecAtIndex(0).GetPath(buffer, 2047);
            s->PutCString(buffer);
        }
        else
        {
            s->PutCString(m_module_spec_list.GetFileSpecAtIndex(0).GetFilename().AsCString("<unknown>"));
        }
    }
    else
    {
        s->Printf (", modules(%zu) = ", num_modules);
        for (size_t i = 0; i < num_modules; i++)
        {
            if (s->GetVerbose())
            {
                char buffer[2048];
                m_module_spec_list.GetFileSpecAtIndex(i).GetPath(buffer, 2047);
                s->PutCString(buffer);
            }
            else
            {
                s->PutCString(m_module_spec_list.GetFileSpecAtIndex(i).GetFilename().AsCString("<unknown>"));
            }
            if (i != num_modules - 1)
                s->PutCString (", ");
        }
    }
}

uint32_t
SearchFilterByModuleList::GetFilterRequiredItems()
{
    return eSymbolContextModule;
}

void
SearchFilterByModuleList::Dump (Stream *s) const
{

}

//----------------------------------------------------------------------
//  SearchFilterByModuleListAndCU:
//  Selects a shared library matching a given file spec
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// SearchFilterByModuleListAndCU constructors
//----------------------------------------------------------------------

SearchFilterByModuleListAndCU::SearchFilterByModuleListAndCU (const lldb::TargetSP &target_sp, 
                                                              const FileSpecList &module_list,
                                                              const FileSpecList &cu_list) :
    SearchFilterByModuleList (target_sp, module_list),
    m_cu_spec_list (cu_list)
{
}


//----------------------------------------------------------------------
// SearchFilterByModuleListAndCU copy constructor
//----------------------------------------------------------------------
SearchFilterByModuleListAndCU::SearchFilterByModuleListAndCU(const SearchFilterByModuleListAndCU& rhs) :
    SearchFilterByModuleList (rhs),
    m_cu_spec_list (rhs.m_cu_spec_list)
{
}

//----------------------------------------------------------------------
// SearchFilterByModuleListAndCU assignment operator
//----------------------------------------------------------------------
const SearchFilterByModuleListAndCU&
SearchFilterByModuleListAndCU::operator=(const SearchFilterByModuleListAndCU& rhs)
{
    if (&rhs != this)
    {
        m_target_sp = rhs.m_target_sp;
        m_module_spec_list = rhs.m_module_spec_list;
        m_cu_spec_list = rhs.m_cu_spec_list;
    }
    return *this;
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
SearchFilterByModuleListAndCU::~SearchFilterByModuleListAndCU()
{
}

bool
SearchFilterByModuleListAndCU::AddressPasses (Address &address)
{
    return true;
}


bool
SearchFilterByModuleListAndCU::CompUnitPasses (FileSpec &fileSpec)
{
    return m_cu_spec_list.FindFileIndex(0, fileSpec, false) != UINT32_MAX;
}

bool
SearchFilterByModuleListAndCU::CompUnitPasses (CompileUnit &compUnit)
{
    bool in_cu_list = m_cu_spec_list.FindFileIndex(0, compUnit, false) != UINT32_MAX;
    if (in_cu_list)
    {
        ModuleSP module_sp(compUnit.GetModule());
        if (module_sp)
        {
            bool module_passes = SearchFilterByModuleList::ModulePasses(module_sp);
            return module_passes;
        }
        else
            return true;
    }
    else
        return false;
}

void
SearchFilterByModuleListAndCU::Search (Searcher &searcher)
{
    if (!m_target_sp)
        return;

    if (searcher.GetDepth() == Searcher::eDepthTarget)
    {
        SymbolContext empty_sc;
        empty_sc.target_sp = m_target_sp;
        searcher.SearchCallback (*this, empty_sc, NULL, false);
    }

    // If the module file spec is a full path, then we can just find the one
    // filespec that passes.  Otherwise, we need to go through all modules and
    // find the ones that match the file name.

    ModuleList matching_modules;
    const ModuleList &target_images = m_target_sp->GetImages();
    Mutex::Locker modules_locker(target_images.GetMutex());
    
    const size_t num_modules = target_images.GetSize ();
    bool no_modules_in_filter = m_module_spec_list.GetSize() == 0;
    for (size_t i = 0; i < num_modules; i++)
    {
        lldb::ModuleSP module_sp = target_images.GetModuleAtIndexUnlocked(i);
        if (no_modules_in_filter || m_module_spec_list.FindFileIndex(0, module_sp->GetFileSpec(), false) != UINT32_MAX)
        {
            SymbolContext matchingContext(m_target_sp, module_sp);
            Searcher::CallbackReturn shouldContinue;

            if (searcher.GetDepth() == Searcher::eDepthModule)
            {
                shouldContinue = DoModuleIteration(matchingContext, searcher);
                if (shouldContinue == Searcher::eCallbackReturnStop)
                    return;
            }
            else
            {
                const size_t num_cu = module_sp->GetNumCompileUnits();
                for (size_t cu_idx = 0; cu_idx < num_cu; cu_idx++)
                {
                    CompUnitSP cu_sp = module_sp->GetCompileUnitAtIndex(cu_idx);
                    matchingContext.comp_unit = cu_sp.get();
                    if (matchingContext.comp_unit)
                    {
                        if (m_cu_spec_list.FindFileIndex(0, *matchingContext.comp_unit, false) != UINT32_MAX)
                        {
                            shouldContinue = DoCUIteration(module_sp, matchingContext, searcher);
                            if (shouldContinue == Searcher::eCallbackReturnStop)
                                return;
                        }
                    }
                }
            }
        }
    }
}

void
SearchFilterByModuleListAndCU::GetDescription (Stream *s)
{
    size_t num_modules = m_module_spec_list.GetSize();
    if (num_modules == 1)
    {
        s->Printf (", module = ");
        if (s->GetVerbose())
        {
            char buffer[2048];
            m_module_spec_list.GetFileSpecAtIndex(0).GetPath(buffer, 2047);
            s->PutCString(buffer);
        }
        else
        {
            s->PutCString(m_module_spec_list.GetFileSpecAtIndex(0).GetFilename().AsCString("<unknown>"));
        }
    }
    else if (num_modules > 0)
    {
        s->Printf (", modules(%zd) = ", num_modules);
        for (size_t i = 0; i < num_modules; i++)
        {
            if (s->GetVerbose())
            {
                char buffer[2048];
                m_module_spec_list.GetFileSpecAtIndex(i).GetPath(buffer, 2047);
                s->PutCString(buffer);
            }
            else
            {
                s->PutCString(m_module_spec_list.GetFileSpecAtIndex(i).GetFilename().AsCString("<unknown>"));
            }
            if (i != num_modules - 1)
                s->PutCString (", ");
        }
    }
}

uint32_t
SearchFilterByModuleListAndCU::GetFilterRequiredItems()
{
    return eSymbolContextModule | eSymbolContextCompUnit;
}

void
SearchFilterByModuleListAndCU::Dump (Stream *s) const
{

}

