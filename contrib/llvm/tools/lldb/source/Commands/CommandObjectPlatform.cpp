//===-- CommandObjectPlatform.cpp -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "CommandObjectPlatform.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Core/DataExtractor.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Interpreter/Args.h"
#include "lldb/Interpreter/CommandInterpreter.h"
#include "lldb/Interpreter/CommandReturnObject.h"
#include "lldb/Interpreter/OptionGroupFile.h"
#include "lldb/Interpreter/OptionGroupPlatform.h"
#include "lldb/Target/ExecutionContext.h"
#include "lldb/Target/Platform.h"
#include "lldb/Target/Process.h"
#include "lldb/Utility/Utils.h"

using namespace lldb;
using namespace lldb_private;

static mode_t
ParsePermissionString(const char* permissions)
{
    if (strlen(permissions) != 9)
        return (mode_t)(-1);
    bool user_r,user_w,user_x,
    group_r,group_w,group_x,
    world_r,world_w,world_x;
    
    user_r = (permissions[0] == 'r');
    user_w = (permissions[1] == 'w');
    user_x = (permissions[2] == 'x');
    
    group_r = (permissions[3] == 'r');
    group_w = (permissions[4] == 'w');
    group_x = (permissions[5] == 'x');
    
    world_r = (permissions[6] == 'r');
    world_w = (permissions[7] == 'w');
    world_x = (permissions[8] == 'x');
    
    mode_t user,group,world;
    user = (user_r ? 4 : 0) | (user_w ? 2 : 0) | (user_x ? 1 : 0);
    group = (group_r ? 4 : 0) | (group_w ? 2 : 0) | (group_x ? 1 : 0);
    world = (world_r ? 4 : 0) | (world_w ? 2 : 0) | (world_x ? 1 : 0);
    
    return user | group | world;
}

static OptionDefinition
g_permissions_options[] =
{
    {   LLDB_OPT_SET_ALL, false, "permissions-value", 'v', OptionParser::eRequiredArgument,       NULL, 0, eArgTypePermissionsNumber         , "Give out the numeric value for permissions (e.g. 757)" },
    {   LLDB_OPT_SET_ALL, false, "permissions-string",'s', OptionParser::eRequiredArgument, NULL, 0, eArgTypePermissionsString  , "Give out the string value for permissions (e.g. rwxr-xr--)." },
    {   LLDB_OPT_SET_ALL, false, "user-read", 'r', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow user to read." },
    {   LLDB_OPT_SET_ALL, false, "user-write", 'w', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow user to write." },
    {   LLDB_OPT_SET_ALL, false, "user-exec", 'x', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow user to execute." },

    {   LLDB_OPT_SET_ALL, false, "group-read", 'R', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow group to read." },
    {   LLDB_OPT_SET_ALL, false, "group-write", 'W', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow group to write." },
    {   LLDB_OPT_SET_ALL, false, "group-exec", 'X', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow group to execute." },

    {   LLDB_OPT_SET_ALL, false, "world-read", 'd', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow world to read." },
    {   LLDB_OPT_SET_ALL, false, "world-write", 't', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow world to write." },
    {   LLDB_OPT_SET_ALL, false, "world-exec", 'e', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone         , "Allow world to execute." },

};

class OptionPermissions : public lldb_private::OptionGroup
{
public:
    OptionPermissions ()
    {
    }
    
    virtual
    ~OptionPermissions ()
    {
    }
    
    virtual lldb_private::Error
    SetOptionValue (CommandInterpreter &interpreter,
                    uint32_t option_idx,
                    const char *option_arg)
    {
        Error error;
        char short_option = (char) GetDefinitions()[option_idx].short_option;
        switch (short_option)
        {
            case 'v':
            {
                bool ok;
                uint32_t perms = Args::StringToUInt32(option_arg, 777, 8, &ok);
                if (!ok)
                    error.SetErrorStringWithFormat("invalid value for permissions: %s", option_arg);
                else
                    m_permissions = perms;
            }
                break;
            case 's':
            {
                mode_t perms = ParsePermissionString(option_arg);
                if (perms == (mode_t)-1)
                    error.SetErrorStringWithFormat("invalid value for permissions: %s", option_arg);
                else
                    m_permissions = perms;
            }
            case 'r':
                m_permissions |= lldb::eFilePermissionsUserRead;
                break;
            case 'w':
                m_permissions |= lldb::eFilePermissionsUserWrite;
                break;
            case 'x':
                m_permissions |= lldb::eFilePermissionsUserExecute;
                break;
            case 'R':
                m_permissions |= lldb::eFilePermissionsGroupRead;
                break;
            case 'W':
                m_permissions |= lldb::eFilePermissionsGroupWrite;
                break;
            case 'X':
                m_permissions |= lldb::eFilePermissionsGroupExecute;
                break;
            case 'd':
                m_permissions |= lldb::eFilePermissionsWorldRead;
                break;
            case 't':
                m_permissions |= lldb::eFilePermissionsWorldWrite;
                break;
            case 'e':
                m_permissions |= lldb::eFilePermissionsWorldExecute;
                break;

            default:
                error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                break;
        }
        
        return error;
    }
    
    void
    OptionParsingStarting (CommandInterpreter &interpreter)
    {
        m_permissions = 0;
    }
    
    virtual uint32_t
    GetNumDefinitions ()
    {
        return llvm::array_lengthof(g_permissions_options);
    }
    
    const lldb_private::OptionDefinition*
    GetDefinitions ()
    {
        return g_permissions_options;
    }
        
    // Instance variables to hold the values for command options.
    
    uint32_t m_permissions;
private:
    DISALLOW_COPY_AND_ASSIGN(OptionPermissions);
};

//----------------------------------------------------------------------
// "platform select <platform-name>"
//----------------------------------------------------------------------
class CommandObjectPlatformSelect : public CommandObjectParsed
{
public:
    CommandObjectPlatformSelect (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "platform select",
                             "Create a platform if needed and select it as the current platform.",
                             "platform select <platform-name>",
                             0),
        m_option_group (interpreter),
        m_platform_options (false) // Don't include the "--platform" option by passing false
    {
        m_option_group.Append (&m_platform_options, LLDB_OPT_SET_ALL, 1);
        m_option_group.Finalize();
    }

    virtual
    ~CommandObjectPlatformSelect ()
    {
    }

    virtual int
    HandleCompletion (Args &input,
                      int &cursor_index,
                      int &cursor_char_position,
                      int match_start_point,
                      int max_return_elements,
                      bool &word_complete,
                      StringList &matches)
    {
        std::string completion_str (input.GetArgumentAtIndex(cursor_index));
        completion_str.erase (cursor_char_position);
        
        CommandCompletions::PlatformPluginNames (m_interpreter, 
                                                 completion_str.c_str(),
                                                 match_start_point,
                                                 max_return_elements,
                                                 NULL,
                                                 word_complete,
                                                 matches);
        return matches.GetSize();
    }

    virtual Options *
    GetOptions ()
    {
        return &m_option_group;
    }

protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        if (args.GetArgumentCount() == 1)
        {
            const char *platform_name = args.GetArgumentAtIndex (0);
            if (platform_name && platform_name[0])
            {
                const bool select = true;
                m_platform_options.SetPlatformName (platform_name);
                Error error;
                ArchSpec platform_arch;
                PlatformSP platform_sp (m_platform_options.CreatePlatformWithOptions (m_interpreter, ArchSpec(), select, error, platform_arch));
                if (platform_sp)
                {
                    platform_sp->GetStatus (result.GetOutputStream());
                    result.SetStatus (eReturnStatusSuccessFinishResult);
                }
                else
                {
                    result.AppendError(error.AsCString());
                    result.SetStatus (eReturnStatusFailed);
                }
            }
            else
            {
                result.AppendError ("invalid platform name");
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("platform create takes a platform name as an argument\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }

    OptionGroupOptions m_option_group;
    OptionGroupPlatform m_platform_options;
};

//----------------------------------------------------------------------
// "platform list"
//----------------------------------------------------------------------
class CommandObjectPlatformList : public CommandObjectParsed
{
public:
    CommandObjectPlatformList (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "platform list",
                             "List all platforms that are available.",
                             NULL,
                             0)
    {
    }

    virtual
    ~CommandObjectPlatformList ()
    {
    }

protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Stream &ostrm = result.GetOutputStream();
        ostrm.Printf("Available platforms:\n");
        
        PlatformSP host_platform_sp (Platform::GetDefaultPlatform());
        ostrm.Printf ("%s: %s\n", 
                      host_platform_sp->GetPluginName().GetCString(),
                      host_platform_sp->GetDescription());

        uint32_t idx;
        for (idx = 0; 1; ++idx)
        {
            const char *plugin_name = PluginManager::GetPlatformPluginNameAtIndex (idx);
            if (plugin_name == NULL)
                break;
            const char *plugin_desc = PluginManager::GetPlatformPluginDescriptionAtIndex (idx);
            if (plugin_desc == NULL)
                break;
            ostrm.Printf("%s: %s\n", plugin_name, plugin_desc);
        }
        
        if (idx == 0)
        {
            result.AppendError ("no platforms are available\n");
            result.SetStatus (eReturnStatusFailed);
        }
        else
            result.SetStatus (eReturnStatusSuccessFinishResult);
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform status"
//----------------------------------------------------------------------
class CommandObjectPlatformStatus : public CommandObjectParsed
{
public:
    CommandObjectPlatformStatus (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "platform status",
                             "Display status for the currently selected platform.",
                             NULL,
                             0)
    {
    }

    virtual
    ~CommandObjectPlatformStatus ()
    {
    }

protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Stream &ostrm = result.GetOutputStream();      
        
        Target *target = m_interpreter.GetDebugger().GetSelectedTarget().get();
        PlatformSP platform_sp;
        if (target)
        {
            platform_sp = target->GetPlatform();
        }
        if (!platform_sp)
        {
            platform_sp = m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform();
        }
        if (platform_sp)
        {
            platform_sp->GetStatus (ostrm);
            result.SetStatus (eReturnStatusSuccessFinishResult);            
        }
        else
        {
            result.AppendError ("no platform us currently selected\n");
            result.SetStatus (eReturnStatusFailed);            
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform connect <connect-url>"
//----------------------------------------------------------------------
class CommandObjectPlatformConnect : public CommandObjectParsed
{
public:
    CommandObjectPlatformConnect (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "platform connect",
                             "Connect a platform by name to be the currently selected platform.",
                             "platform connect <connect-url>",
                             0)
    {
    }

    virtual
    ~CommandObjectPlatformConnect ()
    {
    }

protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Stream &ostrm = result.GetOutputStream();      
        
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            Error error (platform_sp->ConnectRemote (args));
            if (error.Success())
            {
                platform_sp->GetStatus (ostrm);
                result.SetStatus (eReturnStatusSuccessFinishResult);            
            }
            else
            {
                result.AppendErrorWithFormat ("%s\n", error.AsCString());
                result.SetStatus (eReturnStatusFailed);            
            }
        }
        else
        {
            result.AppendError ("no platform is currently selected\n");
            result.SetStatus (eReturnStatusFailed);            
        }
        return result.Succeeded();
    }
    
    virtual Options *
    GetOptions ()
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        OptionGroupOptions* m_platform_options = NULL;
        if (platform_sp)
        {
            m_platform_options = platform_sp->GetConnectionOptions(m_interpreter);
            if (m_platform_options != NULL && !m_platform_options->m_did_finalize)
                m_platform_options->Finalize();
        }
        return m_platform_options;
    }

};

//----------------------------------------------------------------------
// "platform disconnect"
//----------------------------------------------------------------------
class CommandObjectPlatformDisconnect : public CommandObjectParsed
{
public:
    CommandObjectPlatformDisconnect (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "platform disconnect",
                             "Disconnect a platform by name to be the currently selected platform.",
                             "platform disconnect",
                             0)
    {
    }

    virtual
    ~CommandObjectPlatformDisconnect ()
    {
    }

protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            if (args.GetArgumentCount() == 0)
            {
                Error error;
                
                if (platform_sp->IsConnected())
                {
                    // Cache the instance name if there is one since we are 
                    // about to disconnect and the name might go with it.
                    const char *hostname_cstr = platform_sp->GetHostname();
                    std::string hostname;
                    if (hostname_cstr)
                        hostname.assign (hostname_cstr);

                    error = platform_sp->DisconnectRemote ();
                    if (error.Success())
                    {
                        Stream &ostrm = result.GetOutputStream();      
                        if (hostname.empty())
                            ostrm.Printf ("Disconnected from \"%s\"\n", platform_sp->GetPluginName().GetCString());
                        else
                            ostrm.Printf ("Disconnected from \"%s\"\n", hostname.c_str());
                        result.SetStatus (eReturnStatusSuccessFinishResult);            
                    }
                    else
                    {
                        result.AppendErrorWithFormat ("%s", error.AsCString());
                        result.SetStatus (eReturnStatusFailed);            
                    }
                }
                else
                {
                    // Not connected...
                    result.AppendErrorWithFormat ("not connected to '%s'", platform_sp->GetPluginName().GetCString());
                    result.SetStatus (eReturnStatusFailed);            
                }
            }
            else
            {
                // Bad args
                result.AppendError ("\"platform disconnect\" doesn't take any arguments");
                result.SetStatus (eReturnStatusFailed);            
            }
        }
        else
        {
            result.AppendError ("no platform is currently selected");
            result.SetStatus (eReturnStatusFailed);            
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform settings"
//----------------------------------------------------------------------
class CommandObjectPlatformSettings : public CommandObjectParsed
{
public:
    CommandObjectPlatformSettings (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "platform settings",
                             "Set settings for the current target's platform, or for a platform by name.",
                             "platform settings",
                             0),
        m_options (interpreter),
        m_option_working_dir (LLDB_OPT_SET_1, false, "working-dir", 'w', 0, eArgTypePath, "The working directory for the platform.")
    {
        m_options.Append (&m_option_working_dir, LLDB_OPT_SET_ALL, LLDB_OPT_SET_1);
    }
    
    virtual
    ~CommandObjectPlatformSettings ()
    {
    }
    
protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            if (m_option_working_dir.GetOptionValue().OptionWasSet())
                platform_sp->SetWorkingDirectory (ConstString(m_option_working_dir.GetOptionValue().GetCurrentValue().GetPath().c_str()));
        }
        else
        {
            result.AppendError ("no platform is currently selected");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    
    virtual Options *
    GetOptions ()
    {
        if (m_options.DidFinalize() == false)
        {
            m_options.Append(new OptionPermissions());
            m_options.Finalize();
        }
        return &m_options;
    }
protected:
    
    OptionGroupOptions m_options;
    OptionGroupFile m_option_working_dir;

};


//----------------------------------------------------------------------
// "platform mkdir"
//----------------------------------------------------------------------
class CommandObjectPlatformMkDir : public CommandObjectParsed
{
public:
    CommandObjectPlatformMkDir (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform mkdir",
                         "Make a new directory on the remote end.",
                         NULL,
                         0),
    m_options(interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformMkDir ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            std::string cmd_line;
            args.GetCommandString(cmd_line);
            uint32_t mode;
            const OptionPermissions* options_permissions = (OptionPermissions*)m_options.GetGroupWithOption('r');
            if (options_permissions)
                mode = options_permissions->m_permissions;
            else
                mode = lldb::eFilePermissionsUserRWX | lldb::eFilePermissionsGroupRWX | lldb::eFilePermissionsWorldRX;
            Error error = platform_sp->MakeDirectory(cmd_line.c_str(), mode);
            if (error.Success())
            {
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendError(error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    
    virtual Options *
    GetOptions ()
    {
        if (m_options.DidFinalize() == false)
        {
            m_options.Append(new OptionPermissions());
            m_options.Finalize();
        }
        return &m_options;
    }
    OptionGroupOptions m_options;
    
};

//----------------------------------------------------------------------
// "platform fopen"
//----------------------------------------------------------------------
class CommandObjectPlatformFOpen : public CommandObjectParsed
{
public:
    CommandObjectPlatformFOpen (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform file open",
                         "Open a file on the remote end.",
                         NULL,
                         0),
    m_options(interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformFOpen ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            Error error;
            std::string cmd_line;
            args.GetCommandString(cmd_line);
            mode_t perms;
            const OptionPermissions* options_permissions = (OptionPermissions*)m_options.GetGroupWithOption('r');
            if (options_permissions)
                perms = options_permissions->m_permissions;
            else
                perms = lldb::eFilePermissionsUserRW | lldb::eFilePermissionsGroupRW | lldb::eFilePermissionsWorldRead;
            lldb::user_id_t fd = platform_sp->OpenFile(FileSpec(cmd_line.c_str(),false),
                                                       File::eOpenOptionRead | File::eOpenOptionWrite |
                                                       File::eOpenOptionAppend | File::eOpenOptionCanCreate,
                                                       perms,
                                                       error);
            if (error.Success())
            {
                result.AppendMessageWithFormat("File Descriptor = %" PRIu64 "\n",fd);
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendError(error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    virtual Options *
    GetOptions ()
    {
        if (m_options.DidFinalize() == false)
        {
            m_options.Append(new OptionPermissions());
            m_options.Finalize();
        }
        return &m_options;
    }
    OptionGroupOptions m_options;
};

//----------------------------------------------------------------------
// "platform fclose"
//----------------------------------------------------------------------
class CommandObjectPlatformFClose : public CommandObjectParsed
{
public:
    CommandObjectPlatformFClose (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform file close",
                         "Close a file on the remote end.",
                         NULL,
                         0)
    {
    }
    
    virtual
    ~CommandObjectPlatformFClose ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            std::string cmd_line;
            args.GetCommandString(cmd_line);
            const lldb::user_id_t fd = Args::StringToUInt64(cmd_line.c_str(), UINT64_MAX);
            Error error;
            bool success = platform_sp->CloseFile(fd, error);
            if (success)
            {
                result.AppendMessageWithFormat("file %" PRIu64 " closed.\n", fd);
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendError(error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform fread"
//----------------------------------------------------------------------
class CommandObjectPlatformFRead : public CommandObjectParsed
{
public:
    CommandObjectPlatformFRead (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform file read",
                         "Read data from a file on the remote end.",
                         NULL,
                         0),
    m_options (interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformFRead ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            std::string cmd_line;
            args.GetCommandString(cmd_line);
            const lldb::user_id_t fd = Args::StringToUInt64(cmd_line.c_str(), UINT64_MAX);
            std::string buffer(m_options.m_count,0);
            Error error;
            uint32_t retcode = platform_sp->ReadFile(fd, m_options.m_offset, &buffer[0], m_options.m_count, error);
            result.AppendMessageWithFormat("Return = %d\n",retcode);
            result.AppendMessageWithFormat("Data = \"%s\"\n",buffer.c_str());
            result.SetStatus (eReturnStatusSuccessFinishResult);
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
        Options (interpreter)
        {
        }
        
        virtual
        ~CommandOptions ()
        {
        }
        
        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            char short_option = (char) m_getopt_table[option_idx].val;
            bool success = false;
            
            switch (short_option)
            {
                case 'o':
                    m_offset = Args::StringToUInt32(option_arg, 0, 0, &success);
                    if (!success)
                        error.SetErrorStringWithFormat("invalid offset: '%s'", option_arg);
                    break;
                case 'c':
                    m_count = Args::StringToUInt32(option_arg, 0, 0, &success);
                    if (!success)
                        error.SetErrorStringWithFormat("invalid offset: '%s'", option_arg);
                    break;

                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }
            
            return error;
        }
        
        void
        OptionParsingStarting ()
        {
            m_offset = 0;
            m_count = 1;
        }
        
        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        
        uint32_t m_offset;
        uint32_t m_count;
    };
    CommandOptions m_options;
};
OptionDefinition
CommandObjectPlatformFRead::CommandOptions::g_option_table[] =
{
    {   LLDB_OPT_SET_1, false, "offset"           , 'o', OptionParser::eRequiredArgument, NULL, 0, eArgTypeIndex        , "Offset into the file at which to start reading." },
    {   LLDB_OPT_SET_1, false, "count"            , 'c', OptionParser::eRequiredArgument, NULL, 0, eArgTypeCount        , "Number of bytes to read from the file." },
    {  0              , false, NULL               ,  0 , 0                , NULL, 0, eArgTypeNone         , NULL }
};


//----------------------------------------------------------------------
// "platform fwrite"
//----------------------------------------------------------------------
class CommandObjectPlatformFWrite : public CommandObjectParsed
{
public:
    CommandObjectPlatformFWrite (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform file write",
                         "Write data to a file on the remote end.",
                         NULL,
                         0),
    m_options (interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformFWrite ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            std::string cmd_line;
            args.GetCommandString(cmd_line);
            Error error;
            const lldb::user_id_t fd = Args::StringToUInt64(cmd_line.c_str(), UINT64_MAX);
            uint32_t retcode = platform_sp->WriteFile (fd,
                                                       m_options.m_offset,
                                                       &m_options.m_data[0],
                                                       m_options.m_data.size(),
                                                       error);
            result.AppendMessageWithFormat("Return = %d\n",retcode);
            result.SetStatus (eReturnStatusSuccessFinishResult);
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
        Options (interpreter)
        {
        }
        
        virtual
        ~CommandOptions ()
        {
        }
        
        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            char short_option = (char) m_getopt_table[option_idx].val;
            bool success = false;
            
            switch (short_option)
            {
                case 'o':
                    m_offset = Args::StringToUInt32(option_arg, 0, 0, &success);
                    if (!success)
                        error.SetErrorStringWithFormat("invalid offset: '%s'", option_arg);
                    break;
                case 'd':
                    m_data.assign(option_arg);
                    break;
                    
                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }
            
            return error;
        }
        
        void
        OptionParsingStarting ()
        {
            m_offset = 0;
            m_data.clear();
        }
        
        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        
        uint32_t m_offset;
        std::string m_data;
    };
    CommandOptions m_options;
};
OptionDefinition
CommandObjectPlatformFWrite::CommandOptions::g_option_table[] =
{
    {   LLDB_OPT_SET_1, false, "offset"           , 'o', OptionParser::eRequiredArgument, NULL, 0, eArgTypeIndex        , "Offset into the file at which to start reading." },
    {   LLDB_OPT_SET_1, false, "data"            , 'd', OptionParser::eRequiredArgument, NULL, 0, eArgTypeValue        , "Text to write to the file." },
    {  0              , false, NULL               ,  0 , 0                , NULL, 0, eArgTypeNone         , NULL }
};

class CommandObjectPlatformFile : public CommandObjectMultiword
{
public:
    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
    CommandObjectPlatformFile (CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "platform file",
                            "A set of commands to manage file access through a platform",
                            "platform file [open|close|read|write] ...")
    {
        LoadSubCommand ("open", CommandObjectSP (new CommandObjectPlatformFOpen  (interpreter)));
        LoadSubCommand ("close", CommandObjectSP (new CommandObjectPlatformFClose  (interpreter)));
        LoadSubCommand ("read", CommandObjectSP (new CommandObjectPlatformFRead  (interpreter)));
        LoadSubCommand ("write", CommandObjectSP (new CommandObjectPlatformFWrite  (interpreter)));
    }
    
    virtual
    ~CommandObjectPlatformFile ()
    {
    }
    
private:
    //------------------------------------------------------------------
    // For CommandObjectPlatform only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (CommandObjectPlatformFile);
};

//----------------------------------------------------------------------
// "platform get-file remote-file-path host-file-path"
//----------------------------------------------------------------------
class CommandObjectPlatformGetFile : public CommandObjectParsed
{
public:
    CommandObjectPlatformGetFile (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform get-file",
                         "Transfer a file from the remote end to the local host.",
                         "platform get-file <remote-file-spec> <local-file-spec>",
                         0)
    {
        SetHelpLong(
"Examples: \n\
\n\
    platform get-file /the/remote/file/path /the/local/file/path\n\
    # Transfer a file from the remote end with file path /the/remote/file/path to the local host.\n");

        CommandArgumentEntry arg1, arg2;
        CommandArgumentData file_arg_remote, file_arg_host;
    
        // Define the first (and only) variant of this arg.
        file_arg_remote.arg_type = eArgTypeFilename;
        file_arg_remote.arg_repetition = eArgRepeatPlain;
        // There is only one variant this argument could be; put it into the argument entry.
        arg1.push_back (file_arg_remote);
        
        // Define the second (and only) variant of this arg.
        file_arg_host.arg_type = eArgTypeFilename;
        file_arg_host.arg_repetition = eArgRepeatPlain;
        // There is only one variant this argument could be; put it into the argument entry.
        arg2.push_back (file_arg_host);

        // Push the data for the first and the second arguments into the m_arguments vector.
        m_arguments.push_back (arg1);
        m_arguments.push_back (arg2);
    }
    
    virtual
    ~CommandObjectPlatformGetFile ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        // If the number of arguments is incorrect, issue an error message.
        if (args.GetArgumentCount() != 2)
        {
            result.GetErrorStream().Printf("error: required arguments missing; specify both the source and destination file paths\n");
            result.SetStatus(eReturnStatusFailed);
            return false;
        }

        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            const char *remote_file_path = args.GetArgumentAtIndex(0);
            const char *local_file_path = args.GetArgumentAtIndex(1);
            Error error = platform_sp->GetFile(FileSpec(remote_file_path, false),
                                               FileSpec(local_file_path, false));
            if (error.Success())
            {
                result.AppendMessageWithFormat("successfully get-file from %s (remote) to %s (host)\n",
                                               remote_file_path, local_file_path);
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendMessageWithFormat("get-file failed: %s\n", error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform get-size remote-file-path"
//----------------------------------------------------------------------
class CommandObjectPlatformGetSize : public CommandObjectParsed
{
public:
    CommandObjectPlatformGetSize (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform get-size",
                         "Get the file size from the remote end.",
                         "platform get-size <remote-file-spec>",
                         0)
    {
        SetHelpLong(
"Examples: \n\
\n\
    platform get-size /the/remote/file/path\n\
    # Get the file size from the remote end with path /the/remote/file/path.\n");

        CommandArgumentEntry arg1;
        CommandArgumentData file_arg_remote;
    
        // Define the first (and only) variant of this arg.
        file_arg_remote.arg_type = eArgTypeFilename;
        file_arg_remote.arg_repetition = eArgRepeatPlain;
        // There is only one variant this argument could be; put it into the argument entry.
        arg1.push_back (file_arg_remote);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg1);
    }
    
    virtual
    ~CommandObjectPlatformGetSize ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        // If the number of arguments is incorrect, issue an error message.
        if (args.GetArgumentCount() != 1)
        {
            result.GetErrorStream().Printf("error: required argument missing; specify the source file path as the only argument\n");
            result.SetStatus(eReturnStatusFailed);
            return false;
        }

        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            std::string remote_file_path(args.GetArgumentAtIndex(0));
            user_id_t size = platform_sp->GetFileSize(FileSpec(remote_file_path.c_str(), false));
            if (size != UINT64_MAX)
            {
                result.AppendMessageWithFormat("File size of %s (remote): %" PRIu64 "\n", remote_file_path.c_str(), size);
                result.SetStatus (eReturnStatusSuccessFinishResult);
            }
            else
            {
                result.AppendMessageWithFormat("Eroor getting file size of %s (remote)\n", remote_file_path.c_str());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform put-file"
//----------------------------------------------------------------------
class CommandObjectPlatformPutFile : public CommandObjectParsed
{
public:
    CommandObjectPlatformPutFile (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform put-file",
                         "Transfer a file from this system to the remote end.",
                         NULL,
                         0)
    {
    }
    
    virtual
    ~CommandObjectPlatformPutFile ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        const char* src = args.GetArgumentAtIndex(0);
        const char* dst = args.GetArgumentAtIndex(1);

        FileSpec src_fs(src, true);
        FileSpec dst_fs(dst, false);
        
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            Error error (platform_sp->PutFile(src_fs, dst_fs));
            if (error.Success())
            {
                result.SetStatus (eReturnStatusSuccessFinishNoResult);
            }
            else
            {
                result.AppendError (error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform currently selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
};

//----------------------------------------------------------------------
// "platform process launch"
//----------------------------------------------------------------------
class CommandObjectPlatformProcessLaunch : public CommandObjectParsed
{
public:
    CommandObjectPlatformProcessLaunch (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter,
                             "platform process launch",
                             "Launch a new process on a remote platform.",
                             "platform process launch program",
                             eFlagRequiresTarget | eFlagTryTargetAPILock),
        m_options (interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformProcessLaunch ()
    {
    }
    
    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Target *target = m_interpreter.GetDebugger().GetSelectedTarget().get();
        PlatformSP platform_sp;
        if (target)
        {   
            platform_sp = target->GetPlatform();
        }   
        if (!platform_sp)
        {
            platform_sp = m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform();
        }   

        if (platform_sp)
        {
            Error error;
            const size_t argc = args.GetArgumentCount();
            Target *target = m_exe_ctx.GetTargetPtr();
            Module *exe_module = target->GetExecutableModulePointer();
            if (exe_module)
            {
                m_options.launch_info.GetExecutableFile () = exe_module->GetFileSpec();
                char exe_path[PATH_MAX];
                if (m_options.launch_info.GetExecutableFile ().GetPath (exe_path, sizeof(exe_path)))
                    m_options.launch_info.GetArguments().AppendArgument (exe_path);
                m_options.launch_info.GetArchitecture() = exe_module->GetArchitecture();
            }

            if (argc > 0)
            {
                if (m_options.launch_info.GetExecutableFile ())
                {
                    // We already have an executable file, so we will use this
                    // and all arguments to this function are extra arguments
                    m_options.launch_info.GetArguments().AppendArguments (args);
                }
                else
                {
                    // We don't have any file yet, so the first argument is our
                    // executable, and the rest are program arguments
                    const bool first_arg_is_executable = true;
                    m_options.launch_info.SetArguments (args, first_arg_is_executable);
                }
            }
            
            if (m_options.launch_info.GetExecutableFile ())
            {
                Debugger &debugger = m_interpreter.GetDebugger();

                if (argc == 0)
                    target->GetRunArguments(m_options.launch_info.GetArguments());

                ProcessSP process_sp (platform_sp->DebugProcess (m_options.launch_info, 
                                                                 debugger,
                                                                 target,
                                                                 debugger.GetListener(),
                                                                 error));
                if (process_sp && process_sp->IsAlive())
                {
                    result.SetStatus (eReturnStatusSuccessFinishNoResult);
                    return true;
                }
                
                if (error.Success())
                    result.AppendError ("process launch failed");
                else
                    result.AppendError (error.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
            else
            {
                result.AppendError ("'platform process launch' uses the current target file and arguments, or the executable and its arguments can be specified in this command");
                result.SetStatus (eReturnStatusFailed);
                return false;
            }
        }
        else
        {
            result.AppendError ("no platform is selected\n");
        }
        return result.Succeeded();
    }
    
protected:
    ProcessLaunchCommandOptions m_options;
};



//----------------------------------------------------------------------
// "platform process list"
//----------------------------------------------------------------------
class CommandObjectPlatformProcessList : public CommandObjectParsed
{
public:
    CommandObjectPlatformProcessList (CommandInterpreter &interpreter) :
        CommandObjectParsed (interpreter, 
                             "platform process list",
                             "List processes on a remote platform by name, pid, or many other matching attributes.",
                             "platform process list",
                             0),
        m_options (interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformProcessList ()
    {
    }
    
    virtual Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Target *target = m_interpreter.GetDebugger().GetSelectedTarget().get();
        PlatformSP platform_sp;
        if (target)
        {   
            platform_sp = target->GetPlatform();
        }   
        if (!platform_sp)
        {
            platform_sp = m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform();
        }   
        
        if (platform_sp)
        {
            Error error;
            if (args.GetArgumentCount() == 0)
            {
                
                if (platform_sp)
                {
                    Stream &ostrm = result.GetOutputStream();      

                    lldb::pid_t pid = m_options.match_info.GetProcessInfo().GetProcessID();
                    if (pid != LLDB_INVALID_PROCESS_ID)
                    {
                        ProcessInstanceInfo proc_info;
                        if (platform_sp->GetProcessInfo (pid, proc_info))
                        {
                            ProcessInstanceInfo::DumpTableHeader (ostrm, platform_sp.get(), m_options.show_args, m_options.verbose);
                            proc_info.DumpAsTableRow(ostrm, platform_sp.get(), m_options.show_args, m_options.verbose);
                            result.SetStatus (eReturnStatusSuccessFinishResult);
                        }
                        else
                        {
                            result.AppendErrorWithFormat ("no process found with pid = %" PRIu64 "\n", pid);
                            result.SetStatus (eReturnStatusFailed);
                        }
                    }
                    else
                    {
                        ProcessInstanceInfoList proc_infos;
                        const uint32_t matches = platform_sp->FindProcesses (m_options.match_info, proc_infos);
                        const char *match_desc = NULL;
                        const char *match_name = m_options.match_info.GetProcessInfo().GetName();
                        if (match_name && match_name[0])
                        {
                            switch (m_options.match_info.GetNameMatchType())
                            {
                                case eNameMatchIgnore: break;
                                case eNameMatchEquals: match_desc = "matched"; break;
                                case eNameMatchContains: match_desc = "contained"; break;
                                case eNameMatchStartsWith: match_desc = "started with"; break;
                                case eNameMatchEndsWith: match_desc = "ended with"; break;
                                case eNameMatchRegularExpression: match_desc = "matched the regular expression"; break;
                            }
                        }

                        if (matches == 0)
                        {
                            if (match_desc)
                                result.AppendErrorWithFormat ("no processes were found that %s \"%s\" on the \"%s\" platform\n", 
                                                              match_desc,
                                                              match_name,
                                                              platform_sp->GetPluginName().GetCString());
                            else
                                result.AppendErrorWithFormat ("no processes were found on the \"%s\" platform\n", platform_sp->GetPluginName().GetCString());
                            result.SetStatus (eReturnStatusFailed);
                        }
                        else
                        {
                            result.AppendMessageWithFormat ("%u matching process%s found on \"%s\"", 
                                                            matches,
                                                            matches > 1 ? "es were" : " was",
                                                            platform_sp->GetName().GetCString());
                            if (match_desc)
                                result.AppendMessageWithFormat (" whose name %s \"%s\"", 
                                                                match_desc,
                                                                match_name);
                            result.AppendMessageWithFormat ("\n");
                            ProcessInstanceInfo::DumpTableHeader (ostrm, platform_sp.get(), m_options.show_args, m_options.verbose);
                            for (uint32_t i=0; i<matches; ++i)
                            {
                                proc_infos.GetProcessInfoAtIndex(i).DumpAsTableRow(ostrm, platform_sp.get(), m_options.show_args, m_options.verbose);
                            }
                        }
                    }
                }
            }
            else
            {
                result.AppendError ("invalid args: process list takes only options\n");
                result.SetStatus (eReturnStatusFailed);
            }
        }
        else
        {
            result.AppendError ("no platform is selected\n");
            result.SetStatus (eReturnStatusFailed);
        }
        return result.Succeeded();
    }
    
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
            Options (interpreter),
            match_info ()
        {
        }
        
        virtual
        ~CommandOptions ()
        {
        }
        
        virtual Error
        SetOptionValue (uint32_t option_idx, const char *option_arg)
        {
            Error error;
            const int short_option = m_getopt_table[option_idx].val;
            bool success = false;

            switch (short_option)
            {
                case 'p':
                    match_info.GetProcessInfo().SetProcessID (Args::StringToUInt32 (option_arg, LLDB_INVALID_PROCESS_ID, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid process ID string: '%s'", option_arg);
                    break;
                
                case 'P':
                    match_info.GetProcessInfo().SetParentProcessID (Args::StringToUInt32 (option_arg, LLDB_INVALID_PROCESS_ID, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid parent process ID string: '%s'", option_arg);
                    break;

                case 'u':
                    match_info.GetProcessInfo().SetUserID (Args::StringToUInt32 (option_arg, UINT32_MAX, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid user ID string: '%s'", option_arg);
                    break;

                case 'U':
                    match_info.GetProcessInfo().SetEffectiveUserID (Args::StringToUInt32 (option_arg, UINT32_MAX, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid effective user ID string: '%s'", option_arg);
                    break;

                case 'g':
                    match_info.GetProcessInfo().SetGroupID (Args::StringToUInt32 (option_arg, UINT32_MAX, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid group ID string: '%s'", option_arg);
                    break;

                case 'G':
                    match_info.GetProcessInfo().SetEffectiveGroupID (Args::StringToUInt32 (option_arg, UINT32_MAX, 0, &success));
                    if (!success)
                        error.SetErrorStringWithFormat("invalid effective group ID string: '%s'", option_arg);
                    break;

                case 'a':
                    match_info.GetProcessInfo().GetArchitecture().SetTriple (option_arg, m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform().get());
                    break;

                case 'n':
                    match_info.GetProcessInfo().GetExecutableFile().SetFile (option_arg, false);
                    match_info.SetNameMatchType (eNameMatchEquals);
                    break;

                case 'e':
                    match_info.GetProcessInfo().GetExecutableFile().SetFile (option_arg, false);
                    match_info.SetNameMatchType (eNameMatchEndsWith);
                    break;

                case 's':
                    match_info.GetProcessInfo().GetExecutableFile().SetFile (option_arg, false);
                    match_info.SetNameMatchType (eNameMatchStartsWith);
                    break;
                    
                case 'c':
                    match_info.GetProcessInfo().GetExecutableFile().SetFile (option_arg, false);
                    match_info.SetNameMatchType (eNameMatchContains);
                    break;
                    
                case 'r':
                    match_info.GetProcessInfo().GetExecutableFile().SetFile (option_arg, false);
                    match_info.SetNameMatchType (eNameMatchRegularExpression);
                    break;

                case 'A':
                    show_args = true;
                    break;

                case 'v':
                    verbose = true;
                    break;

                default:
                    error.SetErrorStringWithFormat ("unrecognized option '%c'", short_option);
                    break;
            }
            
            return error;
        }
        
        void
        OptionParsingStarting ()
        {
            match_info.Clear();
            show_args = false;
            verbose = false;
        }
        
        const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        
        // Instance variables to hold the values for command options.
        
        ProcessInstanceInfoMatch match_info;
        bool show_args;
        bool verbose;
    };
    CommandOptions m_options;
};

OptionDefinition
CommandObjectPlatformProcessList::CommandOptions::g_option_table[] =
{
{ LLDB_OPT_SET_1            , false, "pid"        , 'p', OptionParser::eRequiredArgument, NULL, 0, eArgTypePid              , "List the process info for a specific process ID." },
{ LLDB_OPT_SET_2            , true , "name"       , 'n', OptionParser::eRequiredArgument, NULL, 0, eArgTypeProcessName      , "Find processes with executable basenames that match a string." },
{ LLDB_OPT_SET_3            , true , "ends-with"  , 'e', OptionParser::eRequiredArgument, NULL, 0, eArgTypeProcessName      , "Find processes with executable basenames that end with a string." },
{ LLDB_OPT_SET_4            , true , "starts-with", 's', OptionParser::eRequiredArgument, NULL, 0, eArgTypeProcessName      , "Find processes with executable basenames that start with a string." },
{ LLDB_OPT_SET_5            , true , "contains"   , 'c', OptionParser::eRequiredArgument, NULL, 0, eArgTypeProcessName      , "Find processes with executable basenames that contain a string." },
{ LLDB_OPT_SET_6            , true , "regex"      , 'r', OptionParser::eRequiredArgument, NULL, 0, eArgTypeRegularExpression, "Find processes with executable basenames that match a regular expression." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "parent"     , 'P', OptionParser::eRequiredArgument, NULL, 0, eArgTypePid              , "Find processes that have a matching parent process ID." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "uid"        , 'u', OptionParser::eRequiredArgument, NULL, 0, eArgTypeUnsignedInteger  , "Find processes that have a matching user ID." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "euid"       , 'U', OptionParser::eRequiredArgument, NULL, 0, eArgTypeUnsignedInteger  , "Find processes that have a matching effective user ID." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "gid"        , 'g', OptionParser::eRequiredArgument, NULL, 0, eArgTypeUnsignedInteger  , "Find processes that have a matching group ID." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "egid"       , 'G', OptionParser::eRequiredArgument, NULL, 0, eArgTypeUnsignedInteger  , "Find processes that have a matching effective group ID." },
{ LLDB_OPT_SET_FROM_TO(2, 6), false, "arch"       , 'a', OptionParser::eRequiredArgument, NULL, 0, eArgTypeArchitecture     , "Find processes that have a matching architecture." },
{ LLDB_OPT_SET_FROM_TO(1, 6), false, "show-args"  , 'A', OptionParser::eNoArgument      , NULL, 0, eArgTypeNone             , "Show process arguments instead of the process executable basename." },
{ LLDB_OPT_SET_FROM_TO(1, 6), false, "verbose"    , 'v', OptionParser::eNoArgument      , NULL, 0, eArgTypeNone             , "Enable verbose output." },
{ 0                         , false, NULL         ,  0 , 0                , NULL, 0, eArgTypeNone             , NULL }
};

//----------------------------------------------------------------------
// "platform process info"
//----------------------------------------------------------------------
class CommandObjectPlatformProcessInfo : public CommandObjectParsed
{
public:
    CommandObjectPlatformProcessInfo (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter, 
                         "platform process info",
                         "Get detailed information for one or more process by process ID.",
                         "platform process info <pid> [<pid> <pid> ...]",
                         0)
    {
        CommandArgumentEntry arg;
        CommandArgumentData pid_args;
        
        // Define the first (and only) variant of this arg.
        pid_args.arg_type = eArgTypePid;
        pid_args.arg_repetition = eArgRepeatStar;
        
        // There is only one variant this argument could be; put it into the argument entry.
        arg.push_back (pid_args);
        
        // Push the data for the first argument into the m_arguments vector.
        m_arguments.push_back (arg);
    }
    
    virtual
    ~CommandObjectPlatformProcessInfo ()
    {
    }
    
protected:
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        Target *target = m_interpreter.GetDebugger().GetSelectedTarget().get();
        PlatformSP platform_sp;
        if (target)
        {   
            platform_sp = target->GetPlatform();
        }   
        if (!platform_sp)
        {
            platform_sp = m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform();
        }   

        if (platform_sp)
        {
            const size_t argc = args.GetArgumentCount();
            if (argc > 0)
            {
                Error error;
                
                if (platform_sp->IsConnected())
                {
                    Stream &ostrm = result.GetOutputStream();      
                    bool success;
                    for (size_t i=0; i<argc; ++ i)
                    {
                        const char *arg = args.GetArgumentAtIndex(i);
                        lldb::pid_t pid = Args::StringToUInt32 (arg, LLDB_INVALID_PROCESS_ID, 0, &success);
                        if (success)
                        {
                            ProcessInstanceInfo proc_info;
                            if (platform_sp->GetProcessInfo (pid, proc_info))
                            {
                                ostrm.Printf ("Process information for process %" PRIu64 ":\n", pid);
                                proc_info.Dump (ostrm, platform_sp.get());
                            }
                            else
                            {
                                ostrm.Printf ("error: no process information is available for process %" PRIu64 "\n", pid);
                            }
                            ostrm.EOL();
                        }
                        else
                        {
                            result.AppendErrorWithFormat ("invalid process ID argument '%s'", arg);
                            result.SetStatus (eReturnStatusFailed);            
                            break;
                        }
                    }
                }
                else
                {
                    // Not connected...
                    result.AppendErrorWithFormat ("not connected to '%s'", platform_sp->GetPluginName().GetCString());
                    result.SetStatus (eReturnStatusFailed);            
                }
            }
            else
            {
                // No args
                result.AppendError ("one or more process id(s) must be specified");
                result.SetStatus (eReturnStatusFailed);            
            }
        }
        else
        {
            result.AppendError ("no platform is currently selected");
            result.SetStatus (eReturnStatusFailed);            
        }
        return result.Succeeded();
    }
};

class CommandObjectPlatformProcessAttach : public CommandObjectParsed
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
            char short_option = (char) m_getopt_table[option_idx].val;
            bool success = false;
            switch (short_option)
            {
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
                    const uint32_t num_matches = process_infos.GetSize();
                    if (num_matches > 0)
                    {
                        for (uint32_t i=0; i<num_matches; ++i)
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
    
    CommandObjectPlatformProcessAttach (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform process attach",
                         "Attach to a process.",
                         "platform process attach <cmd-options>"),
    m_options (interpreter)
    {
    }
    
    ~CommandObjectPlatformProcessAttach ()
    {
    }
    
    bool
    DoExecute (Args& command,
             CommandReturnObject &result)
    {
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (platform_sp)
        {
            Error err;
            ProcessSP remote_process_sp =
            platform_sp->Attach(m_options.attach_info, m_interpreter.GetDebugger(), NULL, m_interpreter.GetDebugger().GetListener(), err);
            if (err.Fail())
            {
                result.AppendError(err.AsCString());
                result.SetStatus (eReturnStatusFailed);
            }
            else if (remote_process_sp.get() == NULL)
            {
                result.AppendError("could not attach: unknown reason");
                result.SetStatus (eReturnStatusFailed);
            }
            else
                result.SetStatus (eReturnStatusSuccessFinishResult);
        }
        else
        {
            result.AppendError ("no platform is currently selected");
            result.SetStatus (eReturnStatusFailed);            
        }
        return result.Succeeded();
    }
    
    Options *
    GetOptions ()
    {
        return &m_options;
    }
    
protected:
    
    CommandOptions m_options;
};


OptionDefinition
CommandObjectPlatformProcessAttach::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_ALL, false, "plugin", 'P', OptionParser::eRequiredArgument, NULL, 0, eArgTypePlugin,        "Name of the process plugin you want to use."},
    { LLDB_OPT_SET_1,   false, "pid",    'p', OptionParser::eRequiredArgument, NULL, 0, eArgTypePid,           "The process ID of an existing process to attach to."},
    { LLDB_OPT_SET_2,   false, "name",   'n', OptionParser::eRequiredArgument, NULL, 0, eArgTypeProcessName,  "The name of the process to attach to."},
    { LLDB_OPT_SET_2,   false, "waitfor",'w', OptionParser::eNoArgument,       NULL, 0, eArgTypeNone,              "Wait for the the process with <process-name> to launch."},
    { 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};


class CommandObjectPlatformProcess : public CommandObjectMultiword
{
public:
    //------------------------------------------------------------------
    // Constructors and Destructors
    //------------------------------------------------------------------
     CommandObjectPlatformProcess (CommandInterpreter &interpreter) :
        CommandObjectMultiword (interpreter,
                                "platform process",
                                "A set of commands to query, launch and attach to platform processes",
                                "platform process [attach|launch|list] ...")
    {
        LoadSubCommand ("attach", CommandObjectSP (new CommandObjectPlatformProcessAttach (interpreter)));
        LoadSubCommand ("launch", CommandObjectSP (new CommandObjectPlatformProcessLaunch (interpreter)));
        LoadSubCommand ("info"  , CommandObjectSP (new CommandObjectPlatformProcessInfo (interpreter)));
        LoadSubCommand ("list"  , CommandObjectSP (new CommandObjectPlatformProcessList (interpreter)));

    }
    
    virtual
    ~CommandObjectPlatformProcess ()
    {
    }
    
private:
    //------------------------------------------------------------------
    // For CommandObjectPlatform only
    //------------------------------------------------------------------
    DISALLOW_COPY_AND_ASSIGN (CommandObjectPlatformProcess);
};

//----------------------------------------------------------------------
// "platform shell"
//----------------------------------------------------------------------
class CommandObjectPlatformShell : public CommandObjectRaw
{
public:
    
    class CommandOptions : public Options
    {
    public:
        
        CommandOptions (CommandInterpreter &interpreter) :
        Options(interpreter),
        timeout(10)
        {
        }
        
        virtual
        ~CommandOptions ()
        {
        }
        
        virtual uint32_t
        GetNumDefinitions ()
        {
            return 1;
        }
        
        virtual const OptionDefinition*
        GetDefinitions ()
        {
            return g_option_table;
        }
        
        virtual Error
        SetOptionValue (uint32_t option_idx,
                        const char *option_value)
        {
            Error error;
            
            const char short_option = (char) g_option_table[option_idx].short_option;
            
            switch (short_option)
            {
                case 't':
                {
                    bool success;
                    timeout = Args::StringToUInt32(option_value, 10, 10, &success);
                    if (!success)
                        error.SetErrorStringWithFormat("could not convert \"%s\" to a numeric value.", option_value);
                    break;
                }
                default:
                    error.SetErrorStringWithFormat("invalid short option character '%c'", short_option);
                    break;
            }
            
            return error;
        }
        
        virtual void
        OptionParsingStarting ()
        {
        }
        
        // Options table: Required for subclasses of Options.
        
        static OptionDefinition g_option_table[];
        uint32_t timeout;
    };
    
    CommandObjectPlatformShell (CommandInterpreter &interpreter) :
    CommandObjectRaw (interpreter, 
                      "platform shell",
                      "Run a shell command on a the selected platform.",
                      "platform shell <shell-command>",
                      0),
    m_options(interpreter)
    {
    }
    
    virtual
    ~CommandObjectPlatformShell ()
    {
    }
    
    virtual
    Options *
    GetOptions ()
    {
        return &m_options;
    }
    
    virtual bool
    DoExecute (const char *raw_command_line, CommandReturnObject &result)
    {
        m_options.NotifyOptionParsingStarting();
        
        const char* expr = NULL;

        // Print out an usage syntax on an empty command line.
        if (raw_command_line[0] == '\0')
        {
            result.GetOutputStream().Printf("%s\n", this->GetSyntax());
            return true;
        }

        if (raw_command_line[0] == '-')
        {
            // We have some options and these options MUST end with --.
            const char *end_options = NULL;
            const char *s = raw_command_line;
            while (s && s[0])
            {
                end_options = ::strstr (s, "--");
                if (end_options)
                {
                    end_options += 2; // Get past the "--"
                    if (::isspace (end_options[0]))
                    {
                        expr = end_options;
                        while (::isspace (*expr))
                            ++expr;
                        break;
                    }
                }
                s = end_options;
            }
            
            if (end_options)
            {
                Args args (raw_command_line, end_options - raw_command_line);
                if (!ParseOptions (args, result))
                    return false;
            }
        }
        
        if (expr == NULL)
            expr = raw_command_line;
        
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        Error error;
        if (platform_sp)
        {
            const char *working_dir = NULL;
            std::string output;
            int status = -1;
            int signo = -1;
            error = (platform_sp->RunShellCommand (expr, working_dir, &status, &signo, &output, m_options.timeout));
            if (!output.empty())
                result.GetOutputStream().PutCString(output.c_str());
            if (status > 0)
            {
                if (signo > 0)
                {
                    const char *signo_cstr = Host::GetSignalAsCString(signo);
                    if (signo_cstr)
                        result.GetOutputStream().Printf("error: command returned with status %i and signal %s\n", status, signo_cstr);
                    else
                        result.GetOutputStream().Printf("error: command returned with status %i and signal %i\n", status, signo);
                }
                else
                    result.GetOutputStream().Printf("error: command returned with status %i\n", status);
            }
        }
        else
        {
            result.GetOutputStream().Printf("error: cannot run remote shell commands without a platform\n");
            error.SetErrorString("error: cannot run remote shell commands without a platform");
        }

        if (error.Fail())
        {
            result.AppendError(error.AsCString());
            result.SetStatus (eReturnStatusFailed);
        }
        else
        {
            result.SetStatus (eReturnStatusSuccessFinishResult);
        }
        return true;
    }
    CommandOptions m_options;
};

OptionDefinition
CommandObjectPlatformShell::CommandOptions::g_option_table[] =
{
    { LLDB_OPT_SET_ALL, false, "timeout",      't', OptionParser::eRequiredArgument, NULL, 0, eArgTypeValue,    "Seconds to wait for the remote host to finish running the command."},
    { 0, false, NULL, 0, 0, NULL, 0, eArgTypeNone, NULL }
};


//----------------------------------------------------------------------
// "platform install" - install a target to a remote end
//----------------------------------------------------------------------
class CommandObjectPlatformInstall : public CommandObjectParsed
{
public:
    CommandObjectPlatformInstall (CommandInterpreter &interpreter) :
    CommandObjectParsed (interpreter,
                         "platform target-install",
                         "Install a target (bundle or executable file) to the remote end.",
                         "platform target-install <local-thing> <remote-sandbox>",
                         0)
    {
    }
    
    virtual
    ~CommandObjectPlatformInstall ()
    {
    }
    
    virtual bool
    DoExecute (Args& args, CommandReturnObject &result)
    {
        if (args.GetArgumentCount() != 2)
        {
            result.AppendError("platform target-install takes two arguments");
            result.SetStatus(eReturnStatusFailed);
            return false;
        }
        // TODO: move the bulk of this code over to the platform itself
        FileSpec src(args.GetArgumentAtIndex(0), true);
        FileSpec dst(args.GetArgumentAtIndex(1), false);
        if (src.Exists() == false)
        {
            result.AppendError("source location does not exist or is not accessible");
            result.SetStatus(eReturnStatusFailed);
            return false;
        }
        PlatformSP platform_sp (m_interpreter.GetDebugger().GetPlatformList().GetSelectedPlatform());
        if (!platform_sp)
        {
            result.AppendError ("no platform currently selected");
            result.SetStatus (eReturnStatusFailed);
            return false;
        }
        
        Error error = platform_sp->Install(src, dst);
        if (error.Success())
        {
            result.SetStatus(eReturnStatusSuccessFinishNoResult);
        }
        else
        {
            result.AppendErrorWithFormat("install failed: %s", error.AsCString());
            result.SetStatus(eReturnStatusFailed);
        }
        return result.Succeeded();
    }
private:

};

//----------------------------------------------------------------------
// CommandObjectPlatform constructor
//----------------------------------------------------------------------
CommandObjectPlatform::CommandObjectPlatform(CommandInterpreter &interpreter) :
    CommandObjectMultiword (interpreter,
                            "platform",
                            "A set of commands to manage and create platforms.",
                            "platform [connect|disconnect|info|list|status|select] ...")
{
    LoadSubCommand ("select", CommandObjectSP (new CommandObjectPlatformSelect (interpreter)));
    LoadSubCommand ("list"  , CommandObjectSP (new CommandObjectPlatformList (interpreter)));
    LoadSubCommand ("status", CommandObjectSP (new CommandObjectPlatformStatus (interpreter)));
    LoadSubCommand ("connect", CommandObjectSP (new CommandObjectPlatformConnect (interpreter)));
    LoadSubCommand ("disconnect", CommandObjectSP (new CommandObjectPlatformDisconnect (interpreter)));
    LoadSubCommand ("settings", CommandObjectSP (new CommandObjectPlatformSettings (interpreter)));
#ifdef LLDB_CONFIGURATION_DEBUG
    LoadSubCommand ("mkdir", CommandObjectSP (new CommandObjectPlatformMkDir (interpreter)));
    LoadSubCommand ("file", CommandObjectSP (new CommandObjectPlatformFile (interpreter)));
    LoadSubCommand ("get-file", CommandObjectSP (new CommandObjectPlatformGetFile (interpreter)));
    LoadSubCommand ("get-size", CommandObjectSP (new CommandObjectPlatformGetSize (interpreter)));
    LoadSubCommand ("put-file", CommandObjectSP (new CommandObjectPlatformPutFile (interpreter)));
#endif
    LoadSubCommand ("process", CommandObjectSP (new CommandObjectPlatformProcess (interpreter)));
    LoadSubCommand ("shell", CommandObjectSP (new CommandObjectPlatformShell (interpreter)));
    LoadSubCommand ("target-install", CommandObjectSP (new CommandObjectPlatformInstall (interpreter)));
}


//----------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------
CommandObjectPlatform::~CommandObjectPlatform()
{
}
