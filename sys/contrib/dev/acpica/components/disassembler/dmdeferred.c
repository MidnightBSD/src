/******************************************************************************
 *
 * Module Name: dmdeferred - Disassembly of deferred AML opcodes
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
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acparser.h>

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmdeferred")


/* Local prototypes */

static ACPI_STATUS
AcpiDmDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength);


/******************************************************************************
 *
 * FUNCTION:    AcpiDmParseDeferredOps
 *
 * PARAMETERS:  Root                - Root of the parse tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the deferred opcodes (Methods, regions, etc.)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiDmParseDeferredOps (
    ACPI_PARSE_OBJECT       *Root)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *Op = Root;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (DmParseDeferredOps);


    /* Traverse the entire parse tree */

    while (Op)
    {
        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (!(OpInfo->Flags & AML_DEFER))
        {
            Op = AcpiPsGetDepthNext (Root, Op);
            continue;
        }

        /* Now we know we have a deferred opcode */

        switch (Op->Common.AmlOpcode)
        {
        case AML_METHOD_OP:
        case AML_BUFFER_OP:
        case AML_PACKAGE_OP:
        case AML_VARIABLE_PACKAGE_OP:

            Status = AcpiDmDeferredParse (
                Op, Op->Named.Data, Op->Named.Length);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }
            break;

        /* We don't need to do anything for these deferred opcodes */

        case AML_REGION_OP:
        case AML_DATA_REGION_OP:
        case AML_CREATE_QWORD_FIELD_OP:
        case AML_CREATE_DWORD_FIELD_OP:
        case AML_CREATE_WORD_FIELD_OP:
        case AML_CREATE_BYTE_FIELD_OP:
        case AML_CREATE_BIT_FIELD_OP:
        case AML_CREATE_FIELD_OP:
        case AML_BANK_FIELD_OP:

            break;

        default:

            ACPI_ERROR ((AE_INFO, "Unhandled deferred AML opcode [0x%.4X]",
                 Op->Common.AmlOpcode));
            break;
        }

        Op = AcpiPsGetDepthNext (Root, Op);
    }

    return_ACPI_STATUS (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiDmDeferredParse
 *
 * PARAMETERS:  Op                  - Root Op of the deferred opcode
 *              Aml                 - Pointer to the raw AML
 *              AmlLength           - Length of the AML
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse one deferred opcode
 *              (Methods, operation regions, etc.)
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiDmDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *SearchOp;
    ACPI_PARSE_OBJECT       *StartOp;
    ACPI_PARSE_OBJECT       *NewRootOp;
    ACPI_PARSE_OBJECT       *ExtraOp;


    ACPI_FUNCTION_TRACE (DmDeferredParse);


    if (!Aml || !AmlLength)
    {
        return_ACPI_STATUS (AE_OK);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Parsing deferred opcode %s [%4.4s]\n",
        Op->Common.AmlOpName, (char *) &Op->Named.Name));

    /* Need a new walk state to parse the AML */

    WalkState = AcpiDsCreateWalkState (0, Op, NULL, NULL);
    if (!WalkState)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, NULL, Aml,
        AmlLength, NULL, ACPI_IMODE_LOAD_PASS1);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parse the AML for this deferred opcode */

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
    WalkState->ParseFlags |= ACPI_PARSE_DISASSEMBLE;
    Status = AcpiPsParseAml (WalkState);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS(Status);
    }

    StartOp = (Op->Common.Value.Arg)->Common.Next;
    SearchOp = StartOp;
    while (SearchOp)
    {
        SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
    }

    /*
     * For Buffer and Package opcodes, link the newly parsed subtree
     * into the main parse tree
     */
    switch (Op->Common.AmlOpcode)
    {
    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VARIABLE_PACKAGE_OP:

        switch (Op->Common.AmlOpcode)
        {
        case AML_PACKAGE_OP:

            ExtraOp = Op->Common.Value.Arg;
            NewRootOp = ExtraOp->Common.Next;
            ACPI_FREE (ExtraOp);
            break;

        case AML_VARIABLE_PACKAGE_OP:
        case AML_BUFFER_OP:
        default:

            NewRootOp = Op->Common.Value.Arg;
            break;
        }

        Op->Common.Value.Arg = NewRootOp->Common.Value.Arg;

        /* Must point all parents to the main tree */

        StartOp = Op;
        SearchOp = StartOp;
        while (SearchOp)
        {
            if (SearchOp->Common.Parent == NewRootOp)
            {
                SearchOp->Common.Parent = Op;
            }

            SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
        }

        ACPI_FREE (NewRootOp);
        break;

    default:

        break;
    }

    return_ACPI_STATUS (AE_OK);
}
