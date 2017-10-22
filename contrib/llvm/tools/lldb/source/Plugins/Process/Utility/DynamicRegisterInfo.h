//===-- DynamicRegisterInfo.h -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef lldb_DynamicRegisterInfo_h_
#define lldb_DynamicRegisterInfo_h_

// C Includes
// C++ Includes
#include <vector>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private.h"
#include "lldb/Core/ConstString.h"

class DynamicRegisterInfo
{
public:
    DynamicRegisterInfo ();

    DynamicRegisterInfo (const lldb_private::PythonDictionary &dict);
    
    virtual 
    ~DynamicRegisterInfo ();

    size_t
    SetRegisterInfo (const lldb_private::PythonDictionary &dict);

    void
    AddRegister (lldb_private::RegisterInfo &reg_info, 
                 lldb_private::ConstString &reg_name, 
                 lldb_private::ConstString &reg_alt_name, 
                 lldb_private::ConstString &set_name);

    void
    Finalize ();

    size_t
    GetNumRegisters() const;

    size_t
    GetNumRegisterSets() const;

    size_t
    GetRegisterDataByteSize() const;

    const lldb_private::RegisterInfo *
    GetRegisterInfoAtIndex (uint32_t i) const;

    const lldb_private::RegisterSet *
    GetRegisterSet (uint32_t i) const;

    uint32_t
    GetRegisterSetIndexByName (lldb_private::ConstString &set_name, bool can_create);

    uint32_t
    ConvertRegisterKindToRegisterNumber (uint32_t kind, uint32_t num) const;

    void
    Clear();

protected:
    //------------------------------------------------------------------
    // Classes that inherit from DynamicRegisterInfo can see and modify these
    //------------------------------------------------------------------
    typedef std::vector <lldb_private::RegisterInfo> reg_collection;
    typedef std::vector <lldb_private::RegisterSet> set_collection;
    typedef std::vector <uint32_t> reg_num_collection;
    typedef std::vector <reg_num_collection> set_reg_num_collection;
    typedef std::vector <lldb_private::ConstString> name_collection;

    reg_collection m_regs;
    set_collection m_sets;
    set_reg_num_collection m_set_reg_nums;
    name_collection m_set_names;
    size_t m_reg_data_byte_size;   // The number of bytes required to store all registers
};

#endif  // lldb_DynamicRegisterInfo_h_
