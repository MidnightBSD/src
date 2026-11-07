/*******************************************************************************
 *
 * Module Name: dbfileio - Debugger file I/O commands. These can't usually
 *              be used when running the debugger in Ring 0 (Kernel mode)
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdebug.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbfileio")


#ifdef ACPI_APPLICATION
#include <contrib/dev/acpica/include/acapps.h>


#ifdef ACPI_DEBUGGER
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

    if (AcpiGbl_DebugFile)
    {
       fclose (AcpiGbl_DebugFile);
       AcpiGbl_DebugFile = NULL;
       AcpiGbl_DbOutputToFile = FALSE;
       AcpiOsPrintf ("Debug output file %s closed\n",
            AcpiGbl_DbDebugFilename);
    }
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

    AcpiDbCloseDebugFile ();
    AcpiGbl_DebugFile = fopen (Name, "w+");
    if (!AcpiGbl_DebugFile)
    {
        AcpiOsPrintf ("Could not open debug file %s\n", Name);
        return;
    }

    AcpiOsPrintf ("Debug output file %s opened\n", Name);
    AcpiUtSafeStrncpy (AcpiGbl_DbDebugFilename, Name,
        sizeof (AcpiGbl_DbDebugFilename));
    AcpiGbl_DbOutputToFile = TRUE;
}
#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbLoadTables
 *
 * PARAMETERS:  ListHead        - List of ACPI tables to load
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Load ACPI tables from a previously constructed table list.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbLoadTables (
    ACPI_NEW_TABLE_DESC     *ListHead)
{
    ACPI_STATUS             Status;
    ACPI_NEW_TABLE_DESC     *TableListHead;
    ACPI_TABLE_HEADER       *Table;


    /* Load all ACPI tables in the list */

    TableListHead = ListHead;
    while (TableListHead)
    {
        Table = TableListHead->Table;

        Status = AcpiLoadTable (Table, NULL);
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

        AcpiOsPrintf ("Acpi table [%4.4s] successfully installed and loaded\n",
            Table->Signature);

        TableListHead = TableListHead->Next;
    }

    return (AE_OK);
}
#endif
