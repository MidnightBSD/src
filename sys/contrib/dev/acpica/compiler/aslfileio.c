/******************************************************************************
 *
 * Module Name: aslfileio - File I/O support
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acapps.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslfileio")


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

    sprintf (AslGbl_MsgBuffer, "\"%s\" (%s) - %s", AslGbl_Files[FileId].Filename,
        AslGbl_FileDescs[FileId].Description, strerror (errno));

    AslCommonError (ASL_ERROR, ErrorId, 0, 0, 0, 0, NULL, AslGbl_MsgBuffer);
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

void
FlOpenFile (
    UINT32                  FileId,
    char                    *Filename,
    char                    *Mode)
{
    FILE                    *File;


    AslGbl_Files[FileId].Filename = Filename;
    AslGbl_Files[FileId].Handle = NULL;

    File = fopen (Filename, Mode);
    if (!File)
    {
        FlFileError (FileId, ASL_MSG_OPEN);
        AslAbort ();
    }

    AslGbl_Files[FileId].Handle = File;
}


/*******************************************************************************
 *
 * FUNCTION:    FlGetFileSize
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      File Size
 *
 * DESCRIPTION: Get current file size. Uses common seek-to-EOF function.
 *              File must be open. Aborts compiler on error.
 *
 ******************************************************************************/

UINT32
FlGetFileSize (
    UINT32                  FileId)
{
    UINT32                  FileSize;


    FileSize = CmGetFileSize (AslGbl_Files[FileId].Handle);
    if (FileSize == ACPI_UINT32_MAX)
    {
        AslAbort();
    }

    return (FileSize);
}


/*******************************************************************************
 *
 * FUNCTION:    FlReadFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *              Buffer              - Where to place the data
 *              Length              - Amount to read
 *
 * RETURN:      Status. AE_ERROR indicates EOF.
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

    Actual = fread (Buffer, 1, Length, AslGbl_Files[FileId].Handle);
    if (Actual < Length)
    {
        if (feof (AslGbl_Files[FileId].Handle))
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

    Actual = fwrite ((char *) Buffer, 1, Length, AslGbl_Files[FileId].Handle);
    if (Actual != Length)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }

    if ((FileId == ASL_FILE_PREPROCESSOR) && AslGbl_PreprocessorOutputFlag)
    {
        /* Duplicate the output to the user preprocessor (.i) file */

        Actual = fwrite ((char *) Buffer, 1, Length,
            AslGbl_Files[ASL_FILE_PREPROCESSOR_USER].Handle);
        if (Actual != Length)
        {
            FlFileError (FileId, ASL_MSG_WRITE);
            AslAbort ();
        }
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
    Actual = vfprintf (AslGbl_Files[FileId].Handle, Format, Args);
    va_end (Args);

    if (Actual == -1)
    {
        FlFileError (FileId, ASL_MSG_WRITE);
        AslAbort ();
    }

    if ((FileId == ASL_FILE_PREPROCESSOR) &&
        AslGbl_PreprocessorOutputFlag)
    {
        /*
         * Duplicate the output to the user preprocessor (.i) file,
         * except: no #line directives.
         */
        if (!strncmp (Format, "#line", 5))
        {
            return;
        }

        va_start (Args, Format);
        Actual = vfprintf (AslGbl_Files[ASL_FILE_PREPROCESSOR_USER].Handle,
            Format, Args);
        va_end (Args);

        if (Actual == -1)
        {
            FlFileError (FileId, ASL_MSG_WRITE);
            AslAbort ();
        }
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
 * DESCRIPTION: Seek to absolute offset.
 *              NOTE: Aborts compiler on any error.
 *
 ******************************************************************************/

void
FlSeekFile (
    UINT32                  FileId,
    long                    Offset)
{
    int                     Error;


    Error = fseek (AslGbl_Files[FileId].Handle, Offset, SEEK_SET);
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
 * DESCRIPTION: Close an open file. Aborts compiler on error
 *
 ******************************************************************************/

void
FlCloseFile (
    UINT32                  FileId)
{
    int                     Error;


    if (!AslGbl_Files[FileId].Handle)
    {
        return;
    }

    Error = fclose (AslGbl_Files[FileId].Handle);
    if (Error)
    {
        FlFileError (FileId, ASL_MSG_CLOSE);
        AslAbort ();
    }

    /* Do not clear/free the filename string */

    AslGbl_Files[FileId].Handle = NULL;
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    FlDeleteFile
 *
 * PARAMETERS:  FileId              - Index into file info array
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete a file.
 *
 ******************************************************************************/

void
FlDeleteFile (
    UINT32                  FileId)
{
    ASL_FILE_INFO           *Info = &AslGbl_Files[FileId];


    if (!Info->Filename)
    {
        return;
    }

    if (remove (Info->Filename))
    {
        printf ("%s (%s file) ",
            Info->Filename, AslGbl_FileDescs[FileId].Description);
        perror ("Could not delete");
    }

    Info->Filename = NULL;
    return;
}
