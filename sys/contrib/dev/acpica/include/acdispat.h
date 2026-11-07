/******************************************************************************
 *
 * Name: acdispat.h - dispatcher (parser to interpreter interface)
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef _ACDISPAT_H_
#define _ACDISPAT_H_


#define NAMEOF_LOCAL_NTE    "__L0"
#define NAMEOF_ARG_NTE      "__A0"


/*
 * dsargs - execution of dynamic arguments for static objects
 */
ACPI_STATUS
AcpiDsGetBufferFieldArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsGetBankFieldArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsGetRegionArguments (
    ACPI_OPERAND_OBJECT     *RgnDesc);

ACPI_STATUS
AcpiDsGetBufferArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsGetPackageArguments (
    ACPI_OPERAND_OBJECT     *ObjDesc);


/*
 * dscontrol - support for execution control opcodes
 */
ACPI_STATUS
AcpiDsExecBeginControlOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsExecEndControlOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);


/*
 * dsopcode - support for late operand evaluation
 */
ACPI_STATUS
AcpiDsEvalBufferFieldOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsEvalRegionOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsEvalTableRegionOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsEvalDataObjectOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsEvalBankFieldOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsInitializeRegion (
    ACPI_HANDLE             ObjHandle);


/*
 * dsexec - Parser/Interpreter interface, method execution callbacks
 */
ACPI_STATUS
AcpiDsGetPredicateValue (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ResultObj);

ACPI_STATUS
AcpiDsExecBeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsExecEndOp (
    ACPI_WALK_STATE         *State);


/*
 * dsfield - Parser/Interpreter interface for AML fields
 */
ACPI_STATUS
AcpiDsCreateField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateBankField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateIndexField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *RegionNode,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateBufferField (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsInitFieldObjects (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);


/*
 * dsload - Parser/Interpreter interface
 */
ACPI_STATUS
AcpiDsInitCallbacks (
    ACPI_WALK_STATE         *WalkState,
    UINT32                  PassNumber);

/* dsload - pass 1 namespace load callbacks */

ACPI_STATUS
AcpiDsLoad1BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsLoad1EndOp (
    ACPI_WALK_STATE         *WalkState);


/* dsload - pass 2 namespace load callbacks */

ACPI_STATUS
AcpiDsLoad2BeginOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       **OutOp);

ACPI_STATUS
AcpiDsLoad2EndOp (
    ACPI_WALK_STATE         *WalkState);


/*
 * dsmthdat - method data (locals/args)
 */
ACPI_STATUS
AcpiDsStoreObjectToLocal (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_OPERAND_OBJECT     *SrcDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataGetEntry (
    UINT16                  Opcode,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     ***Node);

void
AcpiDsMethodDataDeleteAll (
    ACPI_WALK_STATE         *WalkState);

BOOLEAN
AcpiDsIsMethodValue (
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsMethodDataGetValue (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     **DestDesc);

ACPI_STATUS
AcpiDsMethodDataInitArgs (
    ACPI_OPERAND_OBJECT     **Params,
    UINT32                  MaxParamCount,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodDataGetNode (
    UINT8                   Type,
    UINT32                  Index,
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     **Node);

void
AcpiDsMethodDataInit (
    ACPI_WALK_STATE         *WalkState);


/*
 * dsmethod - Parser/Interpreter interface - control method parsing
 */
ACPI_STATUS
AcpiDsAutoSerializeMethod (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OPERAND_OBJECT     *ObjDesc);

ACPI_STATUS
AcpiDsCallControlMethod (
    ACPI_THREAD_STATE       *Thread,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

ACPI_STATUS
AcpiDsRestartControlMethod (
    ACPI_WALK_STATE         *WalkState,
    ACPI_OPERAND_OBJECT     *ReturnDesc);

void
AcpiDsTerminateControlMethod (
    ACPI_OPERAND_OBJECT     *MethodDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsBeginMethodExecution (
    ACPI_NAMESPACE_NODE     *MethodNode,
    ACPI_OPERAND_OBJECT     *ObjDesc,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsMethodError (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState);

/*
 * dsinit
 */
ACPI_STATUS
AcpiDsInitializeObjects (
    UINT32                  TableIndex,
    ACPI_NAMESPACE_NODE     *StartNode);


/*
 * dsobject - Parser/Interpreter interface - object initialization and conversion
 */
ACPI_STATUS
AcpiDsBuildInternalObject (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     **ObjDescPtr);

ACPI_STATUS
AcpiDsBuildInternalBufferObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  BufferLength,
    ACPI_OPERAND_OBJECT     **ObjDescPtr);

ACPI_STATUS
AcpiDsBuildInternalPackageObj (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *op,
    UINT32                  PackageLength,
    ACPI_OPERAND_OBJECT     **ObjDesc);

ACPI_STATUS
AcpiDsInitObjectFromOp (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    UINT16                  Opcode,
    ACPI_OPERAND_OBJECT     **ObjDesc);

ACPI_STATUS
AcpiDsCreateNode (
    ACPI_WALK_STATE         *WalkState,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_PARSE_OBJECT       *Op);


/*
 * dspkginit - Package object initialization
 */
ACPI_STATUS
AcpiDsInitPackageElement (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context);


/*
 * dsutils - Parser/Interpreter interface utility routines
 */
void
AcpiDsClearImplicitReturn (
    ACPI_WALK_STATE         *WalkState);

BOOLEAN
AcpiDsDoImplicitReturn (
    ACPI_OPERAND_OBJECT     *ReturnDesc,
    ACPI_WALK_STATE         *WalkState,
    BOOLEAN                 AddReference);

BOOLEAN
AcpiDsIsResultUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsDeleteResultIfNotUsed (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_OPERAND_OBJECT     *ResultObj,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsCreateOperand (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Arg,
    UINT32                  ArgsRemaining);

ACPI_STATUS
AcpiDsCreateOperands (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *FirstArg);

ACPI_STATUS
AcpiDsResolveOperands (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsClearOperands (
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsEvaluateNamePath (
    ACPI_WALK_STATE         *WalkState);


/*
 * dswscope - Scope Stack manipulation
 */
ACPI_STATUS
AcpiDsScopeStackPush (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_OBJECT_TYPE        Type,
    ACPI_WALK_STATE         *WalkState);


ACPI_STATUS
AcpiDsScopeStackPop (
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsScopeStackClear (
    ACPI_WALK_STATE         *WalkState);


/*
 * dswstate - parser WALK_STATE management routines
 */
ACPI_STATUS
AcpiDsObjStackPush (
    void                    *Object,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsObjStackPop (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState);

ACPI_WALK_STATE *
AcpiDsCreateWalkState (
    ACPI_OWNER_ID           OwnerId,
    ACPI_PARSE_OBJECT       *Origin,
    ACPI_OPERAND_OBJECT     *MthDesc,
    ACPI_THREAD_STATE       *Thread);

ACPI_STATUS
AcpiDsInitAmlWalk (
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAMESPACE_NODE     *MethodNode,
    UINT8                   *AmlStart,
    UINT32                  AmlLength,
    ACPI_EVALUATE_INFO      *Info,
    UINT8                   PassNumber);

void
AcpiDsObjStackPopAndDelete (
    UINT32                  PopCount,
    ACPI_WALK_STATE         *WalkState);

void
AcpiDsDeleteWalkState (
    ACPI_WALK_STATE         *WalkState);

ACPI_WALK_STATE *
AcpiDsPopWalkState (
    ACPI_THREAD_STATE       *Thread);

void
AcpiDsPushWalkState (
    ACPI_WALK_STATE         *WalkState,
    ACPI_THREAD_STATE       *Thread);

ACPI_STATUS
AcpiDsResultStackClear (
    ACPI_WALK_STATE         *WalkState);

ACPI_WALK_STATE *
AcpiDsGetCurrentWalkState (
    ACPI_THREAD_STATE       *Thread);

ACPI_STATUS
AcpiDsResultPop (
    ACPI_OPERAND_OBJECT     **Object,
    ACPI_WALK_STATE         *WalkState);

ACPI_STATUS
AcpiDsResultPush (
    ACPI_OPERAND_OBJECT     *Object,
    ACPI_WALK_STATE         *WalkState);


/*
 * dsdebug - parser debugging routines
 */
void
AcpiDsDumpMethodStack (
    ACPI_STATUS             Status,
    ACPI_WALK_STATE         *WalkState,
    ACPI_PARSE_OBJECT       *Op);

#endif /* _ACDISPAT_H_ */
