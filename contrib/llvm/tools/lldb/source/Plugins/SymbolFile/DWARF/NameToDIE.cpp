//===-- NameToDIE.cpp -------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NameToDIE.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Core/RegularExpression.h"
#include "lldb/Symbol/ObjectFile.h"

#include "DWARFCompileUnit.h"
#include "DWARFDebugInfo.h"
#include "DWARFDebugInfoEntry.h"
#include "SymbolFileDWARF.h"
using namespace lldb;
using namespace lldb_private;

void
NameToDIE::Finalize()
{
    m_map.Sort ();
    m_map.SizeToFit ();
}

void
NameToDIE::Insert (const ConstString& name, uint32_t die_offset)
{
    m_map.Append(name.GetCString(), die_offset);
}

size_t
NameToDIE::Find (const ConstString &name, DIEArray &info_array) const
{
    return m_map.GetValues (name.GetCString(), info_array);
}

size_t
NameToDIE::Find (const RegularExpression& regex, DIEArray &info_array) const
{
    return m_map.GetValues (regex, info_array);
}

size_t
NameToDIE::FindAllEntriesForCompileUnit (uint32_t cu_offset, 
                                         uint32_t cu_end_offset, 
                                         DIEArray &info_array) const
{
    const size_t initial_size = info_array.size();
    const uint32_t size = m_map.GetSize();
    for (uint32_t i=0; i<size; ++i)
    {
        const uint32_t die_offset = m_map.GetValueAtIndexUnchecked(i);
        if (cu_offset < die_offset && die_offset < cu_end_offset)
            info_array.push_back (die_offset);
    }
    return info_array.size() - initial_size;
}

void
NameToDIE::Dump (Stream *s)
{
    const uint32_t size = m_map.GetSize();
    for (uint32_t i=0; i<size; ++i)
    {
        const char *cstr = m_map.GetCStringAtIndex(i);
        s->Printf("%p: {0x%8.8x} \"%s\"\n", cstr, m_map.GetValueAtIndexUnchecked(i), cstr);
    }
}

void
NameToDIE::ForEach (std::function <bool(const char *name, uint32_t die_offset)> const &callback) const
{
    const uint32_t size = m_map.GetSize();
    for (uint32_t i=0; i<size; ++i)
    {
        if (!callback(m_map.GetCStringAtIndexUnchecked(i),
                      m_map.GetValueAtIndexUnchecked (i)))
            break;
    }
}
