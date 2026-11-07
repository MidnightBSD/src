/*******************************************************************************
 *
 * Module Name: dmutils - AML disassembler utilities
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
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acconvert.h>

#ifdef ACPI_ASL_COMPILER
#include <contrib/dev/acpica/include/acnamesp.h>
#endif


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmutils")


/* Data used in keeping track of fields */
#if 0
const char                      *AcpiGbl_FENames[] =
{
    "skip",
    "?access?"
};              /* FE = Field Element */
#endif

/* Operators for Match() */

const char                      *AcpiGbl_MatchOps[] =
{
    "MTR",
    "MEQ",
    "MLE",
    "MLT",
    "MGE",
    "MGT"
};

/* Access type decoding */

const char                      *AcpiGbl_AccessTypes[] =
{
    "AnyAcc",
    "ByteAcc",
    "WordAcc",
    "DWordAcc",
    "QWordAcc",
    "BufferAcc",
    "InvalidAccType",
    "InvalidAccType"
};

/* Lock rule decoding */

const char                      *AcpiGbl_LockRule[] =
{
    "NoLock",
    "Lock"
};

/* Update rule decoding */

const char                      *AcpiGbl_UpdateRules[] =
{
    "Preserve",
    "WriteAsOnes",
    "WriteAsZeros",
    "InvalidUpdateRule"
};

/* Strings used to decode resource descriptors */

const char                      *AcpiGbl_WordDecode[] =
{
    "Memory",
    "IO",
    "BusNumber",
    "UnknownResourceType"
};

const char                      *AcpiGbl_IrqDecode[] =
{
    "IRQNoFlags",
    "IRQ"
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDecodeAttribute
 *
 * PARAMETERS:  Attribute       - Attribute field of AccessAs keyword
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode the AccessAs attribute byte. (Mostly SMBus and
 *              GenericSerialBus stuff.)
 *
 ******************************************************************************/

void
AcpiDmDecodeAttribute (
    UINT8                   Attribute)
{

    switch (Attribute)
    {
    case AML_FIELD_ATTRIB_QUICK:

        AcpiOsPrintf ("AttribQuick");
        break;

    case AML_FIELD_ATTRIB_SEND_RECEIVE:

        AcpiOsPrintf ("AttribSendReceive");
        break;

    case AML_FIELD_ATTRIB_BYTE:

        AcpiOsPrintf ("AttribByte");
        break;

    case AML_FIELD_ATTRIB_WORD:

        AcpiOsPrintf ("AttribWord");
        break;

    case AML_FIELD_ATTRIB_BLOCK:

        AcpiOsPrintf ("AttribBlock");
        break;

    case AML_FIELD_ATTRIB_BYTES:

        AcpiOsPrintf ("AttribBytes");
        break;

    case AML_FIELD_ATTRIB_PROCESS_CALL:

        AcpiOsPrintf ("AttribProcessCall");
        break;

    case AML_FIELD_ATTRIB_BLOCK_PROCESS_CALL:

        AcpiOsPrintf ("AttribBlockProcessCall");
        break;

    case AML_FIELD_ATTRIB_RAW_BYTES:

        AcpiOsPrintf ("AttribRawBytes");
        break;

    case AML_FIELD_ATTRIB_RAW_PROCESS_BYTES:

        AcpiOsPrintf ("AttribRawProcessBytes");
        break;

    default:

        /* A ByteConst is allowed by the grammar */

        AcpiOsPrintf ("0x%2.2X", Attribute);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIndent
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Indent 4 spaces per indentation level.
 *
 ******************************************************************************/

void
AcpiDmIndent (
    UINT32                  Level)
{

    if (!Level)
    {
        return;
    }

    AcpiOsPrintf ("%*.s", (Level * 4), " ");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfListMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      TRUE if a comma was inserted
 *
 * DESCRIPTION: Insert a comma if this Op is a member of an argument list.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmCommaIfListMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (!Op->Common.Next)
    {
        ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);
        return (FALSE);
    }

    if (AcpiDmListType (Op->Common.Parent) & BLOCK_COMMA_LIST)
    {
        /* Exit if Target has been marked IGNORE */

        if (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_IGNORE)
        {
            ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);
            return (FALSE);
        }

        /* Check for a NULL target operand */

        if ((Op->Common.Next->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            (!Op->Common.Next->Common.Value.String))
        {
            /*
             * To handle the Divide() case where there are two optional
             * targets, look ahead one more op. If null, this null target
             * is the one and only target -- no comma needed. Otherwise,
             * we need a comma to prepare for the next target.
             */
            if (!Op->Common.Next->Common.Next)
            {
                ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);
                return (FALSE);
            }
        }

        if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
            (!(Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST)))
        {
            ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);
            return (FALSE);
        }

        /* Emit comma only if this is not a C-style operator */

        if (!Op->Common.OperatorSymbol)
        {
            AcpiOsPrintf (", ");
            ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);
        }

        return (TRUE);
    }

    else if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
             (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST))
    {
        AcpiOsPrintf (", ");
        ASL_CV_PRINT_ONE_COMMENT (Op, AMLCOMMENT_INLINE, NULL, 0);

        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfFieldMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a comma if this Op is a member of a Field argument list.
 *
 ******************************************************************************/

void
AcpiDmCommaIfFieldMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (Op->Common.Next)
    {
        AcpiOsPrintf (", ");
    }
}
