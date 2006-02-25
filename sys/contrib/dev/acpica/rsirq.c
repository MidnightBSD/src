/*******************************************************************************
 *
 * Module Name: rsirq - IRQ resource descriptors
 *              $Revision: 1.1.1.1 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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

#define __RSIRQ_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acresrc.h>

#define _COMPONENT          ACPI_RESOURCES
        ACPI_MODULE_NAME    ("rsirq")


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsIrqResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                        stream
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        consumed the ByteStreamBuffer is
 *                                        returned
 *              OutputBuffer            - Pointer to the return data buffer
 *              StructureSize           - Pointer to where the number of bytes
 *                                        in the return data struct is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *              structure pointed to by the OutputBuffer.  Return the
 *              number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsIrqResource (
    UINT8                   *ByteStreamBuffer,
    ACPI_SIZE               *BytesConsumed,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    ACPI_RESOURCE           *OutputStruct = (void *) *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT8                   Index;
    UINT8                   i;
    ACPI_SIZE               StructSize = ACPI_SIZEOF_RESOURCE (ACPI_RESOURCE_IRQ);


    ACPI_FUNCTION_TRACE ("RsIrqResource");


    /*
     * The number of bytes consumed are contained in the descriptor
     *  (Bits:0-1)
     */
    Temp8 = *Buffer;
    *BytesConsumed = (Temp8 & 0x03) + 1;
    OutputStruct->Id = ACPI_RSTYPE_IRQ;

    /*
     * Point to the 16-bits of Bytes 1 and 2
     */
    Buffer += 1;
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);

    OutputStruct->Data.Irq.NumberOfInterrupts = 0;

    /* Decode the IRQ bits */

    for (i = 0, Index = 0; Index < 16; Index++)
    {
        if ((Temp16 >> Index) & 0x01)
        {
            OutputStruct->Data.Irq.Interrupts[i] = Index;
            i++;
        }
    }

    /* Zero interrupts is valid */

    OutputStruct->Data.Irq.NumberOfInterrupts = i;
    if (i > 0)
    {
        /*
         * Calculate the structure size based upon the number of interrupts
         */
        StructSize += ((ACPI_SIZE) i - 1) * 4;
    }

    /*
     * Point to Byte 3 if it is used
     */
    if (4 == *BytesConsumed)
    {
        Buffer += 2;
        Temp8 = *Buffer;

        /*
         * Check for HE, LL interrupts
         */
        switch (Temp8 & 0x09)
        {
        case 0x01: /* HE */
            OutputStruct->Data.Irq.EdgeLevel = ACPI_EDGE_SENSITIVE;
            OutputStruct->Data.Irq.ActiveHighLow = ACPI_ACTIVE_HIGH;
            break;

        case 0x08: /* LL */
            OutputStruct->Data.Irq.EdgeLevel = ACPI_LEVEL_SENSITIVE;
            OutputStruct->Data.Irq.ActiveHighLow = ACPI_ACTIVE_LOW;
            break;

        default:
            /*
             * Only _LL and _HE polarity/trigger interrupts
             * are allowed (ACPI spec, section "IRQ Format")
             * so 0x00 and 0x09 are illegal.
             */
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Invalid interrupt polarity/trigger in resource list, %X\n", Temp8));
            return_ACPI_STATUS (AE_BAD_DATA);
        }

        /*
         * Check for sharable
         */
        OutputStruct->Data.Irq.SharedExclusive = (Temp8 >> 3) & 0x01;
    }
    else
    {
        /*
         * Assume Edge Sensitive, Active High, Non-Sharable
         * per ACPI Specification
         */
        OutputStruct->Data.Irq.EdgeLevel = ACPI_EDGE_SENSITIVE;
        OutputStruct->Data.Irq.ActiveHighLow = ACPI_ACTIVE_HIGH;
        OutputStruct->Data.Irq.SharedExclusive = ACPI_EXCLUSIVE;
    }

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = (UINT32) StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsIrqStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        used in the OutputBuffer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *              the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsIrqStream (
    ACPI_RESOURCE           *LinkedList,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT8                   Index;
    BOOLEAN                 IRQInfoByteNeeded;


    ACPI_FUNCTION_TRACE ("RsIrqStream");


    /*
     * The descriptor field is set based upon whether a third byte is
     * needed to contain the IRQ Information.
     */
    if (ACPI_EDGE_SENSITIVE == LinkedList->Data.Irq.EdgeLevel &&
        ACPI_ACTIVE_HIGH == LinkedList->Data.Irq.ActiveHighLow &&
        ACPI_EXCLUSIVE == LinkedList->Data.Irq.SharedExclusive)
    {
        *Buffer = 0x22;
        IRQInfoByteNeeded = FALSE;
    }
    else
    {
        *Buffer = 0x23;
        IRQInfoByteNeeded = TRUE;
    }

    Buffer += 1;
    Temp16 = 0;

    /*
     * Loop through all of the interrupts and set the mask bits
     */
    for(Index = 0;
        Index < LinkedList->Data.Irq.NumberOfInterrupts;
        Index++)
    {
        Temp8 = (UINT8) LinkedList->Data.Irq.Interrupts[Index];
        Temp16 |= 0x1 << Temp8;
    }

    ACPI_MOVE_16_TO_16 (Buffer, &Temp16);
    Buffer += 2;

    /*
     * Set the IRQ Info byte if needed.
     */
    if (IRQInfoByteNeeded)
    {
        Temp8 = 0;
        Temp8 = (UINT8) ((LinkedList->Data.Irq.SharedExclusive &
                          0x01) << 4);

        if (ACPI_LEVEL_SENSITIVE == LinkedList->Data.Irq.EdgeLevel &&
            ACPI_ACTIVE_LOW == LinkedList->Data.Irq.ActiveHighLow)
        {
            Temp8 |= 0x08;
        }
        else
        {
            Temp8 |= 0x01;
        }

        *Buffer = Temp8;
        Buffer += 1;
    }

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = ACPI_PTR_DIFF (Buffer, *OutputBuffer);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsExtendedIrqResource
 *
 * PARAMETERS:  ByteStreamBuffer        - Pointer to the resource input byte
 *                                        stream
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        consumed the ByteStreamBuffer is
 *                                        returned
 *              OutputBuffer            - Pointer to the return data buffer
 *              StructureSize           - Pointer to where the number of bytes
 *                                        in the return data struct is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the resource byte stream and fill out the appropriate
 *              structure pointed to by the OutputBuffer.  Return the
 *              number of bytes consumed from the byte stream.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsExtendedIrqResource (
    UINT8                   *ByteStreamBuffer,
    ACPI_SIZE               *BytesConsumed,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *StructureSize)
{
    UINT8                   *Buffer = ByteStreamBuffer;
    ACPI_RESOURCE           *OutputStruct = (void *) *OutputBuffer;
    UINT16                  Temp16 = 0;
    UINT8                   Temp8 = 0;
    UINT8                   *TempPtr;
    UINT8                   Index;
    ACPI_SIZE               StructSize = ACPI_SIZEOF_RESOURCE (ACPI_RESOURCE_EXT_IRQ);


    ACPI_FUNCTION_TRACE ("RsExtendedIrqResource");


    /*
     * Point past the Descriptor to get the number of bytes consumed
     */
    Buffer += 1;
    ACPI_MOVE_16_TO_16 (&Temp16, Buffer);

    /* Validate minimum descriptor length */

    if (Temp16 < 6)
    {
        return_ACPI_STATUS (AE_AML_BAD_RESOURCE_LENGTH);
    }

    *BytesConsumed = Temp16 + 3;
    OutputStruct->Id = ACPI_RSTYPE_EXT_IRQ;

    /*
     * Point to the Byte3
     */
    Buffer += 2;
    Temp8 = *Buffer;

    OutputStruct->Data.ExtendedIrq.ProducerConsumer = Temp8 & 0x01;

    /*
     * Check for Interrupt Mode
     *
     * The definition of an Extended IRQ changed between ACPI spec v1.0b
     * and ACPI spec 2.0 (section 6.4.3.6 in both).
     *
     * - Edge/Level are defined opposite in the table vs the headers
     */
    OutputStruct->Data.ExtendedIrq.EdgeLevel =
                        (Temp8 & 0x2) ? ACPI_EDGE_SENSITIVE : ACPI_LEVEL_SENSITIVE;

    /*
     * Check Interrupt Polarity
     */
    OutputStruct->Data.ExtendedIrq.ActiveHighLow = (Temp8 >> 2) & 0x1;

    /*
     * Check for sharable
     */
    OutputStruct->Data.ExtendedIrq.SharedExclusive = (Temp8 >> 3) & 0x01;

    /*
     * Point to Byte4 (IRQ Table length)
     */
    Buffer += 1;
    Temp8 = *Buffer;

    /* Must have at least one IRQ */

    if (Temp8 < 1)
    {
        return_ACPI_STATUS (AE_AML_BAD_RESOURCE_LENGTH);
    }

    OutputStruct->Data.ExtendedIrq.NumberOfInterrupts = Temp8;

    /*
     * Add any additional structure size to properly calculate
     * the next pointer at the end of this function
     */
    StructSize += (Temp8 - 1) * 4;

    /*
     * Point to Byte5 (First IRQ Number)
     */
    Buffer += 1;

    /*
     * Cycle through every IRQ in the table
     */
    for (Index = 0; Index < Temp8; Index++)
    {
        ACPI_MOVE_32_TO_32 (
            &OutputStruct->Data.ExtendedIrq.Interrupts[Index], Buffer);

        /* Point to the next IRQ */

        Buffer += 4;
    }

    /*
     * This will leave us pointing to the Resource Source Index
     * If it is present, then save it off and calculate the
     * pointer to where the null terminated string goes:
     * Each Interrupt takes 32-bits + the 5 bytes of the
     * stream that are default.
     *
     * Note: Some resource descriptors will have an additional null, so
     * we add 1 to the length.
     */
    if (*BytesConsumed >
        ((ACPI_SIZE) OutputStruct->Data.ExtendedIrq.NumberOfInterrupts * 4) + (5 + 1))
    {
        /* Dereference the Index */

        Temp8 = *Buffer;
        OutputStruct->Data.ExtendedIrq.ResourceSource.Index = (UINT32) Temp8;

        /* Point to the String */

        Buffer += 1;

        /*
         * Point the String pointer to the end of this structure.
         */
        OutputStruct->Data.ExtendedIrq.ResourceSource.StringPtr =
                (char *)((char *) OutputStruct + StructSize);

        TempPtr = (UINT8 *) OutputStruct->Data.ExtendedIrq.ResourceSource.StringPtr;

        /* Copy the string into the buffer */

        Index = 0;
        while (0x00 != *Buffer)
        {
            *TempPtr = *Buffer;

            TempPtr += 1;
            Buffer += 1;
            Index += 1;
        }

        /*
         * Add the terminating null
         */
        *TempPtr = 0x00;
        OutputStruct->Data.ExtendedIrq.ResourceSource.StringLength = Index + 1;

        /*
         * In order for the StructSize to fall on a 32-bit boundary,
         * calculate the length of the string and expand the
         * StructSize to the next 32-bit boundary.
         */
        Temp8 = (UINT8) (Index + 1);
        StructSize += ACPI_ROUND_UP_TO_32BITS (Temp8);
    }
    else
    {
        OutputStruct->Data.ExtendedIrq.ResourceSource.Index = 0x00;
        OutputStruct->Data.ExtendedIrq.ResourceSource.StringLength = 0;
        OutputStruct->Data.ExtendedIrq.ResourceSource.StringPtr = NULL;
    }

    /*
     * Set the Length parameter
     */
    OutputStruct->Length = (UINT32) StructSize;

    /*
     * Return the final size of the structure
     */
    *StructureSize = StructSize;
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiRsExtendedIrqStream
 *
 * PARAMETERS:  LinkedList              - Pointer to the resource linked list
 *              OutputBuffer            - Pointer to the user's return buffer
 *              BytesConsumed           - Pointer to where the number of bytes
 *                                        used in the OutputBuffer is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Take the linked list resource structure and fills in the
 *              the appropriate bytes in a byte stream
 *
 ******************************************************************************/

ACPI_STATUS
AcpiRsExtendedIrqStream (
    ACPI_RESOURCE           *LinkedList,
    UINT8                   **OutputBuffer,
    ACPI_SIZE               *BytesConsumed)
{
    UINT8                   *Buffer = *OutputBuffer;
    UINT16                  *LengthField;
    UINT8                   Temp8 = 0;
    UINT8                   Index;
    char                    *TempPointer = NULL;


    ACPI_FUNCTION_TRACE ("RsExtendedIrqStream");


    /*
     * The descriptor field is static
     */
    *Buffer = 0x89;
    Buffer += 1;

    /*
     * Set a pointer to the Length field - to be filled in later
     */
    LengthField = ACPI_CAST_PTR (UINT16, Buffer);
    Buffer += 2;

    /*
     * Set the Interrupt vector flags
     */
    Temp8 = (UINT8)(LinkedList->Data.ExtendedIrq.ProducerConsumer & 0x01);
    Temp8 |= ((LinkedList->Data.ExtendedIrq.SharedExclusive & 0x01) << 3);

    /*
     * Set the Interrupt Mode
     *
     * The definition of an Extended IRQ changed between ACPI spec v1.0b
     * and ACPI spec 2.0 (section 6.4.3.6 in both).  This code does not
     * implement the more restrictive definition of 1.0b
     *
     * - Edge/Level are defined opposite in the table vs the headers
     */
    if (ACPI_EDGE_SENSITIVE == LinkedList->Data.ExtendedIrq.EdgeLevel)
    {
        Temp8 |= 0x2;
    }

    /*
     * Set the Interrupt Polarity
     */
    Temp8 |= ((LinkedList->Data.ExtendedIrq.ActiveHighLow & 0x1) << 2);

    *Buffer = Temp8;
    Buffer += 1;

    /*
     * Set the Interrupt table length
     */
    Temp8 = (UINT8) LinkedList->Data.ExtendedIrq.NumberOfInterrupts;

    *Buffer = Temp8;
    Buffer += 1;

    for (Index = 0; Index < LinkedList->Data.ExtendedIrq.NumberOfInterrupts;
         Index++)
    {
        ACPI_MOVE_32_TO_32 (Buffer,
                        &LinkedList->Data.ExtendedIrq.Interrupts[Index]);
        Buffer += 4;
    }

    /*
     * Resource Source Index and Resource Source are optional
     */
    if (0 != LinkedList->Data.ExtendedIrq.ResourceSource.StringLength)
    {
        *Buffer = (UINT8) LinkedList->Data.ExtendedIrq.ResourceSource.Index;
        Buffer += 1;

        TempPointer = (char *) Buffer;

        /*
         * Copy the string
         */
        ACPI_STRCPY (TempPointer,
            LinkedList->Data.ExtendedIrq.ResourceSource.StringPtr);

        /*
         * Buffer needs to be set to the length of the sting + one for the
         * terminating null
         */
        Buffer += (ACPI_SIZE)(ACPI_STRLEN (LinkedList->Data.ExtendedIrq.ResourceSource.StringPtr) + 1);
    }

    /*
     * Return the number of bytes consumed in this operation
     */
    *BytesConsumed = ACPI_PTR_DIFF (Buffer, *OutputBuffer);

    /*
     * Set the length field to the number of bytes consumed
     * minus the header size (3 bytes)
     */
    *LengthField = (UINT16) (*BytesConsumed - 3);
    return_ACPI_STATUS (AE_OK);
}

