//===-- Args.h --------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_Command_h_
#define liblldb_Command_h_

// C Includes
#include <getopt.h>

// C++ Includes
#include <list>
#include <string>
#include <vector>
#include <utility>

// Other libraries and framework includes
// Project includes
#include "lldb/lldb-private-types.h"
#include "lldb/lldb-types.h"
#include "lldb/Core/Error.h"

namespace lldb_private {

typedef std::pair<int, std::string> OptionArgValue;
typedef std::pair<std::string, OptionArgValue> OptionArgPair;
typedef std::vector<OptionArgPair> OptionArgVector;
typedef std::shared_ptr<OptionArgVector> OptionArgVectorSP;

struct OptionArgElement
{
    enum {
        eUnrecognizedArg = -1,
        eBareDash = -2,
        eBareDoubleDash = -3
    };
    
    OptionArgElement (int defs_index, int pos, int arg_pos) :
        opt_defs_index(defs_index),
        opt_pos (pos),
        opt_arg_pos (arg_pos)
    {
    }

    int opt_defs_index;
    int opt_pos;
    int opt_arg_pos;
};

typedef std::vector<OptionArgElement> OptionElementVector;

//----------------------------------------------------------------------
/// @class Args Args.h "lldb/Interpreter/Args.h"
/// @brief A command line argument class.
///
/// The Args class is designed to be fed a command line. The
/// command line is copied into an internal buffer and then split up
/// into arguments. Arguments are space delimited if there are no quotes
/// (single, double, or backtick quotes) surrounding the argument. Spaces
/// can be escaped using a \ character to avoid having to surround an
/// argument that contains a space with quotes.
//----------------------------------------------------------------------
class Args
{
public:

    //------------------------------------------------------------------
    /// Construct with an option command string.
    ///
    /// @param[in] command
    ///     A NULL terminated command that will be copied and split up
    ///     into arguments.
    ///
    /// @see Args::SetCommandString(const char *)
    //------------------------------------------------------------------
    Args (const char *command = NULL);

    Args (const char *command, size_t len);

    Args (const Args &rhs);
    
    const Args &
    operator= (const Args &rhs);

    //------------------------------------------------------------------
    /// Destructor.
    //------------------------------------------------------------------
    ~Args();

    //------------------------------------------------------------------
    /// Dump all arguments to the stream \a s.
    ///
    /// @param[in] s
    ///     The stream to which to dump all arguments in the argument
    ///     vector.
    //------------------------------------------------------------------
    void
    Dump (Stream *s);

    //------------------------------------------------------------------
    /// Sets the command string contained by this object.
    ///
    /// The command string will be copied and split up into arguments
    /// that can be accessed via the accessor functions.
    ///
    /// @param[in] command
    ///     A NULL terminated command that will be copied and split up
    ///     into arguments.
    ///
    /// @see Args::GetArgumentCount() const
    /// @see Args::GetArgumentAtIndex (size_t) const
    /// @see Args::GetArgumentVector ()
    /// @see Args::Shift ()
    /// @see Args::Unshift (const char *)
    //------------------------------------------------------------------
    void
    SetCommandString (const char *command);

    void
    SetCommandString (const char *command, size_t len);

    bool
    GetCommandString (std::string &command) const;

    bool
    GetQuotedCommandString (std::string &command) const;

    //------------------------------------------------------------------
    /// Gets the number of arguments left in this command object.
    ///
    /// @return
    ///     The number or arguments in this object.
    //------------------------------------------------------------------
    size_t
    GetArgumentCount () const;

    //------------------------------------------------------------------
    /// Gets the NULL terminated C string argument pointer for the
    /// argument at index \a idx.
    ///
    /// @return
    ///     The NULL terminated C string argument pointer if \a idx is a
    ///     valid argument index, NULL otherwise.
    //------------------------------------------------------------------
    const char *
    GetArgumentAtIndex (size_t idx) const;

    char
    GetArgumentQuoteCharAtIndex (size_t idx) const;

    //------------------------------------------------------------------
    /// Gets the argument vector.
    ///
    /// The value returned by this function can be used by any function
    /// that takes and vector. The return value is just like \a argv
    /// in the standard C entry point function:
    ///     \code
    ///         int main (int argc, const char **argv);
    ///     \endcode
    ///
    /// @return
    ///     An array of NULL terminated C string argument pointers that
    ///     also has a terminating NULL C string pointer
    //------------------------------------------------------------------
    char **
    GetArgumentVector ();

    //------------------------------------------------------------------
    /// Gets the argument vector.
    ///
    /// The value returned by this function can be used by any function
    /// that takes and vector. The return value is just like \a argv
    /// in the standard C entry point function:
    ///     \code
    ///         int main (int argc, const char **argv);
    ///     \endcode
    ///
    /// @return
    ///     An array of NULL terminate C string argument pointers that
    ///     also has a terminating NULL C string pointer
    //------------------------------------------------------------------
    const char **
    GetConstArgumentVector () const;


    //------------------------------------------------------------------
    /// Appends a new argument to the end of the list argument list.
    ///
    /// @param[in] arg_cstr
    ///     The new argument as a NULL terminated C string.
    ///
    /// @param[in] quote_char
    ///     If the argument was originally quoted, put in the quote char here.
    ///
    /// @return
    ///     The NULL terminated C string of the copy of \a arg_cstr.
    //------------------------------------------------------------------
    const char *
    AppendArgument (const char *arg_cstr, char quote_char = '\0');

    void
    AppendArguments (const Args &rhs);
    
    void
    AppendArguments (const char **argv);

    //------------------------------------------------------------------
    /// Insert the argument value at index \a idx to \a arg_cstr.
    ///
    /// @param[in] idx
    ///     The index of where to insert the argument.
    ///
    /// @param[in] arg_cstr
    ///     The new argument as a NULL terminated C string.
    ///
    /// @param[in] quote_char
    ///     If the argument was originally quoted, put in the quote char here.
    ///
    /// @return
    ///     The NULL terminated C string of the copy of \a arg_cstr.
    //------------------------------------------------------------------
    const char *
    InsertArgumentAtIndex (size_t idx, const char *arg_cstr, char quote_char = '\0');

    //------------------------------------------------------------------
    /// Replaces the argument value at index \a idx to \a arg_cstr
    /// if \a idx is a valid argument index.
    ///
    /// @param[in] idx
    ///     The index of the argument that will have its value replaced.
    ///
    /// @param[in] arg_cstr
    ///     The new argument as a NULL terminated C string.
    ///
    /// @param[in] quote_char
    ///     If the argument was originally quoted, put in the quote char here.
    ///
    /// @return
    ///     The NULL terminated C string of the copy of \a arg_cstr if
    ///     \a idx was a valid index, NULL otherwise.
    //------------------------------------------------------------------
    const char *
    ReplaceArgumentAtIndex (size_t idx, const char *arg_cstr, char quote_char = '\0');

    //------------------------------------------------------------------
    /// Deletes the argument value at index
    /// if \a idx is a valid argument index.
    ///
    /// @param[in] idx
    ///     The index of the argument that will have its value replaced.
    ///
    //------------------------------------------------------------------
    void
    DeleteArgumentAtIndex (size_t idx);

    //------------------------------------------------------------------
    /// Sets the argument vector value, optionally copying all
    /// arguments into an internal buffer.
    ///
    /// Sets the arguments to match those found in \a argv. All argument
    /// strings will be copied into an internal buffers.
    //
    //  FIXME: Handle the quote character somehow.
    //------------------------------------------------------------------
    void
    SetArguments (size_t argc, const char **argv);

    void
    SetArguments (const char **argv);

    //------------------------------------------------------------------
    /// Shifts the first argument C string value of the array off the
    /// argument array.
    ///
    /// The string value will be freed, so a copy of the string should
    /// be made by calling Args::GetArgumentAtIndex (size_t) const
    /// first and copying the returned value before calling
    /// Args::Shift().
    ///
    /// @see Args::GetArgumentAtIndex (size_t) const
    //------------------------------------------------------------------
    void
    Shift ();

    //------------------------------------------------------------------
    /// Inserts a class owned copy of \a arg_cstr at the beginning of
    /// the argument vector.
    ///
    /// A copy \a arg_cstr will be made.
    ///
    /// @param[in] arg_cstr
    ///     The argument to push on the front the the argument stack.
    ///
    /// @param[in] quote_char
    ///     If the argument was originally quoted, put in the quote char here.
    ///
    /// @return
    ///     A pointer to the copy of \a arg_cstr that was made.
    //------------------------------------------------------------------
    const char *
    Unshift (const char *arg_cstr, char quote_char = '\0');

    //------------------------------------------------------------------
    /// Parse the arguments in the contained arguments.
    ///
    /// The arguments that are consumed by the argument parsing process
    /// will be removed from the argument vector. The arguements that
    /// get processed start at the second argument. The first argument
    /// is assumed to be the command and will not be touched.
    ///
    /// @see class Options
    //------------------------------------------------------------------
    Error
    ParseOptions (Options &options);
    
    size_t
    FindArgumentIndexForOption (struct option *long_options, int long_options_index);
    
    bool
    IsPositionalArgument (const char *arg);

    // The following works almost identically to ParseOptions, except that no option is required to have arguments,
    // and it builds up the option_arg_vector as it parses the options.

    void
    ParseAliasOptions (Options &options, CommandReturnObject &result, OptionArgVector *option_arg_vector, 
                       std::string &raw_input_line);

    void
    ParseArgsForCompletion (Options &options, OptionElementVector &option_element_vector, uint32_t cursor_index);

    //------------------------------------------------------------------
    // Clear the arguments.
    //
    // For re-setting or blanking out the list of arguments.
    //------------------------------------------------------------------
    void
    Clear ();

    static const char *
    StripSpaces (std::string &s,
                 bool leading = true,
                 bool trailing = true,
                 bool return_null_if_empty = true);

    static int32_t
    StringToSInt32 (const char *s, int32_t fail_value = 0, int base = 0, bool *success_ptr = NULL);

    static uint32_t
    StringToUInt32 (const char *s, uint32_t fail_value = 0, int base = 0, bool *success_ptr = NULL);

    static int64_t
    StringToSInt64 (const char *s, int64_t fail_value = 0, int base = 0, bool *success_ptr = NULL);

    static uint64_t
    StringToUInt64 (const char *s, uint64_t fail_value = 0, int base = 0, bool *success_ptr = NULL);

    static bool
    UInt64ValueIsValidForByteSize (uint64_t uval64, size_t total_byte_size)
    {
        if (total_byte_size > 8)
            return false;
        
        if (total_byte_size == 8)
            return true;
        
        const uint64_t max = ((uint64_t)1 << (uint64_t)(total_byte_size * 8)) - 1;
        return uval64 <= max;
    }

    static bool
    SInt64ValueIsValidForByteSize (int64_t sval64, size_t total_byte_size)
    {
        if (total_byte_size > 8)
            return false;
        
        if (total_byte_size == 8)
            return true;
        
        const int64_t max = ((int64_t)1 << (uint64_t)(total_byte_size * 8 - 1)) - 1;
        const int64_t min = ~(max);
        return min <= sval64 && sval64 <= max;
    }

    static lldb::addr_t
    StringToAddress (const ExecutionContext *exe_ctx,
                     const char *s,
                     lldb::addr_t fail_value,
                     Error *error);

    static bool
    StringToBoolean (const char *s, bool fail_value, bool *success_ptr);
    
    static int64_t
    StringToOptionEnum (const char *s, OptionEnumValueElement *enum_values, int32_t fail_value, Error &error);

    static lldb::ScriptLanguage
    StringToScriptLanguage (const char *s, lldb::ScriptLanguage fail_value, bool *success_ptr);

    static Error
    StringToFormat (const char *s,
                    lldb::Format &format,
                    size_t *byte_size_ptr); // If non-NULL, then a byte size can precede the format character

    static lldb::Encoding
    StringToEncoding (const char *s,
                      lldb::Encoding fail_value = lldb::eEncodingInvalid);

    static uint32_t
    StringToGenericRegister (const char *s);
    
    static const char *
    StringToVersion (const char *s, uint32_t &major, uint32_t &minor, uint32_t &update);

    static const char *
    GetShellSafeArgument (const char *unsafe_arg, std::string &safe_arg);

    // EncodeEscapeSequences will change the textual representation of common
    // escape sequences like "\n" (two characters) into a single '\n'. It does
    // this for all of the supported escaped sequences and for the \0ooo (octal)
    // and \xXX (hex). The resulting "dst" string will contain the character
    // versions of all supported escape sequences. The common supported escape
    // sequences are: "\a", "\b", "\f", "\n", "\r", "\t", "\v", "\'", "\"", "\\".

    static void
    EncodeEscapeSequences (const char *src, std::string &dst);

    // ExpandEscapeSequences will change a string of possibly non-printable
    // characters and expand them into text. So '\n' will turn into two chracters
    // like "\n" which is suitable for human reading. When a character is not
    // printable and isn't one of the common in escape sequences listed in the
    // help for EncodeEscapeSequences, then it will be encoded as octal. Printable
    // characters are left alone.
    static void
    ExpandEscapedCharacters (const char *src, std::string &dst);

    // This one isn't really relevant to Arguments per se, but we're using the Args as a
    // general strings container, so...
    void
    LongestCommonPrefix (std::string &common_prefix);

protected:
    //------------------------------------------------------------------
    // Classes that inherit from Args can see and modify these
    //------------------------------------------------------------------
    typedef std::list<std::string> arg_sstr_collection;
    typedef std::vector<const char *> arg_cstr_collection;
    typedef std::vector<char> arg_quote_char_collection;
    arg_sstr_collection m_args;
    arg_cstr_collection m_argv; ///< The current argument vector.
    arg_quote_char_collection m_args_quote_char;

    void
    UpdateArgsAfterOptionParsing ();

    void
    UpdateArgvFromArgs ();
};

} // namespace lldb_private

#endif  // liblldb_Command_h_
