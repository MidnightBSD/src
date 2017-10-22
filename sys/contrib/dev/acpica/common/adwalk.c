/******************************************************************************
 *
 * Module Name: adwalk - Application-level disassembler parse tree walk routines
 *              $Revision: 1.6 $
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


#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/amlcode.h>
#include <contrib/dev/acpica/acdebug.h>
#include <contrib/dev/acpica/acdisasm.h>
#include <contrib/dev/acpica/acdispat.h>
#include <contrib/dev/acpica/acnamesp.h>
#include <contrib/dev/acpica/acapps.h>


#define _COMPONENT          ACPI_TOOLS
        ACPI_MODULE_NAME    ("adwalk")

/*
 * aslmap - opcode mappings and reserved method names
 */
ACPI_OBJECT_TYPE
AslMapNamedOpcodeToDataType (
    UINT16                  Opcode);

/* Local prototypes */

static ACPI_STATUS
AcpiDmFindOrphanDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmDumpDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmXrefDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmCommonAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static ACPI_STATUS
AcpiDmLoadDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);

static UINT32
AcpiDmInspectPossibleArgs (
    UINT32                  CurrentOpArgCount,
    UINT32                  TargetCount,
    ACPI_PARSE_OBJECT       *Op);

static ACPI_STATUS
AcpiDmResourceDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpTree
 *
 * PARAMETERS:  Origin          - Starting object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to format and output the nodes
 *
 ******************************************************************************/

void
AcpiDmDumpTree (
    ACPI_PARSE_OBJECT       *Origin)
{
    ACPI_OP_WALK_INFO       Info;


    if (!Origin)
    {
        return;
    }

    AcpiOsPrintf ("/*\nAML Parse Tree\n\n");
    Info.Flags = 0;
    Info.Count = 0;
    Info.Level = 0;
    Info.WalkState = NULL;
    AcpiDmWalkParseTree (Origin, AcpiDmDumpDescending, NULL, &Info);
    AcpiOsPrintf ("*/\n\n");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFindOrphanMethods
 *
 * PARAMETERS:  Origin          - Starting object
 *
 * RETURN:      None
 *
 * DESCRIPTION: Parse tree walk to find "orphaned" method invocations -- methods
 *              that are not resolved in the namespace
 *
 ******************************************************************************/

void
AcpiDmFindOrphanMethods (
    ACPI_PARSE_OBJECT       *Origin)
{
    ACPI_OP_WALK_INFO       Info;


    if (!Origin)
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = NULL;
    AcpiDmWalkParseTree (Origin, AcpiDmFindOrphanDescending, NULL, &Info);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFinishNamespaceLoad
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *
 * RETURN:      None
 *
 * DESCRIPTION: Load all namespace items that are created within control
 *              methods. Used before namespace cross reference
 *
 ******************************************************************************/

void
AcpiDmFinishNamespaceLoad (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmLoadDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCrossReferenceNamespace
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *
 * RETURN:      None
 *
 * DESCRIPTION: Cross reference the namespace to create externals
 *
 ******************************************************************************/

void
AcpiDmCrossReferenceNamespace (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmXrefDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmConvertResourceIndexes
 *
 * PARAMETERS:  ParseTreeRoot       - Root of the parse tree
 *              NamespaceRoot       - Root of the internal namespace
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert fixed-offset references to resource descriptors to
 *              symbolic references. Should only be called after namespace has
 *              been cross referenced.
 *
 ******************************************************************************/

void
AcpiDmConvertResourceIndexes (
    ACPI_PARSE_OBJECT       *ParseTreeRoot,
    ACPI_NAMESPACE_NODE     *NamespaceRoot)
{
    ACPI_STATUS             Status;
    ACPI_OP_WALK_INFO       Info;
    ACPI_WALK_STATE         *WalkState;


    if (!ParseTreeRoot)
    {
        return;
    }

    /* Create and initialize a new walk state */

    WalkState = AcpiDsCreateWalkState (0, ParseTreeRoot, NULL, NULL);
    if (!WalkState)
    {
        return;
    }

    Status = AcpiDsScopeStackPush (NamespaceRoot, NamespaceRoot->Type, WalkState);
    if (ACPI_FAILURE (Status))
    {
        return;
    }

    Info.Flags = 0;
    Info.Level = 0;
    Info.WalkState = WalkState;
    AcpiDmWalkParseTree (ParseTreeRoot, AcpiDmResourceDescendingOp,
        AcpiDmCommonAscendingOp, &Info);
    ACPI_FREE (WalkState);
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDumpDescending
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Format and print contents of one parse Op.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmDumpDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    char                    *Path;


    if (!Op)
    {
        return (AE_OK);
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    Info->Count++;

    /* Most of the information (count, level, name) here */

    AcpiOsPrintf ("% 5d [%2.2d] ", Info->Count, Level);
    AcpiDmIndent (Level);
    AcpiOsPrintf ("%-28s", AcpiPsGetOpcodeName (Op->Common.AmlOpcode));

    /* Extra info is helpful */

    switch (Op->Common.AmlOpcode)
    {
    case AML_BYTE_OP:
    case AML_WORD_OP:
    case AML_DWORD_OP:
        AcpiOsPrintf ("%X", (UINT32) Op->Common.Value.Integer);
        break;

    case AML_INT_NAMEPATH_OP:
        if (Op->Common.Value.String)
        {
            AcpiNsExternalizeName (ACPI_UINT32_MAX, Op->Common.Value.String,
                            NULL, &Path);
            AcpiOsPrintf ("%s %p", Path, Op->Common.Node);
            ACPI_FREE (Path);
        }
        else
        {
            AcpiOsPrintf ("[NULL]");
        }
        break;

    case AML_NAME_OP:
    case AML_METHOD_OP:
    case AML_DEVICE_OP:
    case AML_INT_NAMEDFIELD_OP:
        AcpiOsPrintf ("%4.4s", &Op->Named.Name);
        break;
    }

    AcpiOsPrintf ("\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmFindOrphanDescending
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Check namepath Ops for orphaned method invocations
 *
 * Note: Experimental.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmFindOrphanDescending (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *ChildOp;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_PARSE_OBJECT       *ParentOp;
    UINT32                  ArgCount;


    if (!Op)
    {
        return (AE_OK);
    }

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    switch (Op->Common.AmlOpcode)
    {
#ifdef ACPI_UNDER_DEVELOPMENT
    case AML_ADD_OP:
        ChildOp = Op->Common.Value.Arg;
        if ((ChildOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            !ChildOp->Common.Node)
        {
            AcpiNsExternalizeName (ACPI_UINT32_MAX, ChildOp->Common.Value.String,
                            NULL, &Path);
            AcpiOsPrintf ("/* %-16s A-NAMEPATH: %s  */\n", Op->Common.AmlOpName, Path);
            ACPI_FREE (Path);

            NextOp = Op->Common.Next;
            if (!NextOp)
            {
                /* This NamePath has no args, assume it is an integer */

                AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0);
                return (AE_OK);
            }

            ArgCount = AcpiDmInspectPossibleArgs (3, 1, NextOp);
            AcpiOsPrintf ("/* A-CHILDREN: %d Actual %d */\n", ArgCount, AcpiDmCountChildren (Op));

            if (ArgCount < 1)
            {
                /* One Arg means this is just a Store(Name,Target) */

                AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0);
                return (AE_OK);
            }

            AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_METHOD, ArgCount);
        }
        break;
#endif

    case AML_STORE_OP:

        ChildOp = Op->Common.Value.Arg;
        if ((ChildOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            !ChildOp->Common.Node)
        {
            NextOp = Op->Common.Next;
            if (!NextOp)
            {
                /* This NamePath has no args, assume it is an integer */

                AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0);
                return (AE_OK);
            }

            ArgCount = AcpiDmInspectPossibleArgs (2, 1, NextOp);
            if (ArgCount <= 1)
            {
                /* One Arg means this is just a Store(Name,Target) */

                AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_INTEGER, 0);
                return (AE_OK);
            }

            AcpiDmAddToExternalList (ChildOp->Common.Value.String, ACPI_TYPE_METHOD, ArgCount);
        }
        break;

    case AML_INT_NAMEPATH_OP:

        /* Must examine parent to see if this namepath is an argument */

        ParentOp = Op->Common.Parent;
        OpInfo = AcpiPsGetOpcodeInfo (ParentOp->Common.AmlOpcode);

        if ((OpInfo->Class != AML_CLASS_EXECUTE) &&
            (OpInfo->Class != AML_CLASS_CREATE) &&
            (ParentOp->Common.AmlOpcode != AML_INT_METHODCALL_OP) &&
            !Op->Common.Node)
        {
            ArgCount = AcpiDmInspectPossibleArgs (0, 0, Op->Common.Next);

            /*
             * Check if namepath is a predicate for if/while or lone parameter to
             * a return.
             */
            if (ArgCount == 0)
            {
                if (((ParentOp->Common.AmlOpcode == AML_IF_OP) ||
                     (ParentOp->Common.AmlOpcode == AML_WHILE_OP) ||
                     (ParentOp->Common.AmlOpcode == AML_RETURN_OP)) &&

                     /* And namepath is the first argument */
                     (ParentOp->Common.Value.Arg == Op))
                {
                    AcpiDmAddToExternalList (Op->Common.Value.String, ACPI_TYPE_INTEGER, 0);
                    break;
                }
            }

            /*
             * This is a standalone namestring (not a parameter to another
             * operator) - it *must* be a method invocation, nothing else is
             * grammatically possible.
             */
            AcpiDmAddToExternalList (Op->Common.Value.String, ACPI_TYPE_METHOD, ArgCount);

        }
        break;
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmLoadDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending handler for namespace control method object load
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmLoadDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status;
    char                    *Path = NULL;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    /* Only interested in operators that create new names */

    if (!(OpInfo->Flags & AML_NAMED) &&
        !(OpInfo->Flags & AML_CREATE))
    {
        goto Exit;
    }

    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        /* For all named operators, get the new name */

        Path = (char *) Op->Named.Path;
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* New name is the last child */

        NextOp = Op->Common.Value.Arg;

        while (NextOp->Common.Next)
        {
            NextOp = NextOp->Common.Next;
        }
        Path = NextOp->Common.Value.String;
    }

    if (!Path)
    {
        goto Exit;
    }

    /* Insert the name into the namespace */

    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ObjectType,
                ACPI_IMODE_LOAD_PASS2, ACPI_NS_DONT_OPEN_SCOPE,
                WalkState, &Node);

    Op->Common.Node = Node;


Exit:

    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {
            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmXrefDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Descending handler for namespace cross reference
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmXrefDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status;
    char                    *Path = NULL;
    ACPI_PARSE_OBJECT       *NextOp;
    ACPI_NAMESPACE_NODE     *Node;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    if ((!(OpInfo->Flags & AML_NAMED)) &&
        (!(OpInfo->Flags & AML_CREATE)) &&
        (Op->Common.AmlOpcode != AML_INT_NAMEPATH_OP))
    {
        goto Exit;
    }

    /* Get the NamePath from the appropriate place */

    if (OpInfo->Flags & AML_NAMED)
    {
        if ((Op->Common.AmlOpcode == AML_ALIAS_OP) ||
            (Op->Common.AmlOpcode == AML_SCOPE_OP))
        {
            /*
             * Only these two operators refer to an existing name,
             * first argument
             */
            Path = (char *) Op->Named.Path;
        }
    }
    else if (OpInfo->Flags & AML_CREATE)
    {
        /* Referenced Buffer Name is the first child */

        NextOp = Op->Common.Value.Arg;
        if (NextOp->Common.AmlOpcode == AML_INT_NAMEPATH_OP)
        {
            Path = NextOp->Common.Value.String;
        }
    }
    else
    {
        Path = Op->Common.Value.String;
    }

    if (!Path)
    {
        goto Exit;
    }

    /*
     * Lookup the name in the namespace.  Name must exist at this point, or it
     * is an invalid reference.
     *
     * The namespace is also used as a lookup table for references to resource
     * descriptors and the fields within them.
     */
    Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
                ACPI_IMODE_EXECUTE, ACPI_NS_SEARCH_PARENT | ACPI_NS_DONT_OPEN_SCOPE,
                WalkState, &Node);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_NOT_FOUND)
        {
            AcpiDmAddToExternalList (Path, (UINT8) ObjectType, 0);

            /*
             * We could install this into the namespace, but we catch duplicate
             * externals when they are added to the list.
             */
#if 0
            Status = AcpiNsLookup (WalkState->ScopeInfo, Path, ACPI_TYPE_ANY,
                       ACPI_IMODE_LOAD_PASS1, ACPI_NS_DONT_OPEN_SCOPE,
                       WalkState, &Node);
#endif
        }
    }
    else
    {
        Op->Common.Node = Node;
    }


Exit:
    /* Open new scope if necessary */

    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {
            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmResourceDescendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Process one parse op during symbolic resource index conversion.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmResourceDescendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_WALK_STATE         *WalkState;
    ACPI_OBJECT_TYPE        ObjectType;
    ACPI_STATUS             Status;


    WalkState = Info->WalkState;
    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

    /* Open new scope if necessary */

    ObjectType = OpInfo->ObjectType;
    if (AcpiNsOpensScope (ObjectType))
    {
        if (Op->Common.Node)
        {

            Status = AcpiDsScopeStackPush (Op->Common.Node, ObjectType, WalkState);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
    }

    /*
     * Check if this operator contains a reference to a resource descriptor.
     * If so, convert the reference into a symbolic reference.
     */
    AcpiDmCheckResourceReference (Op, WalkState);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommonAscendingOp
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ascending handler for combined parse/namespace walks. Closes
 *              scope if necessary.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiDmCommonAscendingOp (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_OP_WALK_INFO       *Info = Context;
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_OBJECT_TYPE        ObjectType;


    /* Close scope if necessary */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
    ObjectType = OpInfo->ObjectType;
    ObjectType = AslMapNamedOpcodeToDataType (Op->Asl.AmlOpcode);

    if (AcpiNsOpensScope (ObjectType))
    {
        (void) AcpiDsScopeStackPop (Info->WalkState);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmInspectPossibleArgs
 *
 * PARAMETERS:  CurrentOpArgCount   - Which arg of the current op was the
 *                                    possible method invocation found
 *              TargetCount         - Number of targets (0,1,2) for this op
 *              Op                  - Parse op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Examine following args and next ops for possible arguments
 *              for an unrecognized method invocation.
 *
 ******************************************************************************/

static UINT32
AcpiDmInspectPossibleArgs (
    UINT32                  CurrentOpArgCount,
    UINT32                  TargetCount,
    ACPI_PARSE_OBJECT       *Op)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    UINT32                  i;
    UINT32                  Last = 0;
    UINT32                  Lookahead;


    Lookahead = (ACPI_METHOD_NUM_ARGS + TargetCount) - CurrentOpArgCount;

    /* Lookahead for the maximum number of possible arguments */

    for (i = 0; i < Lookahead; i++)
    {
        if (!Op)
        {
            break;
        }

        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);

        /*
         * Any one of these operators is "very probably" not a method arg
         */
        if ((Op->Common.AmlOpcode == AML_STORE_OP) ||
            (Op->Common.AmlOpcode == AML_NOTIFY_OP))
        {
            break;
        }

        if ((OpInfo->Class != AML_CLASS_EXECUTE) &&
            (OpInfo->Class != AML_CLASS_CONTROL))
        {
            Last = i+1;
        }

        Op = Op->Common.Next;
    }

    return (Last);
}


