/******************************************************************************
 *
 * Module Name: aslrestype1 - Miscellaneous small resource descriptors
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype1")

/*
 * This module contains miscellaneous small resource descriptors:
 *
 * EndTag
 * EndDependentFn
 * Memory24
 * Memory32
 * Memory32Fixed
 * StartDependentFn
 * StartDependentFnNoPri
 * VendorShort
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoEndTagDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "EndDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoEndTagDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ASL_RESOURCE_NODE       *Rnode;


    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_END_TAG));

    Descriptor = Rnode->Buffer;
    Descriptor->EndTag.DescriptorType = ACPI_RESOURCE_NAME_END_TAG |
                                        ASL_RDESC_END_TAG_SIZE;
    Descriptor->EndTag.Checksum = 0;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoEndDependentDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "EndDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoEndDependentDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ASL_RESOURCE_NODE       *Rnode;


    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_END_DEPENDENT));

    Descriptor = Rnode->Buffer;
    Descriptor->EndDpf.DescriptorType =
        ACPI_RESOURCE_NAME_END_DEPENDENT | ASL_RDESC_END_DEPEND_SIZE;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory24Descriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory24" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory24Descriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_MEMORY24));

    Descriptor = Rnode->Buffer;
    Descriptor->Memory24.DescriptorType = ACPI_RESOURCE_NAME_MEMORY24;
    Descriptor->Memory24.ResourceLength = 9;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->Memory24.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Flags), 0);
            break;

        case 1: /* Min Address */

            Descriptor->Memory24.Minimum = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Minimum));
            MinOp = InitializerOp;
            break;

        case 2: /* Max Address */

            Descriptor->Memory24.Maximum = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Maximum));
            MaxOp = InitializerOp;
            break;

        case 3: /* Alignment */

            Descriptor->Memory24.Alignment = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Alignment));
            break;

        case 4: /* Length */

            Descriptor->Memory24.AddressLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateWordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Align values (Alignment==0 means 64K) */

    RsSmallAddressCheck (ACPI_RESOURCE_NAME_MEMORY24,
        Descriptor->Memory24.Minimum,
        Descriptor->Memory24.Maximum,
        Descriptor->Memory24.AddressLength,
        Descriptor->Memory24.Alignment,
        MinOp, MaxOp, LengthOp, NULL, Info->DescriptorTypeOp);

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory32Descriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32Descriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *AlignOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_MEMORY32));

    Descriptor = Rnode->Buffer;
    Descriptor->Memory32.DescriptorType = ACPI_RESOURCE_NAME_MEMORY32;
    Descriptor->Memory32.ResourceLength = 17;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->Memory32.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Flags), 0);
            break;

        case 1:  /* Min Address */

            Descriptor->Memory32.Minimum = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Minimum));
            MinOp = InitializerOp;
            break;

        case 2: /* Max Address */

            Descriptor->Memory32.Maximum = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Maximum));
            MaxOp = InitializerOp;
            break;

        case 3: /* Alignment */

            Descriptor->Memory32.Alignment = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Alignment));
            AlignOp = InitializerOp;
            break;

        case 4: /* Length */

            Descriptor->Memory32.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Align values */

    RsSmallAddressCheck (ACPI_RESOURCE_NAME_MEMORY32,
        Descriptor->Memory32.Minimum,
        Descriptor->Memory32.Maximum,
        Descriptor->Memory32.AddressLength,
        Descriptor->Memory32.Alignment,
        MinOp, MaxOp, LengthOp, AlignOp, Info->DescriptorTypeOp);

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory32FixedDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32Fixed" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32FixedDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_FIXED_MEMORY32));

    Descriptor = Rnode->Buffer;
    Descriptor->FixedMemory32.DescriptorType = ACPI_RESOURCE_NAME_FIXED_MEMORY32;
    Descriptor->FixedMemory32.ResourceLength = 9;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Read/Write type */

            RsSetFlagBits (&Descriptor->FixedMemory32.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedMemory32.Flags), 0);
            break;

        case 1: /* Address */

            Descriptor->FixedMemory32.Address = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_BASEADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedMemory32.Address));
            break;

        case 2: /* Length */

            Descriptor->FixedMemory32.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateDwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedMemory32.AddressLength));
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoStartDependentDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    ASL_RESOURCE_INFO       NextInfo;
    UINT32                  CurrentByteOffset;
    UINT32                  i;
    UINT8                   State;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_START_DEPENDENT));

    PreviousRnode = Rnode;
    Descriptor = Rnode->Buffer;

    /* Increment offset past StartDependent descriptor */

    CurrentByteOffset += sizeof (AML_RESOURCE_START_DEPENDENT);

    /* Descriptor has priority byte */

    Descriptor->StartDpf.DescriptorType =
        ACPI_RESOURCE_NAME_START_DEPENDENT | (ASL_RDESC_ST_DEPEND_SIZE + 0x01);

    /* Process all child initialization nodes */

    State = ACPI_RSTATE_START_DEPENDENT;
    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Compatibility Priority */

            if ((UINT8) InitializerOp->Asl.Value.Integer > 2)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_PRIORITY,
                    InitializerOp, NULL);
            }

            RsSetFlagBits (&Descriptor->StartDpf.Flags, InitializerOp, 0, 0);
            break;

        case 1: /* Performance/Robustness Priority */

            if ((UINT8) InitializerOp->Asl.Value.Integer > 2)
            {
                AslError (ASL_ERROR, ASL_MSG_INVALID_PERFORMANCE,
                    InitializerOp, NULL);
            }

            RsSetFlagBits (&Descriptor->StartDpf.Flags, InitializerOp, 2, 0);
            break;

        default:

            NextInfo.CurrentByteOffset = CurrentByteOffset;
            NextInfo.DescriptorTypeOp = InitializerOp;

            NextRnode = RsDoOneResourceDescriptor (&NextInfo, &State);

            /*
             * Update current byte offset to indicate the number of bytes from the
             * start of the buffer. Buffer can include multiple descriptors, we
             * must keep track of the offset of not only each descriptor, but each
             * element (field) within each descriptor as well.
             */
            CurrentByteOffset += RsLinkDescriptorChain (
                &PreviousRnode, NextRnode);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoStartDependentNoPriDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentNoPri" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentNoPriDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    ASL_RESOURCE_INFO       NextInfo;
    UINT32                  CurrentByteOffset;
    UINT8                   State;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    CurrentByteOffset = Info->CurrentByteOffset;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_START_DEPENDENT_NOPRIO));

    Descriptor = Rnode->Buffer;
    Descriptor->StartDpf.DescriptorType =
        ACPI_RESOURCE_NAME_START_DEPENDENT | ASL_RDESC_ST_DEPEND_SIZE;
    PreviousRnode = Rnode;

    /* Increment offset past StartDependentNoPri descriptor */

    CurrentByteOffset += sizeof (AML_RESOURCE_START_DEPENDENT_NOPRIO);

    /* Process all child initialization nodes */

    State = ACPI_RSTATE_START_DEPENDENT;
    while (InitializerOp)
    {
        NextInfo.CurrentByteOffset = CurrentByteOffset;
        NextInfo.DescriptorTypeOp = InitializerOp;

        NextRnode = RsDoOneResourceDescriptor (&NextInfo, &State);

        /*
         * Update current byte offset to indicate the number of bytes from the
         * start of the buffer. Buffer can include multiple descriptors, we
         * must keep track of the offset of not only each descriptor, but each
         * element (field) within each descriptor as well.
         */
        CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode, NextRnode);

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoVendorSmallDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "VendorShort" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoVendorSmallDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *VendorData;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;

    /* Allocate worst case - 7 vendor bytes */

    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_VENDOR_SMALL) + 7);

    Descriptor = Rnode->Buffer;
    Descriptor->VendorSmall.DescriptorType = ACPI_RESOURCE_NAME_VENDOR_SMALL;
    VendorData = ((UINT8 *) Descriptor) + sizeof (AML_RESOURCE_SMALL_HEADER);

    /* Process all child initialization nodes */

    InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    for (i = 0; InitializerOp; i++)
    {
        if (InitializerOp->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
        {
            break;
        }

        /* Maximum 7 vendor data bytes allowed (0-6) */

        if (i >= 7)
        {
            AslError (ASL_ERROR, ASL_MSG_VENDOR_LIST, InitializerOp, NULL);

            /* Eat the excess initializers */

            while (InitializerOp)
            {
                InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
            }
            break;
        }

        VendorData[i] = (UINT8) InitializerOp->Asl.Value.Integer;
        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Adjust the Rnode buffer size, so correct number of bytes are emitted */

    Rnode->BufferLength -= (7 - i);

    /* Set the length in the Type Tag */

    Descriptor->VendorSmall.DescriptorType |= (UINT8) i;
    return (Rnode);
}
