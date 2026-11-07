/******************************************************************************
 *
 * Module Name: aslmapoutput - Output/emit the resource descriptor/device maps
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
#include <contrib/dev/acpica/include/acapps.h>
#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acparser.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/amlcode.h>

/* This module used for application-level code only */

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslmapoutput")

/* Local prototypes */

static void
MpEmitGpioInfo (
    void);

static void
MpEmitSerialInfo (
    void);

static void
MpEmitDeviceTree (
    void);

static ACPI_STATUS
MpEmitOneDevice (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);

static void
MpXrefDevices (
    ACPI_GPIO_INFO          *Info);

static ACPI_STATUS
MpNamespaceXrefBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context);


/* Strings used to decode flag bits */

const char                  *DirectionDecode[] =
{
    "Both I/O   ",
    "InputOnly  ",
    "OutputOnly ",
    "Preserve   "
};

const char                  *PolarityDecode[] =
{
    "ActiveHigh",
    "ActiveLow ",
    "ActiveBoth",
    "Reserved  "
};


/*******************************************************************************
 *
 * FUNCTION:    MpEmitMappingInfo
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: External interface.
 *              Map file has already been opened. Emit all of the collected
 *              hardware mapping information. Includes: GPIO information,
 *              Serial information, and a dump of the entire ACPI device tree.
 *
 ******************************************************************************/

void
MpEmitMappingInfo (
    void)
{

    /* Mapfile option enabled? */

    if (!AslGbl_MapfileFlag)
    {
        return;
    }

    if (!AslGbl_GpioList)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT,
            "\nNo GPIO devices found\n");
    }

    if (!AslGbl_SerialList)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT,
            "\nNo Serial devices found (I2C/SPI/UART)\n");
    }

    if (!AslGbl_GpioList && !AslGbl_SerialList)
    {
        return;
    }

    /* Headers */

    FlPrintFile (ASL_FILE_MAP_OUTPUT, "\nResource Descriptor Connectivity Map\n");
    FlPrintFile (ASL_FILE_MAP_OUTPUT,   "------------------------------------\n");

    /* Emit GPIO and Serial descriptors, then entire ACPI device tree */

    MpEmitGpioInfo ();
    MpEmitSerialInfo ();
    MpEmitDeviceTree ();

    /* Clear the lists - no need to free memory here */

    AslGbl_SerialList = NULL;
    AslGbl_GpioList = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    MpEmitGpioInfo
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit the info about all GPIO devices found during the
 *              compile or disassembly.
 *
 ******************************************************************************/

static void
MpEmitGpioInfo (
    void)
{
    ACPI_GPIO_INFO          *Info;
    char                    *Type;
    char                    *PrevDeviceName = NULL;
    const char              *Direction;
    const char              *Polarity;
    char                    *ParentPathname;
    const char              *Description;
    char                    *HidString;
    const AH_DEVICE_ID      *HidInfo;


    /* Walk the GPIO descriptor list */

    Info = AslGbl_GpioList;
    while (Info)
    {
        HidString = MpGetHidViaNamestring (Info->DeviceName);

        /* Print header info for the controller itself */

        if (!PrevDeviceName ||
            strcmp (PrevDeviceName, Info->DeviceName))
        {
            FlPrintFile (ASL_FILE_MAP_OUTPUT,
                "\n\nGPIO Controller:  %-8s  %-28s",
                HidString, Info->DeviceName);

            HidInfo = AcpiAhMatchHardwareId (HidString);
            if (HidInfo)
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s",
                    HidInfo->Description);
            }

            FlPrintFile (ASL_FILE_MAP_OUTPUT,
                "\n\nPin   Type     Direction    Polarity"
                "    Dest _HID  Destination\n");
        }

        PrevDeviceName = Info->DeviceName;

        /* Setup various strings based upon the type (GpioInt or GpioIo) */

        switch (Info->Type)
        {
        case AML_RESOURCE_GPIO_TYPE_INT:

            Type = "GpioInt";
            Direction = "-Interrupt-";
            Polarity = PolarityDecode[Info->Polarity];
            break;

        case AML_RESOURCE_GPIO_TYPE_IO:

            Type = "GpioIo ";
            Direction = DirectionDecode[Info->Direction];
            Polarity = "          ";
            break;

        default:
            continue;
        }

        /* Emit the GPIO info */

        FlPrintFile (ASL_FILE_MAP_OUTPUT, "%4.4X  %s  %s  %s  ",
            Info->PinNumber, Type, Direction, Polarity);

        ParentPathname = NULL;
        HidString = MpGetConnectionInfo (Info->Op, Info->PinIndex,
            &Info->TargetNode, &ParentPathname);
        if (HidString)
        {
            /*
             * This is a Connection() field
             * Attempt to find all references to the field.
             */
            FlPrintFile (ASL_FILE_MAP_OUTPUT, "%8s   %-28s",
                HidString, ParentPathname);

            MpXrefDevices (Info);
        }
        else
        {
            /*
             * For Devices, attempt to get the _HID description string.
             * Failing that (many _HIDs are not recognized), attempt to
             * get the _DDN description string.
             */
            HidString = MpGetParentDeviceHid (Info->Op, &Info->TargetNode,
                &ParentPathname);

            FlPrintFile (ASL_FILE_MAP_OUTPUT, "%8s   %-28s",
                HidString, ParentPathname);

            /* Get the _HID description or _DDN string */

            HidInfo = AcpiAhMatchHardwareId (HidString);
            if (HidInfo)
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s",
                    HidInfo->Description);
            }
            else if ((Description = MpGetDdnValue (ParentPathname)))
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s (_DDN)",
                    Description);
            }
        }

        FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n");
        ACPI_FREE (ParentPathname);
        Info = Info->Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MpEmitSerialInfo
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit the info about all Serial devices found during the
 *              compile or disassembly.
 *
 ******************************************************************************/

static void
MpEmitSerialInfo (
    void)
{
    ACPI_SERIAL_INFO        *Info;
    char                    *Type;
    char                    *ParentPathname;
    char                    *PrevDeviceName = NULL;
    char                    *HidString;
    const AH_DEVICE_ID      *HidInfo;
    const char              *Description;
    AML_RESOURCE            *Resource;


    /* Walk the constructed serial descriptor list */

    Info = AslGbl_SerialList;
    while (Info)
    {
        Resource = Info->Resource;
        switch (Resource->CommonSerialBus.Type)
        {
        case AML_RESOURCE_I2C_SERIALBUSTYPE:
            Type = "I2C ";
            break;

        case AML_RESOURCE_SPI_SERIALBUSTYPE:
            Type = "SPI ";
            break;

        case AML_RESOURCE_UART_SERIALBUSTYPE:
            Type = "UART";
            break;

        default:
            Type = "UNKN";
            break;
        }

        HidString = MpGetHidViaNamestring (Info->DeviceName);

        /* Print header info for the controller itself */

        if (!PrevDeviceName ||
            strcmp (PrevDeviceName, Info->DeviceName))
        {
            FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n\n%s Controller:  ",
                Type);
            FlPrintFile (ASL_FILE_MAP_OUTPUT, "%-8s  %-28s",
                HidString, Info->DeviceName);

            HidInfo = AcpiAhMatchHardwareId (HidString);
            if (HidInfo)
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s",
                    HidInfo->Description);
            }

            FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n\n");
            FlPrintFile (ASL_FILE_MAP_OUTPUT,
                "Type  Address   Speed      Dest _HID  Destination\n");
        }

        PrevDeviceName = Info->DeviceName;

        FlPrintFile (ASL_FILE_MAP_OUTPUT, "%s   %4.4X    %8.8X    ",
            Type, Info->Address, Info->Speed);

        ParentPathname = NULL;
        HidString = MpGetConnectionInfo (Info->Op, 0, &Info->TargetNode,
            &ParentPathname);
        if (HidString)
        {
            /*
             * This is a Connection() field
             * Attempt to find all references to the field.
             */
            FlPrintFile (ASL_FILE_MAP_OUTPUT, "%8s   %-28s",
                HidString, ParentPathname);
        }
        else
        {
            /* Normal resource template */

            HidString = MpGetParentDeviceHid (Info->Op, &Info->TargetNode,
                &ParentPathname);
            FlPrintFile (ASL_FILE_MAP_OUTPUT, "%8s   %-28s",
                HidString, ParentPathname);

            /* Get the _HID description or _DDN string */

            HidInfo = AcpiAhMatchHardwareId (HidString);
            if (HidInfo)
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s",
                    HidInfo->Description);
            }
            else if ((Description = MpGetDdnValue (ParentPathname)))
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // %s (_DDN)",
                    Description);
            }
        }

        FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n");
        ACPI_FREE (ParentPathname);
        Info = Info->Next;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MpEmitDeviceTree
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Emit information about all devices within the ACPI namespace.
 *
 ******************************************************************************/

static void
MpEmitDeviceTree (
    void)
{

    FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n\nACPI Device Tree\n");
    FlPrintFile (ASL_FILE_MAP_OUTPUT,     "----------------\n\n");

    FlPrintFile (ASL_FILE_MAP_OUTPUT, "Device Pathname                   "
        "_HID      Description\n\n");

    /* Walk the namespace from the root */

    (void) AcpiNsWalkNamespace (ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
        ACPI_UINT32_MAX, FALSE, MpEmitOneDevice, NULL, NULL, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    MpEmitOneDevice
 *
 * PARAMETERS:  ACPI_NAMESPACE_WALK callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Emit information about one ACPI device in the namespace. Used
 *              during dump of all device objects within the namespace.
 *
 ******************************************************************************/

static ACPI_STATUS
MpEmitOneDevice (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    char                    *DevicePathname;
    char                    *DdnString;
    char                    *HidString;
    const AH_DEVICE_ID      *HidInfo;


    /* Device pathname */

    DevicePathname = AcpiNsGetExternalPathname (
        ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle));

    FlPrintFile (ASL_FILE_MAP_OUTPUT, "%-32s", DevicePathname);

    /* _HID or _DDN */

    HidString = MpGetHidValue (
        ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, ObjHandle));
    FlPrintFile (ASL_FILE_MAP_OUTPUT, "%8s", HidString);

    HidInfo = AcpiAhMatchHardwareId (HidString);
    if (HidInfo)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT, "    // %s",
            HidInfo->Description);
    }
    else if ((DdnString = MpGetDdnValue (DevicePathname)))
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT, "    // %s (_DDN)", DdnString);
    }

    FlPrintFile (ASL_FILE_MAP_OUTPUT, "\n");
    ACPI_FREE (DevicePathname);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    MpXrefDevices
 *
 * PARAMETERS:  Info                    - A GPIO Info block
 *
 * RETURN:      None
 *
 * DESCRIPTION: Cross-reference the parse tree and find all references to the
 *              specified GPIO device.
 *
 ******************************************************************************/

static void
MpXrefDevices (
    ACPI_GPIO_INFO          *Info)
{

    /* Walk the entire parse tree */

    TrWalkParseTree (AslGbl_ParseTreeRoot, ASL_WALK_VISIT_DOWNWARD,
        MpNamespaceXrefBegin, NULL, Info);

    if (!Info->References)
    {
        FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // **** No references in table");
    }
}


/*******************************************************************************
 *
 * FUNCTION:    MpNamespaceXrefBegin
 *
 * PARAMETERS:  WALK_PARSE_TREE callback
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk parse tree callback used to cross-reference GPIO pins.
 *
 ******************************************************************************/

static ACPI_STATUS
MpNamespaceXrefBegin (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{
    ACPI_GPIO_INFO          *Info = ACPI_CAST_PTR (ACPI_GPIO_INFO, Context);
    const ACPI_OPCODE_INFO  *OpInfo;
    char                    *DevicePathname;
    ACPI_PARSE_OBJECT       *ParentOp;
    char                    *HidString;


    ACPI_FUNCTION_TRACE_PTR (MpNamespaceXrefBegin, Op);

    /*
     * If this node is the actual declaration of a name
     * [such as the XXXX name in "Method (XXXX)"],
     * we are not interested in it here. We only care about names that
     * are references to other objects within the namespace and the
     * parent objects of name declarations
     */
    if (Op->Asl.CompileFlags & OP_IS_NAME_DECLARATION)
    {
        return (AE_OK);
    }

    /* We are only interested in opcodes that have an associated name */

    OpInfo = AcpiPsGetOpcodeInfo (Op->Asl.AmlOpcode);

    if ((OpInfo->Flags & AML_NAMED) ||
        (OpInfo->Flags & AML_CREATE))
    {
        return (AE_OK);
    }

    if ((Op->Asl.ParseOpcode != PARSEOP_NAMESTRING) &&
        (Op->Asl.ParseOpcode != PARSEOP_NAMESEG)    &&
        (Op->Asl.ParseOpcode != PARSEOP_METHODCALL))
    {
        return (AE_OK);
    }

    if (!Op->Asl.Node)
    {
        return (AE_OK);
    }

    ParentOp = Op->Asl.Parent;
    if (ParentOp->Asl.ParseOpcode == PARSEOP_FIELD)
    {
        return (AE_OK);
    }

    if (Op->Asl.Node == Info->TargetNode)
    {
        while (ParentOp && (!ParentOp->Asl.Node))
        {
            ParentOp = ParentOp->Asl.Parent;
        }

        if (ParentOp)
        {
            DevicePathname = AcpiNsGetExternalPathname (
                ParentOp->Asl.Node);

            if (!Info->References)
            {
                FlPrintFile (ASL_FILE_MAP_OUTPUT, "  // References:");
            }

            HidString = MpGetHidViaNamestring (DevicePathname);

            FlPrintFile (ASL_FILE_MAP_OUTPUT, " %s [%s]",
                DevicePathname, HidString);

            Info->References++;

            ACPI_FREE (DevicePathname);
        }
    }

    return (AE_OK);
}
