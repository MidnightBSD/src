/******************************************************************************
 *
 * Module Name: ahuuids - Table of known ACPI-related UUIDs
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
#include <contrib/dev/acpica/include/acuuid.h>

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("ahuuids")


/*
 * Table of "known" (ACPI-related) UUIDs
 */
const AH_UUID  Gbl_AcpiUuids[] =
{
    {"[Controllers]",               NULL},
    {"GPIO Controller",             UUID_GPIO_CONTROLLER},
    {"USB Controller",              UUID_USB_CONTROLLER},
    {"SATA Controller",             UUID_SATA_CONTROLLER},

    {"[Devices]",                   NULL},
    {"PCI Host Bridge Device",      UUID_PCI_HOST_BRIDGE},
    {"HID I2C Device",              UUID_I2C_DEVICE},
    {"Power Button Device",         UUID_POWER_BUTTON},
    {"Memory Device",               UUID_MEMORY_DEVICE},
    {"Generic Buttons Device",      UUID_GENERIC_BUTTONS_DEVICE},
    {"NVDIMM Root Device",          UUID_NVDIMM_ROOT_DEVICE},
    {"Control Method Battery",      UUID_CONTROL_METHOD_BATTERY},

    {"[Interfaces]",                NULL},
    {"Device Labeling Interface",   UUID_DEVICE_LABELING},
    {"Physical Presence Interface", UUID_PHYSICAL_PRESENCE},

    {"[Trusted Platform Module]",   NULL},
    {"TPM Hardware Information",    UUID_HARDWARE_INFORMATION},
    {"TPM Start Method",            UUID_START_METHOD},
    {"TPM Memory Clear",            UUID_MEMORY_CLEAR},

    {"[Non-volatile DIMM and NFIT table]",       NULL},
    {"NVDIMM Device",               UUID_NFIT_DIMM},
    {"Volatile Memory Region",      UUID_VOLATILE_MEMORY},
    {"Persistent Memory Region",    UUID_PERSISTENT_MEMORY},
    {"NVDIMM Control Region",       UUID_CONTROL_REGION},
    {"NVDIMM Data Region",          UUID_DATA_REGION},
    {"Volatile Virtual Disk",       UUID_VOLATILE_VIRTUAL_DISK},
    {"Volatile Virtual CD",         UUID_VOLATILE_VIRTUAL_CD},
    {"Persistent Virtual Disk",     UUID_PERSISTENT_VIRTUAL_DISK},
    {"Persistent Virtual CD",       UUID_PERSISTENT_VIRTUAL_CD},
    {"Microsoft NVDIMM Command set",UUID_NFIT_DIMM_N_MSFT},
    {"HP NDIMM HPE1",               UUID_NFIT_DIMM_N_HPE1},
    {"HP NDIMM HPE2",               UUID_NFIT_DIMM_N_HPE2},
    {"Virtual NVDIMM",              UUID_NFIT_DIMM_N_HYPERV},

    {"[Processor Properties]",      NULL},
    {"Cache Properties",            UUID_CACHE_PROPERTIES},
    {"Physical Package Property",   UUID_PHYSICAL_PROPERTY},

    {"[Modern Standby]",            NULL},
    {"Modern Standby Microsoft",    UUID_LPS0_MICROSOFT},
    {"Modern Standby Intel",        UUID_LPS0_INTEL},
    {"Modern Standby AMD",          UUID_LPS0_AMD},

    {"[Miscellaneous]",             NULL},
    {"Platform-wide Capabilities",  UUID_PLATFORM_CAPABILITIES},
    {"Dynamic Enumeration",         UUID_DYNAMIC_ENUMERATION},
    {"Battery Thermal Limit",       UUID_BATTERY_THERMAL_LIMIT},
    {"Thermal Extensions",          UUID_THERMAL_EXTENSIONS},
    {"Device Properties for _DSD",  UUID_DEVICE_PROPERTIES},
    {"Device Graphs for _DSD",      UUID_DEVICE_GRAPHS},
    {"Hierarchical Data Extension", UUID_HIERARCHICAL_DATA_EXTENSION},
    {"ARM Coresight Graph",         UUID_CORESIGHT_GRAPH},
    {"USB4 Capabilities",           UUID_USB4_CAPABILITIES},
    {"First Function ID for _DSM",  UUID_1ST_FUNCTION_ID},
    {"Second Function ID for _DSM", UUID_2ND_FUNCTION_ID},
    {"Fan Trip Points",             UUID_FAN_TRIP_POINTS},

    {NULL, NULL}
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiAhMatchUuid
 *
 * PARAMETERS:  Data                - Data buffer containing a UUID
 *
 * RETURN:      ASCII description string for the UUID if it is found.
 *
 * DESCRIPTION: Returns a description string for "known" UUIDs, which are
 *              are UUIDs that are related to ACPI in some way.
 *
 ******************************************************************************/

const char *
AcpiAhMatchUuid (
    UINT8                   *Data)
{
    const AH_UUID           *Info;
    UINT8                   UuidBuffer[UUID_BUFFER_LENGTH];


    /* Walk the table of known ACPI-related UUIDs */

    for (Info = Gbl_AcpiUuids; Info->Description; Info++)
    {
        /* Null string means description is a UUID class */

        if (!Info->String)
        {
            continue;
        }

        AcpiUtConvertStringToUuid (Info->String, UuidBuffer);

        if (!memcmp (Data, UuidBuffer, UUID_BUFFER_LENGTH))
        {
            return (Info->Description);
        }
    }

    return (NULL);
}
