/*******************************************************************************
 *
 * Module Name: rsmem24 - Memory resource descriptors
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
#include <contrib/dev/acpica/include/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsmemory")


/*******************************************************************************
 *
 * AcpiRsConvertMemory24
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsConvertMemory24[4] =
{
    {ACPI_RSC_INITGET,  ACPI_RESOURCE_TYPE_MEMORY24,
                        ACPI_RS_SIZE (ACPI_RESOURCE_MEMORY24),
                        ACPI_RSC_TABLE_SIZE (AcpiRsConvertMemory24)},

    {ACPI_RSC_INITSET,  ACPI_RESOURCE_NAME_MEMORY24,
                        sizeof (AML_RESOURCE_MEMORY24),
                        0},

    /* Read/Write bit */

    {ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET (Data.Memory24.WriteProtect),
                        AML_OFFSET (Memory24.Flags),
                        0},
    /*
     * These fields are contiguous in both the source and destination:
     * Minimum Base Address
     * Maximum Base Address
     * Address Base Alignment
     * Range Length
     */
    {ACPI_RSC_MOVE16,   ACPI_RS_OFFSET (Data.Memory24.Minimum),
                        AML_OFFSET (Memory24.Minimum),
                        4}
};


/*******************************************************************************
 *
 * AcpiRsConvertMemory32
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsConvertMemory32[4] =
{
    {ACPI_RSC_INITGET,  ACPI_RESOURCE_TYPE_MEMORY32,
                        ACPI_RS_SIZE (ACPI_RESOURCE_MEMORY32),
                        ACPI_RSC_TABLE_SIZE (AcpiRsConvertMemory32)},

    {ACPI_RSC_INITSET,  ACPI_RESOURCE_NAME_MEMORY32,
                        sizeof (AML_RESOURCE_MEMORY32),
                        0},

    /* Read/Write bit */

    {ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET (Data.Memory32.WriteProtect),
                        AML_OFFSET (Memory32.Flags),
                        0},
    /*
     * These fields are contiguous in both the source and destination:
     * Minimum Base Address
     * Maximum Base Address
     * Address Base Alignment
     * Range Length
     */
    {ACPI_RSC_MOVE32,   ACPI_RS_OFFSET (Data.Memory32.Minimum),
                        AML_OFFSET (Memory32.Minimum),
                        4}
};


/*******************************************************************************
 *
 * AcpiRsConvertFixedMemory32
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsConvertFixedMemory32[4] =
{
    {ACPI_RSC_INITGET,  ACPI_RESOURCE_TYPE_FIXED_MEMORY32,
                        ACPI_RS_SIZE (ACPI_RESOURCE_FIXED_MEMORY32),
                        ACPI_RSC_TABLE_SIZE (AcpiRsConvertFixedMemory32)},

    {ACPI_RSC_INITSET,  ACPI_RESOURCE_NAME_FIXED_MEMORY32,
                        sizeof (AML_RESOURCE_FIXED_MEMORY32),
                        0},

    /* Read/Write bit */

    {ACPI_RSC_1BITFLAG, ACPI_RS_OFFSET (Data.FixedMemory32.WriteProtect),
                        AML_OFFSET (FixedMemory32.Flags),
                        0},
    /*
     * These fields are contiguous in both the source and destination:
     * Base Address
     * Range Length
     */
    {ACPI_RSC_MOVE32,   ACPI_RS_OFFSET (Data.FixedMemory32.Address),
                        AML_OFFSET (FixedMemory32.Address),
                        2}
};


/*******************************************************************************
 *
 * AcpiRsGetVendorSmall
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsGetVendorSmall[3] =
{
    {ACPI_RSC_INITGET,  ACPI_RESOURCE_TYPE_VENDOR,
                        ACPI_RS_SIZE (ACPI_RESOURCE_VENDOR),
                        ACPI_RSC_TABLE_SIZE (AcpiRsGetVendorSmall)},

    /* Length of the vendor data (byte count) */

    {ACPI_RSC_COUNT16,  ACPI_RS_OFFSET (Data.Vendor.ByteLength),
                        0,
                        sizeof (UINT8)},

    /* Vendor data */

    {ACPI_RSC_MOVE8,    ACPI_RS_OFFSET (Data.Vendor.ByteData[0]),
                        sizeof (AML_RESOURCE_SMALL_HEADER),
                        0}
};


/*******************************************************************************
 *
 * AcpiRsGetVendorLarge
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsGetVendorLarge[3] =
{
    {ACPI_RSC_INITGET,  ACPI_RESOURCE_TYPE_VENDOR,
                        ACPI_RS_SIZE (ACPI_RESOURCE_VENDOR),
                        ACPI_RSC_TABLE_SIZE (AcpiRsGetVendorLarge)},

    /* Length of the vendor data (byte count) */

    {ACPI_RSC_COUNT16,  ACPI_RS_OFFSET (Data.Vendor.ByteLength),
                        0,
                        sizeof (UINT8)},

    /* Vendor data */

    {ACPI_RSC_MOVE8,    ACPI_RS_OFFSET (Data.Vendor.ByteData[0]),
                        sizeof (AML_RESOURCE_LARGE_HEADER),
                        0}
};


/*******************************************************************************
 *
 * AcpiRsSetVendor
 *
 ******************************************************************************/

ACPI_RSCONVERT_INFO     AcpiRsSetVendor[7] =
{
    /* Default is a small vendor descriptor */

    {ACPI_RSC_INITSET,  ACPI_RESOURCE_NAME_VENDOR_SMALL,
                        sizeof (AML_RESOURCE_SMALL_HEADER),
                        ACPI_RSC_TABLE_SIZE (AcpiRsSetVendor)},

    /* Get the length and copy the data */

    {ACPI_RSC_COUNT16,  ACPI_RS_OFFSET (Data.Vendor.ByteLength),
                        0,
                        0},

    {ACPI_RSC_MOVE8,    ACPI_RS_OFFSET (Data.Vendor.ByteData[0]),
                        sizeof (AML_RESOURCE_SMALL_HEADER),
                        0},

    /*
     * All done if the Vendor byte length is 7 or less, meaning that it will
     * fit within a small descriptor
     */
    {ACPI_RSC_EXIT_LE,  0, 0, 7},

    /* Must create a large vendor descriptor */

    {ACPI_RSC_INITSET,  ACPI_RESOURCE_NAME_VENDOR_LARGE,
                        sizeof (AML_RESOURCE_LARGE_HEADER),
                        0},

    {ACPI_RSC_COUNT16,  ACPI_RS_OFFSET (Data.Vendor.ByteLength),
                        0,
                        0},

    {ACPI_RSC_MOVE8,    ACPI_RS_OFFSET (Data.Vendor.ByteData[0]),
                        sizeof (AML_RESOURCE_LARGE_HEADER),
                        0}
};
