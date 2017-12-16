//===-- SourceManager.h -----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_SourceManager_h_
#define liblldb_SourceManager_h_

// C Includes
// C++ Includes
#include <map>
#include <vector>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private.h"
#include "lldb/Host/FileSpec.h"

namespace lldb_private {

class SourceManager
{
public:
#ifndef SWIG
    
    class File
    {
    friend bool operator== (const SourceManager::File &lhs, const SourceManager::File &rhs);
    public:
    
        File (const FileSpec &file_spec, Target *target);
        ~File();

        size_t
        DisplaySourceLines (uint32_t line,
                            uint32_t context_before,
                            uint32_t context_after,
                            Stream *s);
        void
        FindLinesMatchingRegex (RegularExpression& regex, 
                                uint32_t start_line, 
                                uint32_t end_line, 
                                std::vector<uint32_t> &match_lines);

        bool
        GetLine (uint32_t line_no, std::string &buffer);
        
        uint32_t
        GetLineOffset (uint32_t line);

        bool
        LineIsValid (uint32_t line);

        bool
        FileSpecMatches (const FileSpec &file_spec);

        const FileSpec &
        GetFileSpec ()
        {
            return m_file_spec;
        }
        
        uint32_t
        GetSourceMapModificationID() const
        {
            return m_source_map_mod_id;
        }
        
        const char *
        PeekLineData (uint32_t line);

        uint32_t
        GetLineLength (uint32_t line, bool include_newline_chars);

        uint32_t
        GetNumLines ();
        
    protected:

        bool
        CalculateLineOffsets (uint32_t line = UINT32_MAX);

        FileSpec m_file_spec_orig;  // The original file spec that was used (can be different from m_file_spec)
        FileSpec m_file_spec;       // The actualy file spec being used (if the target has source mappings, this might be different from m_file_spec_orig)
        TimeValue m_mod_time;       // Keep the modification time that this file data is valid for
        uint32_t m_source_map_mod_id; // If the target uses path remappings, be sure to clear our notion of a source file if the path modification ID changes
        lldb::DataBufferSP m_data_sp;
        typedef std::vector<uint32_t> LineOffsets;
        LineOffsets m_offsets;
    };

#endif // SWIG

    typedef std::shared_ptr<File> FileSP;

#ifndef SWIG

   // The SourceFileCache class separates the source manager from the cache of source files, so the 
   // cache can be stored in the Debugger, but the source managers can be per target.     
    class SourceFileCache
    {
    public:
        SourceFileCache () {}
        ~SourceFileCache() {}
        
        void AddSourceFile (const FileSP &file_sp);
        FileSP FindSourceFile (const FileSpec &file_spec) const;
        
    protected:
        typedef std::map <FileSpec, FileSP> FileCache;
        FileCache m_file_cache;
    };
#endif


    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
    // A source manager can be made with a non-null target, in which case it can use the path remappings to find 
    // source files that are not in their build locations.  With no target it won't be able to do this.
    SourceManager (const lldb::DebuggerSP &debugger_sp);
    SourceManager (const lldb::TargetSP &target_sp);

    ~SourceManager();


    FileSP
    GetLastFile () 
    {
        return m_last_file_sp;
    }

    size_t
    DisplaySourceLinesWithLineNumbers (const FileSpec &file,
                                       uint32_t line,
                                       uint32_t context_before,
                                       uint32_t context_after,
                                       const char* current_line_cstr,
                                       Stream *s,
                                       const SymbolContextList *bp_locs = NULL);

    // This variant uses the last file we visited.
    size_t
    DisplaySourceLinesWithLineNumbersUsingLastFile (uint32_t start_line,
                                                    uint32_t count,
                                                    uint32_t curr_line,
                                                    const char* current_line_cstr,
                                                    Stream *s,
                                                    const SymbolContextList *bp_locs = NULL);

    size_t
    DisplayMoreWithLineNumbers (Stream *s,
                                uint32_t count,
                                bool reverse,
                                const SymbolContextList *bp_locs = NULL);

    bool
    SetDefaultFileAndLine (const FileSpec &file_spec, uint32_t line);
    
    bool 
    GetDefaultFileAndLine (FileSpec &file_spec, uint32_t &line);
    
    bool 
    DefaultFileAndLineSet ()
    {
        return (m_last_file_sp.get() != NULL);
    }

    void
    FindLinesMatchingRegex (FileSpec &file_spec,
                            RegularExpression& regex, 
                            uint32_t start_line, 
                            uint32_t end_line, 
                            std::vector<uint32_t> &match_lines);
    
    FileSP
    GetFile (const FileSpec &file_spec);

protected:
    
    //------------------------------------------------------------------
    // Classes that inherit from SourceManager can see and modify these
    //------------------------------------------------------------------
    FileSP m_last_file_sp;
    uint32_t m_last_line;
    uint32_t m_last_count;
    bool     m_default_set;
    lldb::TargetWP m_target_wp;
    lldb::DebuggerWP m_debugger_wp;
    
private:
    //------------------------------------------------------------------
    // For SourceManager only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (SourceManager);
};

bool operator== (const SourceManager::File &lhs, const SourceManager::File &rhs);
} // namespace lldb_private

#endif  // liblldb_SourceManager_h_
