/******************************************************************************
 *
 * Module Name: dsdebug - Parser/Interpreter interface - debugging
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
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acinterp.h>


#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dsdebug")


#if defined(ACPI_DEBUG_OUTPUT) || defined(ACPI_DEBUGGER)

/* Local prototypes */

static void
AcpiDsPrintNodePathname (
    ACPI_NAMESPACE_NODE     *Node,
    const char              *Message);


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsPrintNodePathname
 *
 * PARAMETERS:  Node            - Object
 *              Message         - Prefix message
 *
 * DESCRIPTION: Print an object's full namespace pathname
 *              Manages allocation/freeing of a pathname buffer
 *
 ******************************************************************************/

static void
AcpiDsPrintNodePathname (
    ACPI_NAMESPACE_NODE     *Node,
    const char              *Message)
{
    ACPI_BUFFER             Buffer;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (DsPrintNodePathname);

    if (!Node)
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH, "[NULL NAME]"));
        return_VOID;
    }

    /* Convert handle to full pathname and print it (with supplied message) */

    Buffer.Length = ACPI_ALLOCATE_LOCAL_BUFFER;

    Status = AcpiNsHandleToPathname (Node, &Buffer, TRUE);
    if (ACPI_SUCCESS (Status))
    {
        if (Message)
        {
            ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH, "%s ", Message));
        }

        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH, "[%s] (Node %p)",
            (char *) Buffer.Pointer, Node));
        ACPI_FREE (Buffer.Pointer);
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDsDumpMethodStack
 *
 * PARAMETERS:  Status          - Method execution status
 *              WalkState       - Current state of the parse tree walk
 *              Op              - Executing parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Called when a method has been aborted because of an error.
 *              Dumps the method execution stack.
 *
 ******************************************************************************/

void
AcpiDsDumpMethodStack (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_THREAD_STATE       *Thread;
    ACPI_WALK_STATE         *NextWalkState;
    ACPI_NAMESPACE_NODE     *PreviousMethod = NULL;
    ACPI_OPERAND_OBJECT     *MethodDesc;


    ACPI_FUNCTION_TRACE (DsDumpMethodStack);


    /* Ignore control codes, they are not errors */

    if (ACPI_CNTL_EXCEPTION (Status))
    {
        return_VOID;
    }

    /* We may be executing a deferred opcode */

    if (WalkState->DeferredNode)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "Executing subtree for Buffer/Package/Region\n"));
        return_VOID;
    }

    /*
     * If there is no Thread, we are not actually executing a method.
     * This can happen when the iASL compiler calls the interpreter
     * to perform constant folding.
     */
    Thread = WalkState->Thread;
    if (!Thread)
    {
        return_VOID;
    }

    /* Display exception and method name */

    ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
        "\n**** Exception %s during execution of method ",
        AcpiFormatException (Status)));

    AcpiDsPrintNodePathname (WalkState->MethodNode, NULL);

    /* Display stack of executing methods */

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH,
        "\n\nMethod Execution Stack:\n"));
    NextWalkState = Thread->WalkStateList;

    /* Walk list of linked walk states */

    while (NextWalkState)
    {
        MethodDesc = NextWalkState->MethodDesc;
        if (MethodDesc)
        {
            AcpiExStopTraceMethod (
                (ACPI_NAMESPACE_NODE *) MethodDesc->Method.Node,
                MethodDesc, WalkState);
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_DISPATCH,
            "    Method [%4.4s] executing: ",
            AcpiUtGetNodeName (NextWalkState->MethodNode)));

        /* First method is the currently executing method */

        if (NextWalkState == WalkState)
        {
            if (Op)
            {
                /* Display currently executing ASL statement */

                Next = Op->Common.Next;
                Op->Common.Next = NULL;

#ifdef ACPI_DISASSEMBLER
                if (WalkState->MethodNode != AcpiGbl_RootNode)
                {
                    /* More verbose if not module-level code */

                    AcpiOsPrintf ("Failed at ");
                    AcpiDmDisassemble (NextWalkState, Op, ACPI_UINT32_MAX);
                }
#endif
                Op->Common.Next = Next;
            }
        }
        else
        {
            /*
             * This method has called another method
             * NOTE: the method call parse subtree is already deleted at
             * this point, so we cannot disassemble the method invocation.
             */
            ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH, "Call to method "));
            AcpiDsPrintNodePathname (PreviousMethod, NULL);
        }

        PreviousMethod = NextWalkState->MethodNode;
        NextWalkState = NextWalkState->Next;
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_DISPATCH, "\n"));
    }

    return_VOID;
}

#else

void
AcpiDsDumpMethodStack (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    return;
}

#endif
