//===-- RegularExpression.cpp -----------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/RegularExpression.h"
#include "llvm/ADT/StringRef.h"
#include <string.h>

using namespace lldb_private;

//----------------------------------------------------------------------
// Default constructor
//----------------------------------------------------------------------
RegularExpression::RegularExpression() :
    m_re(),
    m_comp_err (1),
    m_preg(),
    m_compile_flags(REG_EXTENDED)
{
    memset(&m_preg,0,sizeof(m_preg));
}

//----------------------------------------------------------------------
// Constructor that compiles "re" using "flags" and stores the
// resulting compiled regular expression into this object.
//----------------------------------------------------------------------
RegularExpression::RegularExpression(const char* re, int flags) :
    m_re(),
    m_comp_err (1),
    m_preg(),
    m_compile_flags(flags)
{
    memset(&m_preg,0,sizeof(m_preg));
    Compile(re);
}

//----------------------------------------------------------------------
// Constructor that compiles "re" using "flags" and stores the
// resulting compiled regular expression into this object.
//----------------------------------------------------------------------
RegularExpression::RegularExpression(const char* re) :
    m_re(),
    m_comp_err (1),
    m_preg(),
    m_compile_flags(REG_EXTENDED)
{
    memset(&m_preg,0,sizeof(m_preg));
    Compile(re);
}

RegularExpression::RegularExpression(const RegularExpression &rhs)
{
    memset(&m_preg,0,sizeof(m_preg));
    Compile(rhs.GetText(), rhs.GetCompileFlags());
}

const RegularExpression &
RegularExpression::operator= (const RegularExpression &rhs)
{
    if (&rhs != this)
    {
        Compile (rhs.GetText(), rhs.GetCompileFlags());
    }
    return *this;
}
//----------------------------------------------------------------------
// Destructor
//
// Any previosuly compiled regular expression contained in this
// object will be freed.
//----------------------------------------------------------------------
RegularExpression::~RegularExpression()
{
    Free();
}

//----------------------------------------------------------------------
// Compile a regular expression using the supplied regular
// expression text and flags. The compied regular expression lives
// in this object so that it can be readily used for regular
// expression matches. Execute() can be called after the regular
// expression is compiled. Any previosuly compiled regular
// expression contained in this object will be freed.
//
// RETURNS
//  True of the refular expression compiles successfully, false
//  otherwise.
//----------------------------------------------------------------------
bool
RegularExpression::Compile(const char* re)
{
    return Compile (re, m_compile_flags);
}

bool
RegularExpression::Compile(const char* re, int flags)
{
    Free();
    m_compile_flags = flags;
    
    if (re && re[0])
    {
        m_re = re;
        m_comp_err = ::regcomp (&m_preg, re, flags);
    }
    else
    {
        // No valid regular expression
        m_comp_err = 1;
    }

    return m_comp_err == 0;
}

//----------------------------------------------------------------------
// Execute a regular expression match using the compiled regular
// expression that is already in this object against the match
// string "s". If any parens are used for regular expression
// matches "match_count" should indicate the number of regmatch_t
// values that are present in "match_ptr". The regular expression
// will be executed using the "execute_flags".
//---------------------------------------------------------------------
bool
RegularExpression::Execute(const char* s, Match *match, int execute_flags) const
{
    int err = 1;
    if (s != NULL && m_comp_err == 0)
    {
        if (match)
        {
            err = ::regexec (&m_preg,
                             s,
                             match->GetSize(),
                             match->GetData(),
                             execute_flags);
        }
        else
        {
            err = ::regexec (&m_preg,
                             s,
                             0,
                             NULL,
                             execute_flags);
        }
    }
    
    if (err != 0)
    {
        // The regular expression didn't compile, so clear the matches
        if (match)
            match->Clear();
        return false;
    }
    return true;
}

bool
RegularExpression::Match::GetMatchAtIndex (const char* s, uint32_t idx, std::string& match_str) const
{
    if (idx < m_matches.size())
    {
        if (m_matches[idx].rm_eo == m_matches[idx].rm_so)
        {
            // Matched the empty string...
            match_str.clear();
            return true;
        }
        else if (m_matches[idx].rm_eo > m_matches[idx].rm_so)
        {
            match_str.assign (s + m_matches[idx].rm_so,
                              m_matches[idx].rm_eo - m_matches[idx].rm_so);
            return true;
        }
    }
    return false;
}

bool
RegularExpression::Match::GetMatchAtIndex (const char* s, uint32_t idx, llvm::StringRef& match_str) const
{
    if (idx < m_matches.size())
    {
        if (m_matches[idx].rm_eo == m_matches[idx].rm_so)
        {
            // Matched the empty string...
            match_str = llvm::StringRef();
            return true;
        }
        else if (m_matches[idx].rm_eo > m_matches[idx].rm_so)
        {
            match_str = llvm::StringRef (s + m_matches[idx].rm_so, m_matches[idx].rm_eo - m_matches[idx].rm_so);
            return true;
        }
    }
    return false;
}

bool
RegularExpression::Match::GetMatchSpanningIndices (const char* s, uint32_t idx1, uint32_t idx2, llvm::StringRef& match_str) const
{
    if (idx1 < m_matches.size() && idx2 < m_matches.size())
    {
        if (m_matches[idx1].rm_so == m_matches[idx2].rm_eo)
        {
            // Matched the empty string...
            match_str = llvm::StringRef();
            return true;
        }
        else if (m_matches[idx1].rm_so < m_matches[idx2].rm_eo)
        {
            match_str = llvm::StringRef (s + m_matches[idx1].rm_so, m_matches[idx2].rm_eo - m_matches[idx1].rm_so);
            return true;
        }
    }
    return false;
}


//----------------------------------------------------------------------
// Returns true if the regular expression compiled and is ready
// for execution.
//----------------------------------------------------------------------
bool
RegularExpression::IsValid () const
{
    return m_comp_err == 0;
}

//----------------------------------------------------------------------
// Returns the text that was used to compile the current regular
// expression.
//----------------------------------------------------------------------
const char*
RegularExpression::GetText () const
{
    if (m_re.empty())
        return NULL;
    return m_re.c_str();
}

//----------------------------------------------------------------------
// Free any contained compiled regular expressions.
//----------------------------------------------------------------------
void
RegularExpression::Free()
{
    if (m_comp_err == 0)
    {
        m_re.clear();
        regfree(&m_preg);
        // Set a compile error since we no longer have a valid regex
        m_comp_err = 1;
    }
}

size_t
RegularExpression::GetErrorAsCString (char *err_str, size_t err_str_max_len) const
{
    if (m_comp_err == 0)
    {
        if (err_str && err_str_max_len) 
            *err_str = '\0';
        return 0;
    }
    
    return ::regerror (m_comp_err, &m_preg, err_str, err_str_max_len);
}

bool
RegularExpression::operator < (const RegularExpression& rhs) const
{
    return (m_re < rhs.m_re);
}

