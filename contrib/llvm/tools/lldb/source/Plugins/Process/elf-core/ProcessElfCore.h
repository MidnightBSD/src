//===-- ProcessElfCore.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Notes about Linux Process core dumps:
//  1) Linux core dump is stored as ELF file.
//  2) The ELF file's PT_NOTE and PT_LOAD segments describes the program's
//     address space and thread contexts.
//  3) PT_NOTE segment contains note entries which describes a thread context.
//  4) PT_LOAD segment describes a valid contigous range of process address
//     space.
//===----------------------------------------------------------------------===//

#ifndef liblldb_ProcessElfCore_h_
#define liblldb_ProcessElfCore_h_

// C++ Includes
#include <list>
#include <vector>

// Other libraries and framework includes
#include "lldb/Core/ConstString.h"
#include "lldb/Core/Error.h"
#include "lldb/Target/Process.h"

#include "Plugins/ObjectFile/ELF/ELFHeader.h"

struct ThreadData;

class ProcessElfCore : public lldb_private::Process
{
public:
    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
    static lldb::ProcessSP
    CreateInstance (lldb_private::Target& target, 
                    lldb_private::Listener &listener, 
                    const lldb_private::FileSpec *crash_file_path);
    
    static void
    Initialize();
    
    static void
    Terminate();
    
    static lldb_private::ConstString
    GetPluginNameStatic();
    
    static const char *
    GetPluginDescriptionStatic();
    
    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
    ProcessElfCore(lldb_private::Target& target, 
                    lldb_private::Listener &listener,
                    const lldb_private::FileSpec &core_file);
    
    virtual
    ~ProcessElfCore();
    
    //------------------------------------------------------------------
    // Check if a given Process
    //------------------------------------------------------------------
    virtual bool
    CanDebug (lldb_private::Target &target,
              bool plugin_specified_by_name);
    
    //------------------------------------------------------------------
    // Creating a new process, or attaching to an existing one
    //------------------------------------------------------------------
    virtual lldb_private::Error
    DoLoadCore ();
    
    virtual lldb_private::DynamicLoader *
    GetDynamicLoader ();

    //------------------------------------------------------------------
    // PluginInterface protocol
    //------------------------------------------------------------------
    virtual lldb_private::ConstString
    GetPluginName();
    
    virtual uint32_t
    GetPluginVersion();
    
    //------------------------------------------------------------------
    // Process Control
    //------------------------------------------------------------------    
    virtual lldb_private::Error
    DoDestroy ();
    
    virtual void
    RefreshStateAfterStop();
    
    //------------------------------------------------------------------
    // Process Queries
    //------------------------------------------------------------------
    virtual bool
    IsAlive ();

    //------------------------------------------------------------------
    // Process Memory
    //------------------------------------------------------------------
    virtual size_t
    ReadMemory (lldb::addr_t addr, void *buf, size_t size, lldb_private::Error &error);
    
    virtual size_t
    DoReadMemory (lldb::addr_t addr, void *buf, size_t size, lldb_private::Error &error);
    
    virtual lldb::addr_t
    GetImageInfoAddress ();

    lldb_private::ArchSpec
    GetArchitecture();

    // Returns AUXV structure found in the core file
    const lldb::DataBufferSP
    GetAuxvData();

protected:
    void
    Clear ( );
    
    virtual bool
    UpdateThreadList (lldb_private::ThreadList &old_thread_list, 
                      lldb_private::ThreadList &new_thread_list);
   
private:
    //------------------------------------------------------------------
    // For ProcessElfCore only
    //------------------------------------------------------------------
    typedef lldb_private::Range<lldb::addr_t, lldb::addr_t> FileRange;
    typedef lldb_private::RangeDataArray<lldb::addr_t, lldb::addr_t, FileRange, 1> VMRangeToFileOffset;

    lldb::ModuleSP m_core_module_sp;
    lldb_private::FileSpec m_core_file;
    std::string  m_dyld_plugin_name;
    DISALLOW_COPY_AND_ASSIGN (ProcessElfCore);

    // True if m_thread_contexts contains valid entries
    bool m_thread_data_valid;

    // Contain thread data read from NOTE segments
    std::vector<ThreadData> m_thread_data;

    // AUXV structure found from the NOTE segment
    lldb_private::DataExtractor m_auxv;

    // Address ranges found in the core
    VMRangeToFileOffset m_core_aranges;

    // Parse thread(s) data structures(prstatus, prpsinfo) from given NOTE segment
    void
    ParseThreadContextsFromNoteSegment (const elf::ELFProgramHeader *segment_header,
                                        lldb_private::DataExtractor segment_data);

    // Returns number of thread contexts stored in the core file
    uint32_t
    GetNumThreadContexts();

    // Parse a contiguous address range of the process from LOAD segment
    lldb::addr_t
    AddAddressRangeFromLoadSegment(const elf::ELFProgramHeader *header);
};

#endif  // liblldb_ProcessElffCore_h_
