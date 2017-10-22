//===-- StringList.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/StringList.h"

#include "lldb/Core/StreamString.h"
#include "lldb/Host/FileSpec.h"

#include <string>

using namespace lldb_private;

StringList::StringList () :
    m_strings ()
{
}

StringList::StringList (const char *str) :
    m_strings ()
{
    if (str)
        m_strings.push_back (str);
}

StringList::StringList (const char **strv, int strc) :
    m_strings ()
{
    for (int i = 0; i < strc; ++i)
    {
        if (strv[i])
            m_strings.push_back (strv[i]);
    }
}

StringList::~StringList ()
{
}

void
StringList::AppendString (const char *str)
{
    if (str)
        m_strings.push_back (str);
}

void
StringList::AppendString (const std::string &s)
{
    m_strings.push_back (s);
}

void
StringList::AppendString (const char *str, size_t str_len)
{
    if (str)
        m_strings.push_back (std::string (str, str_len));
}

void
StringList::AppendList (const char **strv, int strc)
{
    for (int i = 0; i < strc; ++i)
    {
        if (strv[i])
            m_strings.push_back (strv[i]);
    }
}

void
StringList::AppendList (StringList strings)
{
    size_t len = strings.GetSize();

    for (size_t i = 0; i < len; ++i)
        m_strings.push_back (strings.GetStringAtIndex(i));
}

bool
StringList::ReadFileLines (FileSpec &input_file)
{
    return input_file.ReadFileLines (m_strings);
}

size_t
StringList::GetSize () const
{
    return m_strings.size();
}

const char *
StringList::GetStringAtIndex (size_t idx) const
{
    if (idx < m_strings.size())
        return m_strings[idx].c_str();
    return NULL;
}

void
StringList::Join (const char *separator, Stream &strm)
{
    size_t size = GetSize();
    
    if (size == 0)
        return;
    
    for (uint32_t i = 0; i < size; ++i)
    {
        if (i > 0)
            strm.PutCString(separator);
        strm.PutCString(GetStringAtIndex(i));
    }
}

void
StringList::Clear ()
{
    m_strings.clear();
}

void
StringList::LongestCommonPrefix (std::string &common_prefix)
{
    //arg_sstr_collection::iterator pos, end = m_args.end();
    size_t pos = 0;
    size_t end = m_strings.size();

    if (pos == end)
        common_prefix.clear();
    else
        common_prefix = m_strings[pos];

    for (++pos; pos != end; ++pos)
    {
        size_t new_size = strlen (m_strings[pos].c_str());

        // First trim common_prefix if it is longer than the current element:
        if (common_prefix.size() > new_size)
            common_prefix.erase (new_size);

        // Then trim it at the first disparity:

        for (size_t i = 0; i < common_prefix.size(); i++)
        {
            if (m_strings[pos][i]  != common_prefix[i])
            {
                common_prefix.erase(i);
                break;
            }
        }

        // If we've emptied the common prefix, we're done.
        if (common_prefix.empty())
            break;
    }
}

void
StringList::InsertStringAtIndex (size_t idx, const char *str)
{
    if (str)
    {
        if (idx < m_strings.size())
            m_strings.insert (m_strings.begin() + idx, str);
        else
            m_strings.push_back (str);
    }
}

void
StringList::DeleteStringAtIndex (size_t idx)
{
    if (idx < m_strings.size())
        m_strings.erase (m_strings.begin() + idx);
}

size_t
StringList::SplitIntoLines (const char *lines, size_t len)
{
    const size_t orig_size = m_strings.size();

    if (len == 0)
        return 0;

    const char *k_newline_chars = "\r\n";
    const char *p = lines;
    const char *end = lines + len;
    while (p < end)
    {
        size_t count = strcspn (p, k_newline_chars);
        if (count == 0)
        {
            if (p[count] == '\r' || p[count] == '\n')
                m_strings.push_back(std::string());
            else
                break;
        }
        else
        {
            if (p + count > end)
                count = end - p;
            m_strings.push_back(std::string(p, count));
        }
        if (p[count] == '\r' && p[count+1] == '\n')
            count++;    // Skip an extra newline char for the DOS newline
        count++;    // Skip the newline character
        p += count;
    }
    return m_strings.size() - orig_size;
}

void
StringList::RemoveBlankLines ()
{
    if (GetSize() == 0)
        return;

    size_t idx = 0;
    while (idx < m_strings.size())
    {
        if (m_strings[idx].empty())            
            DeleteStringAtIndex(idx);
        else
            idx++;
    }
}

std::string
StringList::CopyList(const char* item_preamble,
                     const char* items_sep)
{
    StreamString strm;
    for (size_t i = 0; i < GetSize(); i++)
    {
        if (i && items_sep && items_sep[0])
            strm << items_sep;
        if (item_preamble)
            strm << item_preamble;
        strm << GetStringAtIndex(i);
    }
    return std::string(strm.GetData());
}

StringList&
StringList::operator << (const char* str)
{
    AppendString(str);
    return *this;
}

StringList&
StringList::operator << (StringList strings)
{
    AppendList(strings);
    return *this;
}

size_t
StringList::AutoComplete (const char *s, StringList &matches, size_t &exact_idx) const
{
    matches.Clear();
    exact_idx = SIZE_MAX;
    if (s && s[0])
    {
        const size_t s_len = strlen (s);
        const size_t num_strings = m_strings.size();
        
        for (size_t i=0; i<num_strings; ++i)
        {
            if (m_strings[i].find(s) == 0)
            {
                if (exact_idx == SIZE_MAX && m_strings[i].size() == s_len)
                    exact_idx = matches.GetSize();
                matches.AppendString (m_strings[i]);
            }
        }
    }
    else
    {
        // No string, so it matches everything
        matches = *this;
    }
    return matches.GetSize();
}

