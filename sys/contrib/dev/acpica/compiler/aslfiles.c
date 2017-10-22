
/******************************************************************************
 *
 * Module Name: aslfiles - file I/O suppoert
 *              $Revision: 1.54 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfiles")

/* Local prototypes */

static void
FlOpenFile (
    UINT32                  FileId,
    char                    *Filename,
    char                    *Mode);

static FILE *
FlOpenLocalFile (
    char                    *LocalName,
    char                    *Mode);

#ifdef ACPI_OBSOLETE_FUNCTIONS
ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename);
#endif


/*******************************************************************************
 *
 * FUNCTION:    AslAbort
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Dump the error log and abort the compiler.  Used for serious
 *              I/O errors
 *
 ******************************************************************************/

void
AslAbort (
    void)
{

    AePrintErrorLog (ASL_FILE_STDOUT);
    if (Gbl_DebugFlag)
    {
        /* Print error summary to the debug file */

        AePrintErrorLog (ASL_FILE_STDERR);
    }

    exit (1);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenLocalFile
 *
 * PARAMETERS:  LocalName           - Single filename (not a pathname)
 *              Mode                - Open mode for fopen
 *
 * RETURN:      File descriptor
 *
 * DESCRIPTION: Build a complete pathname for the input filename and open
 *              the file.
 *
 ******************************************************************************/

static FILE *
FlOpenLocalFile (
    char                    *LocalName,
    char                    *Mode)
{

    strcpy (StringBuffer, Gbl_DirectoryPath);
    strcat (StringBuffer, LocalName);

    DbgPrint (ASL_PARSE_OUTPUT, "FlOpenLocalFile: %s\n", StringBuffer);
    return (fopen (StringBuffer, (const char *) Mode));
}


/*******************************************************************************
 *
 * FUNCTION:    FlFileError
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              ErrorId             - Index into error message array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode errno to an error message and add the entire error
 *              to the error log.
 *
 ******************************************************************************/

void
FlFileError (
    UINT32                  FileId,
    UINT8                   ErrorId)
{

    sprintf (MsgBuffer, "\"%s\" (%s)", Gbl_Files[FileId].Filename,
        strerror (errno));
    AslCommonError (ASL_ERROR, ErrorId, 0, 0, 0, 0, NULL, MsgBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Filename            - file pathname to open
 *              Mode                - Open mode for fopen
 *
 * RETURN:      None
 *
 * DESCRIPTION: Open a file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

static void
FlOpenFile (
    UINT32                  FileId,
    char                    *Filename,
    char                    *Mode)
{
    FILE                    *File;


    File = fopen (Filename, Mode);

    Gbl_Files[FileId].Filename = Filename;
    Gbl_Files[FileId].Handle   = File;

    if (!File)
    {
        FlFileError (FileId, ASL_MSG_OPEN);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlReadFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Buffer              - Where to place the data
 *              Length              - Amount to read
 *
 * RETURN:      Status.  AE_ERROR indicates EOF.
 *
 * DESCRIPTION: Read data from an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

ACPI_STATUS
FlReadFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length)
{
    UINT32                  Actual;


    /* Read and check for error */

    Actual = fread (Buffer, 1, Length, Gbl_Files[FileId].Handle);
    if (Actual != Length)
    {
        if (feof (Gbl_Files[FileId].Handle))
        {
            /* End-of-file, just return error */

            return (AE_ERROR);
        }

        FlFileError (FileId, ASL_MSG_READ);
        AslAbort ();
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlWriteFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Buffer              - Data to write
 *              Length              - Amount of data to write
 *
 * RETURN:      None
 *
 * DESCRIPTION: Write data to an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlWriteFile (
    UINT32                  FileId,
    void                    *Buffer,
    UINT32                  Length)
{
    UINT32                  Actual;


    /* Write and check for error */

    Actual = fwrite ((char *) Buffer, 1, Length, Gbl_Files[FileId].Handle);
    if (Actual != Length)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlPrintFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Format              - Printf format string
 *              ...                 - Printf arguments
 *
 * RETURN:      None
 *
 * DESCRIPTION: Formatted write to an open file.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlPrintFile (
    UINT32                  FileId,
    char                    *Format,
    ...)
{
    INT32                   Actual;
    va_list                 Args;


    va_start (Args, Format);

    Actual = vfprintf (Gbl_Files[FileId].Handle, Format, Args);
    if (Actual == -1)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlSeekFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Offset              - Absolute byte offset in file
 *
 * RETURN:      None
 *
 * DESCRIPTION: Seek to absolute offset
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlSeekFile (
    UINT32                  FileId,
    long                    Offset)
{
    int                     Error;


    Error = fseek (Gbl_Files[FileId].Handle, Offset, SEEK_SET);
    if (Error)
    {
        FlFileError (FileId, ASL_MSG_SEEK);
        AslAbort ();
    }
}


/*******************************************************************************
 *
 * FUNCTION:    FlCloseFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Close an open file.  Aborts compiler on error
 *
 ******************************************************************************/

void
FlCloseFile (
    UINT32                  FileId)
{
    int                     Error;


    if (!Gbl_Files[FileId].Handle)
    {
        return;
    }

    Error = fclose (Gbl_Files[FileId].Handle);
    Gbl_Files[FileId].Handle = NULL;

    if (Error)
    {
        FlFileError (FileId, ASL_MSG_CLOSE);
        AslAbort ();
    }

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    FlSetLineNumber
 *
 * PARAMETERS:  Op        - Parse node for the LINE asl statement
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Set the current line number
 *
 ******************************************************************************/

void
FlSetLineNumber (
    ACPI_PARSE_OBJECT       *Op)
{

    Gbl_CurrentLineNumber = (UINT32) Op->Asl.Value.Integer;
    Gbl_LogicalLineNumber = (UINT32) Op->Asl.Value.Integer;
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenIncludeFile
 *
 * PARAMETERS:  Op        - Parse node for the INCLUDE ASL statement
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Open an include file and push it on the input file stack.
 *
 ******************************************************************************/

void
FlOpenIncludeFile (
    ACPI_PARSE_OBJECT       *Op)
{
    FILE                    *IncFile;


    /* Op must be valid */

    if (!Op)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN,
            Gbl_CurrentLineNumber, Gbl_LogicalLineNumber,
            Gbl_InputByteCount, Gbl_CurrentColumn,
            Gbl_Files[ASL_FILE_INPUT].Filename, " - Null parse node");

        return;
    }

    /*
     * Flush out the "include ()" statement on this line, start
     * the actual include file on the next line
     */
    ResetCurrentLineBuffer ();
    FlPrintFile (ASL_FILE_SOURCE_OUTPUT, "\n");
    Gbl_CurrentLineOffset++;

    /* Prepend the directory pathname and open the include file */

    DbgPrint (ASL_PARSE_OUTPUT, "\nOpen include file: path %s\n\n",
        Op->Asl.Value.String);
    IncFile = FlOpenLocalFile (Op->Asl.Value.String, "r");
    if (!IncFile)
    {
        sprintf (MsgBuffer, "%s (%s)", Op->Asl.Value.String, strerror (errno));
        AslError (ASL_ERROR, ASL_MSG_INCLUDE_FILE_OPEN, Op, MsgBuffer);
        return;
    }

    /* Push the include file on the open input file stack */

    AslPushInputFileStack (IncFile, Op->Asl.Value.String);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenInputFile
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Open the specified input file, and save the directory path to
 *              the file so that include files can be opened in
 *              the same directory.
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenInputFile (
    char                    *InputFilename)
{

    /* Open the input ASL file, text mode */

    FlOpenFile (ASL_FILE_INPUT, InputFilename, "r");
    AslCompilerin = Gbl_Files[ASL_FILE_INPUT].Handle;

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenAmlOutputFile
 *
 * PARAMETERS:  FilenamePrefix       - The user-specified ASL source file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the output filename (*.AML) and open the file.  The file
 *              is created in the same directory as the parent input file.
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenAmlOutputFile (
    char                    *FilenamePrefix)
{
    char                    *Filename;


    /* Output filename usually comes from the ASL itself */

    Filename = Gbl_Files[ASL_FILE_AML_OUTPUT].Filename;
    if (!Filename)
    {
        /* Create the output AML filename */

        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_AML_CODE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_OUTPUT_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }
    }

    /* Open the output AML file in binary mode */

    FlOpenFile (ASL_FILE_AML_OUTPUT, Filename, "w+b");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    FlOpenMiscOutputFiles
 *
 * PARAMETERS:  FilenamePrefix       - The user-specified ASL source file
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create and open the various output files needed, depending on
 *              the command line options
 *
 ******************************************************************************/

ACPI_STATUS
FlOpenMiscOutputFiles (
    char                    *FilenamePrefix)
{
    char                    *Filename;


    /* Create/Open a combined source output file */

    Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_SOURCE);
    if (!Filename)
    {
        AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
            0, 0, 0, 0, NULL, NULL);
        return (AE_ERROR);
    }

    /*
     * Open the source output file, binary mode (so that LF does not get
     * expanded to CR/LF on some systems, messing up our seek
     * calculations.)
     */
    FlOpenFile (ASL_FILE_SOURCE_OUTPUT, Filename, "w+b");

    /* Create/Open a listing output file if asked */

    if (Gbl_ListingFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_LISTING);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the listing file, text mode */

        FlOpenFile (ASL_FILE_LISTING_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_LISTING_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_LISTING_OUTPUT);
    }

    /* Create/Open a assembly code source output file if asked */

    if (Gbl_AsmOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_ASM_SOURCE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the assembly code source file, text mode */

        FlOpenFile (ASL_FILE_ASM_SOURCE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_ASM_SOURCE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_ASM_SOURCE_OUTPUT);
    }

    /* Create/Open a C code source output file if asked */

    if (Gbl_C_OutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_C_SOURCE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the C code source file, text mode */

        FlOpenFile (ASL_FILE_C_SOURCE_OUTPUT, Filename, "w+");

        FlPrintFile (ASL_FILE_C_SOURCE_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_SOURCE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_SOURCE_OUTPUT);
    }

    /* Create/Open a assembly include output file if asked */

    if (Gbl_AsmIncludeOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_ASM_INCLUDE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the assembly include file, text mode */

        FlOpenFile (ASL_FILE_ASM_INCLUDE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_ASM_INCLUDE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_ASM_INCLUDE_OUTPUT);
    }

    /* Create/Open a C include output file if asked */

    if (Gbl_C_IncludeOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_C_INCLUDE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the C include file, text mode */

        FlOpenFile (ASL_FILE_C_INCLUDE_OUTPUT, Filename, "w+");

        FlPrintFile (ASL_FILE_C_INCLUDE_OUTPUT, "/*\n");
        AslCompilerSignon (ASL_FILE_C_INCLUDE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_C_INCLUDE_OUTPUT);
    }

    /* Create/Open a hex output file if asked */

    if (Gbl_HexOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_HEX_DUMP);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the hex file, text mode */

        FlOpenFile (ASL_FILE_HEX_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_HEX_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_HEX_OUTPUT);
    }

    /* Create a namespace output file if asked */

    if (Gbl_NsOutputFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_NAMESPACE);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_LISTING_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the namespace file, text mode */

        FlOpenFile (ASL_FILE_NAMESPACE_OUTPUT, Filename, "w+");

        AslCompilerSignon (ASL_FILE_NAMESPACE_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_NAMESPACE_OUTPUT);
    }

    /* Create/Open a debug output file if asked */

    if (Gbl_DebugFlag)
    {
        Filename = FlGenerateFilename (FilenamePrefix, FILE_SUFFIX_DEBUG);
        if (!Filename)
        {
            AslCommonError (ASL_ERROR, ASL_MSG_DEBUG_FILENAME,
                0, 0, 0, 0, NULL, NULL);
            return (AE_ERROR);
        }

        /* Open the debug file as STDERR, text mode */

        /* TBD: hide this behind a FlReopenFile function */

        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Filename = Filename;
        Gbl_Files[ASL_FILE_DEBUG_OUTPUT].Handle =
            freopen (Filename, "w+t", stderr);

        AslCompilerSignon (ASL_FILE_DEBUG_OUTPUT);
        AslCompilerFileHeader (ASL_FILE_DEBUG_OUTPUT);
    }

    return (AE_OK);
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    FlParseInputPathname
 *
 * PARAMETERS:  InputFilename       - The user-specified ASL source file to be
 *                                    compiled
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Split the input path into a directory and filename part
 *              1) Directory part used to open include files
 *              2) Filename part used to generate output filenames
 *
 ******************************************************************************/

ACPI_STATUS
FlParseInputPathname (
    char                    *InputFilename)
{
    char                    *Substring;


    if (!InputFilename)
    {
        return (AE_OK);
    }

    /* Get the path to the input filename's directory */

    Gbl_DirectoryPath = strdup (InputFilename);
    if (!Gbl_DirectoryPath)
    {
        return (AE_NO_MEMORY);
    }

    Substring = strrchr (Gbl_DirectoryPath, '\\');
    if (!Substring)
    {
        Substring = strrchr (Gbl_DirectoryPath, '/');
        if (!Substring)
        {
            Substring = strrchr (Gbl_DirectoryPath, ':');
        }
    }

    if (!Substring)
    {
        Gbl_DirectoryPath[0] = 0;
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = strdup (InputFilename);
        }
    }
    else
    {
        if (Gbl_UseDefaultAmlFilename)
        {
            Gbl_OutputFilenamePrefix = strdup (Substring + 1);
        }
        *(Substring+1) = 0;
    }

    return (AE_OK);
}
#endif


