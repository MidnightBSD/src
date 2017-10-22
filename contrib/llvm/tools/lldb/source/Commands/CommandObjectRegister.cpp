//===-- CommandObjectRegister.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "CommandObjectRegister.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/RegisterValue.h"
#include "lldb/Core/Scalar.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Interpreter/OptionGroupFormat.h"
#include "lldb/Interpreter/OptionValueArray.h"
#include "lldb/Interpreter/OptionValueUInt64.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/Thread.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// "register read"
//----------------------------------------------------------------------
class CommandObjectRegisterRead : public CommandObjectParsed
{
public:
    CommandObjectRegisterRead (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "register read",
                             "Dump the contents of one or more register values from the current frame.  If no register is specified, dumps them all.",
                             NULL,
                             eFlagRequiresFrame         |
                             eFlagRequiresRegContext    |
                             eFlagProcessMustBeLaunched |
                             eFlagProcessMustBePaused   ),
        m_option_group (interpreter),
        m_format_options (eFormatDefault),
        m_command_options ()
    {
        CommandArgumentEntry arg;
        CommandArgumentData register_arg;
        
        // Define the first (and only) variant of this arg.
        register_arg.arg_type = eArgTypeRegisterName;
        register_arg.arg_repetition = eArgRepeatStar;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg.push_back (register_arg);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg);

        // Add the "--format"
        m_option_group.Append (&m_format_options, OptionGroupFormat::OPTION_GROUP_FORMAT | OptionGroupFormat::OPTION_GROUP_GDB_FMT, LLDB_OPT_SET_ALL);
        m_option_group.Append (&m_command_options);
        m_option_group.Finalize();

    }

    virtual
    ~CommandObjectRegisterRead ()
    {
    }

    Options *
    GetOptions ()
    {
        return &m_option_group;
    }

    bool
    DumpRegister (const ExecutionContext &exe_ctx,
                  Stream &strm,
                  RegisterContext *reg_ctx,
                  const RegisterInfo *reg_info)
    {
        if (reg_info)
        {
            RegisterValue reg_value;

            if (reg_ctx->ReadRegister (reg_info, reg_value))
            {
                strm.Indent ();

                bool prefix_with_altname = m_command_options.alternate_name;
                bool prefix_with_name = !prefix_with_altname;
                reg_value.Dump(&strm, reg_info, prefix_with_name, prefix_with_altname, m_format_options.GetFormat(), 8);
                if ((reg_info->encoding == eEncodingUint) || (reg_info->encoding == eEncodingSint))
                {
                    Process *process = exe_ctx.GetProcessPtr();
                    if (process && reg_info->byte_size == process->GetAddressByteSize())
                    {
                        addr_t reg_addr = reg_value.GetAsUInt64(LLDB_INVALID_ADDRESS);
                        if (reg_addr != LLDB_INVALID_ADDRESS)
                        {
                            Address so_reg_addr;
                            if (exe_ctx.GetTargetRef().GetSectionLoadList().ResolveLoadAddress(reg_addr, so_reg_addr))
                            {
                                strm.PutCString ("  ");
                                so_reg_addr.Dump(&strm, exe_ctx.GetBestExecutionContextScope(), Address::DumpStyleResolvedDescription);
                            }
                        }
                    }
                }
                strm.EOL();
                return true;
            }
        }
        return false;
    }

    bool
    DumpRegisterSet (const ExecutionContext &exe_ctx,
                     Stream &strm,
                     RegisterContext *reg_ctx,
                     size_t set_idx,
                     bool primitive_only=false)
    {
        uint32_t unavailable_count = 0;
        uint32_t available_count = 0;

        if (!reg_ctx)
            return false; // thread has no registers (i.e. core files are corrupt, incomplete crash logs...)

        const RegisterSet * const reg_set = reg_ctx->GetRegisterSet(set_idx);
        if (reg_set)
        {
            strm.Printf ("%s:\n", reg_set->name);
            strm.IndentMore ();
            const size_t num_registers = reg_set->num_registers;
            for (size_t reg_idx = 0; reg_idx < num_registers; ++reg_idx)
            {
                const uint32_t reg = reg_set->registers[reg_idx];
                const RegisterInfo *reg_info = reg_ctx->GetRegisterInfoAtIndex(reg);
                // Skip the dumping of derived register if primitive_only is true.
                if (primitive_only && reg_info && reg_info->value_regs)
                    continue;

                if (DumpRegister (exe_ctx, strm, reg_ctx, reg_info))
                    ++available_count;
                else
                    ++unavailable_count;
            }
            strm.IndentLess ();
            if (unavailable_count)
            {
                strm.Indent ();
                strm.Printf("%u registers were unavailable.\n", unavailable_count);
            }
            strm.EOL();
        }
        return available_count > 0;
    }

protected:
    virtual bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        Stream &strm = result.GetOutputStream();
        RegisterContext *reg_ctx = m_exe_ctx.GetRegisterContext ();

        const RegisterInfo *reg_info = NULL;
        if (command.GetArgumentCount() == 0)
        {
            size_t set_idx;
            
            size_t num_register_sets = 1;
            const size_t set_array_size = m_command_options.set_indexes.GetSize();
            if (set_array_size > 0)
            {
                for (size_t i=0; i<set_array_size; ++i)
                {
                    set_idx = m_command_options.set_indexes[i]->GetUInt64Value (UINT32_MAX, NULL);
                    if (set_idx < reg_ctx->GetRegisterSetCount())
                    {
                        if (!DumpRegisterSet (m_exe_ctx, strm, reg_ctx, set_idx))
                        {
                            if (errno)
                                result.AppendErrorWithFormat ("register read failed with errno: %d\n", errno);
                            else
                                result.AppendError ("unknown error while reading registers.\n");
                            result.SetStatus (eReturnStatusFailed);
                            break;
                        }
                    }
                    else
                    {
                        result.AppendErrorWithFormat ("invalid register set index: %zu\n", set_idx);
                        result.SetStatus (eReturnStatusFailed);
                        break;
                    }
                }
            }
            else
            {
                if (m_command_options.dump_all_sets)
                    num_register_sets = reg_ctx->GetRegisterSetCount();

                for (set_idx = 0; set_idx < num_register_sets; ++set_idx)
                {
                    // When dump_all_sets option is set, dump primitive as well as derived registers.
                    DumpRegisterSet (m_exe_ctx, strm, reg_ctx, set_idx, !m_command_options.dump_all_sets.GetCurrentValue());
                }
            }
        }
        else
        {
            if (m_command_options.dump_all_sets)
            {
                result.AppendError ("the --all option can't be used when registers names are supplied as arguments\n");
                result.SetStatus (eReturnStatusFailed);
            }
            else if (m_command_options.set_indexes.GetSize() > 0)
            {
                result.AppendError ("the --set <set> option can't be used when registers names are supplied as arguments\n");
                result.SetStatus (eReturnStatusFailed);
            }
            else
            {
                const char *arg_cstr;
                for (int arg_idx = 0; (arg_cstr = command.GetArgumentAtIndex(arg_idx)) != NULL; ++arg_idx)
                {
                    // in most LLDB commands we accept $rbx as the name for register RBX - and here we would
                    // reject it and non-existant. we should be more consistent towards the user and allow them
                    // to say reg read $rbx - internally, however, we should be strict and not allow ourselves
                    // to call our registers $rbx in our own API
                    if (*arg_cstr == '$')
                        arg_cstr = arg_cstr+1;
                    reg_info = reg_ctx->GetRegisterInfoByName(arg_cstr);

                    if (reg_info)
                    {
                        if (!DumpRegister (m_exe_ctx, strm, reg_ctx, reg_info))
                            strm.Printf("%-12s = error: unavailable\n", reg_info->name);
                    }
                    else
                    {
                        result.AppendErrorWithFormat ("Invalid register name '%s'.\n", arg_cstr);
                    }
                }
            }
        }
        return result.Succeeded();
    }

    class CommandOptions : public OptionGroup
    {
    public:
        CommandOptions () :
            OptionGroup(),
            set_indexes (OptionValue::ConvertTypeToMask (OptionValue::eTypeUInt64)),
            dump_all_sets (false, false), // Initial and default values are false
            alternate_name (false, false)
        {
        }
        
        virtual
        ~CommandOptions ()
        {
        }
        
        
        virtual uint32_t
        GetNumDefinitions ();

        virtual const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        virtual void
        OptionParsingStarting (CommandInterpreter &interpreter)
        {
            set_indexes.Clear();
            dump_all_sets.Clear();
            alternate_name.Clear();
        }

        virtual Error
        SetOptionValue (CommandInterpreter &interpreter,
                        uint32_t option_idx,
                        const char *option_value)
        {
            Error error;
            const int short_option = g_option_table[option_idx].short_option;
            switch (short_option)
            {
                case 's':
                    {
                        OptionValueSP value_sp (OptionValueUInt64::Create (option_value, error));
                        if (value_sp)
                            set_indexes.AppendValue (value_sp);
                    }
                    break;

                case 'a':
                    // When we don't use OptionValue::SetValueFromCString(const char *) to 
                    // set an option value, it won't be marked as being set in the options
                    // so we make a call to let users know the value was set via option
                    dump_all_sets.SetCurrentValue (true);
                    dump_all_sets.SetOptionWasSet ();
                    break;

                case 'A':
                    // When we don't use OptionValue::SetValueFromCString(const char *) to 
                    // set an option value, it won't be marked as being set in the options
                    // so we make a call to let users know the value was set via option
                    alternate_name.SetCurrentValue (true);
                    dump_all_sets.SetOptionWasSet ();
                    break;
                    
                default:
                    error.SetErrorStringWithFormat("unrecognized short option '%c'", short_option);
                    break;
            }
            return error;
        }
        
        // Options table: Required for subclasses of Options.
        
        static const OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        OptionValueArray set_indexes;
        OptionValueBoolean dump_all_sets;
        OptionValueBoolean alternate_name;
    };

    OptionGroupOptions m_option_group;
    OptionGroupFormat m_format_options;
    CommandOptions m_command_options;
};

const OptionDefinition
CommandObjectRegisterRead::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_ALL, false, "alternate", 'A', no_argument      , NULL, 0, eArgTypeNone      , "Display register names using the alternate register name if there is one."},
    { LLDB_OPT_SET_1  , false, "set"      , 's', required_argument, NULL, 0, eArgTypeIndex     , "Specify which register sets to dump by index."},
    { LLDB_OPT_SET_2  , false, "all"      , 'a', no_argument      , NULL, 0, eArgTypeNone      , "Show all register sets."},
};

uint32_t
CommandObjectRegisterRead::CommandOptions::GetNumDefinitions ()
{
    return sizeof(g_option_table)/sizeof(OptionDefinition);
}


//----------------------------------------------------------------------
// "register write"
//----------------------------------------------------------------------
class CommandObjectRegisterWrite : public CommandObjectParsed
{
public:
    CommandObjectRegisterWrite (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "register write",
                             "Modify a single register value.",
                             NULL,
                             eFlagRequiresFrame         |
                             eFlagRequiresRegContext    |
                             eFlagProcessMustBeLaunched |
                             eFlagProcessMustBePaused)
    {
        CommandArgumentEntry arg1;
        CommandArgumentEntry arg2;
        CommandArgumentData register_arg;
        CommandArgumentData value_arg;
        
        // Define the first (and only) variant of this arg.
        register_arg.arg_type = eArgTypeRegisterName;
        register_arg.arg_repetition = eArgRepeatPlain;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg1.push_back (register_arg);
        
        // Define the first (and only) variant of this arg.
        value_arg.arg_type = eArgTypeValue;
        value_arg.arg_repetition = eArgRepeatPlain;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg2.push_back (value_arg);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg1);
        m_arguments.push_back (arg2);
    }

    virtual
    ~CommandObjectRegisterWrite ()
    {
    }

protected:
    virtual bool
    DoExecute(Args& command, CommandReturnObject &result)
    {
        DataExtractor reg_data;
        RegisterContext *reg_ctx = m_exe_ctx.GetRegisterContext ();

        if (command.GetArgumentCount() != 2)
        {
            result.AppendError ("register write takes exactly 2 arguments: <reg-name> <value>");
            result.SetStatus (eReturnStatusFailed);
        }
        else
        {
            const char *reg_name = command.GetArgumentAtIndex(0);
            const char *value_str = command.GetArgumentAtIndex(1);
            
            
            // in most LLDB commands we accept $rbx as the name for register RBX - and here we would
            // reject it and non-existant. we should be more consistent towards the user and allow them
            // to say reg write $rbx - internally, however, we should be strict and not allow ourselves
            // to call our registers $rbx in our own API
            if (reg_name && *reg_name == '$')
                reg_name = reg_name+1;
            
            const RegisterInfo *reg_info = reg_ctx->GetRegisterInfoByName(reg_name);

            if (reg_info)
            {
                RegisterValue reg_value;
                
                Error error (reg_value.SetValueFromCString (reg_info, value_str));
                if (error.Success())
                {
                    if (reg_ctx->WriteRegister (reg_info, reg_value))
                    {
                        // Toss all frames and anything else in the thread
                        // after a register has been written.
                        m_exe_ctx.GetThreadRef().Flush();
                        result.SetStatus (eReturnStatusSuccessFinishNoResult);
                        return true;
                    }
                }
                if (error.AsCString())
                {
                    result.AppendErrorWithFormat ("Failed to write register '%s' with value '%s': %s\n",
                                                 reg_name,
                                                 value_str,
                                                 error.AsCString());
                }
                else
                {
                    result.AppendErrorWithFormat ("Failed to write register '%s' with value '%s'",
                                                 reg_name,
                                                 value_str);
                }
                result.SetStatus (eReturnStatusFailed);
            }
            else
            {
                result.AppendErrorWithFormat ("Register not found for '%s'.\n", reg_name);
                result.SetStatus (eReturnStatusFailed);
            }
        }
        return result.Succeeded();
    }
};


//----------------------------------------------------------------------
// CommandObjectRegister constructor
//----------------------------------------------------------------------
CommandObjectRegister::CommandObjectRegister(CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "register",
                            "A set of commands to access thread registers.",
                            "register [read|write] ...")
{
    LoadSubCommand ("read",  CommandObjectSP (new CommandObjectRegisterRead (interpreter)));
    LoadSubCommand ("write", CommandObjectSP (new CommandObjectRegisterWrite (interpreter)));
}


//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
CommandObjectRegister::~CommandObjectRegister()
{
}
