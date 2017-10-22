//===-- Options.cpp ---------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "lldb/Interpreter/Options.h"

// C Includes
// C++ Includes
#include <algorithm>
#include <bitset>
#include <map>

// Other libraries and framework includes
// Project includes
#include "lldb/Interpreter/CommandObject.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/CommandCompletions.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Core/StreamString.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;

//-------------------------------------------------------------------------
// Options
//-------------------------------------------------------------------------
Options::Options (CommandInterpreter &interpreter) :
    m_interpreter (interpreter),
    m_getopt_table ()
{
    BuildValidOptionSets();
}

Options::~Options ()
{
}

void
Options::NotifyOptionParsingStarting ()
{
    m_seen_options.clear();
    // Let the subclass reset its option values
    OptionParsingStarting ();
}

Error
Options::NotifyOptionParsingFinished ()
{
    return OptionParsingFinished ();
}

void
Options::OptionSeen (int option_idx)
{
    m_seen_options.insert (option_idx);
}

// Returns true is set_a is a subset of set_b;  Otherwise returns false.

bool
Options::IsASubset (const OptionSet& set_a, const OptionSet& set_b)
{
    bool is_a_subset = true;
    OptionSet::const_iterator pos_a;
    OptionSet::const_iterator pos_b;

    // set_a is a subset of set_b if every member of set_a is also a member of set_b

    for (pos_a = set_a.begin(); pos_a != set_a.end() && is_a_subset; ++pos_a)
    {
        pos_b = set_b.find(*pos_a);
        if (pos_b == set_b.end())
            is_a_subset = false;
    }

    return is_a_subset;
}

// Returns the set difference set_a - set_b, i.e. { x | ElementOf (x, set_a) && !ElementOf (x, set_b) }

size_t
Options::OptionsSetDiff (const OptionSet& set_a, const OptionSet& set_b, OptionSet& diffs)
{
    size_t num_diffs = 0;
    OptionSet::const_iterator pos_a;
    OptionSet::const_iterator pos_b;

    for (pos_a = set_a.begin(); pos_a != set_a.end(); ++pos_a)
    {
        pos_b = set_b.find(*pos_a);
        if (pos_b == set_b.end())
        {
            ++num_diffs;
            diffs.insert(*pos_a);
        }
    }

    return num_diffs;
}

// Returns the union of set_a and set_b.  Does not put duplicate members into the union.

void
Options::OptionsSetUnion (const OptionSet &set_a, const OptionSet &set_b, OptionSet &union_set)
{
    OptionSet::const_iterator pos;
    OptionSet::iterator pos_union;

    // Put all the elements of set_a into the union.

    for (pos = set_a.begin(); pos != set_a.end(); ++pos)
        union_set.insert(*pos);

    // Put all the elements of set_b that are not already there into the union.
    for (pos = set_b.begin(); pos != set_b.end(); ++pos)
    {
        pos_union = union_set.find(*pos);
        if (pos_union == union_set.end())
            union_set.insert(*pos);
    }
}

bool
Options::VerifyOptions (CommandReturnObject &result)
{
    bool options_are_valid = false;

    int num_levels = GetRequiredOptions().size();
    if (num_levels)
    {
        for (int i = 0; i < num_levels && !options_are_valid; ++i)
        {
            // This is the correct set of options if:  1). m_seen_options contains all of m_required_options[i]
            // (i.e. all the required options at this level are a subset of m_seen_options); AND
            // 2). { m_seen_options - m_required_options[i] is a subset of m_options_options[i] (i.e. all the rest of
            // m_seen_options are in the set of optional options at this level.

            // Check to see if all of m_required_options[i] are a subset of m_seen_options
            if (IsASubset (GetRequiredOptions()[i], m_seen_options))
            {
                // Construct the set difference: remaining_options = {m_seen_options} - {m_required_options[i]}
                OptionSet remaining_options;
                OptionsSetDiff (m_seen_options, GetRequiredOptions()[i], remaining_options);
                // Check to see if remaining_options is a subset of m_optional_options[i]
                if (IsASubset (remaining_options, GetOptionalOptions()[i]))
                    options_are_valid = true;
            }
        }
    }
    else
    {
        options_are_valid = true;
    }

    if (options_are_valid)
    {
        result.SetStatus (eReturnStatusSuccessFinishNoResult);
    }
    else
    {
        result.AppendError ("invalid combination of options for the given command");
        result.SetStatus (eReturnStatusFailed);
    }

    return options_are_valid;
}

// This is called in the Options constructor, though we could call it lazily if that ends up being
// a performance problem.

void
Options::BuildValidOptionSets ()
{
    // Check to see if we already did this.
    if (m_required_options.size() != 0)
        return;

    // Check to see if there are any options.
    int num_options = NumCommandOptions ();
    if (num_options == 0)
        return;

    const OptionDefinition *opt_defs = GetDefinitions();
    m_required_options.resize(1);
    m_optional_options.resize(1);
    
    // First count the number of option sets we've got.  Ignore LLDB_ALL_OPTION_SETS...
    
    uint32_t num_option_sets = 0;
    
    for (int i = 0; i < num_options; i++)
    {
        uint32_t this_usage_mask = opt_defs[i].usage_mask;
        if (this_usage_mask == LLDB_OPT_SET_ALL)
        {
            if (num_option_sets == 0)
                num_option_sets = 1;
        }
        else
        {
            for (uint32_t j = 0; j < LLDB_MAX_NUM_OPTION_SETS; j++)
            {
                if (this_usage_mask & (1 << j))
                {
                    if (num_option_sets <= j)
                        num_option_sets = j + 1;
                }
            }
        }
    }

    if (num_option_sets > 0)
    {
        m_required_options.resize(num_option_sets);
        m_optional_options.resize(num_option_sets);
        
        for (int i = 0; i < num_options; ++i)
        {
            for (uint32_t j = 0; j < num_option_sets; j++)
            {
                if (opt_defs[i].usage_mask & 1 << j)
                {
                    if (opt_defs[i].required)
                        m_required_options[j].insert(opt_defs[i].short_option);
                    else
                        m_optional_options[j].insert(opt_defs[i].short_option);
                }
            }
        }
    }
}

uint32_t
Options::NumCommandOptions ()
{
    const OptionDefinition *opt_defs = GetDefinitions ();
    if (opt_defs == NULL) 
        return 0;
        
    int i = 0;

    if (opt_defs != NULL)
    {
        while (opt_defs[i].long_option != NULL)
            ++i;
    }

    return i;
}

struct option *
Options::GetLongOptions ()
{
    // Check to see if this has already been done.
    if (m_getopt_table.empty())
    {
        // Check to see if there are any options.
        const uint32_t num_options = NumCommandOptions();
        if (num_options == 0)
            return NULL;

        uint32_t i;
        const OptionDefinition *opt_defs = GetDefinitions();

        std::map<int, uint32_t> option_seen;

        m_getopt_table.resize(num_options + 1);
        for (i = 0; i < num_options; ++i)
        {
            const int short_opt = opt_defs[i].short_option;

            m_getopt_table[i].name    = opt_defs[i].long_option;
            m_getopt_table[i].has_arg = opt_defs[i].option_has_arg;
            m_getopt_table[i].flag    = NULL;
            m_getopt_table[i].val     = short_opt;

            if (option_seen.find(short_opt) == option_seen.end())
            {
                option_seen[short_opt] = i;
            }
            else if (short_opt)
            {
                m_getopt_table[i].val = 0;
                std::map<int, uint32_t>::const_iterator pos = option_seen.find(short_opt);
                StreamString strm;
                if (isprint8(short_opt))
                    Host::SystemLog (Host::eSystemLogError, "option[%u] --%s has a short option -%c that conflicts with option[%u] --%s, short option won't be used for --%s\n",
                                i,
                                opt_defs[i].long_option,
                                short_opt,
                                pos->second,
                                m_getopt_table[pos->second].name,
                                opt_defs[i].long_option);
                else
                    Host::SystemLog (Host::eSystemLogError, "option[%u] --%s has a short option 0x%x that conflicts with option[%u] --%s, short option won't be used for --%s\n",
                                i,
                                opt_defs[i].long_option,
                                short_opt,
                                pos->second,
                                m_getopt_table[pos->second].name,
                                opt_defs[i].long_option);
            }
        }

        //getopt_long_only requires a NULL final entry in the table:

        m_getopt_table[i].name    = NULL;
        m_getopt_table[i].has_arg = 0;
        m_getopt_table[i].flag    = NULL;
        m_getopt_table[i].val     = 0;
    }

    if (m_getopt_table.empty())
        return NULL;

    return &m_getopt_table.front();
}


// This function takes INDENT, which tells how many spaces to output at the front of each line; SPACES, which is
// a string containing 80 spaces; and TEXT, which is the text that is to be output.   It outputs the text, on
// multiple lines if necessary, to RESULT, with INDENT spaces at the front of each line.  It breaks lines on spaces,
// tabs or newlines, shortening the line if necessary to not break in the middle of a word.  It assumes that each
// output line should contain a maximum of OUTPUT_MAX_COLUMNS characters.


void
Options::OutputFormattedUsageText
(
    Stream &strm,
    const char *text,
    uint32_t output_max_columns
)
{
    int len = strlen (text);

    // Will it all fit on one line?

    if ((len + strm.GetIndentLevel()) < output_max_columns)
    {
        // Output it as a single line.
        strm.Indent (text);
        strm.EOL();
    }
    else
    {
        // We need to break it up into multiple lines.

        int text_width = output_max_columns - strm.GetIndentLevel() - 1;
        int start = 0;
        int end = start;
        int final_end = strlen (text);
        int sub_len;

        while (end < final_end)
        {
            // Don't start the 'text' on a space, since we're already outputting the indentation.
            while ((start < final_end) && (text[start] == ' '))
                start++;

            end = start + text_width;
            if (end > final_end)
                end = final_end;
            else
            {
                // If we're not at the end of the text, make sure we break the line on white space.
                while (end > start
                       && text[end] != ' ' && text[end] != '\t' && text[end] != '\n')
                    end--;
            }

            sub_len = end - start;
            if (start != 0)
                strm.EOL();
            strm.Indent();
            assert (start < final_end);
            assert (start + sub_len <= final_end);
            strm.Write(text + start, sub_len);
            start = end + 1;
        }
        strm.EOL();
    }
}

bool
Options::SupportsLongOption (const char *long_option)
{
    if (long_option && long_option[0])
    {
        const OptionDefinition *opt_defs = GetDefinitions ();
        if (opt_defs)
        {
            const char *long_option_name = long_option;
            if (long_option[0] == '-' && long_option[1] == '-')
                long_option_name += 2;

            for (uint32_t i = 0; opt_defs[i].long_option; ++i)
            {
                if (strcmp(opt_defs[i].long_option, long_option_name) == 0)
                    return true;
            }
        }
    }
    return false;
}

enum OptionDisplayType
{
    eDisplayBestOption,
    eDisplayShortOption,
    eDisplayLongOption
};

static bool
PrintOption (const OptionDefinition &opt_def,
             OptionDisplayType display_type,
             const char *header,
             const char *footer,
             bool show_optional,
             Stream &strm)
{
    const bool has_short_option = isprint8(opt_def.short_option) != 0;

    if (display_type == eDisplayShortOption && !has_short_option)
        return false;

    if (header && header[0])
        strm.PutCString(header);
    
    if (show_optional && !opt_def.required)
    strm.PutChar('[');
    const bool show_short_option = has_short_option && display_type != eDisplayLongOption;
    if (show_short_option)
        strm.Printf ("-%c", opt_def.short_option);
    else
        strm.Printf ("--%s", opt_def.long_option);
    switch (opt_def.option_has_arg)
    {
        case no_argument:
            break;
        case required_argument:
            strm.Printf (" <%s>", CommandObject::GetArgumentName (opt_def.argument_type));
            break;
            
        case optional_argument:
            strm.Printf ("%s[<%s>]",
                         show_short_option ? "" : "=",
                         CommandObject::GetArgumentName (opt_def.argument_type));
            break;
    }
    if (show_optional && !opt_def.required)
        strm.PutChar(']');
    if (footer && footer[0])
        strm.PutCString(footer);
    return true;
}

void
Options::GenerateOptionUsage
(
    Stream &strm,
    CommandObject *cmd
)
{
    const uint32_t screen_width = m_interpreter.GetDebugger().GetTerminalWidth();

    const OptionDefinition *opt_defs = GetDefinitions();
    const uint32_t save_indent_level = strm.GetIndentLevel();
    const char *name;

    StreamString arguments_str;

    if (cmd)
    {
        name = cmd->GetCommandName();
        cmd->GetFormattedCommandArguments (arguments_str);
    }
    else
        name = "";

    strm.PutCString ("\nCommand Options Usage:\n");

    strm.IndentMore(2);

    // First, show each usage level set of options, e.g. <cmd> [options-for-level-0]
    //                                                   <cmd> [options-for-level-1]
    //                                                   etc.

    const uint32_t num_options = NumCommandOptions();
    if (num_options == 0)
        return;
        
    uint32_t num_option_sets = GetRequiredOptions().size();
    
    uint32_t i;
    
    for (uint32_t opt_set = 0; opt_set < num_option_sets; ++opt_set)
    {
        uint32_t opt_set_mask;
        
        opt_set_mask = 1 << opt_set;
        if (opt_set > 0)
            strm.Printf ("\n");
        strm.Indent (name);

        // Different option sets may require different args.
        StreamString args_str;
        if (cmd)
            cmd->GetFormattedCommandArguments(args_str, opt_set_mask);

        // First go through and print all options that take no arguments as
        // a single string. If a command has "-a" "-b" and "-c", this will show
        // up as [-abc]

        std::set<int> options;
        std::set<int>::const_iterator options_pos, options_end;
        for (i = 0; i < num_options; ++i)
        {
            if (opt_defs[i].usage_mask & opt_set_mask && isprint8(opt_defs[i].short_option))
            {
                // Add current option to the end of out_stream.

                if (opt_defs[i].required == true && 
                    opt_defs[i].option_has_arg == no_argument)
                {
                    options.insert (opt_defs[i].short_option);
                }
            }
        }

        if (options.empty() == false)
        {
            // We have some required options with no arguments
            strm.PutCString(" -");
            for (i=0; i<2; ++i)            
                for (options_pos = options.begin(), options_end = options.end();
                     options_pos != options_end;
                     ++options_pos)
                {
                    if (i==0 && ::islower (*options_pos))
                        continue;
                    if (i==1 && ::isupper (*options_pos))
                        continue;
                    strm << (char)*options_pos;
                }
        }

        for (i = 0, options.clear(); i < num_options; ++i)
        {
            if (opt_defs[i].usage_mask & opt_set_mask && isprint8(opt_defs[i].short_option))
            {
                // Add current option to the end of out_stream.

                if (opt_defs[i].required == false &&
                    opt_defs[i].option_has_arg == no_argument)
                {
                    options.insert (opt_defs[i].short_option);
                }
            }
        }

        if (options.empty() == false)
        {
            // We have some required options with no arguments
            strm.PutCString(" [-");
            for (i=0; i<2; ++i)            
                for (options_pos = options.begin(), options_end = options.end();
                     options_pos != options_end;
                     ++options_pos)
                {
                    if (i==0 && ::islower (*options_pos))
                        continue;
                    if (i==1 && ::isupper (*options_pos))
                        continue;
                    strm << (char)*options_pos;
                }
            strm.PutChar(']');
        }

        // First go through and print the required options (list them up front).
        
        for (i = 0; i < num_options; ++i)
        {
            if (opt_defs[i].usage_mask & opt_set_mask && isprint8(opt_defs[i].short_option))
            {
                if (opt_defs[i].required && opt_defs[i].option_has_arg != no_argument)
                    PrintOption (opt_defs[i], eDisplayBestOption, " ", NULL, true, strm);
            }
        }

        // Now go through again, and this time only print the optional options.

        for (i = 0; i < num_options; ++i)
        {
            if (opt_defs[i].usage_mask & opt_set_mask)
            {
                // Add current option to the end of out_stream.

                if (!opt_defs[i].required && opt_defs[i].option_has_arg != no_argument)
                    PrintOption (opt_defs[i], eDisplayBestOption, " ", NULL, true, strm);
            }
        }
        
        if (args_str.GetSize() > 0)
        {
            if (cmd->WantsRawCommandString())
                strm.Printf(" --");
            
            strm.Printf (" %s", args_str.GetData());
        }
    }
    
    if (cmd &&
        cmd->WantsRawCommandString() &&
        arguments_str.GetSize() > 0)
    {        
        strm.PutChar('\n');
        strm.Indent(name);
        strm.Printf(" %s", arguments_str.GetData());
    }
    
    strm.Printf ("\n\n");

    // Now print out all the detailed information about the various options:  long form, short form and help text:
    //   --long_name <argument>  ( -short <argument> )
    //   help text

    // This variable is used to keep track of which options' info we've printed out, because some options can be in
    // more than one usage level, but we only want to print the long form of its information once.

    std::multimap<int, uint32_t> options_seen;
    strm.IndentMore (5);

    // Put the unique command options in a vector & sort it, so we can output them alphabetically (by short_option)
    // when writing out detailed help for each option.

    for (i = 0; i < num_options; ++i)
        options_seen.insert(std::make_pair(opt_defs[i].short_option, i));

    // Go through the unique'd and alphabetically sorted vector of options, find the table entry for each option
    // and write out the detailed help information for that option.

    bool first_option_printed = false;;

    for (auto pos : options_seen)
    {
        i = pos.second;
        //Print out the help information for this option.

        // Put a newline separation between arguments
        if (first_option_printed)
            strm.EOL();
        else
            first_option_printed = true;
        
        CommandArgumentType arg_type = opt_defs[i].argument_type;
        
        StreamString arg_name_str;
        arg_name_str.Printf ("<%s>", CommandObject::GetArgumentName (arg_type));

        strm.Indent ();
        if (opt_defs[i].short_option && isprint8(opt_defs[i].short_option))
        {
            PrintOption (opt_defs[i], eDisplayShortOption, NULL, NULL, false, strm);
            PrintOption (opt_defs[i], eDisplayLongOption, " ( ", " )", false, strm);
        }
        else
        {
            // Short option is not printable, just print long option
            PrintOption (opt_defs[i], eDisplayLongOption, NULL, NULL, false, strm);
        }
        strm.EOL();
        
        strm.IndentMore (5);
        
        if (opt_defs[i].usage_text)
            OutputFormattedUsageText (strm,
                                      opt_defs[i].usage_text,
                                      screen_width);
        if (opt_defs[i].enum_values != NULL)
        {
            strm.Indent ();
            strm.Printf("Values: ");
            for (int k = 0; opt_defs[i].enum_values[k].string_value != NULL; k++) 
            {
                if (k == 0)
                    strm.Printf("%s", opt_defs[i].enum_values[k].string_value);
                else
                    strm.Printf(" | %s", opt_defs[i].enum_values[k].string_value);
            }
            strm.EOL();
        }
        strm.IndentLess (5);
    }

    // Restore the indent level
    strm.SetIndentLevel (save_indent_level);
}

// This function is called when we have been given a potentially incomplete set of
// options, such as when an alias has been defined (more options might be added at
// at the time the alias is invoked).  We need to verify that the options in the set
// m_seen_options are all part of a set that may be used together, but m_seen_options
// may be missing some of the "required" options.

bool
Options::VerifyPartialOptions (CommandReturnObject &result)
{
    bool options_are_valid = false;

    int num_levels = GetRequiredOptions().size();
    if (num_levels)
      {
        for (int i = 0; i < num_levels && !options_are_valid; ++i)
          {
            // In this case we are treating all options as optional rather than required.
            // Therefore a set of options is correct if m_seen_options is a subset of the
            // union of m_required_options and m_optional_options.
            OptionSet union_set;
            OptionsSetUnion (GetRequiredOptions()[i], GetOptionalOptions()[i], union_set);
            if (IsASubset (m_seen_options, union_set))
                options_are_valid = true;
          }
      }

    return options_are_valid;
}

bool
Options::HandleOptionCompletion
(
    Args &input,
    OptionElementVector &opt_element_vector,
    int cursor_index,
    int char_pos,
    int match_start_point,
    int max_return_elements,
    bool &word_complete,
    lldb_private::StringList &matches
)
{
    word_complete = true;
    
    // For now we just scan the completions to see if the cursor position is in
    // an option or its argument.  Otherwise we'll call HandleArgumentCompletion.
    // In the future we can use completion to validate options as well if we want.

    const OptionDefinition *opt_defs = GetDefinitions();

    std::string cur_opt_std_str (input.GetArgumentAtIndex(cursor_index));
    cur_opt_std_str.erase(char_pos);
    const char *cur_opt_str = cur_opt_std_str.c_str();

    for (size_t i = 0; i < opt_element_vector.size(); i++)
    {
        int opt_pos = opt_element_vector[i].opt_pos;
        int opt_arg_pos = opt_element_vector[i].opt_arg_pos;
        int opt_defs_index = opt_element_vector[i].opt_defs_index;
        if (opt_pos == cursor_index)
        {
            // We're completing the option itself.

            if (opt_defs_index == OptionArgElement::eBareDash)
            {
                // We're completing a bare dash.  That means all options are open.
                // FIXME: We should scan the other options provided and only complete options
                // within the option group they belong to.
                char opt_str[3] = {'-', 'a', '\0'};
                
                for (int j = 0 ; opt_defs[j].short_option != 0 ; j++)
                {   
                    opt_str[1] = opt_defs[j].short_option;
                    matches.AppendString (opt_str);
                }
                return true;
            }
            else if (opt_defs_index == OptionArgElement::eBareDoubleDash)
            {
                std::string full_name ("--");
                for (int j = 0 ; opt_defs[j].short_option != 0 ; j++)
                {   
                    full_name.erase(full_name.begin() + 2, full_name.end());
                    full_name.append (opt_defs[j].long_option);
                    matches.AppendString (full_name.c_str());
                }
                return true;
            }
            else if (opt_defs_index != OptionArgElement::eUnrecognizedArg)
            {
                // We recognized it, if it an incomplete long option, complete it anyway (getopt_long_only is
                // happy with shortest unique string, but it's still a nice thing to do.)  Otherwise return
                // The string so the upper level code will know this is a full match and add the " ".
                if (cur_opt_str && strlen (cur_opt_str) > 2
                    && cur_opt_str[0] == '-' && cur_opt_str[1] == '-'
                    && strcmp (opt_defs[opt_defs_index].long_option, cur_opt_str) != 0)
                {
                        std::string full_name ("--");
                        full_name.append (opt_defs[opt_defs_index].long_option);
                        matches.AppendString(full_name.c_str());
                        return true;
                }
                else
                {
                    matches.AppendString(input.GetArgumentAtIndex(cursor_index));
                    return true;
                }
            }
            else
            {
                // FIXME - not handling wrong options yet:
                // Check to see if they are writing a long option & complete it.
                // I think we will only get in here if the long option table has two elements
                // that are not unique up to this point.  getopt_long_only does shortest unique match
                // for long options already.

                if (cur_opt_str && strlen (cur_opt_str) > 2
                    && cur_opt_str[0] == '-' && cur_opt_str[1] == '-')
                {
                    for (int j = 0 ; opt_defs[j].short_option != 0 ; j++)
                    {
                        if (strstr(opt_defs[j].long_option, cur_opt_str + 2) == opt_defs[j].long_option)
                        {
                            std::string full_name ("--");
                            full_name.append (opt_defs[j].long_option);
                            // The options definitions table has duplicates because of the
                            // way the grouping information is stored, so only add once.
                            bool duplicate = false;
                            for (size_t k = 0; k < matches.GetSize(); k++)
                            {
                                if (matches.GetStringAtIndex(k) == full_name)
                                {
                                    duplicate = true;
                                    break;
                                }
                            }
                            if (!duplicate)
                                matches.AppendString(full_name.c_str());
                        }
                    }
                }
                return true;
            }


        }
        else if (opt_arg_pos == cursor_index)
        {
            // Okay the cursor is on the completion of an argument.
            // See if it has a completion, otherwise return no matches.

            if (opt_defs_index != -1)
            {
                HandleOptionArgumentCompletion (input,
                                                cursor_index,
                                                strlen (input.GetArgumentAtIndex(cursor_index)),
                                                opt_element_vector,
                                                i,
                                                match_start_point,
                                                max_return_elements,
                                                word_complete,
                                                matches);
                return true;
            }
            else
            {
                // No completion callback means no completions...
                return true;
            }

        }
        else
        {
            // Not the last element, keep going.
            continue;
        }
    }
    return false;
}

bool
Options::HandleOptionArgumentCompletion
(
    Args &input,
    int cursor_index,
    int char_pos,
    OptionElementVector &opt_element_vector,
    int opt_element_index,
    int match_start_point,
    int max_return_elements,
    bool &word_complete,
    lldb_private::StringList &matches
)
{
    const OptionDefinition *opt_defs = GetDefinitions();
    std::unique_ptr<SearchFilter> filter_ap;

    int opt_arg_pos = opt_element_vector[opt_element_index].opt_arg_pos;
    int opt_defs_index = opt_element_vector[opt_element_index].opt_defs_index;
    
    // See if this is an enumeration type option, and if so complete it here:
    
    OptionEnumValueElement *enum_values = opt_defs[opt_defs_index].enum_values;
    if (enum_values != NULL)
    {
        bool return_value = false;
        std::string match_string(input.GetArgumentAtIndex (opt_arg_pos), input.GetArgumentAtIndex (opt_arg_pos) + char_pos);
        for (int i = 0; enum_values[i].string_value != NULL; i++)
        {
            if (strstr(enum_values[i].string_value, match_string.c_str()) == enum_values[i].string_value)
            {
                matches.AppendString (enum_values[i].string_value);
                return_value = true;
            }
        }
        return return_value;
    }

    // If this is a source file or symbol type completion, and  there is a
    // -shlib option somewhere in the supplied arguments, then make a search filter
    // for that shared library.
    // FIXME: Do we want to also have an "OptionType" so we don't have to match string names?

    uint32_t completion_mask = opt_defs[opt_defs_index].completion_type;
    
    if (completion_mask == 0)
    {
        lldb::CommandArgumentType option_arg_type = opt_defs[opt_defs_index].argument_type;
        if (option_arg_type != eArgTypeNone)
        {
            CommandObject::ArgumentTableEntry *arg_entry = CommandObject::FindArgumentDataByType (opt_defs[opt_defs_index].argument_type);
            if (arg_entry)
                completion_mask = arg_entry->completion_type;
        }
    }

    if (completion_mask & CommandCompletions::eSourceFileCompletion
        || completion_mask & CommandCompletions::eSymbolCompletion)
    {
        for (size_t i = 0; i < opt_element_vector.size(); i++)
        {
            int cur_defs_index = opt_element_vector[i].opt_defs_index;
            int cur_arg_pos    = opt_element_vector[i].opt_arg_pos;
            const char *cur_opt_name = opt_defs[cur_defs_index].long_option;

            // If this is the "shlib" option and there was an argument provided,
            // restrict it to that shared library.
            if (strcmp(cur_opt_name, "shlib") == 0 && cur_arg_pos != -1)
            {
                const char *module_name = input.GetArgumentAtIndex(cur_arg_pos);
                if (module_name)
                {
                    FileSpec module_spec(module_name, false);
                    lldb::TargetSP target_sp = m_interpreter.GetDebugger().GetSelectedTarget();
                    // Search filters require a target...
                    if (target_sp)
                        filter_ap.reset (new SearchFilterByModule (target_sp, module_spec));
                }
                break;
            }
        }
    }

    return CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter,
                                                                completion_mask,
                                                                input.GetArgumentAtIndex (opt_arg_pos),
                                                                match_start_point,
                                                                max_return_elements,
                                                                filter_ap.get(),
                                                                word_complete,
                                                                matches);
    
}


void
OptionGroupOptions::Append (OptionGroup* group)
{
    const OptionDefinition* group_option_defs = group->GetDefinitions ();
    const uint32_t group_option_count = group->GetNumDefinitions();
    for (uint32_t i=0; i<group_option_count; ++i)
    {
        m_option_infos.push_back (OptionInfo (group, i));
        m_option_defs.push_back (group_option_defs[i]);
    }
}

void
OptionGroupOptions::Append (OptionGroup* group, 
                            uint32_t src_mask, 
                            uint32_t dst_mask)
{
    const OptionDefinition* group_option_defs = group->GetDefinitions ();
    const uint32_t group_option_count = group->GetNumDefinitions();
    for (uint32_t i=0; i<group_option_count; ++i)
    {
        if (group_option_defs[i].usage_mask & src_mask)
        {
            m_option_infos.push_back (OptionInfo (group, i));
            m_option_defs.push_back (group_option_defs[i]);
            m_option_defs.back().usage_mask = dst_mask;
        }
    }
}

void
OptionGroupOptions::Finalize ()
{
    m_did_finalize = true;
    OptionDefinition empty_option_def = { 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL };
    m_option_defs.push_back (empty_option_def);
}

Error
OptionGroupOptions::SetOptionValue (uint32_t option_idx,  
                                    const char *option_value)
{
    // After calling OptionGroupOptions::Append(...), you must finalize the groups
    // by calling OptionGroupOptions::Finlize()
    assert (m_did_finalize);
    assert (m_option_infos.size() + 1 == m_option_defs.size());
    Error error;
    if (option_idx < m_option_infos.size())
    {
        error = m_option_infos[option_idx].option_group->SetOptionValue (m_interpreter, 
                                                                         m_option_infos[option_idx].option_index,
                                                                         option_value);
        
    }
    else
    {
        error.SetErrorString ("invalid option index"); // Shouldn't happen...
    }
    return error;
}

void
OptionGroupOptions::OptionParsingStarting ()
{
    std::set<OptionGroup*> group_set;
    OptionInfos::iterator pos, end = m_option_infos.end();
    for (pos = m_option_infos.begin(); pos != end; ++pos)
    {
        OptionGroup* group = pos->option_group;
        if (group_set.find(group) == group_set.end())
        {
            group->OptionParsingStarting (m_interpreter);
            group_set.insert(group);
        }
    }
}
Error
OptionGroupOptions::OptionParsingFinished ()
{
    std::set<OptionGroup*> group_set;
    Error error;
    OptionInfos::iterator pos, end = m_option_infos.end();
    for (pos = m_option_infos.begin(); pos != end; ++pos)
    {
        OptionGroup* group = pos->option_group;
        if (group_set.find(group) == group_set.end())
        {
            error = group->OptionParsingFinished (m_interpreter);
            group_set.insert(group);
            if (error.Fail())
                return error;
        }
    }
    return error;
}
