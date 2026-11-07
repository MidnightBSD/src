/******************************************************************************
 *
 * Module Name: dswscope - Scope stack manipulation
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


#define _COMPONENT          ACPI_DISPATCHER
        ACPI_MODULE_NAME    ("dswscope")


/****************************************************************************
 *
 * FUNCTION:    AcpiDsScopeStackClear
 *
 * PARAMETERS:  WalkState       - Current state
 *
 * RETURN:      None
 *
 * DESCRIPTION: Pop (and free) everything on the scope stack except the
 *              root scope object (which remains at the stack top.)
 *
 ***************************************************************************/

void
AcpiDsScopeStackClear (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *ScopeInfo;

    ACPI_FUNCTION_NAME (DsScopeStackClear);


    while (WalkState->ScopeInfo)
    {
        /* Pop a scope off the stack */

        ScopeInfo = WalkState->ScopeInfo;
        WalkState->ScopeInfo = ScopeInfo->Scope.Next;

        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
            "Popped object type (%s)\n",
            AcpiUtGetTypeName (ScopeInfo->Common.Value)));

        AcpiUtDeleteGenericState (ScopeInfo);
    }
}


/****************************************************************************
 *
 * FUNCTION:    AcpiDsScopeStackPush
 *
 * PARAMETERS:  Node            - Name to be made current
 *              Type            - Type of frame being pushed
 *              WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push the current scope on the scope stack, and make the
 *              passed Node current.
 *
 ***************************************************************************/

ACPI_STATUS
AcpiDsScopeStackPush (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT_TYPE        Type,
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *ScopeInfo;
    ACPI_GENERIC_STATE      *OldScopeInfo;


    ACPI_FUNCTION_TRACE (DsScopeStackPush);


    if (!Node)
    {
        /* Invalid scope   */

        ACPI_ERROR ((AE_INFO, "Null scope parameter"));
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Make sure object type is valid */

    if (!AcpiUtValidObjectType (Type))
    {
        ACPI_WARNING ((AE_INFO,
            "Invalid object type: 0x%X", Type));
    }

    /* Allocate a new scope object */

    ScopeInfo = AcpiUtCreateGenericState ();
    if (!ScopeInfo)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    /* Init new scope object */

    ScopeInfo->Common.DescriptorType = ACPI_DESC_TYPE_STATE_WSCOPE;
    ScopeInfo->Scope.Node = Node;
    ScopeInfo->Common.Value = (UINT16) Type;

    WalkState->ScopeDepth++;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "[%.2d] Pushed scope ", (UINT32) WalkState->ScopeDepth));

    OldScopeInfo = WalkState->ScopeInfo;
    if (OldScopeInfo)
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC,
            "[%4.4s] (%s)",
            AcpiUtGetNodeName (OldScopeInfo->Scope.Node),
            AcpiUtGetTypeName (OldScopeInfo->Common.Value)));
    }
    else
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC, ACPI_NAMESPACE_ROOT));
    }

    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC,
        ", New scope -> [%4.4s] (%s)\n",
        AcpiUtGetNodeName (ScopeInfo->Scope.Node),
        AcpiUtGetTypeName (ScopeInfo->Common.Value)));

    /* Push new scope object onto stack */

    AcpiUtPushGenericState (&WalkState->ScopeInfo, ScopeInfo);
    return_ACPI_STATUS (AE_OK);
}


/****************************************************************************
 *
 * FUNCTION:    AcpiDsScopeStackPop
 *
 * PARAMETERS:  WalkState       - Current state
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop the scope stack once.
 *
 ***************************************************************************/

ACPI_STATUS
AcpiDsScopeStackPop (
    ACPI_WALK_STATE         *WalkState)
{
    ACPI_GENERIC_STATE      *ScopeInfo;
    ACPI_GENERIC_STATE      *NewScopeInfo;


    ACPI_FUNCTION_TRACE (DsScopeStackPop);


    /*
     * Pop scope info object off the stack.
     */
    ScopeInfo = AcpiUtPopGenericState (&WalkState->ScopeInfo);
    if (!ScopeInfo)
    {
        return_ACPI_STATUS (AE_STACK_UNDERFLOW);
    }

    WalkState->ScopeDepth--;

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC,
        "[%.2d] Popped scope [%4.4s] (%s), New scope -> ",
        (UINT32) WalkState->ScopeDepth,
        AcpiUtGetNodeName (ScopeInfo->Scope.Node),
        AcpiUtGetTypeName (ScopeInfo->Common.Value)));

    NewScopeInfo = WalkState->ScopeInfo;
    if (NewScopeInfo)
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC, "[%4.4s] (%s)\n",
            AcpiUtGetNodeName (NewScopeInfo->Scope.Node),
            AcpiUtGetTypeName (NewScopeInfo->Common.Value)));
    }
    else
    {
        ACPI_DEBUG_PRINT_RAW ((ACPI_DB_EXEC, "%s\n", ACPI_NAMESPACE_ROOT));
    }

    AcpiUtDeleteGenericState (ScopeInfo);
    return_ACPI_STATUS (AE_OK);
}
