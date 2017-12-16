//===-- StopInfo.cpp ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "lldb/Target/StopInfo.h"

// C Includes
// C++ Includes
#include <string>

// Other libraries and framework includes
// Project includes
#include "lldb/Core/Log.h"
#include "lldb/Breakpoint/Breakpoint.h"
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Breakpoint/StoppointCallbackContext.h"
#include "lldb/Breakpoint/Watchpoint.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Expression/ClangUserExpression.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"
#include "lldb/Target/ThreadPlan.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/UnixSignals.h"

using namespace lldb;
using namespace lldb_private;

StopInfo::StopInfo (Thread &thread, uint64_t value) :
    m_thread_wp (thread.shared_from_this()),
    m_stop_id (thread.GetProcess()->GetStopID()),
    m_resume_id (thread.GetProcess()->GetResumeID()),
    m_value (value),
    m_override_should_notify (eLazyBoolCalculate),
    m_override_should_stop (eLazyBoolCalculate)
{
}

bool
StopInfo::IsValid () const
{
    ThreadSP thread_sp (m_thread_wp.lock());
    if (thread_sp)
        return thread_sp->GetProcess()->GetStopID() == m_stop_id;
    return false;
}

void
StopInfo::MakeStopInfoValid ()
{
    ThreadSP thread_sp (m_thread_wp.lock());
    if (thread_sp)
    {
        m_stop_id = thread_sp->GetProcess()->GetStopID();
        m_resume_id = thread_sp->GetProcess()->GetResumeID();
    }
}

bool
StopInfo::HasTargetRunSinceMe ()
{
    ThreadSP thread_sp (m_thread_wp.lock());

    if (thread_sp)
    {
        lldb::StateType ret_type = thread_sp->GetProcess()->GetPrivateState();
        if (ret_type == eStateRunning)
        {
            return true;
        }
        else if (ret_type == eStateStopped)
        {
            // This is a little tricky.  We want to count "run and stopped again before you could
            // ask this question as a "TRUE" answer to HasTargetRunSinceMe.  But we don't want to 
            // include any running of the target done for expressions.  So we track both resumes,
            // and resumes caused by expressions, and check if there are any resumes NOT caused
            // by expressions.
            
            uint32_t curr_resume_id = thread_sp->GetProcess()->GetResumeID();
            uint32_t last_user_expression_id = thread_sp->GetProcess()->GetLastUserExpressionResumeID ();
            if (curr_resume_id == m_resume_id)
            {
                return false;
            }
            else if (curr_resume_id > last_user_expression_id)
            {
                return true;
            }
        }
    }
    return false;
}

//----------------------------------------------------------------------
// StopInfoBreakpoint
//----------------------------------------------------------------------

namespace lldb_private
{
class StopInfoBreakpoint : public StopInfo
{
public:

    StopInfoBreakpoint (Thread &thread, break_id_t break_id) :
        StopInfo (thread, break_id),
        m_description(),
        m_should_stop (false),
        m_should_stop_is_valid (false),
        m_should_perform_action (true),
        m_address (LLDB_INVALID_ADDRESS),
        m_break_id(LLDB_INVALID_BREAK_ID),
        m_was_one_shot (false)
    {
        StoreBPInfo();
    }
    
    StopInfoBreakpoint (Thread &thread, break_id_t break_id, bool should_stop) :
        StopInfo (thread, break_id),
        m_description(),
        m_should_stop (should_stop),
        m_should_stop_is_valid (true),
        m_should_perform_action (true),
        m_address (LLDB_INVALID_ADDRESS),
        m_break_id(LLDB_INVALID_BREAK_ID),
        m_was_one_shot (false)
    {
        StoreBPInfo();
    }

    void
    StoreBPInfo ()
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            BreakpointSiteSP bp_site_sp (thread_sp->GetProcess()->GetBreakpointSiteList().FindByID (m_value));
            if (bp_site_sp)
            {
                if (bp_site_sp->GetNumberOfOwners() == 1)
                {
                    BreakpointLocationSP bp_loc_sp = bp_site_sp->GetOwnerAtIndex(0);
                    if (bp_loc_sp)
                    {
                        m_break_id = bp_loc_sp->GetBreakpoint().GetID();
                        m_was_one_shot = bp_loc_sp->GetBreakpoint().IsOneShot();
                    }
                }
                m_address = bp_site_sp->GetLoadAddress();
            }
        }
    }

    virtual ~StopInfoBreakpoint ()
    {
    }
    
    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonBreakpoint;
    }

    virtual bool
    ShouldStopSynchronous (Event *event_ptr)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            if (!m_should_stop_is_valid)
            {
                // Only check once if we should stop at a breakpoint
                BreakpointSiteSP bp_site_sp (thread_sp->GetProcess()->GetBreakpointSiteList().FindByID (m_value));
                if (bp_site_sp)
                {
                    ExecutionContext exe_ctx (thread_sp->GetStackFrameAtIndex(0));
                    StoppointCallbackContext context (event_ptr, exe_ctx, true);
                    m_should_stop = bp_site_sp->ShouldStop (&context);
                }
                else
                {
                    Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));

                    if (log)
                        log->Printf ("Process::%s could not find breakpoint site id: %" PRId64 "...", __FUNCTION__, m_value);

                    m_should_stop = true;
                }
                m_should_stop_is_valid = true;
            }
            return m_should_stop;
        }
        return false;
    }
    
    virtual bool
    DoShouldNotify (Event *event_ptr)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            BreakpointSiteSP bp_site_sp (thread_sp->GetProcess()->GetBreakpointSiteList().FindByID (m_value));
            if (bp_site_sp)
            {
                bool all_internal = true;

                for (uint32_t i = 0; i < bp_site_sp->GetNumberOfOwners(); i++)
                {
                    if (!bp_site_sp->GetOwnerAtIndex(i)->GetBreakpoint().IsInternal())
                    {
                        all_internal = false;
                        break;
                    }
                }
                return all_internal == false;
            }
        }
        return true;
    }

    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
        {
            ThreadSP thread_sp (m_thread_wp.lock());
            if (thread_sp)
            {
                BreakpointSiteSP bp_site_sp (thread_sp->GetProcess()->GetBreakpointSiteList().FindByID (m_value));
                if (bp_site_sp)
                {
                    StreamString strm;
                    // If we have just hit an internal breakpoint, and it has a kind description, print that instead of the
                    // full breakpoint printing:
                    if (bp_site_sp->IsInternal())
                    {
                        size_t num_owners = bp_site_sp->GetNumberOfOwners();
                        for (size_t idx = 0; idx < num_owners; idx++)
                        {
                            const char *kind = bp_site_sp->GetOwnerAtIndex(idx)->GetBreakpoint().GetBreakpointKind();
                            if (kind != NULL)
                            {
                                m_description.assign (kind);
                                return kind;
                            }
                        }
                    }
                    
                    strm.Printf("breakpoint ");
                    bp_site_sp->GetDescription(&strm, eDescriptionLevelBrief);
                    m_description.swap (strm.GetString());
                }
                else
                {
                    StreamString strm;
                    if (m_break_id != LLDB_INVALID_BREAK_ID)
                    {
                        BreakpointSP break_sp = thread_sp->GetProcess()->GetTarget().GetBreakpointByID(m_break_id);
                        if (break_sp)
                        {
                            if (break_sp->IsInternal())
                            {
                                const char *kind = break_sp->GetBreakpointKind();
                                if (kind)
                                    strm.Printf ("internal %s breakpoint(%d).", kind, m_break_id);
                                else
                                    strm.Printf ("internal breakpoint(%d).", m_break_id);
                            }
                            else
                            {
                                strm.Printf ("breakpoint %d.", m_break_id);
                            }
                        } 
                        else
                        {
                            if (m_was_one_shot)
                                strm.Printf ("one-shot breakpoint %d", m_break_id);
                            else
                                strm.Printf ("breakpoint %d which has been deleted.", m_break_id);
                        }
                    }
                    else if (m_address == LLDB_INVALID_ADDRESS)
                        strm.Printf("breakpoint site %" PRIi64 " which has been deleted - unknown address", m_value);
                    else
                        strm.Printf("breakpoint site %" PRIi64 " which has been deleted - was at 0x%" PRIx64, m_value, m_address);
                    
                    m_description.swap (strm.GetString());
                }
            }
        }
        return m_description.c_str();
    }

protected:
    bool
    ShouldStop (Event *event_ptr)
    {
        // This just reports the work done by PerformAction or the synchronous stop.  It should
        // only ever get called after they have had a chance to run.
        assert (m_should_stop_is_valid);
        return m_should_stop;
    }
    
    virtual void
    PerformAction (Event *event_ptr)
    {
        if (!m_should_perform_action)
            return;
        m_should_perform_action = false;
        
        ThreadSP thread_sp (m_thread_wp.lock());

        if (thread_sp)
        {
            Log *log = lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_BREAKPOINTS);
            
            if (!thread_sp->IsValid())
            {
                // This shouldn't ever happen, but just in case, don't do more harm.
                if (log)
                {
                    log->Printf ("PerformAction got called with an invalid thread.");
                }
                m_should_stop = true;
                m_should_stop_is_valid = true;
                return;
            }
            
            BreakpointSiteSP bp_site_sp (thread_sp->GetProcess()->GetBreakpointSiteList().FindByID (m_value));
            
            if (bp_site_sp)
            {
                size_t num_owners = bp_site_sp->GetNumberOfOwners();
                    
                if (num_owners == 0)
                {
                    m_should_stop = true;
                }
                else
                {
                    // We go through each location, and test first its condition.  If the condition says to stop,
                    // then we run the callback for that location.  If that callback says to stop as well, then 
                    // we set m_should_stop to true; we are going to stop.
                    // But we still want to give all the breakpoints whose conditions say we are going to stop a
                    // chance to run their callbacks.
                    // Of course if any callback restarts the target by putting "continue" in the callback, then 
                    // we're going to restart, without running the rest of the callbacks.  And in this case we will
                    // end up not stopping even if another location said we should stop.  But that's better than not
                    // running all the callbacks.
                    
                    m_should_stop = false;

                    ExecutionContext exe_ctx (thread_sp->GetStackFrameAtIndex(0));
                    Process *process  = exe_ctx.GetProcessPtr();
                    if (process->GetModIDRef().IsLastResumeForUserExpression())
                    {
                        // If we are in the middle of evaluating an expression, don't run asynchronous breakpoint commands or
                        // expressions.  That could lead to infinite recursion if the command or condition re-calls the function
                        // with this breakpoint.
                        // TODO: We can keep a list of the breakpoints we've seen while running expressions in the nested
                        // PerformAction calls that can arise when the action runs a function that hits another breakpoint,
                        // and only stop running commands when we see the same breakpoint hit a second time.
                        
                        m_should_stop_is_valid = true;
                        if (log)
                            log->Printf ("StopInfoBreakpoint::PerformAction - Hit a breakpoint while running an expression,"
                                         " not running commands to avoid recursion.");
                        bool ignoring_breakpoints = process->GetIgnoreBreakpointsInExpressions();
                        if (ignoring_breakpoints)
                        {
                            m_should_stop = false;
                            // Internal breakpoints will always stop.  
                            for (size_t j = 0; j < num_owners; j++)
                            {
                                lldb::BreakpointLocationSP bp_loc_sp = bp_site_sp->GetOwnerAtIndex(j);
                                if (bp_loc_sp->GetBreakpoint().IsInternal())
                                {
                                    m_should_stop = true;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            m_should_stop = true;
                        }
                        if (log)
                            log->Printf ("StopInfoBreakpoint::PerformAction - in expression, continuing: %s.",
                                         m_should_stop ? "true" : "false");
                        process->GetTarget().GetDebugger().GetAsyncOutputStream()->Printf("Warning: hit breakpoint while "
                                               "running function, skipping commands and conditions to prevent recursion.");
                        return;
                    }
                    
                    StoppointCallbackContext context (event_ptr, exe_ctx, false);
                    
                    // Let's copy the breakpoint locations out of the site and store them in a local list.  That way if
                    // one of the breakpoint actions changes the site, then we won't be operating on a bad list.
                    
                    BreakpointLocationCollection site_locations;
                    for (size_t j = 0; j < num_owners; j++)
                        site_locations.Add(bp_site_sp->GetOwnerAtIndex(j));

                    for (size_t j = 0; j < num_owners; j++)
                    {
                        lldb::BreakpointLocationSP bp_loc_sp = site_locations.GetByIndex(j);
                        
                        // If another action disabled this breakpoint or its location, then don't run the actions.
                        if (!bp_loc_sp->IsEnabled() || !bp_loc_sp->GetBreakpoint().IsEnabled())
                            continue;
                        
                        // The breakpoint site may have many locations associated with it, not all of them valid for
                        // this thread.  Skip the ones that aren't:
                        if (!bp_loc_sp->ValidForThisThread(thread_sp.get()))
                        {
                            if (log)
                            {
                                StreamString s;
                                bp_loc_sp->GetDescription(&s, eDescriptionLevelBrief);
                                log->Printf ("Breakpoint %s hit on thread 0x%llx but it was not for this thread, continuing.", s.GetData(), thread_sp->GetID());
                            }
                            continue;
                        }
                        // First run the condition for the breakpoint.  If that says we should stop, then we'll run
                        // the callback for the breakpoint.  If the callback says we shouldn't stop that will win.                    
                        
                        if (bp_loc_sp->GetConditionText() != NULL)
                        {
                            Error condition_error;
                            bool condition_says_stop = bp_loc_sp->ConditionSaysStop(exe_ctx, condition_error);
                            
                            if (!condition_error.Success())
                            {
                                Debugger &debugger = exe_ctx.GetTargetRef().GetDebugger();
                                StreamSP error_sp = debugger.GetAsyncErrorStream ();
                                error_sp->Printf ("Stopped due to an error evaluating condition of breakpoint ");
                                bp_loc_sp->GetDescription (error_sp.get(), eDescriptionLevelBrief);
                                error_sp->Printf (": \"%s\"",
                                                  bp_loc_sp->GetConditionText());
                                error_sp->EOL();
                                const char *err_str = condition_error.AsCString("<Unknown Error>");
                                if (log)
                                    log->Printf("Error evaluating condition: \"%s\"\n", err_str);
                                
                                error_sp->PutCString (err_str);
                                error_sp->EOL();
                                error_sp->Flush();
                                // If the condition fails to be parsed or run, we should stop.
                                condition_says_stop = true;
                            }
                            else
                            {
                                if (log)
                                {
                                    StreamString s;
                                    bp_loc_sp->GetDescription(&s, eDescriptionLevelBrief);
                                    log->Printf ("Condition evaluated for breakpoint %s on thread 0x%llx conditon_says_stop: %i.", s.GetData(), thread_sp->GetID(), condition_says_stop);
                                }
                                if (!condition_says_stop)
                                    continue;
                            }
                        }
                                    
                        bool callback_says_stop;
                        
                        // FIXME: For now the callbacks have to run in async mode - the first time we restart we need
                        // to get out of there.  So set it here.
                        // When we figure out how to nest breakpoint hits then this will change.
                        
                        Debugger &debugger = thread_sp->CalculateTarget()->GetDebugger();
                        bool old_async = debugger.GetAsyncExecution();
                        debugger.SetAsyncExecution (true);
                        
                        callback_says_stop = bp_loc_sp->InvokeCallback (&context);
                        
                        debugger.SetAsyncExecution (old_async);
                        
                        if (callback_says_stop)
                            m_should_stop = true;
                        
                        // If we are going to stop for this breakpoint, then remove the breakpoint.
                        if (callback_says_stop && bp_loc_sp && bp_loc_sp->GetBreakpoint().IsOneShot())
                        {
                            thread_sp->GetProcess()->GetTarget().RemoveBreakpointByID (bp_loc_sp->GetBreakpoint().GetID());
                        }
                            
                        // Also make sure that the callback hasn't continued the target.  
                        // If it did, when we'll set m_should_start to false and get out of here.
                        if (HasTargetRunSinceMe ())
                        {
                            m_should_stop = false;
                            break;
                        }
                    }
                }
                // We've figured out what this stop wants to do, so mark it as valid so we don't compute it again.
                m_should_stop_is_valid = true;

            }
            else
            {
                m_should_stop = true;
                m_should_stop_is_valid = true;
                Log * log_process(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));

                if (log_process)
                    log_process->Printf ("Process::%s could not find breakpoint site id: %" PRId64 "...", __FUNCTION__, m_value);
            }
            if (log)
                log->Printf ("Process::%s returning from action with m_should_stop: %d.", __FUNCTION__, m_should_stop);
        }
    }

private:
    std::string m_description;
    bool m_should_stop;
    bool m_should_stop_is_valid;
    bool m_should_perform_action; // Since we are trying to preserve the "state" of the system even if we run functions
                                  // etc. behind the users backs, we need to make sure we only REALLY perform the action once.
    lldb::addr_t m_address;       // We use this to capture the breakpoint site address when we create the StopInfo,
                                  // in case somebody deletes it between the time the StopInfo is made and the
                                  // description is asked for.
    lldb::break_id_t m_break_id;
    bool m_was_one_shot;
};


//----------------------------------------------------------------------
// StopInfoWatchpoint
//----------------------------------------------------------------------

class StopInfoWatchpoint : public StopInfo
{
public:
    // Make sure watchpoint is properly disabled and subsequently enabled while performing watchpoint actions.
    class WatchpointSentry {
    public:
        WatchpointSentry(Process *p, Watchpoint *w):
            process(p),
            watchpoint(w)
        {
            if (process && watchpoint)
            {
                const bool notify = false;
                watchpoint->TurnOnEphemeralMode();
                process->DisableWatchpoint(watchpoint, notify);
            }
        }
        ~WatchpointSentry()
        {
            if (process && watchpoint)
            {
                if (!watchpoint->IsDisabledDuringEphemeralMode())
                {
                    const bool notify = false;
                    process->EnableWatchpoint(watchpoint, notify);
                }
                watchpoint->TurnOffEphemeralMode();
            }
        }
    private:
        Process *process;
        Watchpoint *watchpoint;
    };

    StopInfoWatchpoint (Thread &thread, break_id_t watch_id) :
        StopInfo(thread, watch_id),
        m_description(),
        m_should_stop(false),
        m_should_stop_is_valid(false)
    {
    }
    
    virtual ~StopInfoWatchpoint ()
    {
    }

    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonWatchpoint;
    }

    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
        {
            StreamString strm;
            strm.Printf("watchpoint %" PRIi64, m_value);
            m_description.swap (strm.GetString());
        }
        return m_description.c_str();
    }

protected:
    virtual bool
    ShouldStopSynchronous (Event *event_ptr)
    {
        // ShouldStop() method is idempotent and should not affect hit count.
        // See Process::RunPrivateStateThread()->Process()->HandlePrivateEvent()
        // -->Process()::ShouldBroadcastEvent()->ThreadList::ShouldStop()->
        // Thread::ShouldStop()->ThreadPlanBase::ShouldStop()->
        // StopInfoWatchpoint::ShouldStop() and
        // Event::DoOnRemoval()->Process::ProcessEventData::DoOnRemoval()->
        // StopInfoWatchpoint::PerformAction().
        if (m_should_stop_is_valid)
            return m_should_stop;

        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            WatchpointSP wp_sp (thread_sp->CalculateTarget()->GetWatchpointList().FindByID(GetValue()));
            if (wp_sp)
            {
                // Check if we should stop at a watchpoint.
                ExecutionContext exe_ctx (thread_sp->GetStackFrameAtIndex(0));
                StoppointCallbackContext context (event_ptr, exe_ctx, true);
                m_should_stop = wp_sp->ShouldStop (&context);
            }
            else
            {
                Log *log(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));

                if (log)
                    log->Printf ("Process::%s could not find watchpoint location id: %" PRId64 "...",
                                 __FUNCTION__, GetValue());

                m_should_stop = true;
            }
        }
        m_should_stop_is_valid = true;
        return m_should_stop;
    }
    
    bool
    ShouldStop (Event *event_ptr)
    {
        // This just reports the work done by PerformAction or the synchronous stop.  It should
        // only ever get called after they have had a chance to run.
        assert (m_should_stop_is_valid);
        return m_should_stop;
    }
    
    virtual void
    PerformAction (Event *event_ptr)
    {
        Log *log = lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_WATCHPOINTS);
        // We're going to calculate if we should stop or not in some way during the course of
        // this code.  Also by default we're going to stop, so set that here.
        m_should_stop = true;
        
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {

            WatchpointSP wp_sp (thread_sp->CalculateTarget()->GetWatchpointList().FindByID(GetValue()));
            if (wp_sp)
            {
                ExecutionContext exe_ctx (thread_sp->GetStackFrameAtIndex(0));
                Process* process = exe_ctx.GetProcessPtr();

                // This sentry object makes sure the current watchpoint is disabled while performing watchpoint actions,
                // and it is then enabled after we are finished.
                WatchpointSentry sentry(process, wp_sp.get());

                {
                    // check if this process is running on an architecture where watchpoints trigger
                    // before the associated instruction runs. if so, disable the WP, single-step and then
                    // re-enable the watchpoint
                    if (process)
                    {
                        uint32_t num;
                        bool wp_triggers_after;
                        if (process->GetWatchpointSupportInfo(num, wp_triggers_after).Success())
                        {
                            if (!wp_triggers_after)
                            {
                                StopInfoSP stored_stop_info_sp = thread_sp->GetStopInfo();
                                assert (stored_stop_info_sp.get() == this);
                                
                                ThreadPlanSP new_plan_sp(thread_sp->QueueThreadPlanForStepSingleInstruction(false, // step-over
                                                                                                        false,     // abort_other_plans
                                                                                                        true));    // stop_other_threads
                                new_plan_sp->SetIsMasterPlan (true);
                                new_plan_sp->SetOkayToDiscard (false);
                                new_plan_sp->SetPrivate (true);
                                process->GetThreadList().SetSelectedThreadByID (thread_sp->GetID());
                                process->Resume ();
                                process->WaitForProcessToStop (NULL);
                                process->GetThreadList().SetSelectedThreadByID (thread_sp->GetID());
                                thread_sp->SetStopInfo(stored_stop_info_sp);
                            }
                        }
                    }
                }

                if (m_should_stop && wp_sp->GetConditionText() != NULL)
                {
                    // We need to make sure the user sees any parse errors in their condition, so we'll hook the
                    // constructor errors up to the debugger's Async I/O.
                    ExecutionResults result_code;
                    EvaluateExpressionOptions expr_options;
                    expr_options.SetUnwindOnError(true);
                    expr_options.SetIgnoreBreakpoints(true);
                    ValueObjectSP result_value_sp;
                    Error error;
                    result_code = ClangUserExpression::Evaluate (exe_ctx,
                                                                 expr_options,
                                                                 wp_sp->GetConditionText(),
                                                                 NULL,
                                                                 result_value_sp,
                                                                 error);
                    if (result_code == eExecutionCompleted)
                    {
                        if (result_value_sp)
                        {
                            Scalar scalar_value;
                            if (result_value_sp->ResolveValue (scalar_value))
                            {
                                if (scalar_value.ULongLong(1) == 0)
                                {
                                    // We have been vetoed.  This takes precedence over querying
                                    // the watchpoint whether it should stop (aka ignore count and
                                    // friends).  See also StopInfoWatchpoint::ShouldStop() as well
                                    // as Process::ProcessEventData::DoOnRemoval().
                                    m_should_stop = false;
                                }
                                else
                                    m_should_stop = true;
                                if (log)
                                    log->Printf("Condition successfully evaluated, result is %s.\n", 
                                                m_should_stop ? "true" : "false");
                            }
                            else
                            {
                                m_should_stop = true;
                                if (log)
                                    log->Printf("Failed to get an integer result from the expression.");
                            }
                        }
                    }
                    else
                    {
                        Debugger &debugger = exe_ctx.GetTargetRef().GetDebugger();
                        StreamSP error_sp = debugger.GetAsyncErrorStream ();
                        error_sp->Printf ("Stopped due to an error evaluating condition of watchpoint ");
                        wp_sp->GetDescription (error_sp.get(), eDescriptionLevelBrief);
                        error_sp->Printf (": \"%s\"", 
                                          wp_sp->GetConditionText());
                        error_sp->EOL();
                        const char *err_str = error.AsCString("<Unknown Error>");
                        if (log)
                            log->Printf("Error evaluating condition: \"%s\"\n", err_str);

                        error_sp->PutCString (err_str);
                        error_sp->EOL();                       
                        error_sp->Flush();
                        // If the condition fails to be parsed or run, we should stop.
                        m_should_stop = true;
                    }
                }

                // If the condition says to stop, we run the callback to further decide whether to stop.
                if (m_should_stop)
                {
                    StoppointCallbackContext context (event_ptr, exe_ctx, false);
                    bool stop_requested = wp_sp->InvokeCallback (&context);
                    // Also make sure that the callback hasn't continued the target.  
                    // If it did, when we'll set m_should_stop to false and get out of here.
                    if (HasTargetRunSinceMe ())
                        m_should_stop = false;
                    
                    if (m_should_stop && !stop_requested)
                    {
                        // We have been vetoed by the callback mechanism.
                        m_should_stop = false;
                    }
                }
                // Finally, if we are going to stop, print out the new & old values:
                if (m_should_stop)
                {
                    wp_sp->CaptureWatchedValue(exe_ctx);
                    
                    Debugger &debugger = exe_ctx.GetTargetRef().GetDebugger();
                    StreamSP output_sp = debugger.GetAsyncOutputStream ();
                    wp_sp->DumpSnapshots(output_sp.get());
                    output_sp->EOL();
                    output_sp->Flush();
                }
                
            }
            else
            {
                Log * log_process(lldb_private::GetLogIfAllCategoriesSet (LIBLLDB_LOG_PROCESS));

                if (log_process)
                    log_process->Printf ("Process::%s could not find watchpoint id: %" PRId64 "...", __FUNCTION__, m_value);
            }
            if (log)
                log->Printf ("Process::%s returning from action with m_should_stop: %d.", __FUNCTION__, m_should_stop);
            
            m_should_stop_is_valid = true;
        }
    }
        
private:
    std::string m_description;
    bool m_should_stop;
    bool m_should_stop_is_valid;
};



//----------------------------------------------------------------------
// StopInfoUnixSignal
//----------------------------------------------------------------------

class StopInfoUnixSignal : public StopInfo
{
public:

    StopInfoUnixSignal (Thread &thread, int signo) :
        StopInfo (thread, signo)
    {
    }
    
    virtual ~StopInfoUnixSignal ()
    {
    }


    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonSignal;
    }

    virtual bool
    ShouldStopSynchronous (Event *event_ptr)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
            return thread_sp->GetProcess()->GetUnixSignals().GetShouldStop (m_value);
        return false;
    }

    virtual bool
    ShouldStop (Event *event_ptr)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
            return thread_sp->GetProcess()->GetUnixSignals().GetShouldStop (m_value);
        return false;
    }
    
    
    // If should stop returns false, check if we should notify of this event
    virtual bool
    DoShouldNotify (Event *event_ptr)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            bool should_notify = thread_sp->GetProcess()->GetUnixSignals().GetShouldNotify (m_value);
            if (should_notify)
            {
                StreamString strm;
                strm.Printf ("thread %d received signal: %s",
                             thread_sp->GetIndexID(),
                             thread_sp->GetProcess()->GetUnixSignals().GetSignalAsCString (m_value));
                Process::ProcessEventData::AddRestartedReason(event_ptr, strm.GetData());
            }
            return should_notify;
        }
        return true;
    }

    
    virtual void
    WillResume (lldb::StateType resume_state)
    {
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
        {
            if (thread_sp->GetProcess()->GetUnixSignals().GetShouldSuppress(m_value) == false)
                thread_sp->SetResumeSignal(m_value);
        }
    }

    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
        {
            ThreadSP thread_sp (m_thread_wp.lock());
            if (thread_sp)
            {
                StreamString strm;
                const char *signal_name = thread_sp->GetProcess()->GetUnixSignals().GetSignalAsCString (m_value);
                if (signal_name)
                    strm.Printf("signal %s", signal_name);
                else
                    strm.Printf("signal %" PRIi64, m_value);
                m_description.swap (strm.GetString());
            }
        }
        return m_description.c_str();
    }
};

//----------------------------------------------------------------------
// StopInfoTrace
//----------------------------------------------------------------------

class StopInfoTrace : public StopInfo
{
public:

    StopInfoTrace (Thread &thread) :
        StopInfo (thread, LLDB_INVALID_UID)
    {
    }
    
    virtual ~StopInfoTrace ()
    {
    }
    
    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonTrace;
    }

    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
        return "trace";
        else
            return m_description.c_str();
    }
};


//----------------------------------------------------------------------
// StopInfoException
//----------------------------------------------------------------------

class StopInfoException : public StopInfo
{
public:
    
    StopInfoException (Thread &thread, const char *description) :
        StopInfo (thread, LLDB_INVALID_UID)
    {
        if (description)
            SetDescription (description);
    }
    
    virtual 
    ~StopInfoException ()
    {
    }
    
    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonException;
    }
    
    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
            return "exception";
        else
            return m_description.c_str();
    }
};


//----------------------------------------------------------------------
// StopInfoThreadPlan
//----------------------------------------------------------------------

class StopInfoThreadPlan : public StopInfo
{
public:

    StopInfoThreadPlan (ThreadPlanSP &plan_sp, ValueObjectSP &return_valobj_sp) :
        StopInfo (plan_sp->GetThread(), LLDB_INVALID_UID),
        m_plan_sp (plan_sp),
        m_return_valobj_sp (return_valobj_sp)
    {
    }
    
    virtual ~StopInfoThreadPlan ()
    {
    }

    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonPlanComplete;
    }

    virtual const char *
    GetDescription ()
    {
        if (m_description.empty())
        {
            StreamString strm;            
            m_plan_sp->GetDescription (&strm, eDescriptionLevelBrief);
            m_description.swap (strm.GetString());
        }
        return m_description.c_str();
    }
    
    ValueObjectSP
    GetReturnValueObject()
    {
        return m_return_valobj_sp;
    }
    
protected:
    virtual bool
    ShouldStop (Event *event_ptr)
    {
        if (m_plan_sp)
            return m_plan_sp->ShouldStop(event_ptr);
        else
            return StopInfo::ShouldStop(event_ptr);
    }

private:
    ThreadPlanSP m_plan_sp;
    ValueObjectSP m_return_valobj_sp;
};
    
class StopInfoExec : public StopInfo
{
public:
    
    StopInfoExec (Thread &thread) :
        StopInfo (thread, LLDB_INVALID_UID),
        m_performed_action (false)
    {
    }
    
    virtual
    ~StopInfoExec ()
    {
    }
    
    virtual StopReason
    GetStopReason () const
    {
        return eStopReasonExec;
    }
    
    virtual const char *
    GetDescription ()
    {
        return "exec";
    }
protected:
    
    virtual void
    PerformAction (Event *event_ptr)
    {
        // Only perform the action once
        if (m_performed_action)
            return;
        m_performed_action = true;
        ThreadSP thread_sp (m_thread_wp.lock());
        if (thread_sp)
            thread_sp->GetProcess()->DidExec();
    }
    
    bool m_performed_action;
};

} // namespace lldb_private

StopInfoSP
StopInfo::CreateStopReasonWithBreakpointSiteID (Thread &thread, break_id_t break_id)
{
    return StopInfoSP (new StopInfoBreakpoint (thread, break_id));
}

StopInfoSP
StopInfo::CreateStopReasonWithBreakpointSiteID (Thread &thread, break_id_t break_id, bool should_stop)
{
    return StopInfoSP (new StopInfoBreakpoint (thread, break_id, should_stop));
}

StopInfoSP
StopInfo::CreateStopReasonWithWatchpointID (Thread &thread, break_id_t watch_id)
{
    return StopInfoSP (new StopInfoWatchpoint (thread, watch_id));
}

StopInfoSP
StopInfo::CreateStopReasonWithSignal (Thread &thread, int signo)
{
    return StopInfoSP (new StopInfoUnixSignal (thread, signo));
}

StopInfoSP
StopInfo::CreateStopReasonToTrace (Thread &thread)
{
    return StopInfoSP (new StopInfoTrace (thread));
}

StopInfoSP
StopInfo::CreateStopReasonWithPlan (ThreadPlanSP &plan_sp, ValueObjectSP return_valobj_sp)
{
    return StopInfoSP (new StopInfoThreadPlan (plan_sp, return_valobj_sp));
}

StopInfoSP
StopInfo::CreateStopReasonWithException (Thread &thread, const char *description)
{
    return StopInfoSP (new StopInfoException (thread, description));
}

StopInfoSP
StopInfo::CreateStopReasonWithExec (Thread &thread)
{
    return StopInfoSP (new StopInfoExec (thread));
}

ValueObjectSP
StopInfo::GetReturnValueObject(StopInfoSP &stop_info_sp)
{
    if (stop_info_sp && stop_info_sp->GetStopReason() == eStopReasonPlanComplete)
    {
        StopInfoThreadPlan *plan_stop_info = static_cast<StopInfoThreadPlan *>(stop_info_sp.get());
        return plan_stop_info->GetReturnValueObject();
    }
    else
        return ValueObjectSP();
}
