
/******************************************************************************
 *
 * Module Name: aslrestype1 - Short (type1) resource templates and descriptors
 *              $Revision: 1.40 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslrestype1")


/*******************************************************************************
 *
 * FUNCTION:    RsDoEndTagDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "EndDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoEndTagDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
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
 * FUNCTION:    RsDoDmaDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "DMA" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoDmaDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;
    UINT8                   DmaChannelMask = 0;
    UINT8                   DmaChannels = 0;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_DMA));

    Descriptor = Rnode->Buffer;
    Descriptor->Dma.DescriptorType  = ACPI_RESOURCE_NAME_DMA |
                                        ASL_RDESC_DMA_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* DMA type */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 5, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DMATYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 5);
            break;

        case 1: /* Bus Master */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 2, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_BUSMASTER,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 2);
            break;

        case 2: /* Xfer Type (transfer width) */

            RsSetFlagBits (&Descriptor->Dma.Flags, InitializerOp, 0, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_XFERTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Dma.Flags), 0);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All DMA channel bytes are handled here, after flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 8 channels can be specified in the list */

                DmaChannels++;
                if (DmaChannels > 8)
                {
                    AslError (ASL_ERROR, ASL_MSG_DMA_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only DMA channels 0-7 are allowed (mask is 8 bits) */

                if (InitializerOp->Asl.Value.Integer > 7)
                {
                    AslError (ASL_ERROR, ASL_MSG_DMA_CHANNEL,
                        InitializerOp, NULL);
                }

                /* Build the mask */

                DmaChannelMask |=
                    (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
            }

            if (i == 4) /* case 4: First DMA byte */
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateByteField (InitializerOp, ACPI_RESTAG_DMA,
                    CurrentByteOffset +
                    ASL_RESDESC_OFFSET (Dma.DmaChannelMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the channel mask */

    Descriptor->Dma.DmaChannelMask = DmaChannelMask;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoEndDependentDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "EndDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoEndDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ASL_RESOURCE_NODE       *Rnode;


    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_END_DEPENDENT));

    Descriptor = Rnode->Buffer;
    Descriptor->EndDpf.DescriptorType  = ACPI_RESOURCE_NAME_END_DEPENDENT |
                                      ASL_RDESC_END_DEPEND_SIZE;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoFixedIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "FixedIO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoFixedIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_FIXED_IO));

    Descriptor = Rnode->Buffer;
    Descriptor->Io.DescriptorType  = ACPI_RESOURCE_NAME_FIXED_IO |
                                      ASL_RDESC_FIXED_IO_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Base Address */

            Descriptor->FixedIo.Address =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_BASEADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedIo.Address));
            break;

        case 1: /* Length */

            Descriptor->FixedIo.AddressLength =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedIo.AddressLength));
            break;

        case 2: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
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
 * FUNCTION:    RsDoIoDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IO" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIoDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IO));

    Descriptor = Rnode->Buffer;
    Descriptor->Io.DescriptorType  = ACPI_RESOURCE_NAME_IO |
                                      ASL_RDESC_IO_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Decode size */

            RsSetFlagBits (&Descriptor->Io.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_DECODE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Flags), 0);
            break;

        case 1:  /* Min Address */

            Descriptor->Io.Minimum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Minimum));
            break;

        case 2: /* Max Address */

            Descriptor->Io.Maximum =
                (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Maximum));
            break;

        case 3: /* Alignment */

            Descriptor->Io.Alignment =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.Alignment));
            break;

        case 4: /* Length */

            Descriptor->Io.AddressLength =
                (UINT8) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Io.AddressLength));
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
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
 * FUNCTION:    RsDoIrqDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IRQ" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIrqDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  Interrupts = 0;
    UINT16                  IrqMask = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IRQ));

    /* Length = 3 (with flag byte) */

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RESOURCE_NAME_IRQ |
                                      (ASL_RDESC_IRQ_SIZE + 0x01);

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Interrupt Type (or Mode - edge/level) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 0, 1);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTTYPE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 0);
            break;

        case 1: /* Interrupt Level (or Polarity - Active high/low) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 3, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTLEVEL,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 3);
            break;

        case 2: /* Share Type - Default: exclusive (0) */

            RsSetFlagBits (&Descriptor->Irq.Flags, InitializerOp, 4, 0);
            RsCreateBitField (InitializerOp, ACPI_RESTAG_INTERRUPTSHARE,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.Flags), 4);
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* All IRQ bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 16 interrupts can be specified in the list */

                Interrupts++;
                if (Interrupts > 16)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only interrupts 0-15 are allowed (mask is 16 bits) */

                if (InitializerOp->Asl.Value.Integer > 15)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_NUMBER,
                        InitializerOp, NULL);
                }
                else
                {
                    IrqMask |= (1 << (UINT8) InitializerOp->Asl.Value.Integer);
                }
            }

            /* Case 4: First IRQ value in list */

            if (i == 4)
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateByteField (InitializerOp, ACPI_RESTAG_INTERRUPT,
                    CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.IrqMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the channel mask */

    Descriptor->Irq.IrqMask = IrqMask;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoIrqNoFlagsDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "IRQNoFlags" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoIrqNoFlagsDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT16                  IrqMask = 0;
    UINT32                  Interrupts = 0;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_IRQ_NOFLAGS));

    Descriptor = Rnode->Buffer;
    Descriptor->Irq.DescriptorType  = ACPI_RESOURCE_NAME_IRQ |
                                      ASL_RDESC_IRQ_SIZE;

    /* Process all child initialization nodes */

    for (i = 0; InitializerOp; i++)
    {
        switch (i)
        {
        case 0: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
            break;

        default:

            /* IRQ bytes are handled here, after the flags and name */

            if (InitializerOp->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
            {
                /* Up to 16 interrupts can be specified in the list */

                Interrupts++;
                if (Interrupts > 16)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_LIST,
                        InitializerOp, NULL);
                    return (Rnode);
                }

                /* Only interrupts 0-15 are allowed (mask is 16 bits) */

                if (InitializerOp->Asl.Value.Integer > 15)
                {
                    AslError (ASL_ERROR, ASL_MSG_INTERRUPT_NUMBER,
                        InitializerOp, NULL);
                }
                else
                {
                    IrqMask |= (1 << ((UINT8) InitializerOp->Asl.Value.Integer));
                }
            }

            /* Case 1: First IRQ value in list */

            if (i == 1)
            {
                /* Check now for duplicates in list */

                RsCheckListForDuplicates (InitializerOp);

                /* Create a named field at the start of the list */

                RsCreateByteField (InitializerOp, ACPI_RESTAG_INTERRUPT,
                    CurrentByteOffset + ASL_RESDESC_OFFSET (Irq.IrqMask));
            }
            break;
        }

        InitializerOp = RsCompleteNodeAndGetNext (InitializerOp);
    }

    /* Now we can set the interrupt mask */

    Descriptor->Irq.IrqMask = IrqMask;
    return (Rnode);
}


/*******************************************************************************
 *
 * FUNCTION:    RsDoMemory24Descriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory24" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory24Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_MEMORY24));

    Descriptor = Rnode->Buffer;
    Descriptor->Memory24.DescriptorType  = ACPI_RESOURCE_NAME_MEMORY24;
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
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Minimum));
            break;

        case 2: /* Max Address */

            Descriptor->Memory24.Maximum = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Maximum));
            break;

        case 3: /* Alignment */

            Descriptor->Memory24.Alignment = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.Alignment));
            break;

        case 4: /* Length */

            Descriptor->Memory24.AddressLength = (UINT16) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory24.AddressLength));
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
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
 * FUNCTION:    RsDoMemory32Descriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32Descriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_MEMORY32));

    Descriptor = Rnode->Buffer;
    Descriptor->Memory32.DescriptorType  = ACPI_RESOURCE_NAME_MEMORY32;
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
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MINADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Minimum));
            break;

        case 2: /* Max Address */

            Descriptor->Memory32.Maximum = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_MAXADDR,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Maximum));
            break;

        case 3: /* Alignment */

            Descriptor->Memory32.Alignment = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_ALIGNMENT,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.Alignment));
            break;

        case 4: /* Length */

            Descriptor->Memory32.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (Memory32.AddressLength));
            break;

        case 5: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
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
 * FUNCTION:    RsDoMemory32FixedDescriptor
 *
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "Memory32Fixed" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoMemory32FixedDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_FIXED_MEMORY32));

    Descriptor = Rnode->Buffer;
    Descriptor->FixedMemory32.DescriptorType  = ACPI_RESOURCE_NAME_FIXED_MEMORY32;
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
            RsCreateByteField (InitializerOp, ACPI_RESTAG_BASEADDRESS,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedMemory32.Address));
            break;

        case 2: /* Length */

            Descriptor->FixedMemory32.AddressLength = (UINT32) InitializerOp->Asl.Value.Integer;
            RsCreateByteField (InitializerOp, ACPI_RESTAG_LENGTH,
                CurrentByteOffset + ASL_RESDESC_OFFSET (FixedMemory32.AddressLength));
            break;

        case 3: /* Name */

            UtAttachNamepathToOwner (Op, InitializerOp);
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
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentFn" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    UINT32                  i;
    UINT8                   State;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_START_DEPENDENT));

    PreviousRnode = Rnode;
    Descriptor = Rnode->Buffer;

    /* Descriptor has priority byte */

    Descriptor->StartDpf.DescriptorType  = ACPI_RESOURCE_NAME_START_DEPENDENT |
                                      (ASL_RDESC_ST_DEPEND_SIZE + 0x01);

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
            NextRnode = RsDoOneResourceDescriptor  (InitializerOp,
                        CurrentByteOffset, &State);

            /*
             * Update current byte offset to indicate the number of bytes from the
             * start of the buffer.  Buffer can include multiple descriptors, we
             * must keep track of the offset of not only each descriptor, but each
             * element (field) within each descriptor as well.
             */

            CurrentByteOffset += RsLinkDescriptorChain (&PreviousRnode,
                                    NextRnode);
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
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "StartDependentNoPri" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoStartDependentNoPriDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    ASL_RESOURCE_NODE       *PreviousRnode;
    ASL_RESOURCE_NODE       *NextRnode;
    UINT8                   State;


    InitializerOp = Op->Asl.Child;
    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_START_DEPENDENT_NOPRIO));

    Descriptor = Rnode->Buffer;
    Descriptor->StartDpf.DescriptorType  = ACPI_RESOURCE_NAME_START_DEPENDENT |
                                      ASL_RDESC_ST_DEPEND_SIZE;
    PreviousRnode = Rnode;

    /* Process all child initialization nodes */

    State = ACPI_RSTATE_START_DEPENDENT;
    while (InitializerOp)
    {
        NextRnode = RsDoOneResourceDescriptor  (InitializerOp,
                        CurrentByteOffset, &State);

        /*
         * Update current byte offset to indicate the number of bytes from the
         * start of the buffer.  Buffer can include multiple descriptors, we
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
 * PARAMETERS:  Op                  - Parent resource descriptor parse node
 *              CurrentByteOffset   - Offset into the resource template AML
 *                                    buffer (to track references to the desc)
 *
 * RETURN:      Completed resource node
 *
 * DESCRIPTION: Construct a short "VendorShort" descriptor
 *
 ******************************************************************************/

ASL_RESOURCE_NODE *
RsDoVendorSmallDescriptor (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  CurrentByteOffset)
{
    AML_RESOURCE            *Descriptor;
    ACPI_PARSE_OBJECT       *InitializerOp;
    ASL_RESOURCE_NODE       *Rnode;
    UINT8                   *VendorData;
    UINT32                  i;


    InitializerOp = Op->Asl.Child;

    /* Allocate worst case - 7 vendor bytes */

    Rnode = RsAllocateResourceNode (sizeof (AML_RESOURCE_VENDOR_SMALL) + 7);

    Descriptor = Rnode->Buffer;
    Descriptor->VendorSmall.DescriptorType  = ACPI_RESOURCE_NAME_VENDOR_SMALL;
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


