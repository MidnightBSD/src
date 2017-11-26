//===-- source/Host/common/OptionParser.cpp ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Host/OptionParser.h"

#if (!defined( _MSC_VER ) && defined( _WIN32 ))
#define _BSD_SOURCE // Required so that getopt.h defines optreset
#endif
#include "lldb/Host/HostGetOpt.h"

using namespace lldb_private;

void
OptionParser::Prepare()
{
#ifdef __GLIBC__
    optind = 0;
#else
    optreset = 1;
    optind = 1;
#endif
}

void
OptionParser::EnableError(bool error)
{
    opterr = error ? 1 : 0;
}

int
OptionParser::Parse (int argc,
                     char * const argv [],
                     const char *optstring,
                     const Option *longopts,
                     int *longindex)
{
    return getopt_long_only(argc, argv, optstring, (const option*)longopts, longindex);
}

char*
OptionParser::GetOptionArgument()
{
    return optarg;
}

int
OptionParser::GetOptionIndex()
{
    return optind;
}

int
OptionParser::GetOptionErrorCause()
{
    return optopt;
}

std::string
OptionParser::GetShortOptionString(struct option *long_options)
{
    std::string s;
    int i=0;
    bool done = false;
    while (!done)
    {
        if (long_options[i].name    == 0 &&
            long_options[i].has_arg == 0 &&
            long_options[i].flag    == 0 &&
            long_options[i].val     == 0)
        {
            done = true;
        }
        else
        {
            if (long_options[i].flag == NULL &&
                isalpha(long_options[i].val))
            {
                s.append(1, (char)long_options[i].val);
                switch (long_options[i].has_arg)
                {
                    default:
                    case no_argument:
                        break;
                        
                    case optional_argument:
                        s.append(2, ':');
                        break;
                    case required_argument:
                        s.append(1, ':');
                        break;
                }
            }
            ++i;
        }
    }
    return s;
}
