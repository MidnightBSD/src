//===-- OptionValueUInt64.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionValueUInt64.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Stream.h"
#include "lldb/Interpreter/Args.h"

using namespace lldb;
using namespace lldb_private;

lldb::OptionValueSP
OptionValueUInt64::Create (const char *value_cstr, Error &error)
{
    lldb::OptionValueSP value_sp (new OptionValueUInt64());
    error = value_sp->SetValueFromCString (value_cstr);
    if (error.Fail())
        value_sp.reset();
    return value_sp;
}


void
OptionValueUInt64::DumpValue (const ExecutionContext *exe_ctx, Stream &strm, uint32_t dump_mask)
{
    if (dump_mask & eDumpOptionType)
        strm.Printf ("(%s)", GetTypeAsCString ());
    if (dump_mask & eDumpOptionValue)
    {
        if (dump_mask & eDumpOptionType)
            strm.PutCString (" = ");
        strm.Printf ("%" PRIu64, m_current_value);
    }
}

Error
OptionValueUInt64::SetValueFromCString (const char *value_cstr, VarSetOperationType op)
{
    Error error;
    switch (op)
    {
        case eVarSetOperationClear:
            Clear ();
            break;
            
        case eVarSetOperationReplace:
        case eVarSetOperationAssign:
        {
            bool success = false;
            uint64_t value = Args::StringToUInt64 (value_cstr, 0, 0, &success);
            if (success)
            {
                m_value_was_set = true;
                m_current_value = value;
            }
            else
            {
                error.SetErrorStringWithFormat ("invalid uint64_t string value: '%s'", value_cstr);
            }
        }
            break;
            
        case eVarSetOperationInsertBefore:
        case eVarSetOperationInsertAfter:
        case eVarSetOperationRemove:
        case eVarSetOperationAppend:
        case eVarSetOperationInvalid:
            error = OptionValue::SetValueFromCString (value_cstr, op);
            break;
    }
    return error;
}

lldb::OptionValueSP
OptionValueUInt64::DeepCopy () const
{
    return OptionValueSP(new OptionValueUInt64(*this));
}

