//===-- UnixSignals.cpp -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Target/UnixSignals.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/Args.h"

using namespace lldb_private;

UnixSignals::Signal::Signal 
(
    const char *name, 
    const char *short_name, 
    bool default_suppress, 
    bool default_stop, 
    bool default_notify,
    const char *description
) :
    m_name (name),
    m_short_name (short_name),
    m_description (),
    m_suppress (default_suppress),
    m_stop (default_stop),
    m_notify (default_notify)
{
    if (description)
        m_description.assign (description);
}

//----------------------------------------------------------------------
// UnixSignals constructor
//----------------------------------------------------------------------
UnixSignals::UnixSignals ()
{
    Reset ();
}

//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
UnixSignals::~UnixSignals ()
{
}

void
UnixSignals::Reset ()
{
    // This builds one standard set of Unix Signals.  If yours aren't quite in this
    // order, you can either subclass this class, and use Add & Remove to change them
    // or you can subclass and build them afresh in your constructor;
    m_signals.clear();
    //        SIGNO  NAME         SHORT NAME SUPPRESS STOP   NOTIFY DESCRIPTION
    //        ====== ============ ========== ======== ====== ====== ===================================================
    AddSignal (1,    "SIGHUP",    "HUP",     false,   true , true , "hangup");
    AddSignal (2,    "SIGINT",    "INT",     true ,   true , true , "interrupt");
    AddSignal (3,    "SIGQUIT",   "QUIT",    false,   true , true , "quit");
    AddSignal (4,    "SIGILL",    "ILL",     false,   true , true , "illegal instruction");
    AddSignal (5,    "SIGTRAP",   "TRAP",    true ,   true , true , "trace trap (not reset when caught)");
    AddSignal (6,    "SIGABRT",   "ABRT",    false,   true , true , "abort()");
    AddSignal (7,    "SIGEMT",    "EMT",     false,   true , true , "pollable event");
    AddSignal (8,    "SIGFPE",    "FPE",     false,   true , true , "floating point exception");
    AddSignal (9,    "SIGKILL",   "KILL",    false,   true , true , "kill");
    AddSignal (10,   "SIGBUS",    "BUS",     false,   true , true , "bus error");
    AddSignal (11,   "SIGSEGV",   "SEGV",    false,   true , true , "segmentation violation");
    AddSignal (12,   "SIGSYS",    "SYS",     false,   true , true , "bad argument to system call");
    AddSignal (13,   "SIGPIPE",   "PIPE",    false,   true , true , "write on a pipe with no one to read it");
    AddSignal (14,   "SIGALRM",   "ALRM",    false,   false, false, "alarm clock");
    AddSignal (15,   "SIGTERM",   "TERM",    false,   true , true , "software termination signal from kill");
    AddSignal (16,   "SIGURG",    "URG",     false,   false, false, "urgent condition on IO channel");
    AddSignal (17,   "SIGSTOP",   "STOP",    true ,   true , true , "sendable stop signal not from tty");
    AddSignal (18,   "SIGTSTP",   "TSTP",    false,   true , true , "stop signal from tty");
    AddSignal (19,   "SIGCONT",   "CONT",    false,   true , true , "continue a stopped process");
    AddSignal (20,   "SIGCHLD",   "CHLD",    false,   false, false, "to parent on child stop or exit");
    AddSignal (21,   "SIGTTIN",   "TTIN",    false,   true , true , "to readers process group upon background tty read");
    AddSignal (22,   "SIGTTOU",   "TTOU",    false,   true , true , "to readers process group upon background tty write");
    AddSignal (23,   "SIGIO",     "IO",      false,   false, false, "input/output possible signal");
    AddSignal (24,   "SIGXCPU",   "XCPU",    false,   true , true , "exceeded CPU time limit");
    AddSignal (25,   "SIGXFSZ",   "XFSZ",    false,   true , true , "exceeded file size limit");
    AddSignal (26,   "SIGVTALRM", "VTALRM",  false,   false, false, "virtual time alarm");
    AddSignal (27,   "SIGPROF",   "PROF",    false,   false, false, "profiling time alarm");
    AddSignal (28,   "SIGWINCH",  "WINCH",   false,   false, false, "window size changes");
    AddSignal (29,   "SIGINFO",   "INFO",    false,   true , true , "information request");
    AddSignal (30,   "SIGUSR1",   "USR1",    false,   true , true , "user defined signal 1");
    AddSignal (31,   "SIGUSR2",   "USR2",    false,   true , true , "user defined signal 2");
}

void
UnixSignals::AddSignal 
(
    int signo,
    const char *name,
    const char *short_name,
    bool default_suppress,
    bool default_stop,
    bool default_notify,
    const char *description
)
{
    Signal new_signal (name, short_name, default_suppress, default_stop, default_notify, description);
    m_signals.insert (std::make_pair(signo, new_signal));
}

void
UnixSignals::RemoveSignal (int signo)
{
    collection::iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
        m_signals.erase (pos);
}

const char *
UnixSignals::GetSignalAsCString (int signo) const
{
    collection::const_iterator pos = m_signals.find (signo);
    if (pos == m_signals.end())
        return NULL;
    else
        return pos->second.m_name.GetCString ();
}


bool
UnixSignals::SignalIsValid (int32_t signo) const
{
    return m_signals.find (signo) != m_signals.end();
}


int32_t
UnixSignals::GetSignalNumberFromName (const char *name) const
{
    ConstString const_name (name);

    collection::const_iterator pos, end = m_signals.end ();
    for (pos = m_signals.begin (); pos != end; pos++)
    {
        if ((const_name == pos->second.m_name) || (const_name == pos->second.m_short_name))
            return pos->first;
    }
    
    const int32_t signo = Args::StringToSInt32(name, LLDB_INVALID_SIGNAL_NUMBER, 0);
    if (signo != LLDB_INVALID_SIGNAL_NUMBER)
        return signo;
    return LLDB_INVALID_SIGNAL_NUMBER;
}

int32_t
UnixSignals::GetFirstSignalNumber () const
{
    if (m_signals.empty())
        return LLDB_INVALID_SIGNAL_NUMBER;

    return (*m_signals.begin ()).first;
}

int32_t
UnixSignals::GetNextSignalNumber (int32_t current_signal) const
{
    collection::const_iterator pos = m_signals.find (current_signal);
    collection::const_iterator end = m_signals.end();
    if (pos == end)
        return LLDB_INVALID_SIGNAL_NUMBER;
    else
    {
        pos++;
        if (pos == end)
            return LLDB_INVALID_SIGNAL_NUMBER;
        else
            return pos->first;
    }
}

const char *
UnixSignals::GetSignalInfo
(
    int32_t signo,
    bool &should_suppress,
    bool &should_stop,
    bool &should_notify
) const
{
    collection::const_iterator pos = m_signals.find (signo);
    if (pos == m_signals.end())
        return NULL;
    else
    {
        const Signal &signal = pos->second;
        should_suppress = signal.m_suppress;
        should_stop     = signal.m_stop;
        should_notify   = signal.m_notify;
        return signal.m_name.AsCString("");
    }
}

bool
UnixSignals::GetShouldSuppress (int signo) const
{
    collection::const_iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
        return pos->second.m_suppress;
    return false;
}

bool
UnixSignals::SetShouldSuppress (int signo, bool value)
{
    collection::iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
    {
        pos->second.m_suppress = value;
        return true;
    }
    return false;
}

bool
UnixSignals::SetShouldSuppress (const char *signal_name, bool value)
{
    const int32_t signo = GetSignalNumberFromName (signal_name);
    if (signo != LLDB_INVALID_SIGNAL_NUMBER)
        return SetShouldSuppress (signo, value);
    return false;
}

bool
UnixSignals::GetShouldStop (int signo) const
{
    collection::const_iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
        return pos->second.m_stop;
    return false;
}

bool
UnixSignals::SetShouldStop (int signo, bool value)
{
    collection::iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
    {
        pos->second.m_stop = value;
        return true;
    }
    return false;
}

bool
UnixSignals::SetShouldStop (const char *signal_name, bool value)
{
    const int32_t signo = GetSignalNumberFromName (signal_name);
    if (signo != LLDB_INVALID_SIGNAL_NUMBER)
        return SetShouldStop (signo, value);
    return false;
}

bool
UnixSignals::GetShouldNotify (int signo) const
{
    collection::const_iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
        return pos->second.m_notify;
    return false;
}

bool
UnixSignals::SetShouldNotify (int signo, bool value)
{
    collection::iterator pos = m_signals.find (signo);
    if (pos != m_signals.end())
    {
        pos->second.m_notify = value;
        return true;
    }
    return false;
}

bool
UnixSignals::SetShouldNotify (const char *signal_name, bool value)
{
    const int32_t signo = GetSignalNumberFromName (signal_name);
    if (signo != LLDB_INVALID_SIGNAL_NUMBER)
        return SetShouldNotify (signo, value);
    return false;
}
