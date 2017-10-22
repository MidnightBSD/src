/******************************************************************************
 *
 * Module Name: psloop - Main AML parse loop
 *              $Revision: 1.16 $
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


/*
 * Parse the AML and build an operation tree as most interpreters, (such as
 * Perl) do. Parsing is done by hand rather than with a YACC generated parser
 * to tightly constrain stack and dynamic memory usage. Parsing is kept
 * flexible and the code fairly compact by parsing based on a list of AML
 * opcode templates in AmlOpInfo[].
 */

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/amlcode.h>

#define _COMPONENT          ACPI_PARSER
        ACPI_MODULE_NAME    ("psloop")

static UINT32               AcpiGbl_Depth = 0;


/* Local prototypes */

static ACPI_STATUS
AcpiPsGetAmlOpcode (
    ACPI_WALK_STATE         *WalkState);

static ACPI_STATUS
AcpiPsBuildNamedOp (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       *UnnamedOp,
    ACPI_PARSE_OBJECT       **Op);

static ACPI_STATUS
AcpiPsCreateOp (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       **NewOp);

static ACPI_STATUS
AcpiPsGetArguments (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       *Op);

static ACPI_STATUS
AcpiPsCompleteOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **Op,
    ACPI_STATUS             Status);

static ACPI_STATUS
AcpiPsCompleteFinalOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             Status);


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetAmlOpcode
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Extract the next AML opcode from the input stream.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsGetAmlOpcode (
    ACPI_WALK_STATE         *WalkState)
{

    ACPI_FUNCTION_TRACE_PTR (PsGetAmlOpcode, WalkState);


    WalkState->AmlOffset = (UINT32) ACPI_PTR_DIFF (WalkState->ParserState.Aml,
                                WalkState->ParserState.AmlStart);
    WalkState->Opcode = AcpiPsPeekOpcode (&(WalkState->ParserState));

    /*
     * First cut to determine what we have found:
     * 1) A valid AML opcode
     * 2) A name string
     * 3) An unknown/invalid opcode
     */
    WalkState->OpInfo = AcpiPsGetOpcodeInfo (WalkState->Opcode);

    switch (WalkState->OpInfo->Class)
    {
    case AML_CLASS_ASCII:
    case AML_CLASS_PREFIX:
        /*
         * Starts with a valid prefix or ASCII char, this is a name
         * string. Convert the bare name string to a namepath.
         */
        WalkState->Opcode = AML_INT_NAMEPATH_OP;
        WalkState->ArgTypes = ARGP_NAMESTRING;
        break;

    case AML_CLASS_UNKNOWN:

        /* The opcode is unrecognized. Just skip unknown opcodes */

        ACPI_ERROR ((AE_INFO,
             "Found unknown opcode %X at AML address %p offset %X, ignoring",
              WalkState->Opcode, WalkState->ParserState.Aml, WalkState->AmlOffset));

        ACPI_DUMP_BUFFER (WalkState->ParserState.Aml, 128);

        /* Assume one-byte bad opcode */

        WalkState->ParserState.Aml++;
        return_ACPI_STATUS (AE_CTRL_PARSE_CONTINUE);

    default:

        /* Found opcode info, this is a normal opcode */

        WalkState->ParserState.Aml += AcpiPsGetOpcodeSize (WalkState->Opcode);
        WalkState->ArgTypes = WalkState->OpInfo->ParseArgs;
        break;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsBuildNamedOp
 *
 * PARAMETERS:  WalkState           - Current state
 *              AmlOpStart          - Begin of named Op in AML
 *              UnnamedOp           - Early Op (not a named Op)
 *              Op                  - Returned Op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse a named Op
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsBuildNamedOp (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       *UnnamedOp,
    ACPI_PARSE_OBJECT       **Op)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Arg = NULL;


    ACPI_FUNCTION_TRACE_PTR (PsBuildNamedOp, WalkState);


    UnnamedOp->Common.Value.Arg = NULL;
    UnnamedOp->Common.ArgListLength = 0;
    UnnamedOp->Common.AmlOpcode = WalkState->Opcode;

    /*
     * Get and append arguments until we find the node that contains
     * the name (the type ARGP_NAME).
     */
    while (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) &&
          (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) != ARGP_NAME))
    {
        Status = AcpiPsGetNextArg (WalkState, &(WalkState->ParserState),
                    GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), &Arg);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        AcpiPsAppendArg (UnnamedOp, Arg);
        INCREMENT_ARG_LIST (WalkState->ArgTypes);
    }

    /*
     * Make sure that we found a NAME and didn't run out of arguments
     */
    if (!GET_CURRENT_ARG_TYPE (WalkState->ArgTypes))
    {
        return_ACPI_STATUS (AE_AML_NO_OPERAND);
    }

    /* We know that this arg is a name, move to next arg */

    INCREMENT_ARG_LIST (WalkState->ArgTypes);

    /*
     * Find the object. This will either insert the object into
     * the namespace or simply look it up
     */
    WalkState->Op = NULL;

    Status = WalkState->DescendingCallback (WalkState, Op);
    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status, "During name lookup/catalog"));
        return_ACPI_STATUS (Status);
    }

    if (!*Op)
    {
        return_ACPI_STATUS (AE_CTRL_PARSE_CONTINUE);
    }

    Status = AcpiPsNextParseState (WalkState, *Op, Status);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_CTRL_PENDING)
        {
            return_ACPI_STATUS (AE_CTRL_PARSE_PENDING);
        }
        return_ACPI_STATUS (Status);
    }

    AcpiPsAppendArg (*Op, UnnamedOp->Common.Value.Arg);
    AcpiGbl_Depth++;

    if ((*Op)->Common.AmlOpcode == AML_REGION_OP)
    {
        /*
         * Defer final parsing of an OperationRegion body, because we don't
         * have enough info in the first pass to parse it correctly (i.e.,
         * there may be method calls within the TermArg elements of the body.)
         *
         * However, we must continue parsing because the opregion is not a
         * standalone package -- we don't know where the end is at this point.
         *
         * (Length is unknown until parse of the body complete)
         */
        (*Op)->Named.Data = AmlOpStart;
        (*Op)->Named.Length = 0;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCreateOp
 *
 * PARAMETERS:  WalkState           - Current state
 *              AmlOpStart          - Op start in AML
 *              NewOp               - Returned Op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get Op from AML
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsCreateOp (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       **NewOp)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Op;
    ACPI_PARSE_OBJECT       *NamedOp = NULL;
    ACPI_PARSE_OBJECT       *ParentScope;
    UINT8                   ArgumentCount;
    const ACPI_OPCODE_INFO  *OpInfo;


    ACPI_FUNCTION_TRACE_PTR (PsCreateOp, WalkState);


    Status = AcpiPsGetAmlOpcode (WalkState);
    if (Status == AE_CTRL_PARSE_CONTINUE)
    {
        return_ACPI_STATUS (AE_CTRL_PARSE_CONTINUE);
    }

    /* Create Op structure and append to parent's argument list */

    WalkState->OpInfo = AcpiPsGetOpcodeInfo (WalkState->Opcode);
    Op = AcpiPsAllocOp (WalkState->Opcode);
    if (!Op)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    if (WalkState->OpInfo->Flags & AML_NAMED)
    {
        Status = AcpiPsBuildNamedOp (WalkState, AmlOpStart, Op, &NamedOp);
        AcpiPsFreeOp (Op);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        *NewOp = NamedOp;
        return_ACPI_STATUS (AE_OK);
    }

    /* Not a named opcode, just allocate Op and append to parent */

    if (WalkState->OpInfo->Flags & AML_CREATE)
    {
        /*
         * Backup to beginning of CreateXXXfield declaration
         * BodyLength is unknown until we parse the body
         */
        Op->Named.Data = AmlOpStart;
        Op->Named.Length = 0;
    }

    ParentScope = AcpiPsGetParentScope (&(WalkState->ParserState));
    AcpiPsAppendArg (ParentScope, Op);

    if (ParentScope)
    {
        OpInfo = AcpiPsGetOpcodeInfo (ParentScope->Common.AmlOpcode);
        if (OpInfo->Flags & AML_HAS_TARGET)
        {
            ArgumentCount = AcpiPsGetArgumentCount (OpInfo->Type);
            if (ParentScope->Common.ArgListLength > ArgumentCount)
            {
                Op->Common.Flags |= ACPI_PARSEOP_TARGET;
            }
        }
        else if (ParentScope->Common.AmlOpcode == AML_INCREMENT_OP)
        {
            Op->Common.Flags |= ACPI_PARSEOP_TARGET;
        }
    }

    if (WalkState->DescendingCallback != NULL)
    {
        /*
         * Find the object. This will either insert the object into
         * the namespace or simply look it up
         */
        WalkState->Op = *NewOp = Op;

        Status = WalkState->DescendingCallback (WalkState, &Op);
        Status = AcpiPsNextParseState (WalkState, Op, Status);
        if (Status == AE_CTRL_PENDING)
        {
            Status = AE_CTRL_PARSE_PENDING;
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsGetArguments
 *
 * PARAMETERS:  WalkState           - Current state
 *              AmlOpStart          - Op start in AML
 *              Op                  - Current Op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Get arguments for passed Op.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsGetArguments (
    ACPI_WALK_STATE         *WalkState,
    UINT8                   *AmlOpStart,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Arg = NULL;


    ACPI_FUNCTION_TRACE_PTR (PsGetArguments, WalkState);


    switch (Op->Common.AmlOpcode)
    {
    case AML_BYTE_OP:       /* AML_BYTEDATA_ARG */
    case AML_WORD_OP:       /* AML_WORDDATA_ARG */
    case AML_DWORD_OP:      /* AML_DWORDATA_ARG */
    case AML_QWORD_OP:      /* AML_QWORDATA_ARG */
    case AML_STRING_OP:     /* AML_ASCIICHARLIST_ARG */

        /* Fill in constant or string argument directly */

        AcpiPsGetNextSimpleArg (&(WalkState->ParserState),
            GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), Op);
        break;

    case AML_INT_NAMEPATH_OP:   /* AML_NAMESTRING_ARG */

        Status = AcpiPsGetNextNamepath (WalkState, &(WalkState->ParserState), Op, 1);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

        WalkState->ArgTypes = 0;
        break;

    default:
        /*
         * Op is not a constant or string, append each argument to the Op
         */
        while (GET_CURRENT_ARG_TYPE (WalkState->ArgTypes) && !WalkState->ArgCount)
        {
            WalkState->AmlOffset = (UINT32) ACPI_PTR_DIFF (WalkState->ParserState.Aml,
                WalkState->ParserState.AmlStart);

            Status = AcpiPsGetNextArg (WalkState, &(WalkState->ParserState),
                        GET_CURRENT_ARG_TYPE (WalkState->ArgTypes), &Arg);
            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            if (Arg)
            {
                Arg->Common.AmlOffset = WalkState->AmlOffset;
                AcpiPsAppendArg (Op, Arg);
            }

            INCREMENT_ARG_LIST (WalkState->ArgTypes);
        }


        /* Special processing for certain opcodes */

        /* TBD (remove): Temporary mechanism to disable this code if needed */

#ifdef ACPI_ENABLE_MODULE_LEVEL_CODE

        if ((WalkState->PassNumber <= ACPI_IMODE_LOAD_PASS1) &&
            ((WalkState->ParseFlags & ACPI_PARSE_DISASSEMBLE) == 0))
        {
            /*
             * We want to skip If/Else/While constructs during Pass1 because we
             * want to actually conditionally execute the code during Pass2.
             *
             * Except for disassembly, where we always want to walk the
             * If/Else/While packages
             */
            switch (Op->Common.AmlOpcode)
            {
            case AML_IF_OP:
            case AML_ELSE_OP:
            case AML_WHILE_OP:

                ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
                    "Pass1: Skipping an If/Else/While body\n"));

                /* Skip body of if/else/while in pass 1 */

                WalkState->ParserState.Aml = WalkState->ParserState.PkgEnd;
                WalkState->ArgCount = 0;
                break;

            default:
                break;
            }
        }
#endif

        switch (Op->Common.AmlOpcode)
        {
        case AML_METHOD_OP:
            /*
             * Skip parsing of control method because we don't have enough
             * info in the first pass to parse it correctly.
             *
             * Save the length and address of the body
             */
            Op->Named.Data = WalkState->ParserState.Aml;
            Op->Named.Length = (UINT32)
                (WalkState->ParserState.PkgEnd - WalkState->ParserState.Aml);

            /* Skip body of method */

            WalkState->ParserState.Aml = WalkState->ParserState.PkgEnd;
            WalkState->ArgCount = 0;
            break;

        case AML_BUFFER_OP:
        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:

            if ((Op->Common.Parent) &&
                (Op->Common.Parent->Common.AmlOpcode == AML_NAME_OP) &&
                (WalkState->PassNumber <= ACPI_IMODE_LOAD_PASS2))
            {
                /*
                 * Skip parsing of Buffers and Packages because we don't have
                 * enough info in the first pass to parse them correctly.
                 */
                Op->Named.Data = AmlOpStart;
                Op->Named.Length = (UINT32)
                    (WalkState->ParserState.PkgEnd - AmlOpStart);

                /* Skip body */

                WalkState->ParserState.Aml = WalkState->ParserState.PkgEnd;
                WalkState->ArgCount = 0;
            }
            break;

        case AML_WHILE_OP:

            if (WalkState->ControlState)
            {
                WalkState->ControlState->Control.PackageEnd =
                    WalkState->ParserState.PkgEnd;
            }
            break;

        default:

            /* No action for all other opcodes */
            break;
        }

        break;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCompleteOp
 *
 * PARAMETERS:  WalkState           - Current state
 *              Op                  - Returned Op
 *              Status              - Parse status before complete Op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Complete Op
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsCompleteOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **Op,
    ACPI_STATUS             Status)
{
    ACPI_STATUS             Status2;


    ACPI_FUNCTION_TRACE_PTR (PsCompleteOp, WalkState);


    /*
     * Finished one argument of the containing scope
     */
    WalkState->ParserState.Scope->ParseScope.ArgCount--;

    /* Close this Op (will result in parse subtree deletion) */

    Status2 = AcpiPsCompleteThisOp (WalkState, *Op);
    if (ACPI_FAILURE (Status2))
    {
        return_ACPI_STATUS (Status2);
    }

    *Op = NULL;

    switch (Status)
    {
    case AE_OK:
        break;


    case AE_CTRL_TRANSFER:

        /* We are about to transfer to a called method */

        WalkState->PrevOp = NULL;
        WalkState->PrevArgTypes = WalkState->ArgTypes;
        return_ACPI_STATUS (Status);


    case AE_CTRL_END:

        AcpiPsPopScope (&(WalkState->ParserState), Op,
            &WalkState->ArgTypes, &WalkState->ArgCount);

        if (*Op)
        {
            WalkState->Op = *Op;
            WalkState->OpInfo = AcpiPsGetOpcodeInfo ((*Op)->Common.AmlOpcode);
            WalkState->Opcode = (*Op)->Common.AmlOpcode;

            Status = WalkState->AscendingCallback (WalkState);
            Status = AcpiPsNextParseState (WalkState, *Op, Status);

            Status2 = AcpiPsCompleteThisOp (WalkState, *Op);
            if (ACPI_FAILURE (Status2))
            {
                return_ACPI_STATUS (Status2);
            }
        }

        Status = AE_OK;
        break;


    case AE_CTRL_BREAK:
    case AE_CTRL_CONTINUE:

        /* Pop off scopes until we find the While */

        while (!(*Op) || ((*Op)->Common.AmlOpcode != AML_WHILE_OP))
        {
            AcpiPsPopScope (&(WalkState->ParserState), Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);
        }

        /* Close this iteration of the While loop */

        WalkState->Op = *Op;
        WalkState->OpInfo = AcpiPsGetOpcodeInfo ((*Op)->Common.AmlOpcode);
        WalkState->Opcode = (*Op)->Common.AmlOpcode;

        Status = WalkState->AscendingCallback (WalkState);
        Status = AcpiPsNextParseState (WalkState, *Op, Status);

        Status2 = AcpiPsCompleteThisOp (WalkState, *Op);
        if (ACPI_FAILURE (Status2))
        {
            return_ACPI_STATUS (Status2);
        }

        Status = AE_OK;
        break;


    case AE_CTRL_TERMINATE:

        /* Clean up */
        do
        {
            if (*Op)
            {
                Status2 = AcpiPsCompleteThisOp (WalkState, *Op);
                if (ACPI_FAILURE (Status2))
                {
                    return_ACPI_STATUS (Status2);
                }

                AcpiUtDeleteGenericState (
                    AcpiUtPopGenericState (&WalkState->ControlState));
            }

            AcpiPsPopScope (&(WalkState->ParserState), Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);

        } while (*Op);

        return_ACPI_STATUS (AE_OK);


    default:  /* All other non-AE_OK status */

        do
        {
            if (*Op)
            {
                Status2 = AcpiPsCompleteThisOp (WalkState, *Op);
                if (ACPI_FAILURE (Status2))
                {
                    return_ACPI_STATUS (Status2);
                }
            }

            AcpiPsPopScope (&(WalkState->ParserState), Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);

        } while (*Op);


#if 0
        /*
         * TBD: Cleanup parse ops on error
         */
        if (*Op == NULL)
        {
            AcpiPsPopScope (ParserState, Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);
        }
#endif
        WalkState->PrevOp = NULL;
        WalkState->PrevArgTypes = WalkState->ArgTypes;
        return_ACPI_STATUS (Status);
    }

    /* This scope complete? */

    if (AcpiPsHasCompletedScope (&(WalkState->ParserState)))
    {
        AcpiPsPopScope (&(WalkState->ParserState), Op,
            &WalkState->ArgTypes, &WalkState->ArgCount);
        ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Popped scope, Op=%p\n", *Op));
    }
    else
    {
        *Op = NULL;
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsCompleteFinalOp
 *
 * PARAMETERS:  WalkState           - Current state
 *              Op                  - Current Op
 *              Status              - Current parse status before complete last
 *                                    Op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Complete last Op.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiPsCompleteFinalOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_STATUS             Status)
{
    ACPI_STATUS             Status2;


    ACPI_FUNCTION_TRACE_PTR (PsCompleteFinalOp, WalkState);


    /*
     * Complete the last Op (if not completed), and clear the scope stack.
     * It is easily possible to end an AML "package" with an unbounded number
     * of open scopes (such as when several ASL blocks are closed with
     * sequential closing braces). We want to terminate each one cleanly.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "AML package complete at Op %p\n", Op));
    do
    {
        if (Op)
        {
            if (WalkState->AscendingCallback != NULL)
            {
                WalkState->Op = Op;
                WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
                WalkState->Opcode = Op->Common.AmlOpcode;

                Status = WalkState->AscendingCallback (WalkState);
                Status = AcpiPsNextParseState (WalkState, Op, Status);
                if (Status == AE_CTRL_PENDING)
                {
                    Status = AcpiPsCompleteOp (WalkState, &Op, AE_OK);
                    if (ACPI_FAILURE (Status))
                    {
                        return_ACPI_STATUS (Status);
                    }
                }

                if (Status == AE_CTRL_TERMINATE)
                {
                    Status = AE_OK;

                    /* Clean up */
                    do
                    {
                        if (Op)
                        {
                            Status2 = AcpiPsCompleteThisOp (WalkState, Op);
                            if (ACPI_FAILURE (Status2))
                            {
                                return_ACPI_STATUS (Status2);
                            }
                        }

                        AcpiPsPopScope (&(WalkState->ParserState), &Op,
                            &WalkState->ArgTypes, &WalkState->ArgCount);

                    } while (Op);

                    return_ACPI_STATUS (Status);
                }

                else if (ACPI_FAILURE (Status))
                {
                    /* First error is most important */

                    (void) AcpiPsCompleteThisOp (WalkState, Op);
                    return_ACPI_STATUS (Status);
                }
            }

            Status2 = AcpiPsCompleteThisOp (WalkState, Op);
            if (ACPI_FAILURE (Status2))
            {
                return_ACPI_STATUS (Status2);
            }
        }

        AcpiPsPopScope (&(WalkState->ParserState), &Op, &WalkState->ArgTypes,
            &WalkState->ArgCount);

    } while (Op);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiPsParseLoop
 *
 * PARAMETERS:  WalkState           - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse AML (pointed to by the current parser state) and return
 *              a tree of ops.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiPsParseLoop (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PARSE_OBJECT       *Op = NULL;     /* current op */
    ACPI_PARSE_STATE        *ParserState;
    UINT8                   *AmlOpStart = NULL;


    ACPI_FUNCTION_TRACE_PTR (PsParseLoop, WalkState);


    if (WalkState->DescendingCallback == NULL)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    ParserState = &WalkState->ParserState;
    WalkState->ArgTypes = 0;

#if (!defined (ACPI_NO_METHOD_EXECUTION) && !defined (ACPI_CONSTANT_EVAL_ONLY))

    if (WalkState->WalkType & ACPI_WALK_METHOD_RESTART)
    {
        /* We are restarting a preempted control method */

        if (AcpiPsHasCompletedScope (ParserState))
        {
            /*
             * We must check if a predicate to an IF or WHILE statement
             * was just completed
             */
            if ((ParserState->Scope->ParseScope.Op) &&
               ((ParserState->Scope->ParseScope.Op->Common.AmlOpcode == AML_IF_OP) ||
                (ParserState->Scope->ParseScope.Op->Common.AmlOpcode == AML_WHILE_OP)) &&
                (WalkState->ControlState) &&
                (WalkState->ControlState->Common.State ==
                    ACPI_CONTROL_PREDICATE_EXECUTING))
            {
                /*
                 * A predicate was just completed, get the value of the
                 * predicate and branch based on that value
                 */
                WalkState->Op = NULL;
                Status = AcpiDsGetPredicateValue (WalkState, ACPI_TO_POINTER (TRUE));
                if (ACPI_FAILURE (Status) &&
                    ((Status & AE_CODE_MASK) != AE_CODE_CONTROL))
                {
                    if (Status == AE_AML_NO_RETURN_VALUE)
                    {
                        ACPI_EXCEPTION ((AE_INFO, Status,
                            "Invoked method did not return a value"));

                    }

                    ACPI_EXCEPTION ((AE_INFO, Status, "GetPredicate Failed"));
                    return_ACPI_STATUS (Status);
                }

                Status = AcpiPsNextParseState (WalkState, Op, Status);
            }

            AcpiPsPopScope (ParserState, &Op,
                &WalkState->ArgTypes, &WalkState->ArgCount);
            ACPI_DEBUG_PRINT ((ACPI_DB_PARSE, "Popped scope, Op=%p\n", Op));
        }
        else if (WalkState->PrevOp)
        {
            /* We were in the middle of an op */

            Op = WalkState->PrevOp;
            WalkState->ArgTypes = WalkState->PrevArgTypes;
        }
    }
#endif

    /* Iterative parsing loop, while there is more AML to process: */

    while ((ParserState->Aml < ParserState->AmlEnd) || (Op))
    {
        AmlOpStart = ParserState->Aml;
        if (!Op)
        {
            Status = AcpiPsCreateOp (WalkState, AmlOpStart, &Op);
            if (ACPI_FAILURE (Status))
            {
                if (Status == AE_CTRL_PARSE_CONTINUE)
                {
                    continue;
                }

                if (Status == AE_CTRL_PARSE_PENDING)
                {
                    Status = AE_OK;
                }

                Status = AcpiPsCompleteOp (WalkState, &Op, Status);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }

                continue;
            }

            Op->Common.AmlOffset = WalkState->AmlOffset;

            if (WalkState->OpInfo)
            {
                ACPI_DEBUG_PRINT ((ACPI_DB_PARSE,
                    "Opcode %4.4X [%s] Op %p Aml %p AmlOffset %5.5X\n",
                     (UINT32) Op->Common.AmlOpcode, WalkState->OpInfo->Name,
                     Op, ParserState->Aml, Op->Common.AmlOffset));
            }
        }


        /*
         * Start ArgCount at zero because we don't know if there are
         * any args yet
         */
        WalkState->ArgCount  = 0;

        /* Are there any arguments that must be processed? */

        if (WalkState->ArgTypes)
        {
            /* Get arguments */

            Status = AcpiPsGetArguments (WalkState, AmlOpStart, Op);
            if (ACPI_FAILURE (Status))
            {
                Status = AcpiPsCompleteOp (WalkState, &Op, Status);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }

                continue;
            }
        }

        /* Check for arguments that need to be processed */

        if (WalkState->ArgCount)
        {
            /*
             * There are arguments (complex ones), push Op and
             * prepare for argument
             */
            Status = AcpiPsPushScope (ParserState, Op,
                        WalkState->ArgTypes, WalkState->ArgCount);
            if (ACPI_FAILURE (Status))
            {
                Status = AcpiPsCompleteOp (WalkState, &Op, Status);
                if (ACPI_FAILURE (Status))
                {
                    return_ACPI_STATUS (Status);
                }

                continue;
            }

            Op = NULL;
            continue;
        }

        /*
         * All arguments have been processed -- Op is complete,
         * prepare for next
         */
        WalkState->OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (WalkState->OpInfo->Flags & AML_NAMED)
        {
            if (AcpiGbl_Depth)
            {
                AcpiGbl_Depth--;
            }

            if (Op->Common.AmlOpcode == AML_REGION_OP)
            {
                /*
                 * Skip parsing of control method or opregion body,
                 * because we don't have enough info in the first pass
                 * to parse them correctly.
                 *
                 * Completed parsing an OpRegion declaration, we now
                 * know the length.
                 */
                Op->Named.Length = (UINT32) (ParserState->Aml - Op->Named.Data);
            }
        }

        if (WalkState->OpInfo->Flags & AML_CREATE)
        {
            /*
             * Backup to beginning of CreateXXXfield declaration (1 for
             * Opcode)
             *
             * BodyLength is unknown until we parse the body
             */
            Op->Named.Length = (UINT32) (ParserState->Aml - Op->Named.Data);
        }

        /* This op complete, notify the dispatcher */

        if (WalkState->AscendingCallback != NULL)
        {
            WalkState->Op = Op;
            WalkState->Opcode = Op->Common.AmlOpcode;

            Status = WalkState->AscendingCallback (WalkState);
            Status = AcpiPsNextParseState (WalkState, Op, Status);
            if (Status == AE_CTRL_PENDING)
            {
                Status = AE_OK;
            }
        }

        Status = AcpiPsCompleteOp (WalkState, &Op, Status);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }

    } /* while ParserState->Aml */

    Status = AcpiPsCompleteFinalOp (WalkState, Op, Status);
    return_ACPI_STATUS (Status);
}

