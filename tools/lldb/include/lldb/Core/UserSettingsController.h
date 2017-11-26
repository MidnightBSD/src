//====-- UserSettingsController.h --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_UserSettingsController_h_
#define liblldb_UserSettingsController_h_

// C Includes
// C++ Includes

#include <string>
#include <vector>

// Other libraries and framework includes
// Project includes

#include "lldb/lldb-private.h"
#include "lldb/Core/ConstString.h"
#include "lldb/Core/StringList.h"
#include "lldb/Core/Stream.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Interpreter/OptionValue.h"

namespace lldb_private {

class Properties
{
public:
    Properties () :
        m_collection_sp ()
    {
    }

    Properties (const lldb::OptionValuePropertiesSP &collection_sp) :
        m_collection_sp (collection_sp)
    {
    }
    
    virtual
    ~Properties()
    {
    }
    
    virtual lldb::OptionValuePropertiesSP
    GetValueProperties () const
    {
        // This function is virtual in case subclasses want to lazily
        // implement creating the properties.
        return m_collection_sp;
    }

    virtual lldb::OptionValueSP
    GetPropertyValue (const ExecutionContext *exe_ctx,
                      const char *property_path,
                      bool will_modify,
                      Error &error) const;

    virtual Error
    SetPropertyValue (const ExecutionContext *exe_ctx,
                      VarSetOperationType op,
                      const char *property_path,
                      const char *value);
    
    virtual Error
    DumpPropertyValue (const ExecutionContext *exe_ctx,
                       Stream &strm,
                       const char *property_path,
                       uint32_t dump_mask);

    virtual void
    DumpAllPropertyValues (const ExecutionContext *exe_ctx,
                           Stream &strm,
                           uint32_t dump_mask);
    
    virtual void
    DumpAllDescriptions (CommandInterpreter &interpreter,
                         Stream &strm) const;

    size_t
    Apropos (const char *keyword,
             std::vector<const Property *> &matching_properties) const;

    lldb::OptionValuePropertiesSP
    GetSubProperty (const ExecutionContext *exe_ctx,
                    const ConstString &name);
protected:
    lldb::OptionValuePropertiesSP m_collection_sp;
};

} // namespace lldb_private

#endif // liblldb_UserSettingsController_h_
