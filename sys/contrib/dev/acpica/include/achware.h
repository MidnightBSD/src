/******************************************************************************
 *
 * Name: achware.h -- hardware specific interfaces
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef __ACHWARE_H__
#define __ACHWARE_H__


/* Values for the _SST predefined method */

#define ACPI_SST_INDICATOR_OFF  0
#define ACPI_SST_WORKING        1
#define ACPI_SST_WAKING         2
#define ACPI_SST_SLEEPING       3
#define ACPI_SST_SLEEP_CONTEXT  4


/*
 * hwacpi - high level functions
 */
ACPI_STATUS
AcpiHwSetMode (
    UINT32                  Mode);

UINT32
AcpiHwGetMode (
    void);


/*
 * hwregs - ACPI Register I/O
 */
ACPI_STATUS
AcpiHwValidateRegister (
    ACPI_GENERIC_ADDRESS    *Reg,
    UINT8                   MaxBitWidth,
    UINT64                  *Address);

ACPI_STATUS
AcpiHwRead (
    UINT64                  *Value,
    ACPI_GENERIC_ADDRESS    *Reg);

ACPI_STATUS
AcpiHwWrite (
    UINT64                  Value,
    ACPI_GENERIC_ADDRESS    *Reg);

ACPI_BIT_REGISTER_INFO *
AcpiHwGetBitRegisterInfo (
    UINT32                  RegisterId);

ACPI_STATUS
AcpiHwWritePm1Control (
    UINT32                  Pm1aControl,
    UINT32                  Pm1bControl);

ACPI_STATUS
AcpiHwRegisterRead (
    UINT32                  RegisterId,
    UINT32                  *ReturnValue);

ACPI_STATUS
AcpiHwRegisterWrite (
    UINT32                  RegisterId,
    UINT32                  Value);

ACPI_STATUS
AcpiHwClearAcpiStatus (
    void);


/*
 * hwsleep - sleep/wake support (Legacy sleep registers)
 */
ACPI_STATUS
AcpiHwLegacySleep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiHwLegacyWakePrep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiHwLegacyWake (
    UINT8                   SleepState);


/*
 * hwesleep - sleep/wake support (Extended FADT-V5 sleep registers)
 */
void
AcpiHwExecuteSleepMethod (
    char                    *MethodName,
    UINT32                  IntegerArgument);

ACPI_STATUS
AcpiHwExtendedSleep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiHwExtendedWakePrep (
    UINT8                   SleepState);

ACPI_STATUS
AcpiHwExtendedWake (
    UINT8                   SleepState);


/*
 * hwvalid - Port I/O with validation
 */
ACPI_STATUS
AcpiHwReadPort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  *Value,
    UINT32                  Width);

ACPI_STATUS
AcpiHwWritePort (
    ACPI_IO_ADDRESS         Address,
    UINT32                  Value,
    UINT32                  Width);


/*
 * hwgpe - GPE support
 */
UINT32
AcpiHwGetGpeRegisterBit (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

ACPI_STATUS
AcpiHwLowSetGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    UINT32                  Action);

ACPI_STATUS
AcpiHwDisableGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);

ACPI_STATUS
AcpiHwClearGpe (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo);

ACPI_STATUS
AcpiHwClearGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);

ACPI_STATUS
AcpiHwGetGpeStatus (
    ACPI_GPE_EVENT_INFO     *GpeEventInfo,
    ACPI_EVENT_STATUS       *EventStatus);

ACPI_STATUS
AcpiHwDisableAllGpes (
    void);

ACPI_STATUS
AcpiHwEnableAllRuntimeGpes (
    void);

ACPI_STATUS
AcpiHwEnableAllWakeupGpes (
    void);

UINT8
AcpiHwCheckAllGpes (
    void);

ACPI_STATUS
AcpiHwEnableRuntimeGpeBlock (
    ACPI_GPE_XRUPT_INFO     *GpeXruptInfo,
    ACPI_GPE_BLOCK_INFO     *GpeBlock,
    void                    *Context);


/*
 * hwpci - PCI configuration support
 */
ACPI_STATUS
AcpiHwDerivePciId (
    ACPI_PCI_ID             *PciId,
    ACPI_HANDLE             RootPciDevice,
    ACPI_HANDLE             PciRegion);


#endif /* __ACHWARE_H__ */
