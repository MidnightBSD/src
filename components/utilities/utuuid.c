/******************************************************************************
 *
 * Module Name: utuuid -- UUID support functions
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

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("utuuid")


#if (defined ACPI_ASL_COMPILER || defined ACPI_EXEC_APP || defined ACPI_HELP_APP)
/*
 * UUID support functions.
 *
 * This table is used to convert an input UUID ascii string to a 16 byte
 * buffer and the reverse. The table maps a UUID buffer index 0-15 to
 * the index within the 36-byte UUID string where the associated 2-byte
 * hex value can be found.
 *
 * 36-byte UUID strings are of the form:
 *     aabbccdd-eeff-gghh-iijj-kkllmmnnoopp
 * Where aa-pp are one byte hex numbers, made up of two hex digits
 *
 * Note: This table is basically the inverse of the string-to-offset table
 * found in the ACPI spec in the description of the ToUUID macro.
 */
const UINT8    AcpiGbl_MapToUuidOffset[UUID_BUFFER_LENGTH] =
{
    6,4,2,0,11,9,16,14,19,21,24,26,28,30,32,34
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtConvertStringToUuid
 *
 * PARAMETERS:  InString            - 36-byte formatted UUID string
 *              UuidBuffer          - Where the 16-byte UUID buffer is returned
 *
 * RETURN:      None. Output data is returned in the UuidBuffer
 *
 * DESCRIPTION: Convert a 36-byte formatted UUID string to 16-byte UUID buffer
 *
 ******************************************************************************/

void
AcpiUtConvertStringToUuid (
    char                    *InString,
    UINT8                   *UuidBuffer)
{
    UINT32                  i;


    for (i = 0; i < UUID_BUFFER_LENGTH; i++)
    {
        UuidBuffer[i] = (AcpiUtAsciiCharToHex (
            InString[AcpiGbl_MapToUuidOffset[i]]) << 4);

        UuidBuffer[i] |= AcpiUtAsciiCharToHex (
            InString[AcpiGbl_MapToUuidOffset[i] + 1]);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtConvertUuidToString
 *
 * PARAMETERS:  UuidBuffer          - 16-byte UUID buffer
 *              OutString           - 36-byte formatted UUID string
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Convert 16-byte UUID buffer to 36-byte formatted UUID string
 *              OutString must be 37 bytes to include null terminator.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtConvertUuidToString (
    char                    *UuidBuffer,
    char                    *OutString)
{
    UINT32                  i;


    if (!UuidBuffer || !OutString)
    {
        return (AE_BAD_PARAMETER);
    }

    for (i = 0; i < UUID_BUFFER_LENGTH; i++)
    {
        OutString[AcpiGbl_MapToUuidOffset[i]] =
            AcpiUtHexToAsciiChar (UuidBuffer[i], 4);

        OutString[AcpiGbl_MapToUuidOffset[i] + 1] =
            AcpiUtHexToAsciiChar (UuidBuffer[i], 0);
    }

    /* Insert required hyphens (dashes) */

    OutString[UUID_HYPHEN1_OFFSET] =
    OutString[UUID_HYPHEN2_OFFSET] =
    OutString[UUID_HYPHEN3_OFFSET] =
    OutString[UUID_HYPHEN4_OFFSET] = '-';

    OutString[UUID_STRING_LENGTH] = 0; /* Null terminate */
    return (AE_OK);
}
#endif
