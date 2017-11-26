//===-- Timer.cpp -----------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include "lldb/Core/Timer.h"

#include <map>
#include <vector>
#include <algorithm>

#include "lldb/Core/Stream.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Host/Host.h"

#include <stdio.h>

using namespace lldb_private;

#define TIMER_INDENT_AMOUNT 2
static bool g_quiet = true;
uint32_t Timer::g_depth = 0;
uint32_t Timer::g_display_depth = 0;
FILE * Timer::g_file = NULL;
typedef std::vector<Timer *> TimerStack;
typedef std::map<const char *, uint64_t> TimerCategoryMap;
static lldb::thread_key_t g_key;

static Mutex &
GetCategoryMutex()
{
    static Mutex g_category_mutex(Mutex::eMutexTypeNormal);
    return g_category_mutex;
}

static TimerCategoryMap &
GetCategoryMap()
{
    static TimerCategoryMap g_category_map;
    return g_category_map;
}


static TimerStack *
GetTimerStackForCurrentThread ()
{
    void *timer_stack = Host::ThreadLocalStorageGet(g_key);
    if (timer_stack == NULL)
    {
        Host::ThreadLocalStorageSet(g_key, new TimerStack);
        timer_stack = Host::ThreadLocalStorageGet(g_key);
    }
    return (TimerStack *)timer_stack;
}

void
ThreadSpecificCleanup (void *p)
{
    delete (TimerStack *)p;
}

void
Timer::SetQuiet (bool value)
{
    g_quiet = value;
}

void
Timer::Initialize ()
{
    Timer::g_file = stdout;
    g_key = Host::ThreadLocalStorageCreate(ThreadSpecificCleanup);
}

Timer::Timer (const char *category, const char *format, ...) :
    m_category (category),
    m_total_start (),
    m_timer_start (),
    m_total_ticks (0),
    m_timer_ticks (0)
{
    if (g_depth++ < g_display_depth)
    {
        if (g_quiet == false)
        {
            // Indent
            ::fprintf (g_file, "%*s", g_depth * TIMER_INDENT_AMOUNT, "");
            // Print formatted string
            va_list args;
            va_start (args, format);
            ::vfprintf (g_file, format, args);
            va_end (args);

            // Newline
            ::fprintf (g_file, "\n");
        }
        TimeValue start_time(TimeValue::Now());
        m_total_start = start_time;
        m_timer_start = start_time;
        TimerStack *stack = GetTimerStackForCurrentThread ();
        if (stack)
        {
            if (stack->empty() == false)
                stack->back()->ChildStarted (start_time);
            stack->push_back(this);
        }
    }
}


Timer::~Timer()
{
    if (m_total_start.IsValid())
    {
        TimeValue stop_time = TimeValue::Now();
        if (m_total_start.IsValid())
        {
            m_total_ticks += (stop_time - m_total_start);
            m_total_start.Clear();
        }
        if (m_timer_start.IsValid())
        {
            m_timer_ticks += (stop_time - m_timer_start);
            m_timer_start.Clear();
        }

        TimerStack *stack = GetTimerStackForCurrentThread ();
        if (stack)
        {
            assert (stack->back() == this);
            stack->pop_back();
            if (stack->empty() == false)
                stack->back()->ChildStopped(stop_time);
        }

        const uint64_t total_nsec_uint = GetTotalElapsedNanoSeconds();
        const uint64_t timer_nsec_uint = GetTimerElapsedNanoSeconds();
        const double total_nsec = total_nsec_uint;
        const double timer_nsec = timer_nsec_uint;

        if (g_quiet == false)
        {

            ::fprintf (g_file,
                       "%*s%.9f sec (%.9f sec)\n",
                       (g_depth - 1) *TIMER_INDENT_AMOUNT, "",
                       total_nsec / 1000000000.0,
                       timer_nsec / 1000000000.0);
        }

        // Keep total results for each category so we can dump results.
        Mutex::Locker locker (GetCategoryMutex());
        TimerCategoryMap &category_map = GetCategoryMap();
        category_map[m_category] += timer_nsec_uint;
    }
    if (g_depth > 0)
        --g_depth;
}

uint64_t
Timer::GetTotalElapsedNanoSeconds()
{
    uint64_t total_ticks = m_total_ticks;

    // If we are currently running, we need to add the current
    // elapsed time of the running timer...
    if (m_total_start.IsValid())
        total_ticks += (TimeValue::Now() - m_total_start);

    return total_ticks;
}

uint64_t
Timer::GetTimerElapsedNanoSeconds()
{
    uint64_t timer_ticks = m_timer_ticks;

    // If we are currently running, we need to add the current
    // elapsed time of the running timer...
    if (m_timer_start.IsValid())
        timer_ticks += (TimeValue::Now() - m_timer_start);

    return timer_ticks;
}

void
Timer::ChildStarted (const TimeValue& start_time)
{
    if (m_timer_start.IsValid())
    {
        m_timer_ticks += (start_time - m_timer_start);
        m_timer_start.Clear();
    }
}

void
Timer::ChildStopped (const TimeValue& stop_time)
{
    if (!m_timer_start.IsValid())
        m_timer_start = stop_time;
}

void
Timer::SetDisplayDepth (uint32_t depth)
{
    g_display_depth = depth;
}


/* binary function predicate:
 * - returns whether a person is less than another person
 */
static bool
CategoryMapIteratorSortCriterion (const TimerCategoryMap::const_iterator& lhs, const TimerCategoryMap::const_iterator& rhs)
{
    return lhs->second > rhs->second;
}


void
Timer::ResetCategoryTimes ()
{
    Mutex::Locker locker (GetCategoryMutex());
    TimerCategoryMap &category_map = GetCategoryMap();
    category_map.clear();
}

void
Timer::DumpCategoryTimes (Stream *s)
{
    Mutex::Locker locker (GetCategoryMutex());
    TimerCategoryMap &category_map = GetCategoryMap();
    std::vector<TimerCategoryMap::const_iterator> sorted_iterators;
    TimerCategoryMap::const_iterator pos, end = category_map.end();
    for (pos = category_map.begin(); pos != end; ++pos)
    {
        sorted_iterators.push_back (pos);
    }
    std::sort (sorted_iterators.begin(), sorted_iterators.end(), CategoryMapIteratorSortCriterion);

    const size_t count = sorted_iterators.size();
    for (size_t i=0; i<count; ++i)
    {
        const double timer_nsec = sorted_iterators[i]->second;
        s->Printf("%.9f sec for %s\n", timer_nsec / 1000000000.0, sorted_iterators[i]->first);
    }
}
