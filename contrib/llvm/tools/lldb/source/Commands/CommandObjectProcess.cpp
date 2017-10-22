//===-- CommandObjectProcess.cpp --------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "CommandObjectProcess.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/Breakpoint.h"
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Breakpoint/BreakpointSite.h"
#include "lldb/Core/State.h"
#include "lldb/Core/Module.h"
#include "lldb/Host/Host.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Interpreter/Options.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/StopInfo.h"
#include "lldb/Target/Target.h"
#include "lldb/Target/Thread.h"

using namespace lldb;
using namespace lldb_private;

class CommandObjectProcessLaunchOrAttach : public CommandObjectParsed
{
public:
    CommandObjectProcessLaunchOrAttach (CommandInterpreter &interpreter,
                                       const char *name,
                                       const char *help,
                                       const char *syntax,
                                       uint32_t flags,
                                       const char *new_process_action) :
        CommandObjectParsed (interpreter, name, help, syntax, flags),
        m_new_process_action (new_process_action) {}
    
    virtual ~CommandObjectProcessLaunchOrAttach () {}
protected:
    bool
    StopProcessIfNecessary (Process *&process, StateType &state, CommandReturnObject &result)
    {
        state = eStateInvalid;
        if (process)
        {
            state = process->GetState();
            
            if (process->IsAlive() && state != eStateConnected)
            {       
                char message[1024];
                if (process->GetState() == eStateAttaching)
                    ::snprintf (message, sizeof(message), "There is a pending attach, abort it and %s?", m_new_process_action.c_str());
                else if (process->GetShouldDetach())
                    ::snprintf (message, sizeof(message), "There is a running process, detach from it and %s?", m_new_process_action.c_str());
                else
                    ::snprintf (message, sizeof(message), "There is a running process, kill it and %s?", m_new_process_action.c_str());
        
                if (!m_interpreter.Confirm (message, true))
                {
                    result.SetStatus (eReturnStatusFailed);
                    return false;
                }
                else
                {
                    if (process->GetShouldDetach())
                    {
                        bool keep_stopped = false;
                        Error detach_error (process->Detach(keep_stopped));
                        if (detach_error.Success())
                        {
                            result.SetStatus (eReturnStatusSuccessFinishResult);
                            process = NULL;
                        }
                        else
                        {
                            result.AppendErrorWithFormat ("Failed to detach from process: %s\n", detach_error.AsCString());
                            result.SetStatus (eReturnStatusFailed);
                        }
                    }
                    else
                    {
                        Error destroy_error (process->Destroy());
                        if (destroy_error.Success())
                        {
                            result.SetStatus (eReturnStatusSuccessFinishResult);
                            process = NULL;
                        }
                        else
                        {
                            result.AppendErrorWithFormat ("Failed to kill process: %s\n", destroy_error.AsCString());
                            result.SetStatus (eReturnStatusFailed);
                        }
                    }
                }
            }
        }
        return result.Succeeded();
    }
    std::string m_new_process_action;
};
//-------------------------------------------------------------------------
// CommandObjectProcessLaunch
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessLaunch
class CommandObjectProcessLaunch : public CommandObjectProcessLaunchOrAttach
{
public:

    CommandObjectProcessLaunch (CommandInterpreter &interpreter) :
        CommandObjectProcessLaunchOrAttach (interpreter,
                                            "process launch",
                                            "Launch the executable in the debugger.",
                                            NULL,
                                            eFlagRequiresTarget,
                                            "restart"),
        m_options (interpreter)
    {
        CommandArgumentEntry arg;
        CommandArgumentData run_args_arg;
        
        // Define the first (and only) variant of this arg.
        run_args_arg.arg_type = eArgTypeRunArgs;
        run_args_arg.arg_repetition = eArgRepeatOptional;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg.push_back (run_args_arg);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg);
    }


    ~CommandObjectProcessLaunch ()
    {
    }

    virtual int
    HandleArgumentCompletion (Args &input,
                              int &cursor_index,
                              int &cursor_char_position,
                              OptionElementVector &opt_element_vector,
                              int match_start_point,
                              int max_return_elements,
                              bool &word_complete,
                              StringList &matches)
    {
        std::string completion_str (input.GetArgumentAtIndex(cursor_index));
        completion_str.erase (cursor_char_position);
        
        CommandCompletions::InvokeCommonCompletionCallbacks (m_interpreter, 
                                                             CommandCompletions::eDiskFileCompletion,
                                                             completion_str.c_str(),
                                                             match_start_point,
                                                             max_return_elements,
                                                             NULL,
                                                             word_complete,
                                                             matches);
        return matches.GetSize();
    }

    Options *
    GetOptions ()
    {
        return &m_options;
    }

    virtual const char *GetRepeatCommand (Args &current_command_args, uint32_t index)
    {
        // No repeat for "process launch"...
        return "";
    }

protected:
    bool
    DoExecute (Args& launch_args, CommandReturnObject &result)
    {
        Debugger &debugger = m_interpreter.GetDebugger();
        Target *target = debugger.GetSelectedTarget().get();
        Error error;
        // If our listener is NULL, users aren't allows to launch
        char filename[PATH_MAX];
        const Module *exe_module = target->GetExecutableModulePointer();

        if (exe_module == NULL)
        {
            result.AppendError ("no file in target, create a debug target using the 'target create' command");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }
        
        StateType state = eStateInvalid;
        Process *process = m_exe_ctx.GetProcessPtr();
        
        if (!StopProcessIfNecessary(process, state, result))
            return false;
        
        const char *target_settings_argv0 = target->GetArg0();
        
        exe_module->GetFileSpec().GetPath (filename, sizeof(filename));
        
        if (target_settings_argv0)
        {
            m_options.launch_info.GetArguments().AppendArgument (target_settings_argv0);
            m_options.launch_info.SetExecutableFile(exe_module->GetPlatformFileSpec(), false);
        }
        else
        {
            m_options.launch_info.SetExecutableFile(exe_module->GetPlatformFileSpec(), true);
        }

        if (launch_args.GetArgumentCount() == 0)
        {
            Args target_setting_args;
            if (target->GetRunArguments(target_setting_args))
                m_options.launch_info.GetArguments().AppendArguments (target_setting_args);
        }
        else
        {
            m_options.launch_info.GetArguments().AppendArguments (launch_args);

            // Save the arguments for subsequent runs in the current target.
            target->SetRunArguments (launch_args);
        }
        
        if (target->GetDisableASLR())
            m_options.launch_info.GetFlags().Set (eLaunchFlagDisableASLR);
    
        if (target->GetDisableSTDIO())
            m_options.launch_info.GetFlags().Set (eLaunchFlagDisableSTDIO);

        m_options.launch_info.GetFlags().Set (eLaunchFlagDebug);

        Args environment;
        target->GetEnvironmentAsArgs (environment);
        if (environment.GetArgumentCount() > 0)
            m_options.launch_info.GetEnvironmentEntries ().AppendArguments (environment);

        // Get the value of synchronous execution here.  If you wait till after you have started to
        // run, then you could have hit a breakpoint, whose command might switch the value, and
        // then you'll pick up that incorrect value.
        bool synchronous_execution = m_interpreter.GetSynchronous ();

        // Finalize the file actions, and if none were given, default to opening
        // up a pseudo terminal
        const bool default_to_use_pty = true;
        m_options.launch_info.FinalizeFileActions (target, default_to_use_pty);

        if (state == eStateConnected)
        {
            if (m_options.launch_info.GetFlags().Test (eLaunchFlagLaunchInTTY))
            {
                result.AppendWarning("can't launch in tty when launching through a remote connection");
                m_options.launch_info.GetFlags().Clear (eLaunchFlagLaunchInTTY);
            }
        }
        
        if (!m_options.launch_info.GetArchitecture().IsValid())
            m_options.launch_info.GetArchitecture() = target->GetArchitecture();

        PlatformSP platform_sp (target->GetPlatform());
        
        if (platform_sp && platform_sp->CanDebugProcess ())
        {
            process = target->GetPlatform()->DebugProcess (m_options.launch_info, 
                                                           debugger,
                                                           target,
                                                           debugger.GetListener(),
                                                           error).get();
        }
        else
        {
            const char *plugin_name = m_options.launch_info.GetProcessPluginName();
            process = target->CreateProcess (debugger.GetListener(), plugin_name, NULL).get();
            if (process)
                error = process->Launch (m_options.launch_info);
        }

        if (process == NULL)
        {
            result.SetError (error, "failed to launch or debug process");
            return false;
        }

             
        if (error.Success())
        {
            const char *archname = exe_module->GetArchitecture().GetArchitectureName();

            result.AppendMessageWithFormat ("Process %" PRIu64 " launched: '%s' (%s)\n", process->GetID(), filename, archname);
            result.SetDidChangeProcessState (true);
            if (m_options.launch_info.GetFlags().Test(eLaunchFlagStopAtEntry) == false)
            {
                result.SetStatus (eReturnStatusSuccessContinuingNoResult);
                StateType state = process->WaitForProcessToStop (NULL);

                if (state == eStateStopped)
                {
                    error = process->Resume();
                    if (error.Success())
                    {
                        if (synchronous_execution)
                        {
                            state = process->WaitForProcessToStop (NULL);
                            const bool must_be_alive = true;
                            if (!StateIsStoppedState(state, must_be_alive))
                            {
                                result.AppendErrorWithFormat ("process isn't stopped: %s", StateAsCString(state));
                            }                    
                            result.SetDidChangeProcessState (true);
                            result.SetStatus (eReturnStatusSuccessFinishResult);
                        }
                        else
                        {
                            result.SetStatus (eReturnStatusSuccessContinuingNoResult);
                        }
                    }
                    else
                    {
                        result.AppendErrorWithFormat ("process resume at entry point failed: %s", error.AsCString());
                        result.SetStatus (eReturnStatusFailed);
                    }                    
                }
                else
                {
                    result.AppendErrorWithFormat ("initial process state wasn't stopped: %s", StateAsCString(state));
                    result.SetStatus (eReturnStatusFailed);
                }                    
            }
        }
        else
        {
            result.AppendErrorWithFormat ("process launch failed: %s", error.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }

        return result.Succeeded();
    }

protected:
    ProcessLaunchCommandOptions m_options;
};


//#define SET1 LLDB_OPT_SET_1
//#define SET2 LLDB_OPT_SET_2
//#define SET3 LLDB_OPT_SET_3
//
//OptionDefinition
//CommandObjectProcessLaunch::CommandOptions::g_option_table[] =
//{
//{ SET1 | SET2 | SET3, false, "stop-at-entry", 's', no_argument,       NULL, 0, eArgTypeNone,    "Stop at the entry point of the program when launching a process."},
//{ SET1              , false, "stdin",         'i', required_argument, NULL, 0, eArgTypeDirectoryName,    "Redirect stdin for the process to <path>."},
//{ SET1              , false, "stdout",        'o', required_argument, NULL, 0, eArgTypeDirectoryName,    "Redirect stdout for the process to <path>."},
//{ SET1              , false, "stderr",        'e', required_argument, NULL, 0, eArgTypeDirectoryName,    "Redirect stderr for the process to <path>."},
//{ SET1 | SET2 | SET3, false, "plugin",        'p', required_argument, NULL, 0, eArgTypePlugin,  "Name of the process plugin you want to use."},
//{        SET2       , false, "tty",           't', optional_argument, NULL, 0, eArgTypeDirectoryName,    "Start the process in a terminal. If <path> is specified, look for a terminal whose name contains <path>, else start the process in a new terminal."},
//{               SET3, false, "no-stdio",      'n', no_argument,       NULL, 0, eArgTypeNone,    "Do not set up for terminal I/O to go to running process."},
//{ SET1 | SET2 | SET3, false, "working-dir",   'w', required_argument, NULL, 0, eArgTypeDirectoryName,    "Set the current working directory to <path> when running the inferior."},
//{ 0,                  false, NULL,             0,  0,                 NULL, 0, eArgTypeNone,    NULL }
//};
//
//#undef SET1
//#undef SET2
//#undef SET3

//-------------------------------------------------------------------------
// CommandObjectProcessAttach
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessAttach
class CommandObjectProcessAttach : public CommandObjectProcessLaunchOrAttach
{
public:

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options(interpreter)
        {
            // Keep default values of all options in one place: OptionParsingStarting ()
            OptionParsingStarting ();
        }

        ~CommandOptions ()
        {
        }

        Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            bool success = false;
            switch (short_option)
            {
                case 'c':
                    attach_info.SetContinueOnceAttached(true);
                    break;

                case 'p':   
                    {
                        lldb::pid_t pid = Args::StringToUInt32 (option_arg, LLDB_INVALID_PROCESS_ID, 0, &success);
                        if (!success || pid == LLDB_INVALID_PROCESS_ID)
                        {
                            error.SetErrorStringWithFormat("invalid process ID '%s'", option_arg);
                        }
                        else
                        {
                            attach_info.SetProcessID (pid);
                        }
                    }
                    break;

                case 'P':
                    attach_info.SetProcessPluginName (option_arg);
                    break;

                case 'n': 
                    attach_info.GetExecutableFile().SetFile(option_arg, false);
                    break;

                case 'w':   
                    attach_info.SetWaitForLaunch(true);
                    break;
                    
                case 'i':
                    attach_info.SetIgnoreExisting(false);
                    break;

                default:
                    error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                    break;
            }
            return error;
        }

        void
        OptionParsingStarting ()
        {
            attach_info.Clear();
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        virtual bool
        HandleOptionArgumentCompletion (Args &input,
                                        int cursor_index,
                                        int char_pos,
                                        OptionElementVector &opt_element_vector,
                                        int opt_element_index,
                                        int match_start_point,
                                        int max_return_elements,
                                        bool &word_complete,
                                        StringList &matches)
        {
            int opt_arg_pos = opt_element_vector[opt_element_index].opt_arg_pos;
            int opt_defs_index = opt_element_vector[opt_element_index].opt_defs_index;
    
            // We are only completing the name option for now...
            
            const OptionDefinition *opt_defs = GetDefinitions();
            if (opt_defs[opt_defs_index].short_option == 'n')
            {
                // Are we in the name?
                
                // Look to see if there is a -P argument provided, and if so use that plugin, otherwise
                // use the default plugin.
                
                const char *partial_name = NULL;
                partial_name = input.GetArgumentAtIndex(opt_arg_pos);

                PlatformSP platform_sp (m_interpreter.GetPlatform (true));
                if (platform_sp)
                {
                    ProcessInstanceInfoList process_infos;
                    ProcessInstanceInfoMatch match_info;
                    if (partial_name)
                    {
                        match_info.GetProcessInfo().GetExecutableFile().SetFile(partial_name, false);
                        match_info.SetNameMatchType(eNameMatchStartsWith);
                    }
                    platform_sp->FindProcesses (match_info, process_infos);
                    const size_t num_matches = process_infos.GetSize();
                    if (num_matches > 0)
                    {
                        for (size_t i=0; i<num_matches; ++i)
                        {
                            matches.AppendString (process_infos.GetProcessNameAtIndex(i), 
                                                  process_infos.GetProcessNameLengthAtIndex(i));
                        }
                    }
                }
            }
            
            return false;
        }

        // Options table: Required for subclasses of Options.

        static OptionDefinition g_option_table[];

        // Instance variables to hold the values for command options.

        ProcessAttachInfo attach_info;
    };

    CommandObjectProcessAttach (CommandInterpreter &interpreter) :
        CommandObjectProcessLaunchOrAttach (interpreter,
                                            "process attach",
                                            "Attach to a process.",
                                            "process attach <cmd-options>",
                                            0,
                                            "attach"),
        m_options (interpreter)
    {
    }

    ~CommandObjectProcessAttach ()
    {
    }

    Options *
    GetOptions ()
    {
        return &m_options;
    }

protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        Target *target = m_interpreter.GetDebugger().GetSelectedTarget().get();
        // N.B. The attach should be synchronous.  It doesn't help much to get the prompt back between initiating the attach
        // and the target actually stopping.  So even if the interpreter is set to be asynchronous, we wait for the stop
        // ourselves here.
        
        StateType state = eStateInvalid;
        Process *process = m_exe_ctx.GetProcessPtr();
        
        if (!StopProcessIfNecessary (process, state, result))
            return false;
        
        if (target == NULL)
        {
            // If there isn't a current target create one.
            TargetSP new_target_sp;
            Error error;
            
            error = m_interpreter.GetDebugger().GetTargetList().CreateTarget (m_interpreter.GetDebugger(), 
                                                                              NULL,
                                                                              NULL, 
                                                                              false,
                                                                              NULL, // No platform options
                                                                              new_target_sp);
            target = new_target_sp.get();
            if (target == NULL || error.Fail())
            {
                result.AppendError(error.AsCString("Error creating target"));
                return false;
            }
            m_interpreter.GetDebugger().GetTargetList().SetSelectedTarget(target);
        }
        
        // Record the old executable module, we want to issue a warning if the process of attaching changed the
        // current executable (like somebody said "file foo" then attached to a PID whose executable was bar.)
         
        ModuleSP old_exec_module_sp = target->GetExecutableModule();
        ArchSpec old_arch_spec = target->GetArchitecture();

        if (command.GetArgumentCount())
        {
            result.AppendErrorWithFormat("Invalid arguments for '%s'.\nUsage: %s\n", m_cmd_name.c_str(), m_cmd_syntax.c_str());
            result.SetStatus (eReturnStatusFailed);
        }
        else
        {
            if (state != eStateConnected)
            {
                const char *plugin_name = m_options.attach_info.GetProcessPluginName();
                process = target->CreateProcess (m_interpreter.GetDebugger().GetListener(), plugin_name, NULL).get();
            }

            if (process)
            {
                Error error;
                // If no process info was specified, then use the target executable 
                // name as the process to attach to by default
                if (!m_options.attach_info.ProcessInfoSpecified ())
                {
                    if (old_exec_module_sp)
                        m_options.attach_info.GetExecutableFile().GetFilename() = old_exec_module_sp->GetPlatformFileSpec().GetFilename();

                    if (!m_options.attach_info.ProcessInfoSpecified ())
                    {
                        error.SetErrorString ("no process specified, create a target with a file, or specify the --pid or --name command option");
                    }
                }

                if (error.Success())
                {
                    error = process->Attach (m_options.attach_info);
                    
                    if (error.Success())
                    {
                        result.SetStatus (eReturnStatusSuccessContinuingNoResult);
                    }
                    else
                    {
                        result.AppendErrorWithFormat ("attach failed: %s\n", error.AsCString());
                        result.SetStatus (eReturnStatusFailed);
                        return false;                
                    }
                    // If we're synchronous, wait for the stopped event and report that.
                    // Otherwise just return.  
                    // FIXME: in the async case it will now be possible to get to the command
                    // interpreter with a state eStateAttaching.  Make sure we handle that correctly.
                    StateType state = process->WaitForProcessToStop (NULL);
                    
                    result.SetDidChangeProcessState (true);

                    if (state == eStateStopped)
                    {
                        result.AppendMessageWithFormat ("Process %" PRIu64 " %s\n", process->GetID(), StateAsCString (state));
                        result.SetStatus (eReturnStatusSuccessFinishNoResult);
                    }
                    else
                    {
                        result.AppendError ("attach failed: process did not stop (no such process or permission problem?)");
                        process->Destroy();
                        result.SetStatus (eReturnStatusFailed);
                        return false;                
                    }
                }
            }
        }
        
        if (result.Succeeded())
        {
            // Okay, we're done.  Last step is to warn if the executable module has changed:
            char new_path[PATH_MAX];
            ModuleSP new_exec_module_sp (target->GetExecutableModule());
            if (!old_exec_module_sp)
            {
                // We might not have a module if we attached to a raw pid...
                if (new_exec_module_sp)
                {
                    new_exec_module_sp->GetFileSpec().GetPath(new_path, PATH_MAX);
                    result.AppendMessageWithFormat("Executable module set to \"%s\".\n", new_path);
                }
            }
            else if (old_exec_module_sp->GetFileSpec() != new_exec_module_sp->GetFileSpec())
            {
                char old_path[PATH_MAX];
                
                old_exec_module_sp->GetFileSpec().GetPath (old_path, PATH_MAX);
                new_exec_module_sp->GetFileSpec().GetPath (new_path, PATH_MAX);
                
                result.AppendWarningWithFormat("Executable module changed from \"%s\" to \"%s\".\n",
                                                    old_path, new_path);
            }
            
            if (!old_arch_spec.IsValid())
            {
                result.AppendMessageWithFormat ("Architecture set to: %s.\n", target->GetArchitecture().GetTriple().getTriple().c_str());
            }
            else if (!old_arch_spec.IsExactMatch(target->GetArchitecture()))
            {
                result.AppendWarningWithFormat("Architecture changed from %s to %s.\n", 
                                               old_arch_spec.GetTriple().getTriple().c_str(),
                                               target->GetArchitecture().GetTriple().getTriple().c_str());
            }

            // This supports the use-case scenario of immediately continuing the process once attached.
            if (m_options.attach_info.GetContinueOnceAttached())
                m_interpreter.HandleCommand("process continue", eLazyBoolNo, result);
        }
        return result.Succeeded();
    }
    
    CommandOptions m_options;
};


OptionDefinition
CommandObjectProcessAttach::CommandOptions::g_option_table[] =
{
{ LLDB_OPT_SET_ALL, false, "continue",'c', no_argument,         NULL, 0, eArgTypeNone,         "Immediately continue the process once attached."},
{ LLDB_OPT_SET_ALL, false, "plugin",  'P', required_argument,   NULL, 0, eArgTypePlugin,       "Name of the process plugin you want to use."},
{ LLDB_OPT_SET_1,   false, "pid",     'p', required_argument,   NULL, 0, eArgTypePid,          "The process ID of an existing process to attach to."},
{ LLDB_OPT_SET_2,   false, "name",    'n', required_argument,   NULL, 0, eArgTypeProcessName,  "The name of the process to attach to."},
{ LLDB_OPT_SET_2,   false, "include-existing", 'i', no_argument, NULL, 0, eArgTypeNone,         "Include existing processes when doing attach -w."},
{ LLDB_OPT_SET_2,   false, "waitfor", 'w', no_argument,         NULL, 0, eArgTypeNone,         "Wait for the process with <process-name> to launch."},
{ 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectProcessContinue
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessContinue

class CommandObjectProcessContinue : public CommandObjectParsed
{
public:

    CommandObjectProcessContinue (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process continue",
                             "Continue execution of all threads in the current process.",
                             "process continue",
                             eFlagRequiresProcess       |
                             eFlagTryTargetAPILock      |
                             eFlagProcessMustBeLaunched |
                             eFlagProcessMustBePaused   ),
        m_options(interpreter)
    {
    }


    ~CommandObjectProcessContinue ()
    {
    }

protected:

    class CommandOptions : public Options
    {
    public:

        CommandOptions (CommandInterpreter &interpreter) :
            Options(interpreter)
        {
            // Keep default values of all options in one place: OptionParsingStarting ()
            OptionParsingStarting ();
        }

        ~CommandOptions ()
        {
        }

        Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            bool success = false;
            switch (short_option)
            {
                case 'i':
                    m_ignore = Args::StringToUInt32 (option_arg, 0, 0, &success);
                    if (!success)
                        error.SetErrorStringWithFormat ("invalid value for ignore option: \"%s\", should be a number.", option_arg);
                    break;

                default:
                    error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                    break;
            }
            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_ignore = 0;
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.

        static OptionDefinition g_option_table[];

        uint32_t m_ignore;
    };
    
    bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();
        bool synchronous_execution = m_interpreter.GetSynchronous ();
        StateType state = process->GetState();
        if (state == eStateStopped)
        {
            if (command.GetArgumentCount() != 0)
            {
                result.AppendErrorWithFormat ("The '%s' command does not take any arguments.\n", m_cmd_name.c_str());
                result.SetStatus (eReturnStatusFailed);
                return false;
            }

            if (m_options.m_ignore > 0)
            {
                ThreadSP sel_thread_sp(process->GetThreadList().GetSelectedThread());
                if (sel_thread_sp)
                {
                    StopInfoSP stop_info_sp = sel_thread_sp->GetStopInfo();
                    if (stop_info_sp && stop_info_sp->GetStopReason() == eStopReasonBreakpoint)
                    {
                        lldb::break_id_t bp_site_id = (lldb::break_id_t)stop_info_sp->GetValue();
                        BreakpointSiteSP bp_site_sp(process->GetBreakpointSiteList().FindByID(bp_site_id));
                        if (bp_site_sp)
                        {
                            const size_t num_owners = bp_site_sp->GetNumberOfOwners();
                            for (size_t i = 0; i < num_owners; i++)
                            {
                                Breakpoint &bp_ref = bp_site_sp->GetOwnerAtIndex(i)->GetBreakpoint();
                                if (!bp_ref.IsInternal())
                                {
                                    bp_ref.SetIgnoreCount(m_options.m_ignore);
                                }
                            }
                        }
                    }
                }
            }
            
            {  // Scope for thread list mutex:
                Mutex::Locker locker (process->GetThreadList().GetMutex());
                const uint32_t num_threads = process->GetThreadList().GetSize();

                // Set the actions that the threads should each take when resuming
                for (uint32_t idx=0; idx<num_threads; ++idx)
                {
                    process->GetThreadList().GetThreadAtIndex(idx)->SetResumeState (eStateRunning);
                }
            }
            
            Error error(process->Resume());
            if (error.Success())
            {
                result.AppendMessageWithFormat ("Process %" PRIu64 " resuming\n", process->GetID());
                if (synchronous_execution)
                {
                    state = process->WaitForProcessToStop (NULL);

                    result.SetDidChangeProcessState (true);
                    result.AppendMessageWithFormat ("Process %" PRIu64 " %s\n", process->GetID(), StateAsCString (state));
                    result.SetStatus (eReturnStatusSuccessFinishNoResult);
                }
                else
                {
                    result.SetStatus (eReturnStatusSuccessContinuingNoResult);
                }
            }
            else
            {
                result.AppendErrorWithFormat("Failed to resume process: %s.\n", error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendErrorWithFormat ("Process cannot be continued from its current state (%s).\n",
                                         StateAsCString(state));
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }

    Options *
    GetOptions ()
    {
        return &m_options;
    }
    
    CommandOptions m_options;

};

OptionDefinition
CommandObjectProcessContinue::CommandOptions::g_option_table[] =
{
{ LLDB_OPT_SET_ALL, false, "ignore-count",'i', required_argument,         NULL, 0, eArgTypeUnsignedInteger,
                           "Ignore <N> crossings of the breakpoint (if it exists) for the currently selected thread."},
{ 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectProcessDetach
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessDetach

class CommandObjectProcessDetach : public CommandObjectParsed
{
public:
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter)
        {
            OptionParsingStarting ();
        }

        ~CommandOptions ()
        {
        }

        Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            
            switch (short_option)
            {
                case 's':
                    bool tmp_result;
                    bool success;
                    tmp_result = Args::StringToBoolean(option_arg, false, &success);
                    if (!success)
                        error.SetErrorStringWithFormat("invalid boolean option: \"%s\"", option_arg);
                    else
                    {
                        if (tmp_result)
                            m_keep_stopped = eLazyBoolYes;
                        else
                            m_keep_stopped = eLazyBoolNo;
                    }
                    break;
                default:
                    error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                    break;
            }
            return error;
        }

        void
        OptionParsingStarting ()
        {
            m_keep_stopped = eLazyBoolCalculate;
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.

        static OptionDefinition g_option_table[];

        // Instance variables to hold the values for command options.
        LazyBool m_keep_stopped;
    };

    CommandObjectProcessDetach (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process detach",
                             "Detach from the current process being debugged.",
                             "process detach",
                             eFlagRequiresProcess      |
                             eFlagTryTargetAPILock     |
                             eFlagProcessMustBeLaunched),
        m_options(interpreter)
    {
    }

    ~CommandObjectProcessDetach ()
    {
    }

    Options *
    GetOptions ()
    {
        return &m_options;
    }


protected:
    bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();
        result.AppendMessageWithFormat ("Detaching from process %" PRIu64 "\n", process->GetID());
        // FIXME: This will be a Command Option:
        bool keep_stopped;
        if (m_options.m_keep_stopped == eLazyBoolCalculate)
        {
            // Check the process default:
            if (process->GetDetachKeepsStopped())
                keep_stopped = true;
            else
                keep_stopped = false;
        }
        else if (m_options.m_keep_stopped == eLazyBoolYes)
            keep_stopped = true;
        else
            keep_stopped = false;
        
        Error error (process->Detach(keep_stopped));
        if (error.Success())
        {
            result.SetStatus (eReturnStatusSuccessFinishResult);
        }
        else
        {
            result.AppendErrorWithFormat ("Detach failed: %s\n", error.AsCString());
            result.SetStatus (eReturnStatusFailed);
            return false;
        }
        return result.Succeeded();
    }

    CommandOptions m_options;
};

OptionDefinition
CommandObjectProcessDetach::CommandOptions::g_option_table[] =
{
{ LLDB_OPT_SET_1, false, "keep-stopped",   's', required_argument, NULL, 0, eArgTypeBoolean, "Whether or not the process should be kept stopped on detach (if possible)." },
{ 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectProcessConnect
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessConnect

class CommandObjectProcessConnect : public CommandObjectParsed
{
public:
    
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
            Options(interpreter)
        {
            // Keep default values of all options in one place: OptionParsingStarting ()
            OptionParsingStarting ();
        }
        
        ~CommandOptions ()
        {
        }
        
        Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            
            switch (short_option)
            {
            case 'p':   
                plugin_name.assign (option_arg);    
                break;

            default:
                error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                break;
            }
            return error;
        }
        
        void
        OptionParsingStarting ()
        {
            plugin_name.clear();
        }
        
        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        
        std::string plugin_name;        
    };

    CommandObjectProcessConnect (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process connect",
                             "Connect to a remote debug service.",
                             "process connect <remote-url>",
                             0),
        m_options (interpreter)
    {
    }
    
    ~CommandObjectProcessConnect ()
    {
    }

    
    Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        
        TargetSP target_sp (m_interpreter.GetDebugger().GetSelectedTarget());
        Error error;        
        Process *process = m_exe_ctx.GetProcessPtr();
        if (process)
        {
            if (process->IsAlive())
            {
                result.AppendErrorWithFormat ("Process %" PRIu64 " is currently being debugged, kill the process before connecting.\n",
                                              process->GetID());
                result.SetStatus (eReturnStatusFailed);
                return false;
            }
        }
        
        if (!target_sp)
        {
            // If there isn't a current target create one.
            
            error = m_interpreter.GetDebugger().GetTargetList().CreateTarget (m_interpreter.GetDebugger(), 
                                                                              NULL,
                                                                              NULL, 
                                                                              false,
                                                                              NULL, // No platform options
                                                                              target_sp);
            if (!target_sp || error.Fail())
            {
                result.AppendError(error.AsCString("Error creating target"));
                result.SetStatus (eReturnStatusFailed);
                return false;
            }
            m_interpreter.GetDebugger().GetTargetList().SetSelectedTarget(target_sp.get());
        }
        
        if (command.GetArgumentCount() == 1)
        {
            const char *plugin_name = NULL;
            if (!m_options.plugin_name.empty())
                plugin_name = m_options.plugin_name.c_str();

            const char *remote_url = command.GetArgumentAtIndex(0);
            process = target_sp->CreateProcess (m_interpreter.GetDebugger().GetListener(), plugin_name, NULL).get();
            
            if (process)
            {
                error = process->ConnectRemote (&process->GetTarget().GetDebugger().GetOutputStream(), remote_url);

                if (error.Fail())
                {
                    result.AppendError(error.AsCString("Remote connect failed"));
                    result.SetStatus (eReturnStatusFailed);
                    target_sp->DeleteCurrentProcess();
                    return false;
                }
            }
            else
            {
                result.AppendErrorWithFormat ("Unable to find process plug-in for remote URL '%s'.\nPlease specify a process plug-in name with the --plugin option, or specify an object file using the \"file\" command.\n", 
                                              remote_url);
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendErrorWithFormat ("'%s' takes exactly one argument:\nUsage: %s\n", 
                                          m_cmd_name.c_str(),
                                          m_cmd_syntax.c_str());
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    
    CommandOptions m_options;
};

OptionDefinition
CommandObjectProcessConnect::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_ALL, false, "plugin", 'p', required_argument, NULL, 0, eArgTypePlugin, "Name of the process plugin you want to use."},
    { 0,                false, NULL,      0 , 0,                 NULL, 0, eArgTypeNone,   NULL }
};

//-------------------------------------------------------------------------
// CommandObjectProcessPlugin
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessPlugin

class CommandObjectProcessPlugin : public CommandObjectProxy
{
public:
    
    CommandObjectProcessPlugin (CommandInterpreter &interpreter) :
        CommandObjectProxy (interpreter,
                            "process plugin",
                            "Send a custom command to the current process plug-in.",
                            "process plugin <args>",
                            0)
    {
    }
    
    ~CommandObjectProcessPlugin ()
    {
    }

    virtual CommandObject *
    GetProxyCommandObject()
    {
        Process *process = m_interpreter.GetExecutionContext().GetProcessPtr();
        if (process)
            return process->GetPluginCommandObject();
        return NULL;
    }
};


//-------------------------------------------------------------------------
// CommandObjectProcessLoad
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessLoad

class CommandObjectProcessLoad : public CommandObjectParsed
{
public:

    CommandObjectProcessLoad (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process load",
                             "Load a shared library into the current process.",
                             "process load <filename> [<filename> ...]",
                             eFlagRequiresProcess       |
                             eFlagTryTargetAPILock      |
                             eFlagProcessMustBeLaunched |
                             eFlagProcessMustBePaused   )
    {
    }

    ~CommandObjectProcessLoad ()
    {
    }

protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();

        const size_t argc = command.GetArgumentCount();
        
        for (uint32_t i=0; i<argc; ++i)
        {
            Error error;
            const char *image_path = command.GetArgumentAtIndex(i);
            FileSpec image_spec (image_path, false);
            process->GetTarget().GetPlatform()->ResolveRemotePath(image_spec, image_spec);
            uint32_t image_token = process->LoadImage(image_spec, error);
            if (image_token != LLDB_INVALID_IMAGE_TOKEN)
            {
                result.AppendMessageWithFormat ("Loading \"%s\"...ok\nImage %u loaded.\n", image_path, image_token);  
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendErrorWithFormat ("failed to load '%s': %s", image_path, error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        return result.Succeeded();
    }
};


//-------------------------------------------------------------------------
// CommandObjectProcessUnload
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessUnload

class CommandObjectProcessUnload : public CommandObjectParsed
{
public:

    CommandObjectProcessUnload (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process unload",
                             "Unload a shared library from the current process using the index returned by a previous call to \"process load\".",
                             "process unload <index>",
                             eFlagRequiresProcess       |
                             eFlagTryTargetAPILock      |
                             eFlagProcessMustBeLaunched |
                             eFlagProcessMustBePaused   )
    {
    }

    ~CommandObjectProcessUnload ()
    {
    }

protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();

        const size_t argc = command.GetArgumentCount();
        
        for (uint32_t i=0; i<argc; ++i)
        {
            const char *image_token_cstr = command.GetArgumentAtIndex(i);
            uint32_t image_token = Args::StringToUInt32(image_token_cstr, LLDB_INVALID_IMAGE_TOKEN, 0);
            if (image_token == LLDB_INVALID_IMAGE_TOKEN)
            {
                result.AppendErrorWithFormat ("invalid image index argument '%s'", image_token_cstr);
                result.SetStatus (eReturnStatusFailed);
                break;
            }
            else
            {
                Error error (process->UnloadImage(image_token));
                if (error.Success())
                {
                    result.AppendMessageWithFormat ("Unloading shared library with index %u...ok\n", image_token);  
                    result.SetStatus (eReturnStatusSuccessFinishResult);
                }
                else
                {
                    result.AppendErrorWithFormat ("failed to unload image: %s", error.AsCString());
                    result.SetStatus (eReturnStatusFailed);
                    break;
                }
            }
        }
        return result.Succeeded();
    }
};

//-------------------------------------------------------------------------
// CommandObjectProcessSignal
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessSignal

class CommandObjectProcessSignal : public CommandObjectParsed
{
public:

    CommandObjectProcessSignal (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process signal",
                             "Send a UNIX signal to the current process being debugged.",
                             NULL,
                             eFlagRequiresProcess | eFlagTryTargetAPILock)
    {
        CommandArgumentEntry arg;
        CommandArgumentData signal_arg;
        
        // Define the first (and only) variant of this arg.
        signal_arg.arg_type = eArgTypeUnixSignal;
        signal_arg.arg_repetition = eArgRepeatPlain;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg.push_back (signal_arg);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg);
    }

    ~CommandObjectProcessSignal ()
    {
    }

protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();

        if (command.GetArgumentCount() == 1)
        {
            int signo = LLDB_INVALID_SIGNAL_NUMBER;
            
            const char *signal_name = command.GetArgumentAtIndex(0);
            if (::isxdigit (signal_name[0]))
                signo = Args::StringToSInt32(signal_name, LLDB_INVALID_SIGNAL_NUMBER, 0);
            else
                signo = process->GetUnixSignals().GetSignalNumberFromName (signal_name);
            
            if (signo == LLDB_INVALID_SIGNAL_NUMBER)
            {
                result.AppendErrorWithFormat ("Invalid signal argument '%s'.\n", command.GetArgumentAtIndex(0));
                result.SetStatus (eReturnStatusFailed);
            }
            else
            {
                Error error (process->Signal (signo));
                if (error.Success())
                {
                    result.SetStatus (eReturnStatusSuccessFinishResult);
                }
                else
                {
                    result.AppendErrorWithFormat ("Failed to send signal %i: %s\n", signo, error.AsCString());
                    result.SetStatus (eReturnStatusFailed);
                }
            }
        }
        else
        {
            result.AppendErrorWithFormat("'%s' takes exactly one signal number argument:\nUsage: %s\n", m_cmd_name.c_str(),
                                        m_cmd_syntax.c_str());
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};


//-------------------------------------------------------------------------
// CommandObjectProcessInterrupt
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessInterrupt

class CommandObjectProcessInterrupt : public CommandObjectParsed
{
public:


    CommandObjectProcessInterrupt (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process interrupt",
                             "Interrupt the current process being debugged.",
                             "process interrupt",
                             eFlagRequiresProcess      |
                             eFlagTryTargetAPILock     |
                             eFlagProcessMustBeLaunched)
    {
    }

    ~CommandObjectProcessInterrupt ()
    {
    }

protected:
    bool
    DoExecute (Args& command,
               CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();
        if (process == NULL)
        {
            result.AppendError ("no process to halt");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        if (command.GetArgumentCount() == 0)
        {
            bool clear_thread_plans = true;
            Error error(process->Halt (clear_thread_plans));
            if (error.Success())
            {
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendErrorWithFormat ("Failed to halt process: %s\n", error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendErrorWithFormat("'%s' takes no arguments:\nUsage: %s\n",
                                        m_cmd_name.c_str(),
                                        m_cmd_syntax.c_str());
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//-------------------------------------------------------------------------
// CommandObjectProcessKill
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessKill

class CommandObjectProcessKill : public CommandObjectParsed
{
public:

    CommandObjectProcessKill (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "process kill",
                             "Terminate the current process being debugged.",
                             "process kill",
                             eFlagRequiresProcess      |
                             eFlagTryTargetAPILock     |
                             eFlagProcessMustBeLaunched)
    {
    }

    ~CommandObjectProcessKill ()
    {
    }

protected:
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        Process *process = m_exe_ctx.GetProcessPtr();
        if (process == NULL)
        {
            result.AppendError ("no process to kill");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        if (command.GetArgumentCount() == 0)
        {
            Error error (process->Destroy());
            if (error.Success())
            {
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendErrorWithFormat ("Failed to kill process: %s\n", error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendErrorWithFormat("'%s' takes no arguments:\nUsage: %s\n",
                                        m_cmd_name.c_str(),
                                        m_cmd_syntax.c_str());
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//-------------------------------------------------------------------------
// CommandObjectProcessStatus
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessStatus

class CommandObjectProcessStatus : public CommandObjectParsed
{
public:
    CommandObjectProcessStatus (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "process status",
                             "Show the current status and location of executing process.",
                             "process status",
                             eFlagRequiresProcess | eFlagTryTargetAPILock)
    {
    }

    ~CommandObjectProcessStatus()
    {
    }


    bool
    DoExecute (Args& command, CommandReturnObject &result)
    {
        Stream &strm = result.GetOutputStream();
        result.SetStatus (eReturnStatusSuccessFinishNoResult);
        // No need to check "process" for validity as eFlagRequiresProcess ensures it is valid        
        Process *process = m_exe_ctx.GetProcessPtr();
        const bool only_threads_with_stop_reason = true;
        const uint32_t start_frame = 0;
        const uint32_t num_frames = 1;
        const uint32_t num_frames_with_source = 1;
        process->GetStatus(strm);
        process->GetThreadStatus (strm, 
                                  only_threads_with_stop_reason, 
                                  start_frame,
                                  num_frames,
                                  num_frames_with_source);
        return result.Succeeded();
    }
};

//-------------------------------------------------------------------------
// CommandObjectProcessHandle
//-------------------------------------------------------------------------
#pragma mark CommandObjectProcessHandle

class CommandObjectProcessHandle : public CommandObjectParsed
{
public:

    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter)
        {
            OptionParsingStarting ();
        }

        ~CommandOptions ()
        {
        }

        Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            
            switch (short_option)
            {
                case 's':
                    stop = option_arg;
                    break;
                case 'n':
                    notify = option_arg;
                    break;
                case 'p':
                    pass = option_arg;
                    break;
                default:
                    error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                    break;
            }
            return error;
        }

        void
        OptionParsingStarting ()
        {
            stop.clear();
            notify.clear();
            pass.clear();
        }

        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }

        // Options table: Required for subclasses of Options.

        static OptionDefinition g_option_table[];

        // Instance variables to hold the values for command options.

        std::string stop;
        std::string notify;
        std::string pass;
    };


    CommandObjectProcessHandle (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "process handle",
                             "Show or update what the process and debugger should do with various signals received from the OS.",
                             NULL),
        m_options (interpreter)
    {
        SetHelpLong ("If no signals are specified, update them all.  If no update option is specified, list the current values.\n");
        CommandArgumentEntry arg;
        CommandArgumentData signal_arg;

        signal_arg.arg_type = eArgTypeUnixSignal;
        signal_arg.arg_repetition = eArgRepeatStar;

        arg.push_back (signal_arg);
        
        m_arguments.push_back (arg);
    }

    ~CommandObjectProcessHandle ()
    {
    }

    Options *
    GetOptions ()
    {
        return &m_options;
    }

    bool
    VerifyCommandOptionValue (const std::string &option, int &real_value)
    {
        bool okay = true;

        bool success = false;
        bool tmp_value = Args::StringToBoolean (option.c_str(), false, &success);

        if (success && tmp_value)
            real_value = 1;
        else if (success && !tmp_value)
            real_value = 0;
        else
        {
            // If the value isn't 'true' or 'false', it had better be 0 or 1.
            real_value = Args::StringToUInt32 (option.c_str(), 3);
            if (real_value != 0 && real_value != 1)
                okay = false;
        }
        
        return okay;
    }

    void
    PrintSignalHeader (Stream &str)
    {
        str.Printf ("NAME        PASS   STOP   NOTIFY\n");
        str.Printf ("==========  =====  =====  ======\n");
    }  

    void
    PrintSignal (Stream &str, int32_t signo, const char *sig_name, UnixSignals &signals)
    {
        bool stop;
        bool suppress;
        bool notify;

        str.Printf ("%-10s  ", sig_name);
        if (signals.GetSignalInfo (signo, suppress, stop, notify))
        {
            bool pass = !suppress;
            str.Printf ("%s  %s  %s", 
                        (pass ? "true " : "false"), 
                        (stop ? "true " : "false"), 
                        (notify ? "true " : "false"));
        }
        str.Printf ("\n");
    }

    void
    PrintSignalInformation (Stream &str, Args &signal_args, int num_valid_signals, UnixSignals &signals)
    {
        PrintSignalHeader (str);

        if (num_valid_signals > 0)
        {
            size_t num_args = signal_args.GetArgumentCount();
            for (size_t i = 0; i < num_args; ++i)
            {
                int32_t signo = signals.GetSignalNumberFromName (signal_args.GetArgumentAtIndex (i));
                if (signo != LLDB_INVALID_SIGNAL_NUMBER)
                    PrintSignal (str, signo, signal_args.GetArgumentAtIndex (i), signals);
            }
        }
        else // Print info for ALL signals
        {
            int32_t signo = signals.GetFirstSignalNumber(); 
            while (signo != LLDB_INVALID_SIGNAL_NUMBER)
            {
                PrintSignal (str, signo, signals.GetSignalAsCString (signo), signals);
                signo = signals.GetNextSignalNumber (signo);
            }
        }
    }

protected:
    bool
    DoExecute (Args &signal_args, CommandReturnObject &result)
    {
        TargetSP target_sp = m_interpreter.GetDebugger().GetSelectedTarget();
        
        if (!target_sp)
        {
            result.AppendError ("No current target;"
                                " cannot handle signals until you have a valid target and process.\n");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }
        
        ProcessSP process_sp = target_sp->GetProcessSP();

        if (!process_sp)
        {
            result.AppendError ("No current process; cannot handle signals until you have a valid process.\n");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        int stop_action = -1;   // -1 means leave the current setting alone
        int pass_action = -1;   // -1 means leave the current setting alone
        int notify_action = -1; // -1 means leave the current setting alone

        if (! m_options.stop.empty()
            && ! VerifyCommandOptionValue (m_options.stop, stop_action))
        {
            result.AppendError ("Invalid argument for command option --stop; must be true or false.\n");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        if (! m_options.notify.empty()
            && ! VerifyCommandOptionValue (m_options.notify, notify_action))
        {
            result.AppendError ("Invalid argument for command option --notify; must be true or false.\n");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        if (! m_options.pass.empty()
            && ! VerifyCommandOptionValue (m_options.pass, pass_action))
        {
            result.AppendError ("Invalid argument for command option --pass; must be true or false.\n");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }

        size_t num_args = signal_args.GetArgumentCount();
        UnixSignals &signals = process_sp->GetUnixSignals();
        int num_signals_set = 0;

        if (num_args > 0)
        {
            for (size_t i = 0; i < num_args; ++i)
            {
                int32_t signo = signals.GetSignalNumberFromName (signal_args.GetArgumentAtIndex (i));
                if (signo != LLDB_INVALID_SIGNAL_NUMBER)
                {
                    // Casting the actions as bools here should be okay, because VerifyCommandOptionValue guarantees
                    // the value is either 0 or 1.
                    if (stop_action != -1)
                        signals.SetShouldStop (signo, (bool) stop_action);
                    if (pass_action != -1)
                    {
                        bool suppress = ! ((bool) pass_action);
                        signals.SetShouldSuppress (signo, suppress);
                    }
                    if (notify_action != -1)
                        signals.SetShouldNotify (signo, (bool) notify_action);
                    ++num_signals_set;
                }
                else
                {
                    result.AppendErrorWithFormat ("Invalid signal name '%s'\n", signal_args.GetArgumentAtIndex (i));
                }
            }
        }
        else
        {
            // No signal specified, if any command options were specified, update ALL signals.
            if ((notify_action != -1) || (stop_action != -1) || (pass_action != -1))
            {
                if (m_interpreter.Confirm ("Do you really want to update all the signals?", false))
                {
                    int32_t signo = signals.GetFirstSignalNumber();
                    while (signo != LLDB_INVALID_SIGNAL_NUMBER)
                    {
                        if (notify_action != -1)
                            signals.SetShouldNotify (signo, (bool) notify_action);
                        if (stop_action != -1)
                            signals.SetShouldStop (signo, (bool) stop_action);
                        if (pass_action != -1)
                        {
                            bool suppress = ! ((bool) pass_action);
                            signals.SetShouldSuppress (signo, suppress);
                        }
                        signo = signals.GetNextSignalNumber (signo);
                    }
                }
            }
        }

        PrintSignalInformation (result.GetOutputStream(), signal_args, num_signals_set, signals);

        if (num_signals_set > 0)
            result.SetStatus (eReturnStatusSuccessFinishNoResult);
        else
            result.SetStatus (eReturnStatusFailed);

        return result.Succeeded();
    }

    CommandOptions m_options;
};

OptionDefinition
CommandObjectProcessHandle::CommandOptions::g_option_table[] =
{
{ LLDB_OPT_SET_1, false, "stop",   's', required_argument, NULL, 0, eArgTypeBoolean, "Whether or not the process should be stopped if the signal is received." },
{ LLDB_OPT_SET_1, false, "notify", 'n', required_argument, NULL, 0, eArgTypeBoolean, "Whether or not the debugger should notify the user if the signal is received." },
{ LLDB_OPT_SET_1, false, "pass",  'p', required_argument, NULL, 0, eArgTypeBoolean, "Whether or not the signal should be passed to the process." },
{ 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};

//-------------------------------------------------------------------------
// CommandObjectMultiwordProcess
//-------------------------------------------------------------------------

CommandObjectMultiwordProcess::CommandObjectMultiwordProcess (CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "process",
                            "A set of commands for operating on a process.",
                            "process <subcommand> [<subcommand-options>]")
{
    LoadSubCommand ("attach",      CommandObjectSP (new CommandObjectProcessAttach    (interpreter)));
    LoadSubCommand ("launch",      CommandObjectSP (new CommandObjectProcessLaunch    (interpreter)));
    LoadSubCommand ("continue",    CommandObjectSP (new CommandObjectProcessContinue  (interpreter)));
    LoadSubCommand ("connect",     CommandObjectSP (new CommandObjectProcessConnect   (interpreter)));
    LoadSubCommand ("detach",      CommandObjectSP (new CommandObjectProcessDetach    (interpreter)));
    LoadSubCommand ("load",        CommandObjectSP (new CommandObjectProcessLoad      (interpreter)));
    LoadSubCommand ("unload",      CommandObjectSP (new CommandObjectProcessUnload    (interpreter)));
    LoadSubCommand ("signal",      CommandObjectSP (new CommandObjectProcessSignal    (interpreter)));
    LoadSubCommand ("handle",      CommandObjectSP (new CommandObjectProcessHandle    (interpreter)));
    LoadSubCommand ("status",      CommandObjectSP (new CommandObjectProcessStatus    (interpreter)));
    LoadSubCommand ("interrupt",   CommandObjectSP (new CommandObjectProcessInterrupt (interpreter)));
    LoadSubCommand ("kill",        CommandObjectSP (new CommandObjectProcessKill      (interpreter)));
    LoadSubCommand ("plugin",      CommandObjectSP (new CommandObjectProcessPlugin    (interpreter)));
}

CommandObjectMultiwordProcess::~CommandObjectMultiwordProcess ()
{
}

