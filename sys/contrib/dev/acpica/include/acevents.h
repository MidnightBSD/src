/******************************************************************************
 *
 * Name: acevents.h - Event subcomponent prototypes and defines
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef __ACEVENTS_H__
#define __ACEVENTS_H__


/*
 * Conditions to trigger post enabling GPE polling:
 * It is not sufficient to trigger edge-triggered GPE with specific GPE
 * chips, software need to poll once after enabling.
 */
#ifdef ACPI_USE_GPE_POLLING
#define ACPI_GPE_IS_POLLING_NEEDED(__gpe__)             \
    ((__gpe__)->RuntimeCount == 1 &&                    \
     (__gpe__)->Flags & ACPI_GPE_INITIALIZED &&         \
     ((__gpe__)->Flags & ACPI_GPE_XRUPT_TYPE_MASK) == ACPI_GPE_EDGE_TRIGGERED)
#else
#define ACPI_GPE_IS_POLLING_NEEDED(__gpe__)             FALSE
#endif


/*
 * evevent
 */
ACPI_STATUS
AcpiEvInitializeEvents (
    void);

ACPI_STATUS
AcpiEvInstallXruptHandlers (
    void);

UINT32
AcpiEvFixedEventDetect (
    void);


/*
 * evmisc
 */
BOOLEAN
AcpiEvIsNotifyObject (
    ACPI_NAMESPACE_NODE     *Node);

UINT32
AcpiEvGetGpeNumberIndex (
    UINT32                  GpeNumber);

ACPI_STATUS
AcpiEvQueueNotifyRequest (
    ACPI_NAMESPACE_NODE     *Node,
    UINT32                  NotifyValue);


/*
 * evglock - Global Lock support
 */
ACPI_STATUS
AcpiEvInitGlobalLockHandler (
    void);

ACPI_HW_DEPENDENT_RETURN_OK (
ACPI_STATUS
AcpiEvAcquireGlobalLock(
    UINT16                  Timeout))

ACPI_HW_DEPENDENT_RETURN_OK (
ACPI_STATUS
AcpiEvReleaseGlobalLock(
    void))

ACPI_STATUS
AcpiEvRemoveGlobalLockHandler (
    void);


/*
 * evgpe - Low-level GPE support
 */
UINT32
AcpiEvGpeDetect (
    ACPI_GPE_XRUPT_INFO     *GpeXruptList);

ACPI_STATUS
AcpiEvUpdateGpeEnableMask (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

ACPI_STATUS
AcpiEvEnableGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

ACPI_STATUS
AcpiEvMaskGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    BOOLEAN                 IsMasked);

ACPI_STATUS
AcpiEvAddGpeReference (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    BOOLEAN                 ClearOnEnable);

ACPI_STATUS
AcpiEvRemoveGpeReference (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

ACPI_GPE_EVENT_INFO *
AcpiEvGetGpeEventInfo (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber);

ACPI_GPE_EVENT_INFO *
AcpiEvLowGetGpeInfo (
    UINT32                  GpeNumber,
    ACPI_GPE_BLOCK_INFO     *GpeBlock);

ACPI_STATUS
AcpiEvFinishGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

UINT32
AcpiEvDetectGpe (
    ACPI_NAMESPACE_NODE     *GpeDevice,
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    UINT32                  GpeNumber);


/*
 * evgpeblk - Upper-level GPE block support
 */
ACPI_STATUS
AcpiEvCreateGpeBlock (
    ACPI_NAMESPACE_NODE     *GpeDevice,
    UINT64                  Address,
    UINT8                   SpaceId,
    UINT32                  RegisterCount,
    UINT16                  GpeBlockBaseNumber,
    UINT32                  InterruptNumber,
    ACPI_GPE_BLOCK_INFO     **ReturnGpeBlock);

ACPI_STATUS
AcpiEvInitializeGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);

ACPI_HW_DEPENDENT_RETURN_OK (
ACPI_STATUS
AcpiEvDeleteGpeBlock (
    ACPI_GPE_BLOCK_INFO     *GpeBlock))

UINT32
AcpiEvGpeDispatch (
    ACPI_NAMESPACE_NODE     *GpeDevice,
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    UINT32                  GpeNumber);


/*
 * evgpeinit - GPE initialization and update
 */
ACPI_STATUS
AcpiEvGpeInitialize (
    void);

ACPI_HW_DEPENDENT_RETURN_VOID (
void
AcpiEvUpdateGpes (
    ACPI_OWNER_ID           TableOwnerId))

ACPI_HW_DEPENDENT_RETURN_STATUS (
ACPI_STATUS
AcpiEvClearGpeDispatchByOwner (
    ACPI_OWNER_ID           OwnerId))

ACPI_STATUS
AcpiEvMatchGpeMethod (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);


/*
 * evgpeutil - GPE utilities
 */
ACPI_STATUS
AcpiEvWalkGpeList (
    ACPI_GPE_CALLBACK       GpeWalkCallback,
    void                    *Context);

ACPI_STATUS
AcpiEvGetGpeDevice (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);

ACPI_STATUS
AcpiEvGetGpeXruptBlock (
    UINT32                  InterruptNumber,
    ACPI_GPE_XRUPT_INFO     **GpeXruptBlock);

ACPI_STATUS
AcpiEvDeleteGpeXrupt (
    ACPI_GPE_XRUPT_INFO     *GpeXrupt);

ACPI_STATUS
AcpiEvDeleteGpeHandlers (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);


/*
 * evhandler - Address space handling
 */
ACPI_OPERAND_OBJECT *
AcpiEvFindRegionHandler (
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_OPERAND_OBJECT     *HandlerObj);

BOOLEAN
AcpiEvHasDefaultHandler (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_ADR_SPACE_TYPE     SpaceId);

ACPI_STATUS
AcpiEvInstallRegionHandlers (
    void);

ACPI_STATUS
AcpiEvInstallSpaceHandler (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context);


/*
 * evregion - Operation region support
 */
ACPI_STATUS
AcpiEvInitializeOpRegions (
    void);

ACPI_STATUS
AcpiEvAddressSpaceDispatch (
    ACPI_OPERAND_OBJECT     *RegionObj,
    ACPI_OPERAND_OBJECT     *FieldObj,
    UINT32                  Function,
    UINT32                  RegionOffset,
    UINT32                  BitWidth,
    UINT64                  *Value);

ACPI_STATUS
AcpiEvAttachRegion (
    ACPI_OPERAND_OBJECT     *HandlerObj,
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked);

void
AcpiEvDetachRegion (
    ACPI_OPERAND_OBJECT     *RegionObj,
    BOOLEAN                 AcpiNsIsLocked);

void
AcpiEvExecuteRegMethods (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    UINT32                  Function);

ACPI_STATUS
AcpiEvExecuteRegMethod (
    ACPI_OPERAND_OBJECT     *RegionObj,
    UINT32                  Function);


/*
 * evregini - Region initialization and setup
 */
ACPI_STATUS
AcpiEvSystemMemoryRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvIoSpaceRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvPciConfigRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvCmosRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvPciBarRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvDataTableRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvDefaultRegionSetup (
    ACPI_HANDLE             Handle,
    UINT32                  Function,
    void                    *HandlerContext,
    void                    **RegionContext);

ACPI_STATUS
AcpiEvInitializeRegion (
    ACPI_OPERAND_OBJECT     *RegionObj);

BOOLEAN
AcpiEvIsPciRootBridge (
    ACPI_NAMESPACE_NODE     *Node);


/*
 * evsci - SCI (System Control Interrupt) handling/dispatch
 */
UINT32 ACPI_SYSTEM_XFACE
AcpiEvGpeXruptHandler (
    void                    *Context);

UINT32
AcpiEvSciDispatch (
    void);

UINT32
AcpiEvInstallSciHandler (
    void);

ACPI_STATUS
AcpiEvRemoveAllSciHandlers (
    void);

ACPI_HW_DEPENDENT_RETURN_VOID (
void
AcpiEvTerminate (
    void))

#endif  /* __ACEVENTS_H__  */
