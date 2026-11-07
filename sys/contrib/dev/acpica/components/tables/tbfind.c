/******************************************************************************
 *
 * Module Name: tbfind   - find table
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/actables.h>

#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbfind")


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbFindTable
 *
 * PARAMETERS:  Signature           - String with ACPI table signature
 *              OemId               - String with the table OEM ID
 *              OemTableId          - String with the OEM Table ID
 *              TableIndex          - Where the table index is returned
 *
 * RETURN:      Status and table index
 *
 * DESCRIPTION: Find an ACPI table (in the RSDT/XSDT) that matches the
 *              Signature, OEM ID and OEM Table ID. Returns an index that can
 *              be used to get the table header or entire table.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiTbFindTable (
    char                    *Signature,
    char                    *OemId,
    char                    *OemTableId,
    UINT32                  *TableIndex)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_TABLE_HEADER       Header;
    UINT32                  i;


    ACPI_FUNCTION_TRACE (TbFindTable);


    /* Validate the input table signature */

    if (!AcpiUtValidNameseg (Signature))
    {
        return_ACPI_STATUS (AE_BAD_SIGNATURE);
    }

    /* Don't allow the OEM strings to be too long */

    if ((strlen (OemId) > ACPI_OEM_ID_SIZE) ||
        (strlen (OemTableId) > ACPI_OEM_TABLE_ID_SIZE))
    {
        return_ACPI_STATUS (AE_AML_STRING_LIMIT);
    }

    /* Normalize the input strings */

    memset (&Header, 0, sizeof (ACPI_TABLE_HEADER));
    ACPI_COPY_NAMESEG (Header.Signature, Signature);
    if (strlen (OemId) >= ACPI_OEM_ID_SIZE)
    {
        memcpy (Header.OemId, OemId, ACPI_OEM_ID_SIZE);
    }
    else
    {
        memcpy (Header.OemId, OemId, strlen (OemId));
        /* Remainder is already zeroed by memset above */
    }

    if (strlen (OemTableId) >= ACPI_OEM_TABLE_ID_SIZE)
    {
        memcpy (Header.OemTableId, OemTableId, ACPI_OEM_TABLE_ID_SIZE);
    }
    else
    {
        memcpy (Header.OemTableId, OemTableId, strlen (OemTableId));
        /* Remainder is already zeroed by memset above */
    }

    /* Search for the table */

    (void) AcpiUtAcquireMutex (ACPI_MTX_TABLES);
    for (i = 0; i < AcpiGbl_RootTableList.CurrentTableCount; ++i)
    {
        if (memcmp (&(AcpiGbl_RootTableList.Tables[i].Signature),
            Header.Signature, ACPI_NAMESEG_SIZE))
        {
            /* Not the requested table */

            continue;
        }

        /* Table with matching signature has been found */

        if (!AcpiGbl_RootTableList.Tables[i].Pointer)
        {
            /* Table is not currently mapped, map it */

            Status = AcpiTbValidateTable (&AcpiGbl_RootTableList.Tables[i]);
            if (ACPI_FAILURE (Status))
            {
                goto UnlockAndExit;
            }

            if (!AcpiGbl_RootTableList.Tables[i].Pointer)
            {
                continue;
            }
        }

        /* Check for table match on all IDs */

        if (!memcmp (AcpiGbl_RootTableList.Tables[i].Pointer->Signature,
                Header.Signature, ACPI_NAMESEG_SIZE) &&
            (!OemId[0] ||
             !memcmp (AcpiGbl_RootTableList.Tables[i].Pointer->OemId,
                 Header.OemId, ACPI_OEM_ID_SIZE)) &&
            (!OemTableId[0] ||
             !memcmp (AcpiGbl_RootTableList.Tables[i].Pointer->OemTableId,
                 Header.OemTableId, ACPI_OEM_TABLE_ID_SIZE)))
        {
            *TableIndex = i;

            ACPI_DEBUG_PRINT ((ACPI_DB_TABLES, "Found table [%4.4s]\n",
                Header.Signature));
            goto UnlockAndExit;
        }
    }
    Status = AE_NOT_FOUND;

UnlockAndExit:
    (void) AcpiUtReleaseMutex (ACPI_MTX_TABLES);
    return_ACPI_STATUS (Status);
}
