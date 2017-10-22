//===-- Condition.h ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_DBCondition_h_
#define liblldb_DBCondition_h_
#if defined(__cplusplus)


#include <pthread.h>
#include "lldb/Host/Mutex.h"

namespace lldb_private {

class TimeValue;

//----------------------------------------------------------------------
/// @class Condition Condition.h "lldb/Host/Condition.h"
/// @brief A C++ wrapper class for pthread condition variables.
///
/// A class that wraps up a pthread condition (pthread_cond_t). The
/// class will create a pthread condition when an instance is
/// constructed, and detroy it when it is destructed. It also provides
/// access to the standard pthread condition calls.
//----------------------------------------------------------------------
class Condition
{
public:

    //------------------------------------------------------------------
    /// Default constructor
    ///
    /// The default constructor will initialize a new pthread condition
    /// and maintain the condition in the object state.
    //------------------------------------------------------------------
    Condition ();

    //------------------------------------------------------------------
    /// Destructor
    ///
    /// Destroys the pthread condition that the object owns.
    //------------------------------------------------------------------
    ~Condition ();

    //------------------------------------------------------------------
    /// Unblock all threads waiting for a condition variable
    ///
    /// @return
    ///     The return value from \c pthread_cond_broadcast()
    //------------------------------------------------------------------
    int
    Broadcast ();

    //------------------------------------------------------------------
    /// Unblocks one thread waiting for the condition variable
    ///
    /// @return
    ///     The return value from \c pthread_cond_signal()
    //------------------------------------------------------------------
    int
    Signal ();

    //------------------------------------------------------------------
    /// Wait for the condition variable to be signaled.
    ///
    /// The Wait() function atomically blocks the current thread
    /// waiting on this object's condition variable, and unblocks
    /// \a mutex. The waiting thread unblocks only after another thread
    /// signals or broadcasts this object's condition variable.
    ///
    /// If \a abstime is non-NULL, this function will return when the
    /// system time reaches the time specified in \a abstime if the
    /// condition variable doesn't get unblocked. If \a abstime is NULL
    /// this function will wait for an infinite amount of time for the
    /// condition variable to be unblocked.
    ///
    /// The current thread re-acquires the lock on \a mutex following
    /// the wait.
    ///
    /// @param[in] mutex
    ///     The mutex to use in the \c pthread_cond_timedwait() or
    ///     \c pthread_cond_wait() calls.
    ///
    /// @param[in] abstime
    ///     An absolute time at which to stop waiting if non-NULL, else
    ///     wait an infinite amount of time for the condition variable
    ///     toget signaled.
    ///
    /// @param[out] timed_out
    ///     If not NULL, will be set to true if the wait timed out, and
    //      false otherwise.
    ///
    /// @see Condition::Broadcast()
    /// @see Condition::Signal()
    //------------------------------------------------------------------
    int
    Wait (Mutex &mutex, const TimeValue *abstime = NULL, bool *timed_out = NULL);

protected:
    //------------------------------------------------------------------
    // Member variables
    //------------------------------------------------------------------
    pthread_cond_t m_condition; ///< The condition variable.
    
    //------------------------------------------------------------------
    /// Get accessor to the pthread condition object.
    ///
    /// @return
    ///     A pointer to the condition variable owned by this object.
    //------------------------------------------------------------------
    pthread_cond_t *
    GetCondition ();
};

} // namespace lldb_private

#endif  // #if defined(__cplusplus)
#endif

