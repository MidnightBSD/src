//===-- StringList.h --------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_StringList_h_
#define liblldb_StringList_h_

#include <stdint.h>

#include "lldb/Core/STLUtils.h"
#include "lldb/lldb-forward.h"

namespace lldb_private {

class StringList
{
public:

    StringList ();

    StringList (const char *str);

    StringList (const char **strv, int strc);
    
    virtual
    ~StringList ();

    void
    AppendString (const std::string &s);
    
    void
    AppendString (std::string &&s);
    
    void
    AppendString (const char *str);

    void
    AppendString (const char *str, size_t str_len);

    void
    AppendList (const char ** strv, int strc);

    void
    AppendList (StringList strings);

    bool
    ReadFileLines (FileSpec &input_file);
    
    size_t
    GetSize () const;

    void
    SetSize (size_t n)
    {
        m_strings.resize(n);
    }

    size_t
    GetMaxStringLength () const;
    
    std::string &
    operator [](size_t idx)
    {
        // No bounds checking, verify "idx" is good prior to calling this function
        return m_strings[idx];
    }
    
    const std::string &
    operator [](size_t idx) const
    {
        // No bounds checking, verify "idx" is good prior to calling this function
        return m_strings[idx];
    }

    void
    PopBack ()
    {
        m_strings.pop_back();
    }
    const char *
    GetStringAtIndex (size_t idx) const;

    void
    Join (const char *separator, Stream &strm);

    void
    Clear ();

    void
    LongestCommonPrefix (std::string &common_prefix);

    void
    InsertStringAtIndex (size_t idx, const std::string &str);
    
    void
    InsertStringAtIndex (size_t idx, std::string &&str);
    
    void
    InsertStringAtIndex (size_t id, const char *str);

    void
    DeleteStringAtIndex (size_t id);

    void
    RemoveBlankLines ();

    size_t
    SplitIntoLines (const std::string &lines);

    size_t
    SplitIntoLines (const char *lines, size_t len);
    
    std::string
    CopyList(const char* item_preamble = NULL,
             const char* items_sep = "\n") const;
    
    StringList&
    operator << (const char* str);

    StringList&
    operator << (StringList strings);
    
    // This string list contains a list of valid auto completion
    // strings, and the "s" is passed in. "matches" is filled in
    // with zero or more string values that start with "s", and
    // the first string to exactly match one of the string
    // values in this collection, will have "exact_matches_idx"
    // filled in to match the index, or "exact_matches_idx" will
    // have SIZE_MAX
    size_t
    AutoComplete (const char *s,
                  StringList &matches,
                  size_t &exact_matches_idx) const;

private:

    STLStringArray m_strings;
};

} // namespace lldb_private

#endif // liblldb_StringList_h_
