/*******************************************************************************
 *
 * Module Name: utresdecode - Resource descriptor keyword strings
 *
 ******************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acresrc.h>


#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utresdecode")


#if defined (ACPI_DEBUG_OUTPUT) || \
    defined (ACPI_DISASSEMBLER) || \
    defined (ACPI_DEBUGGER)

/*
 * Strings used to decode resource descriptors.
 * Used by both the disassembler and the debugger resource dump routines
 */
const char                      *AcpiGbl_BmDecode[] =
{
    "NotBusMaster",
    "BusMaster"
};

const char                      *AcpiGbl_ConfigDecode[] =
{
    "0 - Good Configuration",
    "1 - Acceptable Configuration",
    "2 - Suboptimal Configuration",
    "3 - ***Invalid Configuration***",
};

const char                      *AcpiGbl_ConsumeDecode[] =
{
    "ResourceProducer",
    "ResourceConsumer"
};

const char                      *AcpiGbl_DecDecode[] =
{
    "PosDecode",
    "SubDecode"
};

const char                      *AcpiGbl_HeDecode[] =
{
    "Level",
    "Edge"
};

const char                      *AcpiGbl_IoDecode[] =
{
    "Decode10",
    "Decode16"
};

const char                      *AcpiGbl_LlDecode[] =
{
    "ActiveHigh",
    "ActiveLow",
    "ActiveBoth",
    "Reserved"
};

const char                      *AcpiGbl_MaxDecode[] =
{
    "MaxNotFixed",
    "MaxFixed"
};

const char                      *AcpiGbl_MemDecode[] =
{
    "NonCacheable",
    "Cacheable",
    "WriteCombining",
    "Prefetchable"
};

const char                      *AcpiGbl_MinDecode[] =
{
    "MinNotFixed",
    "MinFixed"
};

const char                      *AcpiGbl_MtpDecode[] =
{
    "AddressRangeMemory",
    "AddressRangeReserved",
    "AddressRangeACPI",
    "AddressRangeNVS"
};

const char                      *AcpiGbl_PhyDecode[] =
{
    "Type C",
    "Type D",
    "Unknown Type",
    "Unknown Type"
};

const char                      *AcpiGbl_RngDecode[] =
{
    "InvalidRanges",
    "NonISAOnlyRanges",
    "ISAOnlyRanges",
    "EntireRange"
};

const char                      *AcpiGbl_RwDecode[] =
{
    "ReadOnly",
    "ReadWrite"
};

const char                      *AcpiGbl_ShrDecode[] =
{
    "Exclusive",
    "Shared",
    "ExclusiveAndWake",         /* ACPI 5.0 */
    "SharedAndWake"             /* ACPI 5.0 */
};

const char                      *AcpiGbl_SizDecode[] =
{
    "Transfer8",
    "Transfer8_16",
    "Transfer16",
    "InvalidSize"
};

const char                      *AcpiGbl_TrsDecode[] =
{
    "DenseTranslation",
    "SparseTranslation"
};

const char                      *AcpiGbl_TtpDecode[] =
{
    "TypeStatic",
    "TypeTranslation"
};

const char                      *AcpiGbl_TypDecode[] =
{
    "Compatibility",
    "TypeA",
    "TypeB",
    "TypeF"
};

const char                      *AcpiGbl_PpcDecode[] =
{
    "PullDefault",
    "PullUp",
    "PullDown",
    "PullNone"
};

const char                      *AcpiGbl_IorDecode[] =
{
    "IoRestrictionNone",
    "IoRestrictionInputOnly",
    "IoRestrictionOutputOnly",
    "IoRestrictionNoneAndPreserve"
};

const char                      *AcpiGbl_DtsDecode[] =
{
    "Width8bit",
    "Width16bit",
    "Width32bit",
    "Width64bit",
    "Width128bit",
    "Width256bit",
};

/* GPIO connection type */

const char                      *AcpiGbl_CtDecode[] =
{
    "Interrupt",
    "I/O"
};

/* Serial bus type */

const char                      *AcpiGbl_SbtDecode[] =
{
    "/* UNKNOWN serial bus type */",
    "I2C",
    "SPI",
    "UART",
    "CSI2"
};

/* I2C serial bus access mode */

const char                      *AcpiGbl_AmDecode[] =
{
    "AddressingMode7Bit",
    "AddressingMode10Bit"
};

/* I2C serial bus slave mode */

const char                      *AcpiGbl_SmDecode[] =
{
    "ControllerInitiated",
    "DeviceInitiated"
};

/* SPI serial bus wire mode */

const char                      *AcpiGbl_WmDecode[] =
{
    "FourWireMode",
    "ThreeWireMode"
};

/* SPI serial clock phase */

const char                      *AcpiGbl_CphDecode[] =
{
    "ClockPhaseFirst",
    "ClockPhaseSecond"
};

/* SPI serial bus clock polarity */

const char                      *AcpiGbl_CpoDecode[] =
{
    "ClockPolarityLow",
    "ClockPolarityHigh"
};

/* SPI serial bus device polarity */

const char                      *AcpiGbl_DpDecode[] =
{
    "PolarityLow",
    "PolarityHigh"
};

/* UART serial bus endian */

const char                      *AcpiGbl_EdDecode[] =
{
    "LittleEndian",
    "BigEndian"
};

/* UART serial bus bits per byte */

const char                      *AcpiGbl_BpbDecode[] =
{
    "DataBitsFive",
    "DataBitsSix",
    "DataBitsSeven",
    "DataBitsEight",
    "DataBitsNine",
    "/* UNKNOWN Bits per byte */",
    "/* UNKNOWN Bits per byte */",
    "/* UNKNOWN Bits per byte */"
};

/* UART serial bus stop bits */

const char                      *AcpiGbl_SbDecode[] =
{
    "StopBitsZero",
    "StopBitsOne",
    "StopBitsOnePlusHalf",
    "StopBitsTwo"
};

/* UART serial bus flow control */

const char                      *AcpiGbl_FcDecode[] =
{
    "FlowControlNone",
    "FlowControlHardware",
    "FlowControlXON",
    "/* UNKNOWN flow control keyword */"
};

/* UART serial bus parity type */

const char                      *AcpiGbl_PtDecode[] =
{
    "ParityTypeNone",
    "ParityTypeEven",
    "ParityTypeOdd",
    "ParityTypeMark",
    "ParityTypeSpace",
    "/* UNKNOWN parity keyword */",
    "/* UNKNOWN parity keyword */",
    "/* UNKNOWN parity keyword */"
};

/* PinConfig type */

const char                      *AcpiGbl_PtypDecode[] =
{
    "Default",
    "Bias Pull-up",
    "Bias Pull-down",
    "Bias Default",
    "Bias Disable",
    "Bias High Impedance",
    "Bias Bus Hold",
    "Drive Open Drain",
    "Drive Open Source",
    "Drive Push Pull",
    "Drive Strength",
    "Slew Rate",
    "Input Debounce",
    "Input Schmitt Trigger",
};

const char                      *AcpiGbl_ClockInputMode[] =
{
    "Fixed",
    "Variable",
};

const char                      *AcpiGbl_ClockInputScale[] =
{
    "Hz",
    "KHz",
    "MHz",
};

#endif
