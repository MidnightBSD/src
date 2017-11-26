//===-- DWARFCallFrameInfo.h ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_DWARFCallFrameInfo_h_
#define liblldb_DWARFCallFrameInfo_h_

#include <map>

#include "lldb/Core/AddressRange.h"
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/Flags.h"
#include "lldb/Core/RangeMap.h"
#include "lldb/Core/VMRange.h"
#include "lldb/Core/dwarf.h"
#include "lldb/Host/Mutex.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/UnwindPlan.h"
#include "lldb/lldb-private.h"

namespace lldb_private {

// DWARFCallFrameInfo is a class which can read eh_frame and DWARF
// Call Frame Information FDEs.  It stores little information internally.
// Only two APIs are exported - one to find the high/low pc values
// of a function given a text address via the information in the
// eh_frame / debug_frame, and one to generate an UnwindPlan based
// on the FDE in the eh_frame / debug_frame section.

class DWARFCallFrameInfo
{
public:

    DWARFCallFrameInfo (ObjectFile& objfile, 
                        lldb::SectionSP& section, 
                        lldb::RegisterKind reg_kind, 
                        bool is_eh_frame);

    ~DWARFCallFrameInfo();

    // Locate an AddressRange that includes the provided Address in this 
    // object's eh_frame/debug_info
    // Returns true if a range is found to cover that address.
    bool
    GetAddressRange (Address addr, AddressRange &range);

    // Return an UnwindPlan based on the call frame information encoded 
    // in the FDE of this DWARFCallFrameInfo section.
    bool
    GetUnwindPlan (Address addr, UnwindPlan& unwind_plan);

    typedef RangeVector<lldb::addr_t, uint32_t> FunctionAddressAndSizeVector;

    //------------------------------------------------------------------
    // Build a vector of file address and size for all functions in this Module
    // based on the eh_frame FDE entries.
    //
    // The eh_frame information can be a useful source of file address and size of
    // the functions in a Module.  Often a binary's non-exported symbols are stripped
    // before shipping so lldb won't know the start addr / size of many functions
    // in the Module.  But the eh_frame can help to give the addresses of these 
    // stripped symbols, at least.
    //
    // @param[out] function_info
    //      A vector provided by the caller is filled out.  May be empty if no FDEs/no eh_frame
    //      is present in this Module.

    void
    GetFunctionAddressAndSizeVector (FunctionAddressAndSizeVector &function_info);

private:
    enum
    {
        CFI_AUG_MAX_SIZE = 8,
        CFI_HEADER_SIZE = 8
    };

    struct CIE
    {
        dw_offset_t cie_offset;
        uint8_t     version;
        char        augmentation[CFI_AUG_MAX_SIZE];  // This is typically empty or very short.
        uint32_t    code_align;
        int32_t     data_align;
        uint32_t    return_addr_reg_num;
        dw_offset_t inst_offset;        // offset of CIE instructions in mCFIData
        uint32_t    inst_length;        // length of CIE instructions in mCFIData
        uint8_t     ptr_encoding;
        lldb_private::UnwindPlan::Row initial_row;

        CIE(dw_offset_t offset) : cie_offset(offset), version (-1), code_align (0),
                                  data_align (0), return_addr_reg_num (LLDB_INVALID_REGNUM), inst_offset (0),
                                  inst_length (0), ptr_encoding (0), initial_row() {}
    };

    typedef std::shared_ptr<CIE> CIESP;

    typedef std::map<off_t, CIESP> cie_map_t;

    // Start address (file address), size, offset of FDE location
    // used for finding an FDE for a given File address; the start address field is
    // an offset into an individual Module.
    typedef RangeDataVector<lldb::addr_t, uint32_t, dw_offset_t> FDEEntryMap;

    bool
    IsEHFrame() const;

    bool
    GetFDEEntryByFileAddress (lldb::addr_t file_offset, FDEEntryMap::Entry& fde_entry);

    void
    GetFDEIndex ();

    bool
    FDEToUnwindPlan (uint32_t offset, Address startaddr, UnwindPlan& unwind_plan);

    const CIE* 
    GetCIE(dw_offset_t cie_offset);
    
    void
    GetCFIData();

    ObjectFile&                 m_objfile;
    lldb::SectionSP             m_section_sp;
    lldb::RegisterKind          m_reg_kind;
    Flags                       m_flags;
    cie_map_t                   m_cie_map;

    DataExtractor               m_cfi_data;
    bool                        m_cfi_data_initialized;   // only copy the section into the DE once

    FDEEntryMap                 m_fde_index;
    bool                        m_fde_index_initialized;  // only scan the section for FDEs once
    Mutex                       m_fde_index_mutex;        // and isolate the thread that does it

    bool                        m_is_eh_frame;

    CIESP
    ParseCIE (const uint32_t cie_offset);

};

} // namespace lldb_private

#endif  // liblldb_DWARFCallFrameInfo_h_
