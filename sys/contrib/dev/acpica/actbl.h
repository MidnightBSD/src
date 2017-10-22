/******************************************************************************
 *
 * Name: actbl.h - Basic ACPI Table Definitions
 *       $Revision: 1.84 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2007, Intel Corp.
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

#ifndef __ACTBL_H__
#define __ACTBL_H__

/*
 * Values for description table header signatures. Useful because they make
 * it more difficult to inadvertently type in the wrong signature.
 */
#define ACPI_SIG_DSDT           "DSDT"      /* Differentiated System Description Table */
#define ACPI_SIG_FADT           "FACP"      /* Fixed ACPI Description Table */
#define ACPI_SIG_FACS           "FACS"      /* Firmware ACPI Control Structure */
#define ACPI_SIG_PSDT           "PSDT"      /* Persistent System Description Table */
#define ACPI_SIG_RSDP           "RSD PTR "  /* Root System Description Pointer */
#define ACPI_SIG_RSDT           "RSDT"      /* Root System Description Table */
#define ACPI_SIG_XSDT           "XSDT"      /* Extended  System Description Table */
#define ACPI_SIG_SSDT           "SSDT"      /* Secondary System Description Table */
#define ACPI_RSDP_NAME          "RSDP"      /* Short name for RSDP, not signature */


/*
 * All tables and structures must be byte-packed to match the ACPI
 * specification, since the tables are provided by the system BIOS
 */
#pragma pack(1)


/*
 * These are the ACPI tables that are directly consumed by the subsystem.
 *
 * The RSDP and FACS do not use the common ACPI table header. All other ACPI
 * tables use the header.
 *
 * Note about bitfields: The UINT8 type is used for bitfields in ACPI tables.
 * This is the only type that is even remotely portable. Anything else is not
 * portable, so do not use any other bitfield types.
 */

/*******************************************************************************
 *
 * ACPI Table Header. This common header is used by all tables except the
 * RSDP and FACS. The define is used for direct inclusion of header into
 * other ACPI tables
 *
 ******************************************************************************/

typedef struct acpi_table_header
{
    char                    Signature[ACPI_NAME_SIZE];          /* ASCII table signature */
    UINT32                  Length;                             /* Length of table in bytes, including this header */
    UINT8                   Revision;                           /* ACPI Specification minor version # */
    UINT8                   Checksum;                           /* To make sum of entire table == 0 */
    char                    OemId[ACPI_OEM_ID_SIZE];            /* ASCII OEM identification */
    char                    OemTableId[ACPI_OEM_TABLE_ID_SIZE]; /* ASCII OEM table identification */
    UINT32                  OemRevision;                        /* OEM revision number */
    char                    AslCompilerId[ACPI_NAME_SIZE];      /* ASCII ASL compiler vendor ID */
    UINT32                  AslCompilerRevision;                /* ASL compiler version */

} ACPI_TABLE_HEADER;


/*
 * GAS - Generic Address Structure (ACPI 2.0+)
 *
 * Note: Since this structure is used in the ACPI tables, it is byte aligned.
 * If misalignment is not supported, access to the Address field must be
 * performed with care.
 */
typedef struct acpi_generic_address
{
    UINT8                   SpaceId;                /* Address space where struct or register exists */
    UINT8                   BitWidth;               /* Size in bits of given register */
    UINT8                   BitOffset;              /* Bit offset within the register */
    UINT8                   AccessWidth;            /* Minimum Access size (ACPI 3.0) */
    UINT64                  Address;                /* 64-bit address of struct or register */

} ACPI_GENERIC_ADDRESS;


/*******************************************************************************
 *
 * RSDP - Root System Description Pointer (Signature is "RSD PTR ")
 *
 ******************************************************************************/

typedef struct acpi_table_rsdp
{
    char                    Signature[8];               /* ACPI signature, contains "RSD PTR " */
    UINT8                   Checksum;                   /* ACPI 1.0 checksum */
    char                    OemId[ACPI_OEM_ID_SIZE];    /* OEM identification */
    UINT8                   Revision;                   /* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
    UINT32                  RsdtPhysicalAddress;        /* 32-bit physical address of the RSDT */
    UINT32                  Length;                     /* Table length in bytes, including header (ACPI 2.0+) */
    UINT64                  XsdtPhysicalAddress;        /* 64-bit physical address of the XSDT (ACPI 2.0+) */
    UINT8                   ExtendedChecksum;           /* Checksum of entire table (ACPI 2.0+) */
    UINT8                   Reserved[3];                /* Reserved, must be zero */

} ACPI_TABLE_RSDP;

#define ACPI_RSDP_REV0_SIZE     20                  /* Size of original ACPI 1.0 RSDP */


/*******************************************************************************
 *
 * RSDT/XSDT - Root System Description Tables
 *
 ******************************************************************************/

typedef struct acpi_table_rsdt
{
    ACPI_TABLE_HEADER       Header;                 /* Common ACPI table header */
    UINT32                  TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} ACPI_TABLE_RSDT;

typedef struct acpi_table_xsdt
{
    ACPI_TABLE_HEADER       Header;                 /* Common ACPI table header */
    UINT64                  TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} ACPI_TABLE_XSDT;


/*******************************************************************************
 *
 * FACS - Firmware ACPI Control Structure (FACS)
 *
 ******************************************************************************/

typedef struct acpi_table_facs
{
    char                    Signature[4];           /* ASCII table signature */
    UINT32                  Length;                 /* Length of structure, in bytes */
    UINT32                  HardwareSignature;      /* Hardware configuration signature */
    UINT32                  FirmwareWakingVector;   /* 32-bit physical address of the Firmware Waking Vector */
    UINT32                  GlobalLock;             /* Global Lock for shared hardware resources */
    UINT32                  Flags;
    UINT64                  XFirmwareWakingVector;  /* 64-bit version of the Firmware Waking Vector (ACPI 2.0+) */
    UINT8                   Version;                /* Version of this table (ACPI 2.0+) */
    UINT8                   Reserved[31];           /* Reserved, must be zero */

} ACPI_TABLE_FACS;

/* Flag macros */

#define ACPI_FACS_S4_BIOS_PRESENT (1)               /* 00: S4BIOS support is present */

/* Global lock flags */

#define ACPI_GLOCK_PENDING      0x01                /* 00: Pending global lock ownership */
#define ACPI_GLOCK_OWNED        0x02                /* 01: Global lock is owned */


/*******************************************************************************
 *
 * FADT - Fixed ACPI Description Table (Signature "FACP")
 *
 ******************************************************************************/

/* Fields common to all versions of the FADT */

typedef struct acpi_table_fadt
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    UINT32                  Facs;               /* 32-bit physical address of FACS */
    UINT32                  Dsdt;               /* 32-bit physical address of DSDT */
    UINT8                   Model;              /* System Interrupt Model (ACPI 1.0) - not used in ACPI 2.0+ */
    UINT8                   PreferredProfile;   /* Conveys preferred power management profile to OSPM. */
    UINT16                  SciInterrupt;       /* System vector of SCI interrupt */
    UINT32                  SmiCommand;         /* 32-bit Port address of SMI command port */
    UINT8                   AcpiEnable;         /* Value to write to smi_cmd to enable ACPI */
    UINT8                   AcpiDisable;        /* Value to write to smi_cmd to disable ACPI */
    UINT8                   S4BiosRequest;      /* Value to write to SMI CMD to enter S4BIOS state */
    UINT8                   PstateControl;      /* Processor performance state control*/
    UINT32                  Pm1aEventBlock;     /* 32-bit Port address of Power Mgt 1a Event Reg Blk */
    UINT32                  Pm1bEventBlock;     /* 32-bit Port address of Power Mgt 1b Event Reg Blk */
    UINT32                  Pm1aControlBlock;   /* 32-bit Port address of Power Mgt 1a Control Reg Blk */
    UINT32                  Pm1bControlBlock;   /* 32-bit Port address of Power Mgt 1b Control Reg Blk */
    UINT32                  Pm2ControlBlock;    /* 32-bit Port address of Power Mgt 2 Control Reg Blk */
    UINT32                  PmTimerBlock;       /* 32-bit Port address of Power Mgt Timer Ctrl Reg Blk */
    UINT32                  Gpe0Block;          /* 32-bit Port address of General Purpose Event 0 Reg Blk */
    UINT32                  Gpe1Block;          /* 32-bit Port address of General Purpose Event 1 Reg Blk */
    UINT8                   Pm1EventLength;     /* Byte Length of ports at Pm1xEventBlock */
    UINT8                   Pm1ControlLength;   /* Byte Length of ports at Pm1xControlBlock */
    UINT8                   Pm2ControlLength;   /* Byte Length of ports at Pm2ControlBlock */
    UINT8                   PmTimerLength;      /* Byte Length of ports at PmTimerBlock */
    UINT8                   Gpe0BlockLength;    /* Byte Length of ports at Gpe0Block */
    UINT8                   Gpe1BlockLength;    /* Byte Length of ports at Gpe1Block */
    UINT8                   Gpe1Base;           /* Offset in GPE number space where GPE1 events start */
    UINT8                   CstControl;         /* Support for the _CST object and C States change notification */
    UINT16                  C2Latency;          /* Worst case HW latency to enter/exit C2 state */
    UINT16                  C3Latency;          /* Worst case HW latency to enter/exit C3 state */
    UINT16                  FlushSize;          /* Processor's memory cache line width, in bytes */
    UINT16                  FlushStride;        /* Number of flush strides that need to be read */
    UINT8                   DutyOffset;         /* Processor duty cycle index in processor's P_CNT reg*/
    UINT8                   DutyWidth;          /* Processor duty cycle value bit width in P_CNT register.*/
    UINT8                   DayAlarm;           /* Index to day-of-month alarm in RTC CMOS RAM */
    UINT8                   MonthAlarm;         /* Index to month-of-year alarm in RTC CMOS RAM */
    UINT8                   Century;            /* Index to century in RTC CMOS RAM */
    UINT16                  BootFlags;          /* IA-PC Boot Architecture Flags. See Table 5-10 for description */
    UINT8                   Reserved;           /* Reserved, must be zero */
    UINT32                  Flags;              /* Miscellaneous flag bits (see below for individual flags) */
    ACPI_GENERIC_ADDRESS    ResetRegister;      /* 64-bit address of the Reset register */
    UINT8                   ResetValue;         /* Value to write to the ResetRegister port to reset the system */
    UINT8                   Reserved4[3];       /* Reserved, must be zero */
    UINT64                  XFacs;              /* 64-bit physical address of FACS */
    UINT64                  XDsdt;              /* 64-bit physical address of DSDT */
    ACPI_GENERIC_ADDRESS    XPm1aEventBlock;    /* 64-bit Extended Power Mgt 1a Event Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bEventBlock;    /* 64-bit Extended Power Mgt 1b Event Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1aControlBlock;  /* 64-bit Extended Power Mgt 1a Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bControlBlock;  /* 64-bit Extended Power Mgt 1b Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm2ControlBlock;   /* 64-bit Extended Power Mgt 2 Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPmTimerBlock;      /* 64-bit Extended Power Mgt Timer Ctrl Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe0Block;         /* 64-bit Extended General Purpose Event 0 Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe1Block;         /* 64-bit Extended General Purpose Event 1 Reg Blk address */

} ACPI_TABLE_FADT;


/* FADT flags */

#define ACPI_FADT_WBINVD            (1)         /* 00: The wbinvd instruction works properly */
#define ACPI_FADT_WBINVD_FLUSH      (1<<1)      /* 01: The wbinvd flushes but does not invalidate */
#define ACPI_FADT_C1_SUPPORTED      (1<<2)      /* 02: All processors support C1 state */
#define ACPI_FADT_C2_MP_SUPPORTED   (1<<3)      /* 03: C2 state works on MP system */
#define ACPI_FADT_POWER_BUTTON      (1<<4)      /* 04: Power button is handled as a generic feature */
#define ACPI_FADT_SLEEP_BUTTON      (1<<5)      /* 05: Sleep button is handled as a generic feature, or  not present */
#define ACPI_FADT_FIXED_RTC         (1<<6)      /* 06: RTC wakeup stat not in fixed register space */
#define ACPI_FADT_S4_RTC_WAKE       (1<<7)      /* 07: RTC wakeup stat not possible from S4 */
#define ACPI_FADT_32BIT_TIMER       (1<<8)      /* 08: tmr_val is 32 bits 0=24-bits */
#define ACPI_FADT_DOCKING_SUPPORTED (1<<9)      /* 09: Docking supported */
#define ACPI_FADT_RESET_REGISTER    (1<<10)     /* 10: System reset via the FADT RESET_REG supported */
#define ACPI_FADT_SEALED_CASE       (1<<11)     /* 11: No internal expansion capabilities and case is sealed */
#define ACPI_FADT_HEADLESS          (1<<12)     /* 12: No local video capabilities or local input devices */
#define ACPI_FADT_SLEEP_TYPE        (1<<13)     /* 13: Must execute native instruction after writing  SLP_TYPx register */
#define ACPI_FADT_PCI_EXPRESS_WAKE  (1<<14)     /* 14: System supports PCIEXP_WAKE (STS/EN) bits (ACPI 3.0) */
#define ACPI_FADT_PLATFORM_CLOCK    (1<<15)     /* 15: OSPM should use platform-provided timer (ACPI 3.0) */
#define ACPI_FADT_S4_RTC_VALID      (1<<16)     /* 16: Contents of RTC_STS valid after S4 wake (ACPI 3.0) */
#define ACPI_FADT_REMOTE_POWER_ON   (1<<17)     /* 17: System is compatible with remote power on (ACPI 3.0) */
#define ACPI_FADT_APIC_CLUSTER      (1<<18)     /* 18: All local APICs must use cluster model (ACPI 3.0) */
#define ACPI_FADT_APIC_PHYSICAL     (1<<19)     /* 19: All local xAPICs must use physical dest mode (ACPI 3.0) */


/*
 * FADT Prefered Power Management Profiles
 */
enum AcpiPreferedPmProfiles
{
    PM_UNSPECIFIED          = 0,
    PM_DESKTOP              = 1,
    PM_MOBILE               = 2,
    PM_WORKSTATION          = 3,
    PM_ENTERPRISE_SERVER    = 4,
    PM_SOHO_SERVER          = 5,
    PM_APPLIANCE_PC         = 6
};


/* FADT Boot Arch Flags */

#define BAF_LEGACY_DEVICES              0x0001
#define BAF_8042_KEYBOARD_CONTROLLER    0x0002

#define FADT2_REVISION_ID               3
#define FADT2_MINUS_REVISION_ID         2


/* Reset to default packing */

#pragma pack()

/*
 * Get the remaining ACPI tables
 */
#include <contrib/dev/acpica/actbl1.h>

/* Macros used to generate offsets to specific table fields */

#define ACPI_FADT_OFFSET(f)             (UINT8) ACPI_OFFSET (ACPI_TABLE_FADT, f)

#endif /* __ACTBL_H__ */
