//===-- OptionGroupOutputFile.h -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_OptionGroupOutputFile_h_
#define liblldb_OptionGroupOutputFile_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/Options.h"
#include "lldb/Interpreter/OptionValueBoolean.h"
#include "lldb/Interpreter/OptionValueFileSpec.h"

namespace lldb_private {
//-------------------------------------------------------------------------
// OptionGroupOutputFile
//-------------------------------------------------------------------------

class OptionGroupOutputFile : public OptionGroup
{
public:
    
    OptionGroupOutputFile ();
    
    virtual
    ~OptionGroupOutputFile ();

    
    virtual uint32_t
    GetNumDefinitions ();
    
    virtual const OptionDefinition*
    GetDefinitions ();
    
    virtual Error
    SetOptionValue (CommandInterpreter &interpreter,
                    uint32_t option_idx,
                    const char *option_value);
    
    virtual void
    OptionParsingStarting (CommandInterpreter &interpreter);
    
    const OptionValueFileSpec &
    GetFile ()
    {
        return m_file;
    }

    const OptionValueBoolean &
    GetAppend ()
    {
        return m_append;
    }
    
    bool
    AnyOptionWasSet () const
    {
        return m_file.OptionWasSet() || m_append.OptionWasSet();
    }

protected:
    OptionValueFileSpec m_file;
    OptionValueBoolean m_append;
    
};

} // namespace lldb_private

#endif  // liblldb_OptionGroupOutputFile_h_
