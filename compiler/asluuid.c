/******************************************************************************
 *
 * Module Name: asluuid-- compiler UUID support
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asluuid")


extern UINT8                AcpiGbl_MapToUuidOffset[UUID_BUFFER_LENGTH];


/*******************************************************************************
 *
 * FUNCTION:    AuValiduateUuid
 *
 * PARAMETERS:  InString            - 36-byte formatted UUID string
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check all 36 characters for correct format
 *
 ******************************************************************************/

ACPI_STATUS
AuValidateUuid (
    char                    *InString)
{
    UINT32                  i;


    if (!InString || (strlen (InString) != UUID_STRING_LENGTH))
    {
        return (AE_BAD_PARAMETER);
    }

    /* Check all 36 characters for correct format */

    for (i = 0; i < UUID_STRING_LENGTH; i++)
    {
        /* Must have 4 hyphens (dashes) in these positions: */

        if ((i == UUID_HYPHEN1_OFFSET) ||
            (i == UUID_HYPHEN2_OFFSET) ||
            (i == UUID_HYPHEN3_OFFSET) ||
            (i == UUID_HYPHEN4_OFFSET))
        {
            if (InString[i] != '-')
            {
                return (AE_BAD_PARAMETER);
            }
        }
        else
        {
            /* All other positions must contain hex digits */

            if (!isxdigit ((int) InString[i]))
            {
                return (AE_BAD_PARAMETER);
            }
        }
    }

    return (AE_OK);
}
