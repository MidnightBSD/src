//===-- LibCxxUnorderedMap.cpp -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "lldb/DataFormatters/CXXFormatterFunctions.h"

#include "lldb/Core/DataBufferHeap.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/ValueObject.h"
#include "lldb/Core/ValueObjectConstResult.h"
#include "lldb/Host/Endian.h"
#include "lldb/Symbol/ClangASTContext.h"
#include "lldb/Target/ObjCLanguageRuntime.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::LibcxxStdUnorderedMapSyntheticFrontEnd (lldb::ValueObjectSP valobj_sp) :
SyntheticChildrenFrontEnd(*valobj_sp.get()),
m_tree(NULL),
m_num_elements(0),
m_next_element(nullptr),
m_children(),
m_elements_cache()
{
    if (valobj_sp)
        Update();
}

size_t
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::CalculateNumChildren ()
{
    if (m_num_elements != UINT32_MAX)
        return m_num_elements;
    return 0;
}

lldb::ValueObjectSP
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::GetChildAtIndex (size_t idx)
{
    if (idx >= CalculateNumChildren())
        return lldb::ValueObjectSP();
    if (m_tree == NULL)
        return lldb::ValueObjectSP();
    
    auto cached = m_children.find(idx);
    if (cached != m_children.end())
        return cached->second;
    
    while (idx >= m_elements_cache.size())
    {
        if (m_next_element == nullptr)
            return lldb::ValueObjectSP();
        
        Error error;
        ValueObjectSP node_sp = m_next_element->Dereference(error);
        if (!node_sp || error.Fail())
            return lldb::ValueObjectSP();
        
        ValueObjectSP value_sp = node_sp->GetChildMemberWithName(ConstString("__value_"), true);
        ValueObjectSP hash_sp = node_sp->GetChildMemberWithName(ConstString("__hash_"), true);
        if (!hash_sp || !value_sp)
            return lldb::ValueObjectSP();
        m_elements_cache.push_back({value_sp.get(),hash_sp->GetValueAsUnsigned(0)});
        m_next_element = node_sp->GetChildMemberWithName(ConstString("__next_"),true).get();
        if (!m_next_element || m_next_element->GetValueAsUnsigned(0) == 0)
            m_next_element = nullptr;
    }
    
    std::pair<ValueObject*, uint64_t> val_hash = m_elements_cache[idx];
    if (!val_hash.first)
        return lldb::ValueObjectSP();
    StreamString stream;
    stream.Printf("[%zu]",idx);
    DataExtractor data;
    val_hash.first->GetData(data);
    const bool thread_and_frame_only_if_stopped = true;
    ExecutionContext exe_ctx = val_hash.first->GetExecutionContextRef().Lock(thread_and_frame_only_if_stopped);
    return val_hash.first->CreateValueObjectFromData(stream.GetData(),
                                                     data,
                                                     exe_ctx,
                                                     val_hash.first->GetClangType());
}

bool
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::Update()
{
    m_num_elements = UINT32_MAX;
    m_next_element = nullptr;
    m_elements_cache.clear();
    m_children.clear();
    ValueObjectSP table_sp = m_backend.GetChildMemberWithName(ConstString("__table_"), true);
    if (!table_sp)
        return false;
    ValueObjectSP num_elements_sp = table_sp->GetChildAtNamePath({ConstString("__p2_"),ConstString("__first_")});
    if (!num_elements_sp)
        return false;
    m_num_elements = num_elements_sp->GetValueAsUnsigned(0);
    m_tree = table_sp->GetChildAtNamePath({ConstString("__p1_"),ConstString("__first_"),ConstString("__next_")}).get();
    if (m_num_elements > 0)
        m_next_element = table_sp->GetChildAtNamePath({ConstString("__p1_"),ConstString("__first_"),ConstString("__next_")}).get();
    return false;
}

bool
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::MightHaveChildren ()
{
    return true;
}

size_t
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::GetIndexOfChildWithName (const ConstString &name)
{
    return ExtractIndexFromString(name.GetCString());
}

lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEnd::~LibcxxStdUnorderedMapSyntheticFrontEnd ()
{}

SyntheticChildrenFrontEnd*
lldb_private::formatters::LibcxxStdUnorderedMapSyntheticFrontEndCreator (CXXSyntheticChildren*, lldb::ValueObjectSP valobj_sp)
{
    if (!valobj_sp)
        return NULL;
    return (new LibcxxStdUnorderedMapSyntheticFrontEnd(valobj_sp));
}
