/*******************************************************************************
 *
 * Module Name: dmresrcs.c - "Small" Resource Descriptor disassembly
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
#include <contrib/dev/acpica/include/acdisasm.h>


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbresrcs")


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIrqDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a IRQ descriptor, either Irq() or IrqNoFlags()
 *
 ******************************************************************************/

void
AcpiDmIrqDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("%s (",
        AcpiGbl_IrqDecode [ACPI_GET_1BIT_FLAG (Length)]);

    /* Decode flags byte if present */

    if (Length & 1)
    {
        AcpiOsPrintf ("%s, %s, %s, ",
            AcpiGbl_HeDecode [ACPI_GET_1BIT_FLAG (Resource->Irq.Flags)],
            AcpiGbl_LlDecode [ACPI_EXTRACT_1BIT_FLAG (Resource->Irq.Flags, 3)],
            AcpiGbl_ShrDecode [ACPI_EXTRACT_2BIT_FLAG (Resource->Irq.Flags, 4)]);
    }

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");

    AcpiDmIndent (Level + 1);
    AcpiDmBitList (Resource->Irq.IrqMask);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDmaDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a DMA descriptor
 *
 ******************************************************************************/

void
AcpiDmDmaDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("DMA (%s, %s, %s, ",
        AcpiGbl_TypDecode [ACPI_EXTRACT_2BIT_FLAG (Resource->Dma.Flags, 5)],
        AcpiGbl_BmDecode  [ACPI_EXTRACT_1BIT_FLAG (Resource->Dma.Flags, 2)],
        AcpiGbl_SizDecode [ACPI_GET_2BIT_FLAG (Resource->Dma.Flags)]);

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");

    AcpiDmIndent (Level + 1);
    AcpiDmBitList (Resource->Dma.DmaChannelMask);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFixedDmaDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a FixedDMA descriptor
 *
 ******************************************************************************/

void
AcpiDmFixedDmaDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("FixedDMA (0x%4.4X, 0x%4.4X, ",
        Resource->FixedDma.RequestLines,
        Resource->FixedDma.Channels);

    if (Resource->FixedDma.Width <= 5)
    {
        AcpiOsPrintf ("%s, ",
            AcpiGbl_DtsDecode [Resource->FixedDma.Width]);
    }
    else
    {
        AcpiOsPrintf ("%X /* INVALID DMA WIDTH */, ",
            Resource->FixedDma.Width);
    }

    /* Insert a descriptor name */

    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIoDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode an IO descriptor
 *
 ******************************************************************************/

void
AcpiDmIoDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("IO (%s,\n",
        AcpiGbl_IoDecode [ACPI_GET_1BIT_FLAG (Resource->Io.Flags)]);

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->Io.Minimum, "Range Minimum");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->Io.Maximum, "Range Maximum");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->Io.Alignment, "Alignment");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->Io.AddressLength, "Length");

    /* Insert a descriptor name */

    AcpiDmIndent (Level + 1);
    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFixedIoDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Fixed IO descriptor
 *
 ******************************************************************************/

void
AcpiDmFixedIoDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("FixedIO (\n");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger16 (Resource->FixedIo.Address, "Address");

    AcpiDmIndent (Level + 1);
    AcpiDmDumpInteger8 (Resource->FixedIo.AddressLength, "Length");

    /* Insert a descriptor name */

    AcpiDmIndent (Level + 1);
    AcpiDmDescriptorName ();
    AcpiOsPrintf (")\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmStartDependentDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Start Dependent functions descriptor
 *
 ******************************************************************************/

void
AcpiDmStartDependentDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);

    if (Length & 1)
    {
        AcpiOsPrintf ("StartDependentFn (0x%2.2X, 0x%2.2X)\n",
            (UINT32) ACPI_GET_2BIT_FLAG (Resource->StartDpf.Flags),
            (UINT32) ACPI_EXTRACT_2BIT_FLAG (Resource->StartDpf.Flags, 2));
    }
    else
    {
        AcpiOsPrintf ("StartDependentFnNoPri ()\n");
    }

    AcpiDmIndent (Level);
    AcpiOsPrintf ("{\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmEndDependentDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode an End Dependent functions descriptor
 *
 ******************************************************************************/

void
AcpiDmEndDependentDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmIndent (Level);
    AcpiOsPrintf ("}\n");
    AcpiDmIndent (Level);
    AcpiOsPrintf ("EndDependentFn ()\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmVendorSmallDescriptor
 *
 * PARAMETERS:  Info                - Extra resource info
 *              Resource            - Pointer to the resource descriptor
 *              Length              - Length of the descriptor in bytes
 *              Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode a Vendor Small Descriptor
 *
 ******************************************************************************/

void
AcpiDmVendorSmallDescriptor (
    ACPI_OP_WALK_INFO       *Info,
    AML_RESOURCE            *Resource,
    UINT32                  Length,
    UINT32                  Level)
{

    AcpiDmVendorCommon ("Short",
        ACPI_ADD_PTR (UINT8, Resource, sizeof (AML_RESOURCE_SMALL_HEADER)),
        Length, Level);
}
