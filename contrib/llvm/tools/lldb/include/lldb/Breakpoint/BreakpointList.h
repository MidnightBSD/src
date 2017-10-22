//===-- BreakpointList.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_BreakpointList_h_
#define liblldb_BreakpointList_h_

// C Includes
// C++ Includes
#include <list>
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/Breakpoint.h"
#include "lldb/Host/Mutex.h"

namespace lldb_private {

//----------------------------------------------------------------------
/// @class BreakpointList BreakpointList.h "lldb/Breakpoint/BreakpointList.h"
/// @brief This class manages a list of breakpoints.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
/// General Outline:
/// Allows adding and removing breakpoints and find by ID and index.
//----------------------------------------------------------------------

class BreakpointList
{
public:
    BreakpointList (bool is_internal);

    ~BreakpointList();

    //------------------------------------------------------------------
    /// Add the breakpoint \a bp_sp to the list.
    ///
    /// @param[in] bp_sp
    ///   Shared pointer to the breakpoint that will get added to the list.
    ///
    /// @result
    ///   Returns breakpoint id.
    //------------------------------------------------------------------
    lldb::break_id_t
    Add (lldb::BreakpointSP& bp_sp, bool notify);

    //------------------------------------------------------------------
    /// Standard "Dump" method.  At present it does nothing.
    //------------------------------------------------------------------
    void
    Dump (Stream *s) const;

    //------------------------------------------------------------------
    /// Returns a shared pointer to the breakpoint with id \a breakID.
    ///
    /// @param[in] breakID
    ///   The breakpoint ID to seek for.
    ///
    /// @result
    ///   A shared pointer to the breakpoint.  May contain a NULL pointer if the
    ///   breakpoint doesn't exist.
    //------------------------------------------------------------------
    lldb::BreakpointSP
    FindBreakpointByID (lldb::break_id_t breakID);

    //------------------------------------------------------------------
    /// Returns a shared pointer to the breakpoint with id \a breakID.  Const version.
    ///
    /// @param[in] breakID
    ///   The breakpoint ID to seek for.
    ///
    /// @result
    ///   A shared pointer to the breakpoint.  May contain a NULL pointer if the
    ///   breakpoint doesn't exist.
    //------------------------------------------------------------------
    const lldb::BreakpointSP
    FindBreakpointByID (lldb::break_id_t breakID) const;

    //------------------------------------------------------------------
    /// Returns a shared pointer to the breakpoint with index \a i.
    ///
    /// @param[in] i
    ///   The breakpoint index to seek for.
    ///
    /// @result
    ///   A shared pointer to the breakpoint.  May contain a NULL pointer if the
    ///   breakpoint doesn't exist.
    //------------------------------------------------------------------
    lldb::BreakpointSP
    GetBreakpointAtIndex (size_t i);

    //------------------------------------------------------------------
    /// Returns a shared pointer to the breakpoint with index \a i, const version
    ///
    /// @param[in] i
    ///   The breakpoint index to seek for.
    ///
    /// @result
    ///   A shared pointer to the breakpoint.  May contain a NULL pointer if the
    ///   breakpoint doesn't exist.
    //------------------------------------------------------------------
    const lldb::BreakpointSP
    GetBreakpointAtIndex (size_t i) const;

    //------------------------------------------------------------------
    /// Returns the number of elements in this breakpoint list.
    ///
    /// @result
    ///   The number of elements.
    //------------------------------------------------------------------
    size_t
    GetSize() const 
    {
        Mutex::Locker locker(m_mutex);
        return m_breakpoints.size(); 
    }

    //------------------------------------------------------------------
    /// Removes the breakpoint given by \b breakID from this list.
    ///
    /// @param[in] breakID
    ///   The breakpoint index to remove.
    ///
    /// @result
    ///   \b true if the breakpoint \a breakID was in the list.
    //------------------------------------------------------------------
    bool
    Remove (lldb::break_id_t breakID, bool notify);

    void
    SetEnabledAll (bool enabled);

    //------------------------------------------------------------------
    /// Removes all the breakpoints from this list.
    //------------------------------------------------------------------
    void
    RemoveAll (bool notify);

    //------------------------------------------------------------------
    /// Tell all the breakpoints to update themselves due to a change in the
    /// modules in \a module_list.  \a added says whether the module was loaded
    /// or unloaded.
    ///
    /// @param[in] module_list
    ///   The module list that has changed.
    ///
    /// @param[in] added
    ///   \b true if the modules are loaded, \b false if unloaded.
    //------------------------------------------------------------------
    void
    UpdateBreakpoints (ModuleList &module_list, bool added);
    
    void
    UpdateBreakpointsWhenModuleIsReplaced (lldb::ModuleSP old_module_sp, lldb::ModuleSP new_module_sp);

    void
    ClearAllBreakpointSites ();
    
    //------------------------------------------------------------------
    /// Sets the passed in Locker to hold the Breakpoint List mutex.
    ///
    /// @param[in] locker
    ///   The locker object that is set.
    //------------------------------------------------------------------
    void
    GetListMutex (lldb_private::Mutex::Locker &locker);

protected:
    typedef std::list<lldb::BreakpointSP> bp_collection;

    bp_collection::iterator
    GetBreakpointIDIterator(lldb::break_id_t breakID);

    bp_collection::const_iterator
    GetBreakpointIDConstIterator(lldb::break_id_t breakID) const;

    mutable Mutex m_mutex;
    bp_collection m_breakpoints;  // The breakpoint list, currently a list.
    lldb::break_id_t m_next_break_id;
    bool m_is_internal;

private:
    DISALLOW_COPY_AND_ASSIGN (BreakpointList);
};

} // namespace lldb_private

#endif  // liblldb_BreakpointList_h_
