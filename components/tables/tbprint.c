/******************************************************************************
 *
 * Module Name: tbprint - Table output utilities
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
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acutils.h>

#define _COMPONENT          ACPI_TABLES
        ACPI_MODULE_NAME    ("tbprint")


/* Local prototypes */

static void
AcpiTbFixString (
    char                    *String,
    ACPI_SIZE               Length);

static void
AcpiTbCleanupTableHeader (
    ACPI_TABLE_HEADER       *OutHeader,
    ACPI_TABLE_HEADER       *Header);


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbFixString
 *
 * PARAMETERS:  String              - String to be repaired
 *              Length              - Maximum length
 *
 * RETURN:      None
 *
 * DESCRIPTION: Replace every non-printable or non-ascii byte in the string
 *              with a question mark '?'.
 *
 ******************************************************************************/

static void
AcpiTbFixString (
    char                    *String,
    ACPI_SIZE               Length)
{

    while (Length && *String)
    {
        if (!isprint ((int) (UINT8) *String))
        {
            *String = '?';
        }

        String++;
        Length--;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbCleanupTableHeader
 *
 * PARAMETERS:  OutHeader           - Where the cleaned header is returned
 *              Header              - Input ACPI table header
 *
 * RETURN:      Returns the cleaned header in OutHeader
 *
 * DESCRIPTION: Copy the table header and ensure that all "string" fields in
 *              the header consist of printable characters.
 *
 ******************************************************************************/

static void
AcpiTbCleanupTableHeader (
    ACPI_TABLE_HEADER       *OutHeader,
    ACPI_TABLE_HEADER       *Header)
{

    memcpy (OutHeader, Header, sizeof (ACPI_TABLE_HEADER));

    AcpiTbFixString (OutHeader->Signature, ACPI_NAMESEG_SIZE);
    AcpiTbFixString (OutHeader->OemId, ACPI_OEM_ID_SIZE);
    AcpiTbFixString (OutHeader->OemTableId, ACPI_OEM_TABLE_ID_SIZE);
    AcpiTbFixString (OutHeader->AslCompilerId, ACPI_NAMESEG_SIZE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiTbPrintTableHeader
 *
 * PARAMETERS:  Address             - Table physical address
 *              Header              - Table header
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print an ACPI table header. Special cases for FACS and RSDP.
 *
 ******************************************************************************/

void
AcpiTbPrintTableHeader (
    ACPI_PHYSICAL_ADDRESS   Address,
    ACPI_TABLE_HEADER       *Header)
{
    ACPI_TABLE_HEADER       LocalHeader;

#pragma GCC diagnostic push
#if defined(__GNUC__) && __GNUC__ >= 11
#pragma GCC diagnostic ignored "-Wstringop-overread"
#endif

    if (ACPI_COMPARE_NAMESEG (Header->Signature, ACPI_SIG_FACS))
    {
        /* FACS only has signature and length fields */

        ACPI_INFO (("%-4.4s 0x%8.8X%8.8X %06X",
            Header->Signature, ACPI_FORMAT_UINT64 (Address),
            Header->Length));
    }
    else if (ACPI_VALIDATE_RSDP_SIG (ACPI_CAST_PTR (ACPI_TABLE_RSDP,
        Header)->Signature))
    {
        /* RSDP has no common fields */

        memcpy (LocalHeader.OemId, ACPI_CAST_PTR (ACPI_TABLE_RSDP,
            Header)->OemId, ACPI_OEM_ID_SIZE);
        AcpiTbFixString (LocalHeader.OemId, ACPI_OEM_ID_SIZE);

        ACPI_INFO (("RSDP 0x%8.8X%8.8X %06X (v%.2d %-6.6s)",
            ACPI_FORMAT_UINT64 (Address),
            (ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Revision > 0) ?
                ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Length : 20,
            ACPI_CAST_PTR (ACPI_TABLE_RSDP, Header)->Revision,
            LocalHeader.OemId));
    }
    else if (AcpiGbl_CDAT && !AcpiUtValidNameseg (Header->Signature))
    {
	/* CDAT does not use the common ACPI table header */

        ACPI_INFO (("%-4.4s 0x%8.8X%8.8X %06X",
            ACPI_SIG_CDAT, ACPI_FORMAT_UINT64 (Address),
            ACPI_CAST_PTR (ACPI_TABLE_CDAT, Header)->Length));
    }
    else
    {
        /* Standard ACPI table with full common header */

        AcpiTbCleanupTableHeader (&LocalHeader, Header);

        ACPI_INFO ((
            "%-4.4s 0x%8.8X%8.8X"
            " %06X (v%.2d %-6.6s %-8.8s %08X %-4.4s %08X)",
            LocalHeader.Signature, ACPI_FORMAT_UINT64 (Address),
            LocalHeader.Length, LocalHeader.Revision, LocalHeader.OemId,
            LocalHeader.OemTableId, LocalHeader.OemRevision,
            LocalHeader.AslCompilerId, LocalHeader.AslCompilerRevision));
    }
#pragma GCC diagnostic pop
}
