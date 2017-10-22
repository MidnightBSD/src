//===-- OptionValueRegex.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Interpreter/OptionValueRegex.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/Stream.h"

using namespace lldb;
using namespace lldb_private;

void
OptionValueRegex::DumpValue (const ExecutionContext *exe_ctx, Stream &strm, uint32_t dump_mask)
{
    if (dump_mask & eDumpOptionType)
        strm.Printf ("(%s)", GetTypeAsCString ());
    if (dump_mask & eDumpOptionValue)
    {
        if (dump_mask & eDumpOptionType)
            strm.PutCString (" = ");
        if (m_regex.IsValid())
        {
            const char *regex_text = m_regex.GetText();
            if (regex_text && regex_text[0])
                strm.Printf ("%s", regex_text);
        }
        else
        {
            
        }
    }
}

Error
OptionValueRegex::SetValueFromCString (const char *value_cstr,
                                        VarSetOperationType op)
{
    Error error;
    switch (op)
    {
    case eVarSetOperationInvalid:
    case eVarSetOperationInsertBefore:
    case eVarSetOperationInsertAfter:
    case eVarSetOperationRemove:
    case eVarSetOperationAppend:
        error = OptionValue::SetValueFromCString (value_cstr, op);
        break;

    case eVarSetOperationClear:
        Clear();
        break;

    case eVarSetOperationReplace:
    case eVarSetOperationAssign:
        if (m_regex.Compile (value_cstr, m_regex.GetCompileFlags()))
        {
            m_value_was_set = true;
        }
        else
        {
            char regex_error[1024];
            if (m_regex.GetErrorAsCString(regex_error, sizeof(regex_error)))
                error.SetErrorString (regex_error);
            else
                error.SetErrorStringWithFormat ("regex error %u", m_regex.GetErrorCode());
        }
        break;
    }
    return error;
}


lldb::OptionValueSP
OptionValueRegex::DeepCopy () const
{
    return OptionValueSP(new OptionValueRegex(m_regex.GetText(), m_regex.GetCompileFlags()));
}
