/******************************************************************************
 *
 * Module Name: utascii - Utility ascii functions
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


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidNameseg
 *
 * PARAMETERS:  Name            - The name or table signature to be examined.
 *                                Four characters, does not have to be a
 *                                NULL terminated string.
 *
 * RETURN:      TRUE if signature is has 4 valid ACPI characters
 *
 * DESCRIPTION: Validate an ACPI table signature.
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidNameseg (
    char                    *Name)
{
    UINT32                  i;


    /* Validate each character in the signature */

    for (i = 0; i < ACPI_NAMESEG_SIZE; i++)
    {
        if (!AcpiUtValidNameChar (Name[i], i))
        {
            return (FALSE);
        }
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidNameChar
 *
 * PARAMETERS:  Char            - The character to be examined
 *              Position        - Byte position (0-3)
 *
 * RETURN:      TRUE if the character is valid, FALSE otherwise
 *
 * DESCRIPTION: Check for a valid ACPI character. Must be one of:
 *              1) Upper case alpha
 *              2) numeric
 *              3) underscore
 *
 *              We allow a '!' as the last character because of the ASF! table
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidNameChar (
    char                    Character,
    UINT32                  Position)
{

    if (!((Character >= 'A' && Character <= 'Z') ||
          (Character >= '0' && Character <= '9') ||
          (Character == '_')))
    {
        /* Allow a '!' in the last position */

        if (Character == '!' && Position == 3)
        {
            return (TRUE);
        }

        return (FALSE);
    }

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCheckAndRepairAscii
 *
 * PARAMETERS:  Name                - Ascii string
 *              Count               - Number of characters to check
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ensure that the requested number of characters are printable
 *              Ascii characters. Sets non-printable and null chars to <space>.
 *
 ******************************************************************************/

void
AcpiUtCheckAndRepairAscii (
    UINT8                   *Name,
    char                    *RepairedName,
    UINT32                  Count)
{
    UINT32                  i;


    for (i = 0; i < Count; i++)
    {
        RepairedName[i] = (char) Name[i];

        if (!Name[i])
        {
            return;
        }
        if (!isprint (Name[i]))
        {
            RepairedName[i] = ' ';
        }
    }
}
