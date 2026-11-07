/******************************************************************************
 *
 * Module Name: aslstubs - Stubs used to link to Aml interpreter
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/actables.h>
#include <contrib/dev/acpica/include/acevents.h>
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acparser.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslstubs")


/*
 * Stubs to simplify linkage to the ACPICA core subsystem.
 * Things like Events, Global Lock, etc. are not used
 * by the compiler, so they are stubbed out here.
 */
ACPI_STATUS
AcpiNsInitializeObjects (
    void)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiPsExecuteTable (
    ACPI_EVALUATE_INFO      *Info)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiHwReadPort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiHwWritePort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState)
{
    return (Status);
}

ACPI_STATUS
AcpiDsMethodDataGetValue (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **DestDesc)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsMethodDataGetNode (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     **Node)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsStoreObjectToLocal (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *SrcDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvInstallRegionHandlers (
    void)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvQueueNotifyRequest (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  NotifyValue)
{
    return (AE_OK);
}

BOOLEAN
AcpiEvIsNotifyObject (
    ACPI_NAMESPACE_NODE     *Node)
{
    return (FALSE);
}

#if (!ACPI_REDUCED_HARDWARE)
ACPI_STATUS
AcpiEvDeleteGpeBlock (
    ACPI_GPE_BLOCK_INFO     *GpeBlock)
{
    return (AE_OK);
}

void
AcpiEvUpdateGpes (
    ACPI_OWNER_ID           TableOwnerId)
{
    return;
}

ACPI_STATUS
AcpiEvClearGpeDispatchByOwner (
    ACPI_OWNER_ID           OwnerId)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvAcquireGlobalLock (
    UINT16                  Timeout)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiEvReleaseGlobalLock (
    void)
{
    return (AE_OK);
}
#endif /* !ACPI_REDUCED_HARDWARE */

ACPI_STATUS
AcpiEvInitializeRegion (
    ACPI_OPERAND_OBJECT     *RegionObj)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiExReadDataFromField (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **RetBufferDesc)
{
    return (AE_SUPPORT);
}

ACPI_STATUS
AcpiExWriteDataToField (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     **ResultDesc)
{
    return (AE_SUPPORT);
}

ACPI_STATUS
AcpiExLoadTableOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **ReturnDesc)
{
    return (AE_SUPPORT);
}

ACPI_STATUS
AcpiExUnloadTable (
    ACPI_OPERAND_OBJECT     *DdbHandle)
{
    return (AE_SUPPORT);
}

ACPI_STATUS
AcpiExLoadOp (
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_OPERAND_OBJECT     *Target,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_SUPPORT);
}

void
AcpiExDoDebugObject (
    ACPI_OPERAND_OBJECT     *SourceDesc,
    UINT32                  Level,
    UINT32                  Index)
{
    return;
}

void
AcpiExStartTraceMethod (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStopTraceMethod (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStartTraceOpcode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

void
AcpiExStopTraceOpcode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState)

{
    return;
}

void
AcpiExTracePoint (
    ACPI_TRACE_EVENT_TYPE   Type,
    BOOLEAN                 Begin,
    UINT8                   *Aml,
    char                    *Pathname)
{
    return;
}

ACPI_STATUS
AcpiTbFindTable (
    char                    *Signature,
    char                    *OemId,
    char                    *OemTableId,
    UINT32                  *TableIndex)
{
    return (AE_SUPPORT);
}

ACPI_STATUS
AcpiNsLoadTable (
    UINT32                  TableIndex,
    ACPI_NAMESPACE_NODE     *Node)
{
    return (AE_NOT_IMPLEMENTED);
}

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc)
{
    return (AE_OK);
}

void
AcpiDsTerminateControlMethod (
    ACPI_OPERAND_OBJECT     *MethodDesc,
    ACPI_WALK_STATE         *WalkState)
{
    return;
}

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op)
{
    return (AE_OK);
}

ACPI_STATUS
AcpiDsMethodDataInitArgs (
    ACPI_OPERAND_OBJECT     **Params,
    UINT32                  MaxParamCount,
    ACPI_WALK_STATE         *WalkState)
{
    return (AE_OK);
}
