//===-- UnwindAssembly.cpp ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-private.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Core/PluginInterface.h"
#include "lldb/Target/UnwindAssembly.h"

using namespace lldb;
using namespace lldb_private;

UnwindAssemblySP
UnwindAssembly::FindPlugin (const ArchSpec &arch)
{
    UnwindAssemblyCreateInstance create_callback;

    for (uint32_t idx = 0;
         (create_callback = PluginManager::GetUnwindAssemblyCreateCallbackAtIndex(idx)) != NULL;
         ++idx)
    {
        UnwindAssemblySP assembly_profiler_ap (create_callback (arch));
        if (assembly_profiler_ap.get ())
            return assembly_profiler_ap;
    }
    return NULL;
}

UnwindAssembly::UnwindAssembly (const ArchSpec &arch) :
    m_arch (arch)
{
}

UnwindAssembly::~UnwindAssembly ()
{
}
