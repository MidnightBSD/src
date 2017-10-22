/*******************************************************************************
 *
 * Module Name: dbfileio - Debugger file I/O commands.  These can't usually
 *              be used when running the debugger in Ring 0 (Kernel mode)
 *              $Revision: 1.94 $
 *
 ******************************************************************************/

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


#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acdebug.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/actables.h>
#include <contrib/dev/acpica/acdisasm.h>

#if (defined ACPI_DEBUGGER || defined ACPI_DISASSEMBLER)

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbfileio")

/*
 * NOTE: this is here for lack of a better place.  It is used in all
 * flavors of the debugger, need LCD file
 */
#ifdef ACPI_APPLICATION
#include <stdio.h>
FILE                        *AcpiGbl_DebugFile = NULL;
#endif


#ifdef ACPI_DEBUGGER

/* Local prototypes */

#ifdef ACPI_APPLICATION

static ACPI_STATUS
AcpiDbCheckTextModeCorruption (
    UINT8                   *Table,
    UINT32                  TableLength,
    UINT32                  FileLength);

static ACPI_STATUS
AeLocalLoadTable (
    ACPI_TABLE_HEADER       *TablePtr);
#endif

/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCloseDebugFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: If open, close the current debug output file
 *
 ******************************************************************************/

void
AcpiDbCloseDebugFile (
    void)
{

#ifdef ACPI_APPLICATION

    if (AcpiGbl_DebugFile)
    {
       fclose (AcpiGbl_DebugFile);
       AcpiGbl_DebugFile = NULL;
       AcpiGbl_DbOutputToFile = FALSE;
       AcpiOsPrintf ("Debug output file %s closed\n", AcpiGbl_DbDebugFilename);
    }
#endif
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbOpenDebugFile
 *
 * PARAMETERS:  Name                - Filename to open
 *
 * RETURN:      None
 *
 * DESCRIPTION: Open a file where debug output will be directed.
 *
 ******************************************************************************/

void
AcpiDbOpenDebugFile (
    char                    *Name)
{

#ifdef ACPI_APPLICATION

    AcpiDbCloseDebugFile ();
    AcpiGbl_DebugFile = fopen (Name, "w+");
    if (AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("Debug output file %s opened\n", Name);
        ACPI_STRCPY (AcpiGbl_DbDebugFilename, Name);
        AcpiGbl_DbOutputToFile = TRUE;
    }
    else
    {
        AcpiOsPrintf ("Could not open debug file %s\n", Name);
    }

#endif
}
#endif


#ifdef ACPI_APPLICATION
/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCheckTextModeCorruption
 *
 * PARAMETERS:  Table           - Table buffer
 *              TableLength     - Length of table from the table header
 *              FileLength      - Length of the file that contains the table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check table for text mode file corruption where all linefeed
 *              characters (LF) have been replaced by carriage return linefeed
 *              pairs (CR/LF).
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbCheckTextModeCorruption (
    UINT8                   *Table,
    UINT32                  TableLength,
    UINT32                  FileLength)
{
    UINT32                  i;
    UINT32                  Pairs = 0;


    if (TableLength != FileLength)
    {
        ACPI_WARNING ((AE_INFO,
            "File length (0x%X) is not the same as the table length (0x%X)",
            FileLength, TableLength));
    }

    /* Scan entire table to determine if each LF has been prefixed with a CR */

    for (i = 1; i < FileLength; i++)
    {
        if (Table[i] == 0x0A)
        {
            if (Table[i - 1] != 0x0D)
            {
                /* The LF does not have a preceeding CR, table not corrupted */

                return (AE_OK);
            }
            else
            {
                /* Found a CR/LF pair */

                Pairs++;
            }
            i++;
        }
    }

    if (!Pairs)
    {
        return (AE_OK);
    }

    /*
     * Entire table scanned, each CR is part of a CR/LF pair --
     * meaning that the table was treated as a text file somewhere.
     *
     * NOTE: We can't "fix" the table, because any existing CR/LF pairs in the
     * original table are left untouched by the text conversion process --
     * meaning that we cannot simply replace CR/LF pairs with LFs.
     */
    AcpiOsPrintf ("Table has been corrupted by text mode conversion\n");
    AcpiOsPrintf ("All LFs (%d) were changed to CR/LF pairs\n", Pairs);
    AcpiOsPrintf ("Table cannot be repaired!\n");
    return (AE_BAD_VALUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbReadTable
 *
 * PARAMETERS:  fp              - File that contains table
 *              Table           - Return value, buffer with table
 *              TableLength     - Return value, length of table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load the DSDT from the file pointer
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDbReadTable (
    FILE                    *fp,
    ACPI_TABLE_HEADER       **Table,
    UINT32                  *TableLength)
{
    ACPI_TABLE_HEADER       TableHeader;
    UINT32                  Actual;
    ACPI_STATUS             Status;
    UINT32                  FileSize;
    BOOLEAN                 StandardHeader = TRUE;


    /* Get the file size */

    fseek (fp, 0, SEEK_END);
    FileSize = (UINT32) ftell (fp);
    fseek (fp, 0, SEEK_SET);

    if (FileSize < 4)
    {
        return (AE_BAD_HEADER);
    }

    /* Read the signature */

    if (fread (&TableHeader, 1, 4, fp) != 4)
    {
        AcpiOsPrintf ("Could not read the table signature\n");
        return (AE_BAD_HEADER);
    }

    fseek (fp, 0, SEEK_SET);

    /* The RSDT and FACS tables do not have standard ACPI headers */

    if (ACPI_COMPARE_NAME (TableHeader.Signature, "RSD ") ||
        ACPI_COMPARE_NAME (TableHeader.Signature, "FACS"))
    {
        *TableLength = FileSize;
        StandardHeader = FALSE;
    }
    else
    {
    /* Read the table header */

        if (fread (&TableHeader, 1, sizeof (TableHeader), fp) !=
                sizeof (ACPI_TABLE_HEADER))
        {
            AcpiOsPrintf ("Could not read the table header\n");
            return (AE_BAD_HEADER);
        }

#if 0
        /* Validate the table header/length */

        Status = AcpiTbValidateTableHeader (&TableHeader);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Table header is invalid!\n");
            return (Status);
        }
#endif

        /* File size must be at least as long as the Header-specified length */

        if (TableHeader.Length > FileSize)
        {
            AcpiOsPrintf (
                "TableHeader length [0x%X] greater than the input file size [0x%X]\n",
                TableHeader.Length, FileSize);
            return (AE_BAD_HEADER);
        }

#ifdef ACPI_OBSOLETE_CODE
        /* We only support a limited number of table types */

        if (ACPI_STRNCMP ((char *) TableHeader.Signature, DSDT_SIG, 4) &&
            ACPI_STRNCMP ((char *) TableHeader.Signature, PSDT_SIG, 4) &&
            ACPI_STRNCMP ((char *) TableHeader.Signature, SSDT_SIG, 4))
        {
            AcpiOsPrintf ("Table signature [%4.4s] is invalid or not supported\n",
                (char *) TableHeader.Signature);
            ACPI_DUMP_BUFFER (&TableHeader, sizeof (ACPI_TABLE_HEADER));
            return (AE_ERROR);
        }
#endif

        *TableLength = TableHeader.Length;
    }

    /* Allocate a buffer for the table */

    *Table = AcpiOsAllocate ((size_t) FileSize);
    if (!*Table)
    {
        AcpiOsPrintf (
            "Could not allocate memory for ACPI table %4.4s (size=0x%X)\n",
            TableHeader.Signature, *TableLength);
        return (AE_NO_MEMORY);
    }

    /* Get the rest of the table */

    fseek (fp, 0, SEEK_SET);
    Actual = fread (*Table, 1, (size_t) FileSize, fp);
    if (Actual == FileSize)
    {
        if (StandardHeader)
        {
            /* Now validate the checksum */

            Status = AcpiTbChecksum ((void *) *Table,
                        ACPI_CAST_PTR (ACPI_TABLE_HEADER, *Table)->Length);

            if (Status == AE_BAD_CHECKSUM)
            {
                Status = AcpiDbCheckTextModeCorruption ((UINT8 *) *Table,
                            FileSize, (*Table)->Length);
                return (Status);
            }
        }
        return (AE_OK);
    }

    if (Actual > 0)
    {
        AcpiOsPrintf ("Warning - reading table, asked for %X got %X\n",
            FileSize, Actual);
        return (AE_OK);
    }

    AcpiOsPrintf ("Error - could not read the table file\n");
    AcpiOsFree (*Table);
    *Table = NULL;
    *TableLength = 0;

    return (AE_ERROR);
}


/*******************************************************************************
 *
 * FUNCTION:    AeLocalLoadTable
 *
 * PARAMETERS:  Table           - pointer to a buffer containing the entire
 *                                table to be loaded
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to load a table from the caller's
 *              buffer.  The buffer must contain an entire ACPI Table including
 *              a valid header.  The header fields will be verified, and if it
 *              is determined that the table is invalid, the call will fail.
 *
 ******************************************************************************/

static ACPI_STATUS
AeLocalLoadTable (
    ACPI_TABLE_HEADER       *Table)
{
    ACPI_STATUS             Status = AE_OK;
/*    ACPI_TABLE_DESC         TableInfo; */


    ACPI_FUNCTION_TRACE (AeLocalLoadTable);
#if 0


    if (!Table)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    TableInfo.Pointer = Table;
    Status = AcpiTbRecognizeTable (&TableInfo, ACPI_TABLE_ALL);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Install the new table into the local data structures */

    Status = AcpiTbInstallTable (&TableInfo);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_ALREADY_EXISTS)
        {
            /* Table already exists, no error */

            Status = AE_OK;
        }

        /* Free table allocated by AcpiTbGetTable */

        AcpiTbDeleteSingleTable (&TableInfo);
        return_ACPI_STATUS (Status);
    }

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))

    Status = AcpiNsLoadTable (TableInfo.InstalledDesc, AcpiGbl_RootNode);
    if (ACPI_FAILURE (Status))
    {
        /* Uninstall table and free the buffer */

        AcpiTbDeleteTablesByType (ACPI_TABLE_ID_DSDT);
        return_ACPI_STATUS (Status);
    }
#endif
#endif

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbReadTableFromFile
 *
 * PARAMETERS:  Filename         - File where table is located
 *              Table            - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get an ACPI table from a file
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbReadTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **Table)
{
    FILE                    *fp;
    UINT32                  TableLength;
    ACPI_STATUS             Status;


    /* Open the file */

    fp = fopen (Filename, "rb");
    if (!fp)
    {
        AcpiOsPrintf ("Could not open input file %s\n", Filename);
        return (AE_ERROR);
    }

    /* Get the entire file */

    fprintf (stderr, "Loading Acpi table from file %s\n", Filename);
    Status = AcpiDbReadTable (fp, Table, &TableLength);
    fclose(fp);

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not get table from the file\n");
        return (Status);
    }

    return (AE_OK);
 }
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetTableFromFile
 *
 * PARAMETERS:  Filename        - File where table is located
 *              ReturnTable     - Where a pointer to the table is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load an ACPI table from a file
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbGetTableFromFile (
    char                    *Filename,
    ACPI_TABLE_HEADER       **ReturnTable)
{
#ifdef ACPI_APPLICATION
    ACPI_STATUS             Status;
    ACPI_TABLE_HEADER       *Table;
    BOOLEAN                 IsAmlTable = TRUE;


    Status = AcpiDbReadTableFromFile (Filename, &Table);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

#ifdef ACPI_DATA_TABLE_DISASSEMBLY
    IsAmlTable = AcpiUtIsAmlTable (Table);
#endif

    if (IsAmlTable)
    {
        /* Attempt to recognize and install the table */

        Status = AeLocalLoadTable (Table);
        if (ACPI_FAILURE (Status))
        {
            if (Status == AE_ALREADY_EXISTS)
            {
                AcpiOsPrintf ("Table %4.4s is already installed\n",
                    Table->Signature);
            }
            else
            {
                AcpiOsPrintf ("Could not install table, %s\n",
                    AcpiFormatException (Status));
            }

            return (Status);
        }

        fprintf (stderr,
            "Acpi table [%4.4s] successfully installed and loaded\n",
            Table->Signature);
    }

    AcpiGbl_AcpiHardwarePresent = FALSE;
    if (ReturnTable)
    {
        *ReturnTable = Table;
    }


#endif  /* ACPI_APPLICATION */
    return (AE_OK);
}

#endif  /* ACPI_DEBUGGER */

