//===-- SymbolFileDWARFDebugMap.h ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef SymbolFileDWARF_SymbolFileDWARFDebugMap_h_
#define SymbolFileDWARF_SymbolFileDWARFDebugMap_h_


#include <vector>
#include <bitset>

#include "clang/AST/CharUnits.h"

#include "lldb/Core/RangeMap.h"
#include "lldb/Symbol/SymbolFile.h"

#include "UniqueDWARFASTType.h"

class SymbolFileDWARF;
class DWARFCompileUnit;
class DWARFDebugAranges;
class DWARFDebugInfoEntry;
class DWARFDeclContext;
class DebugMapModule;

class SymbolFileDWARFDebugMap : public lldb_private::SymbolFile
{
public:
    
    //------------------------------------------------------------------
    // Static Functions
    //------------------------------------------------------------------
    static void
    Initialize();

    static void
    Terminate();

    static lldb_private::ConstString
    GetPluginNameStatic();

    static const char *
    GetPluginDescriptionStatic();

    static lldb_private::SymbolFile *
    CreateInstance (lldb_private::ObjectFile* obj_file);

    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
                            SymbolFileDWARFDebugMap (lldb_private::ObjectFile* ofile);
    virtual               ~ SymbolFileDWARFDebugMap ();

    virtual uint32_t        CalculateAbilities ();

    virtual void            InitializeObject();

    //------------------------------------------------------------------
    // Compile Unit function calls
    //------------------------------------------------------------------
    virtual uint32_t        GetNumCompileUnits ();
    virtual lldb::CompUnitSP ParseCompileUnitAtIndex (uint32_t index);

    virtual lldb::LanguageType ParseCompileUnitLanguage (const lldb_private::SymbolContext& sc);
    virtual size_t          ParseCompileUnitFunctions (const lldb_private::SymbolContext& sc);
    virtual bool            ParseCompileUnitLineTable (const lldb_private::SymbolContext& sc);
    virtual bool            ParseCompileUnitSupportFiles (const lldb_private::SymbolContext& sc, lldb_private::FileSpecList &support_files);
    virtual size_t          ParseFunctionBlocks (const lldb_private::SymbolContext& sc);
    virtual size_t          ParseTypes (const lldb_private::SymbolContext& sc);
    virtual size_t          ParseVariablesForContext (const lldb_private::SymbolContext& sc);

    virtual lldb_private::Type* ResolveTypeUID (lldb::user_id_t type_uid);
    virtual clang::DeclContext* GetClangDeclContextContainingTypeUID (lldb::user_id_t type_uid);
    virtual clang::DeclContext* GetClangDeclContextForTypeUID (const lldb_private::SymbolContext &sc, lldb::user_id_t type_uid);
    virtual bool            ResolveClangOpaqueTypeDefinition (lldb_private::ClangASTType& clang_type);
    virtual uint32_t        ResolveSymbolContext (const lldb_private::Address& so_addr, uint32_t resolve_scope, lldb_private::SymbolContext& sc);
    virtual uint32_t        ResolveSymbolContext (const lldb_private::FileSpec& file_spec, uint32_t line, bool check_inlines, uint32_t resolve_scope, lldb_private::SymbolContextList& sc_list);
    virtual uint32_t        FindGlobalVariables (const lldb_private::ConstString &name, const lldb_private::ClangNamespaceDecl *namespace_decl, bool append, uint32_t max_matches, lldb_private::VariableList& variables);
    virtual uint32_t        FindGlobalVariables (const lldb_private::RegularExpression& regex, bool append, uint32_t max_matches, lldb_private::VariableList& variables);
    virtual uint32_t        FindFunctions (const lldb_private::ConstString &name, const lldb_private::ClangNamespaceDecl *namespace_decl, uint32_t name_type_mask, bool include_inlines, bool append, lldb_private::SymbolContextList& sc_list);
    virtual uint32_t        FindFunctions (const lldb_private::RegularExpression& regex, bool include_inlines, bool append, lldb_private::SymbolContextList& sc_list);
    virtual uint32_t        FindTypes (const lldb_private::SymbolContext& sc, const lldb_private::ConstString &name, const lldb_private::ClangNamespaceDecl *namespace_decl, bool append, uint32_t max_matches, lldb_private::TypeList& types);
    virtual lldb_private::ClangNamespaceDecl
                            FindNamespace (const lldb_private::SymbolContext& sc,
                                           const lldb_private::ConstString &name,
                                           const lldb_private::ClangNamespaceDecl *parent_namespace_decl);
    virtual size_t          GetTypes (lldb_private::SymbolContextScope *sc_scope,
                                      uint32_t type_mask,
                                      lldb_private::TypeList &type_list);


    //------------------------------------------------------------------
    // ClangASTContext callbacks for external source lookups.
    //------------------------------------------------------------------
    static void
    CompleteTagDecl (void *baton, clang::TagDecl *);
    
    static void
    CompleteObjCInterfaceDecl (void *baton, clang::ObjCInterfaceDecl *);
    
    static bool 
    LayoutRecordType (void *baton, 
                      const clang::RecordDecl *record_decl,
                      uint64_t &size, 
                      uint64_t &alignment,
                      llvm::DenseMap <const clang::FieldDecl *, uint64_t> &field_offsets,
                      llvm::DenseMap <const clang::CXXRecordDecl *, clang::CharUnits> &base_offsets,
                      llvm::DenseMap <const clang::CXXRecordDecl *, clang::CharUnits> &vbase_offsets);


    //------------------------------------------------------------------
    // PluginInterface protocol
    //------------------------------------------------------------------
    virtual lldb_private::ConstString
    GetPluginName();

    virtual uint32_t
    GetPluginVersion();

protected:
    enum
    {
        kHaveInitializedOSOs = (1 << 0),
        kNumFlags
    };

    friend class DWARFCompileUnit;
    friend class SymbolFileDWARF;
    friend class DebugMapModule;
    struct OSOInfo
    {
        lldb::ModuleSP module_sp;
        
        OSOInfo() :
            module_sp ()
        {
        }
    };
    
    typedef std::shared_ptr<OSOInfo> OSOInfoSP;

    typedef lldb_private::RangeDataVector<lldb::addr_t, lldb::addr_t, lldb::addr_t> FileRangeMap;

    //------------------------------------------------------------------
    // Class specific types
    //------------------------------------------------------------------
    struct CompileUnitInfo
    {
        lldb_private::FileSpec so_file;
        lldb_private::ConstString oso_path;
        lldb_private::TimeValue oso_mod_time;
        OSOInfoSP oso_sp;
        lldb::CompUnitSP compile_unit_sp;
        uint32_t first_symbol_index;
        uint32_t last_symbol_index;
        uint32_t first_symbol_id;
        uint32_t last_symbol_id;
        FileRangeMap file_range_map;
        bool file_range_map_valid;
        

        CompileUnitInfo() :
            so_file (),
            oso_path (),
            oso_mod_time (),
            oso_sp (),
            compile_unit_sp (),
            first_symbol_index (UINT32_MAX),
            last_symbol_index (UINT32_MAX),
            first_symbol_id (UINT32_MAX),
            last_symbol_id (UINT32_MAX),
            file_range_map (),
            file_range_map_valid (false)
        {
        }
        
        const FileRangeMap &
        GetFileRangeMap(SymbolFileDWARFDebugMap *exe_symfile);
    };

    //------------------------------------------------------------------
    // Protected Member Functions
    //------------------------------------------------------------------
    void
    InitOSO ();

    static uint32_t
    GetOSOIndexFromUserID (lldb::user_id_t uid)
    {
        return (uint32_t)((uid >> 32ull) - 1ull);
    }
    
    static SymbolFileDWARF *
    GetSymbolFileAsSymbolFileDWARF (SymbolFile *sym_file);
    
    bool
    GetFileSpecForSO (uint32_t oso_idx, lldb_private::FileSpec &file_spec);

    CompileUnitInfo *
    GetCompUnitInfo (const lldb_private::SymbolContext& sc);

    size_t
    GetCompUnitInfosForModule (const lldb_private::Module *oso_module,
                               std::vector<CompileUnitInfo *>& cu_infos);
    
    lldb_private::Module *
    GetModuleByCompUnitInfo (CompileUnitInfo *comp_unit_info);

    lldb_private::Module *
    GetModuleByOSOIndex (uint32_t oso_idx);

    lldb_private::ObjectFile *
    GetObjectFileByCompUnitInfo (CompileUnitInfo *comp_unit_info);

    lldb_private::ObjectFile *
    GetObjectFileByOSOIndex (uint32_t oso_idx);

    uint32_t
    GetCompUnitInfoIndex (const CompileUnitInfo *comp_unit_info);

    SymbolFileDWARF *
    GetSymbolFile (const lldb_private::SymbolContext& sc);

    SymbolFileDWARF *
    GetSymbolFileByCompUnitInfo (CompileUnitInfo *comp_unit_info);

    SymbolFileDWARF *
    GetSymbolFileByOSOIndex (uint32_t oso_idx);

    CompileUnitInfo *
    GetCompileUnitInfoForSymbolWithIndex (uint32_t symbol_idx, uint32_t *oso_idx_ptr);
    
    CompileUnitInfo *
    GetCompileUnitInfoForSymbolWithID (lldb::user_id_t symbol_id, uint32_t *oso_idx_ptr);

    static int
    SymbolContainsSymbolWithIndex (uint32_t *symbol_idx_ptr, const CompileUnitInfo *comp_unit_info);

    static int
    SymbolContainsSymbolWithID (lldb::user_id_t *symbol_idx_ptr, const CompileUnitInfo *comp_unit_info);

    uint32_t
    PrivateFindGlobalVariables (const lldb_private::ConstString &name,
                                const lldb_private::ClangNamespaceDecl *namespace_decl,
                                const std::vector<uint32_t> &name_symbol_indexes,
                                uint32_t max_matches,
                                lldb_private::VariableList& variables);


    void
    SetCompileUnit (SymbolFileDWARF *oso_dwarf, const lldb::CompUnitSP &cu_sp);

    lldb::CompUnitSP
    GetCompileUnit (SymbolFileDWARF *oso_dwarf);
    
    CompileUnitInfo *
    GetCompileUnitInfo (SymbolFileDWARF *oso_dwarf);

    lldb::TypeSP
    FindDefinitionTypeForDWARFDeclContext (const DWARFDeclContext &die_decl_ctx);    

    bool
    Supports_DW_AT_APPLE_objc_complete_type (SymbolFileDWARF *skip_dwarf_oso);

    lldb::TypeSP
    FindCompleteObjCDefinitionTypeForDIE (const DWARFDebugInfoEntry *die, 
                                          const lldb_private::ConstString &type_name,
                                          bool must_be_implementation);
    

    UniqueDWARFASTTypeMap &
    GetUniqueDWARFASTTypeMap ()
    {
        return m_unique_ast_type_map;
    }
    
    
    //------------------------------------------------------------------
    // OSOEntry
    //------------------------------------------------------------------
    class OSOEntry
    {
    public:
        
        OSOEntry () :
        m_exe_sym_idx (UINT32_MAX),
        m_oso_file_addr (LLDB_INVALID_ADDRESS)
        {
        }
        
        OSOEntry (uint32_t exe_sym_idx,
                  lldb::addr_t oso_file_addr) :
        m_exe_sym_idx (exe_sym_idx),
        m_oso_file_addr (oso_file_addr)
        {
        }
        
        uint32_t
        GetExeSymbolIndex () const
        {
            return m_exe_sym_idx;
        }
        
        bool
        operator < (const OSOEntry &rhs) const
        {
            return m_exe_sym_idx < rhs.m_exe_sym_idx;
        }
        
        lldb::addr_t
        GetOSOFileAddress () const
        {
            return m_oso_file_addr;
        }
        
        void
        SetOSOFileAddress (lldb::addr_t oso_file_addr)
        {
            m_oso_file_addr = oso_file_addr;
        }
    protected:
        uint32_t m_exe_sym_idx;
        lldb::addr_t m_oso_file_addr;
    };

    typedef lldb_private::RangeDataVector<lldb::addr_t, lldb::addr_t, OSOEntry> DebugMap;

    //------------------------------------------------------------------
    // Member Variables
    //------------------------------------------------------------------
    std::bitset<kNumFlags> m_flags;
    std::vector<CompileUnitInfo> m_compile_unit_infos;
    std::vector<uint32_t> m_func_indexes;   // Sorted by address
    std::vector<uint32_t> m_glob_indexes;
    std::map<lldb_private::ConstString, OSOInfoSP> m_oso_map;
    UniqueDWARFASTTypeMap m_unique_ast_type_map;
    lldb_private::LazyBool m_supports_DW_AT_APPLE_objc_complete_type;
    DebugMap m_debug_map;
    
    //------------------------------------------------------------------
    // When an object file from the debug map gets parsed in
    // SymbolFileDWARF, it needs to tell the debug map about the object
    // files addresses by calling this function once for each N_FUN,
    // N_GSYM and N_STSYM and after all entries in the debug map have
    // been matched up, FinalizeOSOFileRanges() should be called.
    //------------------------------------------------------------------
    bool
    AddOSOFileRange (CompileUnitInfo *cu_info,
                     lldb::addr_t exe_file_addr,
                     lldb::addr_t oso_file_addr,
                     lldb::addr_t oso_byte_size);
    
    //------------------------------------------------------------------
    // Called after calling AddOSOFileRange() for each object file debug
    // map entry to finalize the info for the unlinked compile unit.
    //------------------------------------------------------------------
    void
    FinalizeOSOFileRanges (CompileUnitInfo *cu_info);

    //------------------------------------------------------------------
    /// Convert \a addr from a .o file address, to an executable address.
    ///
    /// @param[in] addr
    ///     A section offset address from a .o file
    ///
    /// @return
    ///     Returns true if \a addr was converted to be an executable
    ///     section/offset address, false otherwise.
    //------------------------------------------------------------------
    bool
    LinkOSOAddress (lldb_private::Address &addr);
    
    //------------------------------------------------------------------
    /// Convert a .o file "file address" to an executable "file address".
    ///
    /// @param[in] oso_symfile
    ///     The DWARF symbol file that contains \a oso_file_addr
    ///
    /// @param[in] oso_file_addr
    ///     A .o file "file address" to convert.
    ///
    /// @return
    ///     LLDB_INVALID_ADDRESS if \a oso_file_addr is not in the
    ///     linked executable, otherwise a valid "file address" from the
    ///     linked executable that contains the debug map.
    //------------------------------------------------------------------
    lldb::addr_t
    LinkOSOFileAddress (SymbolFileDWARF *oso_symfile, lldb::addr_t oso_file_addr);
            
    //------------------------------------------------------------------
    /// Given a line table full of lines with "file adresses" that are
    /// for a .o file represented by \a oso_symfile, link a new line table
    /// and return it.
    ///
    /// @param[in] oso_symfile
    ///     The DWARF symbol file that produced the \a line_table
    ///
    /// @param[in] addr
    ///     A section offset address from a .o file
    ///
    /// @return
    ///     Returns a valid line table full of linked addresses, or NULL
    ///     if none of the line table adresses exist in the main
    ///     executable.
    //------------------------------------------------------------------
    lldb_private::LineTable *
    LinkOSOLineTable (SymbolFileDWARF *oso_symfile,
                      lldb_private::LineTable *line_table);
    
    size_t
    AddOSOARanges (SymbolFileDWARF* dwarf2Data,
                   DWARFDebugAranges* debug_aranges);
};

#endif // #ifndef SymbolFileDWARF_SymbolFileDWARFDebugMap_h_
