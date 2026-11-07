/******************************************************************************
 *
 * Module Name: aslrestype2e - Large Extended address resource descriptors
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
        ACPI_MODULE_NAME    ("aslrestype2e")

/*
 * This module contains the Extended (64-bit) address space descriptors:
 *
 * ExtendedIO
 * ExtendedMemory
 * ExtendedSpace
 */

/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedIoDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedIoDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);
    CurrentByteOffset = Info->CurrentByteOffset;

    Rnode = RsAllocateResourceNode (
        sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.ResourceType = ACPI_ADDRESS_TYPE_IO_RANGE;
    Descriptor->ExtAddress64.RevisionID = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 2: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 3: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 4: /* Range Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 0, 3);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_RANGETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 0, 2);
            break;

        case 5: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
            break;

        case 6: /* Address Min */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 7: /* Address Max */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 8: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 9: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 10: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 11: /* ResourceTag */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        case 12: /* Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 4);
            break;

        case 13: /* Translation Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TRANSTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Info->DescriptorTypeOp);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) +
        StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedMemoryDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedMemory" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedMemoryDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);
    CurrentByteOffset = Info->CurrentByteOffset;

    Rnode = RsAllocateResourceNode (
                sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.ResourceType = ACPI_ADDRESS_TYPE_MEMORY_RANGE;
    Descriptor->ExtAddress64.RevisionID = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 1: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 2: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 3: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 4: /* Memory Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 1, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_MEMTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 1, 2);
            break;

        case 5: /* Read/Write Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_READWRITETYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 0);
            break;

        case 6: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;


        case 13: /* Address Range */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 3, 0);
            RsCreateMultiBitField (InitializerOp, ACPI_RESTAG_MEMATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 3, 2);
            break;

        case 14: /* Type */

            RsSetFlagBits (&Descriptor->ExtAddress64.SpecificFlags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_TYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.SpecificFlags), 5);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Info->DescriptorTypeOp);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) +
        StringLength;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoExtendedSpaceDescriptor
 *
 * PARAMETERS:  Info                - Parse Op and resource template offset
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a long "ExtendedSpace" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoExtendedSpaceDescriptor (
    ASL_RESOURCE_INFO       *Info)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ACPI_PARSE_OBJECT       *MinOp = NULL;
    ACPI_PARSE_OBJECT       *MaxOp = NULL;
    ACPI_PARSE_OBJECT       *LengthOp = NULL;
    ACPI_PARSE_OBJECT       *GranOp = NULL;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  StringLength = 0;
    UINT32                  CurrentByteOffset;
    UINT32                  i;


    InitializerOp = Info->DescriptorTypeOp->Asl.Child;
    StringLength = RsGetStringDataLength (InitializerOp);
    CurrentByteOffset = Info->CurrentByteOffset;

    Rnode = RsAllocateResourceNode (
        sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) + 1 + StringLength);

    Descriptor = Rnode->Buffer;
    Descriptor->ExtAddress64.DescriptorType = ACPI_RESOURCE_NAME_EXTENDED_ADDRESS64;
    Descriptor->ExtAddress64.RevisionID = AML_RESOURCE_EXTENDED_ADDRESS_REVISION;

    Descriptor->ExtAddress64.ResourceLength = (UINT16)
        (sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) -
         sizeof (AML_RESOURCE_LARGE_HEADER));

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Resource Type */

            Descriptor->ExtAddress64.ResourceType =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 1: /* Resource Usage */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 0, 1);
            break;

        case 2: /* DecodeType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 1, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 1);
            break;

        case 3: /* MinType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MINTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 2);
            break;

        case 4: /* MaxType */

            RsSetFlagBits (&Descriptor->ExtAddress64.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_MAXTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Flags), 3);
            break;

        case 5: /* Type-Specific flags */

            Descriptor->ExtAddress64.SpecificFlags =
                (UINT8) InitializerOp->Asl.Value.Integer;
            break;

        case 6: /* Address Granularity */

            Descriptor->ExtAddress64.Granularity = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_GRANULARITY,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Granularity));
            GranOp = InitializerOp;
            break;

        case 7: /* Min Address */

            Descriptor->ExtAddress64.Minimum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Minimum));
            MinOp = InitializerOp;
            break;

        case 8: /* Max Address */

            Descriptor->ExtAddress64.Maximum = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.Maximum));
            MaxOp = InitializerOp;
            break;

        case 9: /* Translation Offset */

            Descriptor->ExtAddress64.TranslationOffset = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TRANSLATION,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TranslationOffset));
            break;

        case 10: /* Address Length */

            Descriptor->ExtAddress64.AddressLength = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.AddressLength));
            LengthOp = InitializerOp;
            break;

        case 11: /* Type-Specific Attributes */

            Descriptor->ExtAddress64.TypeSpecific = InitializerOp->Asl.Value.Integer;
            RsCreateQwordField (InitializerOp, ACPI_RESTAG_TYPESPECIFICATTRIBUTES,
                CurrentByteOffset + ASL_RESDESC_OFFSET (ExtAddress64.TypeSpecific));
            break;

        case 12: /* ResourceTag */

            UtAttachNamepathToOwner (Info->DescriptorTypeOp, InitializerOp);
            break;

        default:

            AslError (ASL_ERROR, ASL_MSG_RESOURCE_LIST, InitializerOp, NULL);
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Validate the Min/Max/Len/Gran values */

    RsLargeAddressCheck (
        Descriptor->ExtAddress64.Minimum,
        Descriptor->ExtAddress64.Maximum,
        Descriptor->ExtAddress64.AddressLength,
        Descriptor->ExtAddress64.Granularity,
        Descriptor->ExtAddress64.Flags,
        MinOp, MaxOp, LengthOp, GranOp, Info->DescriptorTypeOp);

    Rnode->BufferLength = sizeof (AML_RESOURCE_EXTENDED_ADDRESS64) +
        StringLength;
    return (Rnode);
}
