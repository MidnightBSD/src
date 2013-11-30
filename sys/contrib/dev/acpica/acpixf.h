
/******************************************************************************
 *
 * Name: acpixf.h - External interfaces to the ACPI subsystem
 *
 *****************************************************************************/

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


#ifndef __ACXFACE_H__
#define __ACXFACE_H__

#include <contrib/dev/acpica/actypes.h>
#include <contrib/dev/acpica/actbl.h>


 /*
 * Global interfaces
 */

ACPI_STATUS
AcpiInitializeSubsystem (
    void);

ACPI_STATUS
AcpiEnableSubsystem (
    UINT32                  Flags);

ACPI_STATUS
AcpiInitializeObjects (
    UINT32                  Flags);

ACPI_STATUS
AcpiTerminate (
    void);

ACPI_STATUS
AcpiSubsystemStatus (
    void);

ACPI_STATUS
AcpiEnable (
    void);

ACPI_STATUS
AcpiDisable (
    void);

ACPI_STATUS
AcpiGetSystemInfo (
    ACPI_BUFFER             *RetBuffer);

const char *
AcpiFormatException (
    ACPI_STATUS             Exception);

ACPI_STATUS
AcpiPurgeCachedObjects (
    void);

ACPI_STATUS
AcpiInstallInitializationHandler (
    ACPI_INIT_HANDLER       Handler,
    UINT32                  Function);

/*
 * ACPI Memory manager
 */

void *
AcpiAllocate (
    UINT32                  Size);

void *
AcpiCallocate (
    UINT32                  Size);

void
AcpiFree (
    void                    *Address);


/*
 * ACPI table manipulation interfaces
 */

ACPI_STATUS
AcpiFindRootPointer (
    UINT32                  Flags,
    ACPI_POINTER            *RsdpAddress);

ACPI_STATUS
AcpiLoadTables (
    void);

ACPI_STATUS
AcpiLoadTable (
    ACPI_TABLE_HEADER       *TablePtr);

ACPI_STATUS
AcpiUnloadTable (
    ACPI_TABLE_TYPE         TableType);

ACPI_STATUS
AcpiGetTableHeader (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_TABLE_HEADER       *OutTableHeader);

ACPI_STATUS
AcpiGetTable (
    ACPI_TABLE_TYPE         TableType,
    UINT32                  Instance,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiGetFirmwareTable (
    ACPI_STRING             Signature,
    UINT32                  Instance,
    UINT32                  Flags,
    ACPI_TABLE_HEADER       **TablePointer);


/*
 * Namespace and name interfaces
 */

ACPI_STATUS
AcpiWalkNamespace (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             StartObject,
    UINT32                  MaxDepth,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AcpiGetDevices (
    char                    *HID,
    ACPI_WALK_CALLBACK      UserFunction,
    void                    *Context,
    void                    **ReturnValue);

ACPI_STATUS
AcpiGetName (
    ACPI_HANDLE             Handle,
    UINT32                  NameType,
    ACPI_BUFFER             *RetPathPtr);

ACPI_STATUS
AcpiGetHandle (
    ACPI_HANDLE             Parent,
    ACPI_STRING             Pathname,
    ACPI_HANDLE             *RetHandle);

ACPI_STATUS
AcpiAttachData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler,
    void                    *Data);

ACPI_STATUS
AcpiDetachData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler);

ACPI_STATUS
AcpiGetData (
    ACPI_HANDLE             ObjHandle,
    ACPI_OBJECT_HANDLER     Handler,
    void                    **Data);


/*
 * Object manipulation and enumeration
 */

ACPI_STATUS
AcpiEvaluateObject (
    ACPI_HANDLE             Object,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ParameterObjects,
    ACPI_BUFFER             *ReturnObjectBuffer);

ACPI_STATUS
AcpiEvaluateObjectTyped (
    ACPI_HANDLE             Object,
    ACPI_STRING             Pathname,
    ACPI_OBJECT_LIST        *ExternalParams,
    ACPI_BUFFER             *ReturnBuffer,
    ACPI_OBJECT_TYPE        ReturnType);

ACPI_STATUS
AcpiGetObjectInfo (
    ACPI_HANDLE             Handle,
    ACPI_BUFFER             *ReturnBuffer);

ACPI_STATUS
AcpiGetNextObject (
    ACPI_OBJECT_TYPE        Type,
    ACPI_HANDLE             Parent,
    ACPI_HANDLE             Child,
    ACPI_HANDLE             *OutHandle);

ACPI_STATUS
AcpiGetType (
    ACPI_HANDLE             Object,
    ACPI_OBJECT_TYPE        *OutType);

ACPI_STATUS
AcpiGetParent (
    ACPI_HANDLE             Object,
    ACPI_HANDLE             *OutHandle);


/*
 * Event handler interfaces
 */

ACPI_STATUS
AcpiInstallFixedEventHandler (
    UINT32                  AcpiEvent,
    ACPI_EVENT_HANDLER      Handler,
    void                    *Context);

ACPI_STATUS
AcpiRemoveFixedEventHandler (
    UINT32                  AcpiEvent,
    ACPI_EVENT_HANDLER      Handler);

ACPI_STATUS
AcpiInstallNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler,
    void                    *Context);

ACPI_STATUS
AcpiRemoveNotifyHandler (
    ACPI_HANDLE             Device,
    UINT32                  HandlerType,
    ACPI_NOTIFY_HANDLER     Handler);

ACPI_STATUS
AcpiInstallAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler,
    ACPI_ADR_SPACE_SETUP    Setup,
    void                    *Context);

ACPI_STATUS
AcpiRemoveAddressSpaceHandler (
    ACPI_HANDLE             Device,
    ACPI_ADR_SPACE_TYPE     SpaceId,
    ACPI_ADR_SPACE_HANDLER  Handler);

ACPI_STATUS
AcpiInstallGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Type,
    ACPI_EVENT_HANDLER      Address,
    void                    *Context);

ACPI_STATUS
AcpiInstallExceptionHandler (
    ACPI_EXCEPTION_HANDLER  Handler);


/*
 * Event interfaces
 */

ACPI_STATUS
AcpiAcquireGlobalLock (
    UINT16                  Timeout,
    UINT32                  *Handle);

ACPI_STATUS
AcpiReleaseGlobalLock (
    UINT32                  Handle);

ACPI_STATUS
AcpiRemoveGpeHandler (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    ACPI_EVENT_HANDLER      Address);

ACPI_STATUS
AcpiEnableEvent (
    UINT32                  Event,
    UINT32                  Flags);

ACPI_STATUS
AcpiDisableEvent (
    UINT32                  Event,
    UINT32                  Flags);

ACPI_STATUS
AcpiClearEvent (
    UINT32                  Event);

ACPI_STATUS
AcpiGetEventStatus (
    UINT32                  Event,
    ACPI_EVENT_STATUS       *EventStatus);

ACPI_STATUS
AcpiSetGpeType (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT8                   Type);

ACPI_STATUS
AcpiEnableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Flags);

ACPI_STATUS
AcpiDisableGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Flags);

ACPI_STATUS
AcpiClearGpe (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Flags);

ACPI_STATUS
AcpiGetGpeStatus (
    ACPI_HANDLE             GpeDevice,
    UINT32                  GpeNumber,
    UINT32                  Flags,
    ACPI_EVENT_STATUS       *EventStatus);

ACPI_STATUS
AcpiInstallGpeBlock (
    ACPI_HANDLE             GpeDevice,
    ACPI_GENERIC_ADDRESS    *GpeBlockAddress,
    UINT32                  RegisterCount,
    UINT32                  InterruptLevel);

ACPI_STATUS
AcpiRemoveGpeBlock (
    ACPI_HANDLE             GpeDevice);


/*
 * Resource interfaces
 */

typedef
ACPI_STATUS (*ACPI_WALK_RESOURCE_CALLBACK) (
    ACPI_RESOURCE           *Resource,
    void                    *Context);


ACPI_STATUS
AcpiGetCurrentResources(
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiGetPossibleResources(
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiWalkResources (
    ACPI_HANDLE                     DeviceHandle,
    char                            *Path,
    ACPI_WALK_RESOURCE_CALLBACK     UserFunction,
    void                            *Context);

ACPI_STATUS
AcpiSetCurrentResources (
    ACPI_HANDLE             DeviceHandle,
    ACPI_BUFFER             *InBuffer);

ACPI_STATUS
AcpiGetIrqRoutingTable  (
    ACPI_HANDLE             BusDeviceHandle,
    ACPI_BUFFER             *RetBuffer);

ACPI_STATUS
AcpiResourceToAddress64 (
    ACPI_RESOURCE           *Resource,
    ACPI_RESOURCE_ADDRESS64 *Out);

/*
 * Hardware (ACPI device) interfaces
 */

ACPI_STATUS
AcpiGetRegister (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue,
    UINT32                  Flags);

ACPI_STATUS
AcpiSetRegister (
    UINT32                  RegisterId,
    UINT32                  Value,
    UINT32                  Flags);

ACPI_STATUS
AcpiSetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS   PhysicalAddress);

ACPI_STATUS
AcpiGetFirmwareWakingVector (
    ACPI_PHYSICAL_ADDRESS   *PhysicalAddress);

ACPI_STATUS
AcpiGetSleepTypeData (
    UINT8                   SleepState,
    UINT8                   *Slp_TypA,
    UINT8                   *Slp_TypB);

ACPI_STATUS
AcpiEnterSleepStatePrep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiEnterSleepState (
    UINT8                   SleepState);

ACPI_STATUS
AcpiEnterSleepStateS4bios (
    void);

ACPI_STATUS
AcpiLeaveSleepState (
    UINT8                   SleepState);


#endif /* __ACXFACE_H__ */
