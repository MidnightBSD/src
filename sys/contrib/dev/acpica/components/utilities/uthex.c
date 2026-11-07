/******************************************************************************
 *
 * Module Name: uthex -- Hex/ASCII support functions
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
        ACPI_MODULE_NAME    ("uthex")


/* Hex to ASCII conversion table */

static const char           AcpiGbl_HexToAscii[] =
{
    '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtHexToAsciiChar
 *
 * PARAMETERS:  Integer             - Contains the hex digit
 *              Position            - bit position of the digit within the
 *                                    integer (multiple of 4)
 *
 * RETURN:      The converted Ascii character
 *
 * DESCRIPTION: Convert a hex digit to an Ascii character
 *
 ******************************************************************************/

char
AcpiUtHexToAsciiChar (
    UINT64                  Integer,
    UINT32                  Position)
{
    UINT64                  Index;

    AcpiUtShortShiftRight (Integer, Position, &Index);
    return (AcpiGbl_HexToAscii[Index & 0xF]);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAsciiToHexByte
 *
 * PARAMETERS:  TwoAsciiChars               - Pointer to two ASCII characters
 *              ReturnByte                  - Where converted byte is returned
 *
 * RETURN:      Status and converted hex byte
 *
 * DESCRIPTION: Perform ascii-to-hex translation, exactly two ASCII characters
 *              to a single converted byte value.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtAsciiToHexByte (
    char                    *TwoAsciiChars,
    UINT8                   *ReturnByte)
{

    /* Both ASCII characters must be valid hex digits */

    if (!isxdigit ((int) TwoAsciiChars[0]) ||
        !isxdigit ((int) TwoAsciiChars[1]))
    {
        return (AE_BAD_HEX_CONSTANT);
    }

    *ReturnByte =
        AcpiUtAsciiCharToHex (TwoAsciiChars[1]) |
        (AcpiUtAsciiCharToHex (TwoAsciiChars[0]) << 4);

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAsciiCharToHex
 *
 * PARAMETERS:  HexChar                 - Hex character in Ascii. Must be:
 *                                        0-9 or A-F or a-f
 *
 * RETURN:      The binary value of the ascii/hex character
 *
 * DESCRIPTION: Perform ascii-to-hex translation
 *
 ******************************************************************************/

UINT8
AcpiUtAsciiCharToHex (
    int                     HexChar)
{

    /* Values 0-9 */

    if (HexChar <= '9')
    {
        return ((UINT8) (HexChar - '0'));
    }

    /* Upper case A-F */

    if (HexChar <= 'F')
    {
        return ((UINT8) (HexChar - 0x37));
    }

    /* Lower case a-f */

    return ((UINT8) (HexChar - 0x57));
}
