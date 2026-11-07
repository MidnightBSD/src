/******************************************************************************
 *
 * Module Name: utcksum - Support generating table checksums
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
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acutils.h>


/* This module used for application-level code only */

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("utcksum")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtVerifyChecksum
 *
 * PARAMETERS:  Table               - ACPI table to verify
 *              Length              - Length of entire table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Verifies that the table checksums to zero. Optionally returns
 *              exception on bad checksum.
 *              Note: We don't have to check for a CDAT here, since CDAT is
 *              not in the RSDT/XSDT, and the CDAT table is never installed
 *              via ACPICA.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtVerifyChecksum (
    ACPI_TABLE_HEADER       *Table,
    UINT32                  Length)
{
    UINT8                   Checksum;


    /*
     * FACS/S3PT:
     * They are the odd tables, have no standard ACPI header and no checksum
     */
    if (ACPI_COMPARE_NAMESEG (Table->Signature, ACPI_SIG_S3PT) ||
        ACPI_COMPARE_NAMESEG (Table->Signature, ACPI_SIG_FACS))
    {
        return (AE_OK);
    }

    /* Compute the checksum on the table */

    Length = Table->Length;
    Checksum = AcpiUtGenerateChecksum (ACPI_CAST_PTR (UINT8, Table), Length, Table->Checksum);

    /* Computed checksum matches table? */

    if (Checksum != Table->Checksum)
    {
        ACPI_BIOS_WARNING ((AE_INFO,
            "Incorrect checksum in table [%4.4s] - 0x%2.2X, "
            "should be 0x%2.2X",
            Table->Signature, Table->Checksum,
            Checksum));

#if (ACPI_CHECKSUM_ABORT)
        return (AE_BAD_CHECKSUM);
#endif
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtVerifyCdatChecksum
 *
 * PARAMETERS:  Table               - CDAT ACPI table to verify
 *              Length              - Length of entire table
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Verifies that the CDAT table checksums to zero. Optionally
 *              returns an exception on bad checksum.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtVerifyCdatChecksum (
    ACPI_TABLE_CDAT         *CdatTable,
    UINT32                  Length)
{
    UINT8                   Checksum;


    /* Compute the checksum on the table */

    Checksum = AcpiUtGenerateChecksum (ACPI_CAST_PTR (UINT8, CdatTable),
                    CdatTable->Length, CdatTable->Checksum);

    /* Computed checksum matches table? */

    if (Checksum != CdatTable->Checksum)
    {
        ACPI_BIOS_WARNING ((AE_INFO,
            "Incorrect checksum in table [%4.4s] - 0x%2.2X, "
            "should be 0x%2.2X",
            AcpiGbl_CDAT, CdatTable->Checksum, Checksum));

#if (ACPI_CHECKSUM_ABORT)
        return (AE_BAD_CHECKSUM);
#endif
    }

    CdatTable->Checksum = Checksum;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGenerateChecksum
 *
 * PARAMETERS:  Table               - Pointer to table to be checksummed
 *              Length              - Length of the table
 *              OriginalChecksum    - Value of the checksum field
 *
 * RETURN:      8 bit checksum of buffer
 *
 * DESCRIPTION: Computes an 8 bit checksum of the table.
 *
 ******************************************************************************/

UINT8
AcpiUtGenerateChecksum (
    void                    *Table,
    UINT32                  Length,
    UINT8                   OriginalChecksum)
{
    UINT8                   Checksum;


    /* Sum the entire table as-is */

    Checksum = AcpiUtChecksum ((UINT8 *) Table, Length);

    /* Subtract off the existing checksum value in the table */

    Checksum = (UINT8) (Checksum - OriginalChecksum);

    /* Compute and return the final checksum */

    Checksum = (UINT8) (0 - Checksum);
    return (Checksum);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtChecksum
 *
 * PARAMETERS:  Buffer          - Pointer to memory region to be checked
 *              Length          - Length of this memory region
 *
 * RETURN:      Checksum (UINT8)
 *
 * DESCRIPTION: Calculates circular checksum of memory region.
 *
 ******************************************************************************/

UINT8
AcpiUtChecksum (
    UINT8                   *Buffer,
    UINT32                  Length)
{
    UINT8                   Sum = 0;
    UINT8                   *End = Buffer + Length;


    while (Buffer < End)
    {
        Sum = (UINT8) (Sum + *(Buffer++));
    }

    return (Sum);
}
