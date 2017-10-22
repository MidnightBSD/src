//===-- OperatingSystem.cpp --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#include "lldb/Target/OperatingSystem.h"
// C Includes
// C++ Includes
// Other libraries and framework includes
#include "lldb/Core/PluginManager.h"
#include "lldb/Target/Thread.h"

using namespace lldb;
using namespace lldb_private;


OperatingSystem*
OperatingSystem::FindPlugin (Process *process, const char *plugin_name)
{
    OperatingSystemCreateInstance create_callback = NULL;
    if (plugin_name)
    {
        ConstString const_plugin_name(plugin_name);
        create_callback  = PluginManager::GetOperatingSystemCreateCallbackForPluginName (const_plugin_name);
        if (create_callback)
        {
            std::unique_ptr<OperatingSystem> instance_ap(create_callback(process, true));
            if (instance_ap.get())
                return instance_ap.release();
        }
    }
    else
    {
        for (uint32_t idx = 0; (create_callback = PluginManager::GetOperatingSystemCreateCallbackAtIndex(idx)) != NULL; ++idx)
        {
            std::unique_ptr<OperatingSystem> instance_ap(create_callback(process, false));
            if (instance_ap.get())
                return instance_ap.release();
        }
    }
    return NULL;
}


OperatingSystem::OperatingSystem (Process *process) :
    m_process (process)
{
}

OperatingSystem::~OperatingSystem()
{
}


bool
OperatingSystem::IsOperatingSystemPluginThread (const lldb::ThreadSP &thread_sp)
{
    if (thread_sp)
        return thread_sp->IsOperatingSystemPluginThread();
    return false;
}

