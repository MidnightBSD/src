/******************************************************************************
 *
 * Name: actypes.h - Common data types for the entire ACPI subsystem
 *       $Revision: 1.316 $
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

#ifndef __ACTYPES_H__
#define __ACTYPES_H__

/* acpisrc:StructDefs -- for acpisrc conversion */

/*
 * ACPI_MACHINE_WIDTH must be specified in an OS- or compiler-dependent header
 * and must be either 32 or 64. 16-bit ACPICA is no longer supported, as of
 * 12/2006.
 */
#ifndef ACPI_MACHINE_WIDTH
#error ACPI_MACHINE_WIDTH not defined
#endif

/*! [Begin] no source code translation */

/*
 * Data type ranges
 * Note: These macros are designed to be compiler independent as well as
 * working around problems that some 32-bit compilers have with 64-bit
 * constants.
 */
#define ACPI_UINT8_MAX                  (UINT8) (~((UINT8)  0)) /* 0xFF               */
#define ACPI_UINT16_MAX                 (UINT16)(~((UINT16) 0)) /* 0xFFFF             */
#define ACPI_UINT32_MAX                 (UINT32)(~((UINT32) 0)) /* 0xFFFFFFFF         */
#define ACPI_UINT64_MAX                 (UINT64)(~((UINT64) 0)) /* 0xFFFFFFFFFFFFFFFF */
#define ACPI_ASCII_MAX                  0x7F


/*
 * Architecture-specific ACPICA Subsystem Data Types
 *
 * The goal of these types is to provide source code portability across
 * 16-bit, 32-bit, and 64-bit targets.
 *
 * 1) The following types are of fixed size for all targets (16/32/64):
 *
 * BOOLEAN      Logical boolean
 *
 * UINT8        8-bit  (1 byte) unsigned value
 * UINT16       16-bit (2 byte) unsigned value
 * UINT32       32-bit (4 byte) unsigned value
 * UINT64       64-bit (8 byte) unsigned value
 *
 * INT16        16-bit (2 byte) signed value
 * INT32        32-bit (4 byte) signed value
 * INT64        64-bit (8 byte) signed value
 *
 * COMPILER_DEPENDENT_UINT64/INT64 - These types are defined in the
 * compiler-dependent header(s) and were introduced because there is no common
 * 64-bit integer type across the various compilation models, as shown in
 * the table below.
 *
 * Datatype  LP64 ILP64 LLP64 ILP32 LP32 16bit
 * char      8    8     8     8     8    8
 * short     16   16    16    16    16   16
 * _int32         32
 * int       32   64    32    32    16   16
 * long      64   64    32    32    32   32
 * long long            64    64
 * pointer   64   64    64    32    32   32
 *
 * Note: ILP64 and LP32 are currently not supported.
 *
 *
 * 2) These types represent the native word size of the target mode of the
 * processor, and may be 16-bit, 32-bit, or 64-bit as required. They are
 * usually used for memory allocation, efficient loop counters, and array
 * indexes. The types are similar to the size_t type in the C library and are
 * required because there is no C type that consistently represents the native
 * data width.
 *
 * ACPI_SIZE        16/32/64-bit unsigned value
 * ACPI_NATIVE_UINT 16/32/64-bit unsigned value
 * ACPI_NATIVE_INT  16/32/64-bit signed value
 *
 */

/*******************************************************************************
 *
 * Common types for all compilers, all targets
 *
 ******************************************************************************/

typedef unsigned char                   BOOLEAN;
typedef unsigned char                   UINT8;
typedef unsigned short                  UINT16;
typedef COMPILER_DEPENDENT_UINT64       UINT64;
typedef COMPILER_DEPENDENT_INT64        INT64;

/*! [End] no source code translation !*/


/*******************************************************************************
 *
 * Types specific to 64-bit targets
 *
 ******************************************************************************/

#if ACPI_MACHINE_WIDTH == 64

/*! [Begin] no source code translation (keep the typedefs as-is) */

typedef unsigned int                    UINT32;
typedef int                             INT32;

/*! [End] no source code translation !*/


typedef UINT64                          ACPI_NATIVE_UINT;
typedef INT64                           ACPI_NATIVE_INT;

typedef UINT64                          ACPI_IO_ADDRESS;
typedef UINT64                          ACPI_PHYSICAL_ADDRESS;

#define ACPI_MAX_PTR                    ACPI_UINT64_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT64_MAX

#define ACPI_USE_NATIVE_DIVIDE          /* Has native 64-bit integer support */

/*
 * In the case of the Itanium Processor Family (IPF), the hardware does not
 * support misaligned memory transfers. Set the MISALIGNMENT_NOT_SUPPORTED flag
 * to indicate that special precautions must be taken to avoid alignment faults.
 * (IA64 or ia64 is currently used by existing compilers to indicate IPF.)
 *
 * Note: EM64T and other X86-64 processors support misaligned transfers,
 * so there is no need to define this flag.
 */
#if defined (__IA64__) || defined (__ia64__)
#define ACPI_MISALIGNMENT_NOT_SUPPORTED
#endif


/*******************************************************************************
 *
 * Types specific to 32-bit targets
 *
 ******************************************************************************/

#elif ACPI_MACHINE_WIDTH == 32

/*! [Begin] no source code translation (keep the typedefs as-is) */

typedef unsigned int                    UINT32;
typedef int                             INT32;

/*! [End] no source code translation !*/


typedef UINT32                          ACPI_NATIVE_UINT;
typedef INT32                           ACPI_NATIVE_INT;

typedef UINT32                          ACPI_IO_ADDRESS;
typedef UINT32                          ACPI_PHYSICAL_ADDRESS;

#define ACPI_MAX_PTR                    ACPI_UINT32_MAX
#define ACPI_SIZE_MAX                   ACPI_UINT32_MAX

#else

/* ACPI_MACHINE_WIDTH must be either 64 or 32 */

#error unknown ACPI_MACHINE_WIDTH
#endif


/* Variable-width type, used instead of clib size_t */

typedef ACPI_NATIVE_UINT                ACPI_SIZE;


/*******************************************************************************
 *
 * OS-dependent and compiler-dependent types
 *
 * If the defaults below are not appropriate for the host system, they can
 * be defined in the compiler-specific or OS-specific header, and this will
 * take precedence.
 *
 ******************************************************************************/


/* Value returned by AcpiOsGetThreadId */

#ifndef ACPI_THREAD_ID
#define ACPI_THREAD_ID                  ACPI_NATIVE_UINT
#endif

/* Object returned from AcpiOsCreateLock */

#ifndef ACPI_SPINLOCK
#define ACPI_SPINLOCK                   void *
#endif

/* Flags for AcpiOsAcquireLock/AcpiOsReleaseLock */

#ifndef ACPI_CPU_FLAGS
#define ACPI_CPU_FLAGS                  ACPI_NATIVE_UINT
#endif

/* Object returned from AcpiOsCreateCache */

#ifndef ACPI_CACHE_T
#define ACPI_CACHE_T                    ACPI_MEMORY_LIST
#endif

/* Use C99 uintptr_t for pointer casting if available, "void *" otherwise */

#ifndef ACPI_UINTPTR_T
#define ACPI_UINTPTR_T                  void *
#endif

/*
 * ACPI_PRINTF_LIKE is used to tag functions as "printf-like" because
 * some compilers can catch printf format string problems
 */
#ifndef ACPI_PRINTF_LIKE
#define ACPI_PRINTF_LIKE(c)
#endif

/*
 * Some compilers complain about unused variables. Sometimes we don't want to
 * use all the variables (for example, _AcpiModuleName). This allows us
 * to to tell the compiler in a per-variable manner that a variable
 * is unused
 */
#ifndef ACPI_UNUSED_VAR
#define ACPI_UNUSED_VAR
#endif

/*
 * All ACPICA functions that are available to the rest of the kernel are
 * tagged with this macro which can be defined as appropriate for the host.
 */
#ifndef ACPI_EXPORT_SYMBOL
#define ACPI_EXPORT_SYMBOL(Symbol)
#endif


/*******************************************************************************
 *
 * Independent types
 *
 ******************************************************************************/

/* Logical defines and NULL */

#ifdef FALSE
#undef FALSE
#endif
#define FALSE                           (1 == 0)

#ifdef TRUE
#undef TRUE
#endif
#define TRUE                            (1 == 1)

#ifndef NULL
#define NULL                            (void *) 0
#endif


/*
 * Mescellaneous types
 */
typedef UINT32                          ACPI_STATUS;    /* All ACPI Exceptions */
typedef UINT32                          ACPI_NAME;      /* 4-byte ACPI name */
typedef char *                          ACPI_STRING;    /* Null terminated ASCII string */
typedef void *                          ACPI_HANDLE;    /* Actually a ptr to a NS Node */

typedef struct uint64_struct
{
    UINT32                          Lo;
    UINT32                          Hi;

} UINT64_STRUCT;

typedef union uint64_overlay
{
    UINT64                          Full;
    UINT64_STRUCT                   Part;

} UINT64_OVERLAY;

typedef struct uint32_struct
{
    UINT32                          Lo;
    UINT32                          Hi;

} UINT32_STRUCT;


/* Synchronization objects */

#define ACPI_MUTEX                      void *
#define ACPI_SEMAPHORE                  void *


/*
 * Acpi integer width. In ACPI version 1, integers are 32 bits.  In ACPI
 * version 2, integers are 64 bits. Note that this pertains to the ACPI integer
 * type only, not other integers used in the implementation of the ACPI CA
 * subsystem.
 */
typedef UINT64                          ACPI_INTEGER;
#define ACPI_INTEGER_MAX                ACPI_UINT64_MAX
#define ACPI_INTEGER_BIT_SIZE           64
#define ACPI_MAX_DECIMAL_DIGITS         20  /* 2^64 = 18,446,744,073,709,551,616 */


#if ACPI_MACHINE_WIDTH == 64
#define ACPI_USE_NATIVE_DIVIDE          /* Use compiler native 64-bit divide */
#endif

#define ACPI_MAX64_DECIMAL_DIGITS       20
#define ACPI_MAX32_DECIMAL_DIGITS       10
#define ACPI_MAX16_DECIMAL_DIGITS        5
#define ACPI_MAX8_DECIMAL_DIGITS         3

/*
 * Constants with special meanings
 */
#define ACPI_ROOT_OBJECT                ACPI_ADD_PTR (ACPI_HANDLE, NULL, ACPI_MAX_PTR)


/*
 * Initialization sequence
 */
#define ACPI_FULL_INITIALIZATION        0x00
#define ACPI_NO_ADDRESS_SPACE_INIT      0x01
#define ACPI_NO_HARDWARE_INIT           0x02
#define ACPI_NO_EVENT_INIT              0x04
#define ACPI_NO_HANDLER_INIT            0x08
#define ACPI_NO_ACPI_ENABLE             0x10
#define ACPI_NO_DEVICE_INIT             0x20
#define ACPI_NO_OBJECT_INIT             0x40

/*
 * Initialization state
 */
#define ACPI_SUBSYSTEM_INITIALIZE       0x01
#define ACPI_INITIALIZED_OK             0x02

/*
 * Power state values
 */
#define ACPI_STATE_UNKNOWN              (UINT8) 0xFF

#define ACPI_STATE_S0                   (UINT8) 0
#define ACPI_STATE_S1                   (UINT8) 1
#define ACPI_STATE_S2                   (UINT8) 2
#define ACPI_STATE_S3                   (UINT8) 3
#define ACPI_STATE_S4                   (UINT8) 4
#define ACPI_STATE_S5                   (UINT8) 5
#define ACPI_S_STATES_MAX               ACPI_STATE_S5
#define ACPI_S_STATE_COUNT              6

#define ACPI_STATE_D0                   (UINT8) 0
#define ACPI_STATE_D1                   (UINT8) 1
#define ACPI_STATE_D2                   (UINT8) 2
#define ACPI_STATE_D3                   (UINT8) 3
#define ACPI_D_STATES_MAX               ACPI_STATE_D3
#define ACPI_D_STATE_COUNT              4

#define ACPI_STATE_C0                   (UINT8) 0
#define ACPI_STATE_C1                   (UINT8) 1
#define ACPI_STATE_C2                   (UINT8) 2
#define ACPI_STATE_C3                   (UINT8) 3
#define ACPI_C_STATES_MAX               ACPI_STATE_C3
#define ACPI_C_STATE_COUNT              4

/*
 * Sleep type invalid value
 */
#define ACPI_SLEEP_TYPE_MAX             0x7
#define ACPI_SLEEP_TYPE_INVALID         0xFF

/*
 * Standard notify values
 */
#define ACPI_NOTIFY_BUS_CHECK           (UINT8) 0
#define ACPI_NOTIFY_DEVICE_CHECK        (UINT8) 1
#define ACPI_NOTIFY_DEVICE_WAKE         (UINT8) 2
#define ACPI_NOTIFY_EJECT_REQUEST       (UINT8) 3
#define ACPI_NOTIFY_DEVICE_CHECK_LIGHT  (UINT8) 4
#define ACPI_NOTIFY_FREQUENCY_MISMATCH  (UINT8) 5
#define ACPI_NOTIFY_BUS_MODE_MISMATCH   (UINT8) 6
#define ACPI_NOTIFY_POWER_FAULT         (UINT8) 7

/*
 * Types associated with ACPI names and objects.  The first group of
 * values (up to ACPI_TYPE_EXTERNAL_MAX) correspond to the definition
 * of the ACPI ObjectType() operator (See the ACPI Spec).  Therefore,
 * only add to the first group if the spec changes.
 *
 * NOTE: Types must be kept in sync with the global AcpiNsProperties
 * and AcpiNsTypeNames arrays.
 */
typedef UINT32                          ACPI_OBJECT_TYPE;

#define ACPI_TYPE_ANY                   0x00
#define ACPI_TYPE_INTEGER               0x01  /* Byte/Word/Dword/Zero/One/Ones */
#define ACPI_TYPE_STRING                0x02
#define ACPI_TYPE_BUFFER                0x03
#define ACPI_TYPE_PACKAGE               0x04  /* ByteConst, multiple DataTerm/Constant/SuperName */
#define ACPI_TYPE_FIELD_UNIT            0x05
#define ACPI_TYPE_DEVICE                0x06  /* Name, multiple Node */
#define ACPI_TYPE_EVENT                 0x07
#define ACPI_TYPE_METHOD                0x08  /* Name, ByteConst, multiple Code */
#define ACPI_TYPE_MUTEX                 0x09
#define ACPI_TYPE_REGION                0x0A
#define ACPI_TYPE_POWER                 0x0B  /* Name,ByteConst,WordConst,multi Node */
#define ACPI_TYPE_PROCESSOR             0x0C  /* Name,ByteConst,DWordConst,ByteConst,multi NmO */
#define ACPI_TYPE_THERMAL               0x0D  /* Name, multiple Node */
#define ACPI_TYPE_BUFFER_FIELD          0x0E
#define ACPI_TYPE_DDB_HANDLE            0x0F
#define ACPI_TYPE_DEBUG_OBJECT          0x10

#define ACPI_TYPE_EXTERNAL_MAX          0x10

/*
 * These are object types that do not map directly to the ACPI
 * ObjectType() operator. They are used for various internal purposes only.
 * If new predefined ACPI_TYPEs are added (via the ACPI specification), these
 * internal types must move upwards. (There is code that depends on these
 * values being contiguous with the external types above.)
 */
#define ACPI_TYPE_LOCAL_REGION_FIELD    0x11
#define ACPI_TYPE_LOCAL_BANK_FIELD      0x12
#define ACPI_TYPE_LOCAL_INDEX_FIELD     0x13
#define ACPI_TYPE_LOCAL_REFERENCE       0x14  /* Arg#, Local#, Name, Debug, RefOf, Index */
#define ACPI_TYPE_LOCAL_ALIAS           0x15
#define ACPI_TYPE_LOCAL_METHOD_ALIAS    0x16
#define ACPI_TYPE_LOCAL_NOTIFY          0x17
#define ACPI_TYPE_LOCAL_ADDRESS_HANDLER 0x18
#define ACPI_TYPE_LOCAL_RESOURCE        0x19
#define ACPI_TYPE_LOCAL_RESOURCE_FIELD  0x1A
#define ACPI_TYPE_LOCAL_SCOPE           0x1B  /* 1 Name, multiple ObjectList Nodes */

#define ACPI_TYPE_NS_NODE_MAX           0x1B  /* Last typecode used within a NS Node */

/*
 * These are special object types that never appear in
 * a Namespace node, only in an ACPI_OPERAND_OBJECT
 */
#define ACPI_TYPE_LOCAL_EXTRA           0x1C
#define ACPI_TYPE_LOCAL_DATA            0x1D

#define ACPI_TYPE_LOCAL_MAX             0x1D

/* All types above here are invalid */

#define ACPI_TYPE_INVALID               0x1E
#define ACPI_TYPE_NOT_FOUND             0xFF

/*
 * All I/O
 */
#define ACPI_READ                       0
#define ACPI_WRITE                      1
#define ACPI_IO_MASK                    1

/*
 * Event Types: Fixed & General Purpose
 */
typedef UINT32                          ACPI_EVENT_TYPE;

/*
 * Fixed events
 */
#define ACPI_EVENT_PMTIMER              0
#define ACPI_EVENT_GLOBAL               1
#define ACPI_EVENT_POWER_BUTTON         2
#define ACPI_EVENT_SLEEP_BUTTON         3
#define ACPI_EVENT_RTC                  4
#define ACPI_EVENT_MAX                  4
#define ACPI_NUM_FIXED_EVENTS           ACPI_EVENT_MAX + 1

/*
 * Event Status - Per event
 * -------------
 * The encoding of ACPI_EVENT_STATUS is illustrated below.
 * Note that a set bit (1) indicates the property is TRUE
 * (e.g. if bit 0 is set then the event is enabled).
 * +-------------+-+-+-+
 * |   Bits 31:3 |2|1|0|
 * +-------------+-+-+-+
 *          |     | | |
 *          |     | | +- Enabled?
 *          |     | +--- Enabled for wake?
 *          |     +----- Set?
 *          +----------- <Reserved>
 */
typedef UINT32                          ACPI_EVENT_STATUS;

#define ACPI_EVENT_FLAG_DISABLED        (ACPI_EVENT_STATUS) 0x00
#define ACPI_EVENT_FLAG_ENABLED         (ACPI_EVENT_STATUS) 0x01
#define ACPI_EVENT_FLAG_WAKE_ENABLED    (ACPI_EVENT_STATUS) 0x02
#define ACPI_EVENT_FLAG_SET             (ACPI_EVENT_STATUS) 0x04

/*
 * General Purpose Events (GPE)
 */
#define ACPI_GPE_INVALID                0xFF
#define ACPI_GPE_MAX                    0xFF
#define ACPI_NUM_GPE                    256

#define ACPI_GPE_ENABLE                 0
#define ACPI_GPE_DISABLE                1


/*
 * GPE info flags - Per GPE
 * +-+-+-+---+---+-+
 * |7|6|5|4:3|2:1|0|
 * +-+-+-+---+---+-+
 *  | | |  |   |  |
 *  | | |  |   |  +--- Interrupt type: Edge or Level Triggered
 *  | | |  |   +--- Type: Wake-only, Runtime-only, or wake/runtime
 *  | | |  +--- Type of dispatch -- to method, handler, or none
 *  | | +--- Enabled for runtime?
 *  | +--- Enabled for wake?
 *  +--- Unused
 */
#define ACPI_GPE_XRUPT_TYPE_MASK        (UINT8) 0x01
#define ACPI_GPE_LEVEL_TRIGGERED        (UINT8) 0x01
#define ACPI_GPE_EDGE_TRIGGERED         (UINT8) 0x00

#define ACPI_GPE_TYPE_MASK              (UINT8) 0x06
#define ACPI_GPE_TYPE_WAKE_RUN          (UINT8) 0x06
#define ACPI_GPE_TYPE_WAKE              (UINT8) 0x02
#define ACPI_GPE_TYPE_RUNTIME           (UINT8) 0x04    /* Default */

#define ACPI_GPE_DISPATCH_MASK          (UINT8) 0x18
#define ACPI_GPE_DISPATCH_HANDLER       (UINT8) 0x08
#define ACPI_GPE_DISPATCH_METHOD        (UINT8) 0x10
#define ACPI_GPE_DISPATCH_NOT_USED      (UINT8) 0x00    /* Default */

#define ACPI_GPE_RUN_ENABLE_MASK        (UINT8) 0x20
#define ACPI_GPE_RUN_ENABLED            (UINT8) 0x20
#define ACPI_GPE_RUN_DISABLED           (UINT8) 0x00    /* Default */

#define ACPI_GPE_WAKE_ENABLE_MASK       (UINT8) 0x40
#define ACPI_GPE_WAKE_ENABLED           (UINT8) 0x40
#define ACPI_GPE_WAKE_DISABLED          (UINT8) 0x00    /* Default */

#define ACPI_GPE_ENABLE_MASK            (UINT8) 0x60    /* Both run/wake */

/*
 * Flags for GPE and Lock interfaces
 */
#define ACPI_EVENT_WAKE_ENABLE          0x2             /* AcpiGpeEnable */
#define ACPI_EVENT_WAKE_DISABLE         0x2             /* AcpiGpeDisable */

#define ACPI_NOT_ISR                    0x1
#define ACPI_ISR                        0x0


/* Notify types */

#define ACPI_SYSTEM_NOTIFY              0x1
#define ACPI_DEVICE_NOTIFY              0x2
#define ACPI_ALL_NOTIFY                 0x3
#define ACPI_MAX_NOTIFY_HANDLER_TYPE    0x3

#define ACPI_MAX_SYS_NOTIFY             0x7f


/* Address Space (Operation Region) Types */

typedef UINT8                           ACPI_ADR_SPACE_TYPE;

#define ACPI_ADR_SPACE_SYSTEM_MEMORY    (ACPI_ADR_SPACE_TYPE) 0
#define ACPI_ADR_SPACE_SYSTEM_IO        (ACPI_ADR_SPACE_TYPE) 1
#define ACPI_ADR_SPACE_PCI_CONFIG       (ACPI_ADR_SPACE_TYPE) 2
#define ACPI_ADR_SPACE_EC               (ACPI_ADR_SPACE_TYPE) 3
#define ACPI_ADR_SPACE_SMBUS            (ACPI_ADR_SPACE_TYPE) 4
#define ACPI_ADR_SPACE_CMOS             (ACPI_ADR_SPACE_TYPE) 5
#define ACPI_ADR_SPACE_PCI_BAR_TARGET   (ACPI_ADR_SPACE_TYPE) 6
#define ACPI_ADR_SPACE_DATA_TABLE       (ACPI_ADR_SPACE_TYPE) 7
#define ACPI_ADR_SPACE_FIXED_HARDWARE   (ACPI_ADR_SPACE_TYPE) 127


/*
 * BitRegister IDs
 * These are bitfields defined within the full ACPI registers
 */
#define ACPI_BITREG_TIMER_STATUS                0x00
#define ACPI_BITREG_BUS_MASTER_STATUS           0x01
#define ACPI_BITREG_GLOBAL_LOCK_STATUS          0x02
#define ACPI_BITREG_POWER_BUTTON_STATUS         0x03
#define ACPI_BITREG_SLEEP_BUTTON_STATUS         0x04
#define ACPI_BITREG_RT_CLOCK_STATUS             0x05
#define ACPI_BITREG_WAKE_STATUS                 0x06
#define ACPI_BITREG_PCIEXP_WAKE_STATUS          0x07

#define ACPI_BITREG_TIMER_ENABLE                0x08
#define ACPI_BITREG_GLOBAL_LOCK_ENABLE          0x09
#define ACPI_BITREG_POWER_BUTTON_ENABLE         0x0A
#define ACPI_BITREG_SLEEP_BUTTON_ENABLE         0x0B
#define ACPI_BITREG_RT_CLOCK_ENABLE             0x0C
#define ACPI_BITREG_WAKE_ENABLE                 0x0D
#define ACPI_BITREG_PCIEXP_WAKE_DISABLE         0x0E

#define ACPI_BITREG_SCI_ENABLE                  0x0F
#define ACPI_BITREG_BUS_MASTER_RLD              0x10
#define ACPI_BITREG_GLOBAL_LOCK_RELEASE         0x11
#define ACPI_BITREG_SLEEP_TYPE_A                0x12
#define ACPI_BITREG_SLEEP_TYPE_B                0x13
#define ACPI_BITREG_SLEEP_ENABLE                0x14

#define ACPI_BITREG_ARB_DISABLE                 0x15

#define ACPI_BITREG_MAX                         0x15
#define ACPI_NUM_BITREG                         ACPI_BITREG_MAX + 1


/*
 * External ACPI object definition
 */
typedef union acpi_object
{
    ACPI_OBJECT_TYPE                Type;   /* See definition of AcpiNsType for values */
    struct
    {
        ACPI_OBJECT_TYPE                Type;
        ACPI_INTEGER                    Value;      /* The actual number */
    } Integer;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          Length;     /* # of bytes in string, excluding trailing null */
        char                            *Pointer;   /* points to the string value */
    } String;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          Length;     /* # of bytes in buffer */
        UINT8                           *Pointer;   /* points to the buffer */
    } Buffer;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          Fill1;
        ACPI_HANDLE                     Handle;     /* object reference */
    } Reference;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          Count;      /* # of elements in package */
        union acpi_object               *Elements;  /* Pointer to an array of ACPI_OBJECTs */
    } Package;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          ProcId;
        ACPI_IO_ADDRESS                 PblkAddress;
        UINT32                          PblkLength;
    } Processor;

    struct
    {
        ACPI_OBJECT_TYPE                Type;
        UINT32                          SystemLevel;
        UINT32                          ResourceOrder;
    } PowerResource;

} ACPI_OBJECT;


/*
 * List of objects, used as a parameter list for control method evaluation
 */
typedef struct acpi_object_list
{
    UINT32                          Count;
    ACPI_OBJECT                     *Pointer;

} ACPI_OBJECT_LIST;


/*
 * Miscellaneous common Data Structures used by the interfaces
 */
#define ACPI_NO_BUFFER              0
#define ACPI_ALLOCATE_BUFFER        (ACPI_SIZE) (-1)
#define ACPI_ALLOCATE_LOCAL_BUFFER  (ACPI_SIZE) (-2)

typedef struct acpi_buffer
{
    ACPI_SIZE                       Length;         /* Length in bytes of the buffer */
    void                            *Pointer;       /* pointer to buffer */

} ACPI_BUFFER;


/*
 * NameType for AcpiGetName
 */
#define ACPI_FULL_PATHNAME              0
#define ACPI_SINGLE_NAME                1
#define ACPI_NAME_TYPE_MAX              1


/*
 * Structure and flags for AcpiGetSystemInfo
 */
#define ACPI_SYS_MODE_UNKNOWN           0x0000
#define ACPI_SYS_MODE_ACPI              0x0001
#define ACPI_SYS_MODE_LEGACY            0x0002
#define ACPI_SYS_MODES_MASK             0x0003


/*
 * System info returned by AcpiGetSystemInfo()
 */
typedef struct acpi_system_info
{
    UINT32                          AcpiCaVersion;
    UINT32                          Flags;
    UINT32                          TimerResolution;
    UINT32                          Reserved1;
    UINT32                          Reserved2;
    UINT32                          DebugLevel;
    UINT32                          DebugLayer;

} ACPI_SYSTEM_INFO;


/*
 * Types specific to the OS service interfaces
 */
typedef UINT32
(ACPI_SYSTEM_XFACE *ACPI_OSD_HANDLER) (
    void                            *Context);

typedef void
(ACPI_SYSTEM_XFACE *ACPI_OSD_EXEC_CALLBACK) (
    void                            *Context);

/*
 * Various handlers and callback procedures
 */
typedef
UINT32 (*ACPI_EVENT_HANDLER) (
    void                            *Context);

typedef
void (*ACPI_NOTIFY_HANDLER) (
    ACPI_HANDLE                     Device,
    UINT32                          Value,
    void                            *Context);

typedef
void (*ACPI_OBJECT_HANDLER) (
    ACPI_HANDLE                     Object,
    UINT32                          Function,
    void                            *Data);

typedef
ACPI_STATUS (*ACPI_INIT_HANDLER) (
    ACPI_HANDLE                     Object,
    UINT32                          Function);

#define ACPI_INIT_DEVICE_INI        1

typedef
ACPI_STATUS (*ACPI_EXCEPTION_HANDLER) (
    ACPI_STATUS                     AmlStatus,
    ACPI_NAME                       Name,
    UINT16                          Opcode,
    UINT32                          AmlOffset,
    void                            *Context);


/* Address Spaces (For Operation Regions) */

typedef
ACPI_STATUS (*ACPI_ADR_SPACE_HANDLER) (
    UINT32                          Function,
    ACPI_PHYSICAL_ADDRESS           Address,
    UINT32                          BitWidth,
    ACPI_INTEGER                    *Value,
    void                            *HandlerContext,
    void                            *RegionContext);

#define ACPI_DEFAULT_HANDLER            NULL


typedef
ACPI_STATUS (*ACPI_ADR_SPACE_SETUP) (
    ACPI_HANDLE                     RegionHandle,
    UINT32                          Function,
    void                            *HandlerContext,
    void                            **RegionContext);

#define ACPI_REGION_ACTIVATE    0
#define ACPI_REGION_DEACTIVATE  1

typedef
ACPI_STATUS (*ACPI_WALK_CALLBACK) (
    ACPI_HANDLE                     ObjHandle,
    UINT32                          NestingLevel,
    void                            *Context,
    void                            **ReturnValue);


/* Interrupt handler return values */

#define ACPI_INTERRUPT_NOT_HANDLED      0x00
#define ACPI_INTERRUPT_HANDLED          0x01


/* Common string version of device HIDs and UIDs */

typedef struct acpi_device_id
{
    char                            Value[ACPI_DEVICE_ID_LENGTH];

} ACPI_DEVICE_ID;

/* Common string version of device CIDs */

typedef struct acpi_compatible_id
{
    char                            Value[ACPI_MAX_CID_LENGTH];

} ACPI_COMPATIBLE_ID;

typedef struct acpi_compatible_id_list
{
    UINT32                          Count;
    UINT32                          Size;
    ACPI_COMPATIBLE_ID              Id[1];

} ACPI_COMPATIBLE_ID_LIST;


/* Structure and flags for AcpiGetObjectInfo */

#define ACPI_VALID_STA                  0x0001
#define ACPI_VALID_ADR                  0x0002
#define ACPI_VALID_HID                  0x0004
#define ACPI_VALID_UID                  0x0008
#define ACPI_VALID_CID                  0x0010
#define ACPI_VALID_SXDS                 0x0020

/* Flags for _STA method */

#define ACPI_STA_DEVICE_PRESENT         0x01
#define ACPI_STA_DEVICE_ENABLED         0x02
#define ACPI_STA_DEVICE_UI              0x04
#define ACPI_STA_DEVICE_FUNCTIONING     0x08
#define ACPI_STA_DEVICE_OK              0x08 /* Synonym */
#define ACPI_STA_BATTERY_PRESENT        0x10


#define ACPI_COMMON_OBJ_INFO \
    ACPI_OBJECT_TYPE                Type;           /* ACPI object type */ \
    ACPI_NAME                       Name            /* ACPI object Name */


typedef struct acpi_obj_info_header
{
    ACPI_COMMON_OBJ_INFO;

} ACPI_OBJ_INFO_HEADER;


/* Structure returned from Get Object Info */

typedef struct acpi_device_info
{
    ACPI_COMMON_OBJ_INFO;

    UINT32                          Valid;              /* Indicates which fields below are valid */
    UINT32                          CurrentStatus;      /* _STA value */
    ACPI_INTEGER                    Address;            /* _ADR value if any */
    ACPI_DEVICE_ID                  HardwareId;         /* _HID value if any */
    ACPI_DEVICE_ID                  UniqueId;           /* _UID value if any */
    UINT8                           HighestDstates[4];  /* _SxD values: 0xFF indicates not valid */
    ACPI_COMPATIBLE_ID_LIST         CompatibilityId;    /* List of _CIDs if any */

} ACPI_DEVICE_INFO;


/* Context structs for address space handlers */

typedef struct acpi_pci_id
{
    UINT16                          Segment;
    UINT16                          Bus;
    UINT16                          Device;
    UINT16                          Function;

} ACPI_PCI_ID;


typedef struct acpi_mem_space_context
{
    UINT32                          Length;
    ACPI_PHYSICAL_ADDRESS           Address;
    ACPI_PHYSICAL_ADDRESS           MappedPhysicalAddress;
    UINT8                           *MappedLogicalAddress;
    ACPI_SIZE                       MappedLength;

} ACPI_MEM_SPACE_CONTEXT;


/*
 * Definitions for Resource Attributes
 */
typedef UINT16                          ACPI_RS_LENGTH;    /* Resource Length field is fixed at 16 bits */
typedef UINT32                          ACPI_RSDESC_SIZE;  /* Max Resource Descriptor size is (Length+3) = (64K-1)+3 */

/*
 *  Memory Attributes
 */
#define ACPI_READ_ONLY_MEMORY           (UINT8) 0x00
#define ACPI_READ_WRITE_MEMORY          (UINT8) 0x01

#define ACPI_NON_CACHEABLE_MEMORY       (UINT8) 0x00
#define ACPI_CACHABLE_MEMORY            (UINT8) 0x01
#define ACPI_WRITE_COMBINING_MEMORY     (UINT8) 0x02
#define ACPI_PREFETCHABLE_MEMORY        (UINT8) 0x03

/*
 *  IO Attributes
 *  The ISA IO ranges are:     n000-n0FFh,  n400-n4FFh, n800-n8FFh, nC00-nCFFh.
 *  The non-ISA IO ranges are: n100-n3FFh,  n500-n7FFh, n900-nBFFh, nCD0-nFFFh.
 */
#define ACPI_NON_ISA_ONLY_RANGES        (UINT8) 0x01
#define ACPI_ISA_ONLY_RANGES            (UINT8) 0x02
#define ACPI_ENTIRE_RANGE               (ACPI_NON_ISA_ONLY_RANGES | ACPI_ISA_ONLY_RANGES)

/* Type of translation - 1=Sparse, 0=Dense */

#define ACPI_SPARSE_TRANSLATION         (UINT8) 0x01

/*
 *  IO Port Descriptor Decode
 */
#define ACPI_DECODE_10                  (UINT8) 0x00    /* 10-bit IO address decode */
#define ACPI_DECODE_16                  (UINT8) 0x01    /* 16-bit IO address decode */

/*
 *  IRQ Attributes
 */
#define ACPI_LEVEL_SENSITIVE            (UINT8) 0x00
#define ACPI_EDGE_SENSITIVE             (UINT8) 0x01

#define ACPI_ACTIVE_HIGH                (UINT8) 0x00
#define ACPI_ACTIVE_LOW                 (UINT8) 0x01

#define ACPI_EXCLUSIVE                  (UINT8) 0x00
#define ACPI_SHARED                     (UINT8) 0x01

/*
 *  DMA Attributes
 */
#define ACPI_COMPATIBILITY              (UINT8) 0x00
#define ACPI_TYPE_A                     (UINT8) 0x01
#define ACPI_TYPE_B                     (UINT8) 0x02
#define ACPI_TYPE_F                     (UINT8) 0x03

#define ACPI_NOT_BUS_MASTER             (UINT8) 0x00
#define ACPI_BUS_MASTER                 (UINT8) 0x01

#define ACPI_TRANSFER_8                 (UINT8) 0x00
#define ACPI_TRANSFER_8_16              (UINT8) 0x01
#define ACPI_TRANSFER_16                (UINT8) 0x02

/*
 * Start Dependent Functions Priority definitions
 */
#define ACPI_GOOD_CONFIGURATION         (UINT8) 0x00
#define ACPI_ACCEPTABLE_CONFIGURATION   (UINT8) 0x01
#define ACPI_SUB_OPTIMAL_CONFIGURATION  (UINT8) 0x02

/*
 *  16, 32 and 64-bit Address Descriptor resource types
 */
#define ACPI_MEMORY_RANGE               (UINT8) 0x00
#define ACPI_IO_RANGE                   (UINT8) 0x01
#define ACPI_BUS_NUMBER_RANGE           (UINT8) 0x02

#define ACPI_ADDRESS_NOT_FIXED          (UINT8) 0x00
#define ACPI_ADDRESS_FIXED              (UINT8) 0x01

#define ACPI_POS_DECODE                 (UINT8) 0x00
#define ACPI_SUB_DECODE                 (UINT8) 0x01

#define ACPI_PRODUCER                   (UINT8) 0x00
#define ACPI_CONSUMER                   (UINT8) 0x01


/*
 * If possible, pack the following structures to byte alignment
 */
#ifndef ACPI_MISALIGNMENT_NOT_SUPPORTED
#pragma pack(1)
#endif

/* UUID data structures for use in vendor-defined resource descriptors */

typedef struct acpi_uuid
{
    UINT8                           Data[ACPI_UUID_LENGTH];
} ACPI_UUID;

typedef struct acpi_vendor_uuid
{
    UINT8                           Subtype;
    UINT8                           Data[ACPI_UUID_LENGTH];

} ACPI_VENDOR_UUID;

/*
 *  Structures used to describe device resources
 */
typedef struct acpi_resource_irq
{
    UINT8                           Triggering;
    UINT8                           Polarity;
    UINT8                           Sharable;
    UINT8                           InterruptCount;
    UINT8                           Interrupts[1];

} ACPI_RESOURCE_IRQ;


typedef struct ACPI_RESOURCE_DMA
{
    UINT8                           Type;
    UINT8                           BusMaster;
    UINT8                           Transfer;
    UINT8                           ChannelCount;
    UINT8                           Channels[1];

} ACPI_RESOURCE_DMA;


typedef struct acpi_resource_start_dependent
{
    UINT8                           CompatibilityPriority;
    UINT8                           PerformanceRobustness;

} ACPI_RESOURCE_START_DEPENDENT;


/*
 * END_DEPENDENT_FUNCTIONS_RESOURCE struct is not
 * needed because it has no fields
 */


typedef struct acpi_resource_io
{
    UINT8                           IoDecode;
    UINT8                           Alignment;
    UINT8                           AddressLength;
    UINT16                          Minimum;
    UINT16                          Maximum;

} ACPI_RESOURCE_IO;

typedef struct acpi_resource_fixed_io
{
    UINT16                          Address;
    UINT8                           AddressLength;

} ACPI_RESOURCE_FIXED_IO;

typedef struct acpi_resource_vendor
{
    UINT16                          ByteLength;
    UINT8                           ByteData[1];

} ACPI_RESOURCE_VENDOR;

/* Vendor resource with UUID info (introduced in ACPI 3.0) */

typedef struct acpi_resource_vendor_typed
{
    UINT16                          ByteLength;
    UINT8                           UuidSubtype;
    UINT8                           Uuid[ACPI_UUID_LENGTH];
    UINT8                           ByteData[1];

} ACPI_RESOURCE_VENDOR_TYPED;

typedef struct acpi_resource_end_tag
{
    UINT8                           Checksum;

} ACPI_RESOURCE_END_TAG;

typedef struct acpi_resource_memory24
{
    UINT8                           WriteProtect;
    UINT16                          Minimum;
    UINT16                          Maximum;
    UINT16                          Alignment;
    UINT16                          AddressLength;

} ACPI_RESOURCE_MEMORY24;

typedef struct acpi_resource_memory32
{
    UINT8                           WriteProtect;
    UINT32                          Minimum;
    UINT32                          Maximum;
    UINT32                          Alignment;
    UINT32                          AddressLength;

} ACPI_RESOURCE_MEMORY32;

typedef struct acpi_resource_fixed_memory32
{
    UINT8                           WriteProtect;
    UINT32                          Address;
    UINT32                          AddressLength;

} ACPI_RESOURCE_FIXED_MEMORY32;

typedef struct acpi_memory_attribute
{
    UINT8                           WriteProtect;
    UINT8                           Caching;
    UINT8                           RangeType;
    UINT8                           Translation;

} ACPI_MEMORY_ATTRIBUTE;

typedef struct acpi_io_attribute
{
    UINT8                           RangeType;
    UINT8                           Translation;
    UINT8                           TranslationType;
    UINT8                           Reserved1;

} ACPI_IO_ATTRIBUTE;

typedef union acpi_resource_attribute
{
    ACPI_MEMORY_ATTRIBUTE           Mem;
    ACPI_IO_ATTRIBUTE               Io;

    /* Used for the *WordSpace macros */

    UINT8                           TypeSpecific;

} ACPI_RESOURCE_ATTRIBUTE;

typedef struct acpi_resource_source
{
    UINT8                           Index;
    UINT16                          StringLength;
    char                            *StringPtr;

} ACPI_RESOURCE_SOURCE;

/* Fields common to all address descriptors, 16/32/64 bit */

#define ACPI_RESOURCE_ADDRESS_COMMON \
    UINT8                           ResourceType; \
    UINT8                           ProducerConsumer; \
    UINT8                           Decode; \
    UINT8                           MinAddressFixed; \
    UINT8                           MaxAddressFixed; \
    ACPI_RESOURCE_ATTRIBUTE         Info;

typedef struct acpi_resource_address
{
    ACPI_RESOURCE_ADDRESS_COMMON

} ACPI_RESOURCE_ADDRESS;

typedef struct acpi_resource_address16
{
    ACPI_RESOURCE_ADDRESS_COMMON
    UINT16                          Granularity;
    UINT16                          Minimum;
    UINT16                          Maximum;
    UINT16                          TranslationOffset;
    UINT16                          AddressLength;
    ACPI_RESOURCE_SOURCE            ResourceSource;

} ACPI_RESOURCE_ADDRESS16;

typedef struct acpi_resource_address32
{
    ACPI_RESOURCE_ADDRESS_COMMON
    UINT32                          Granularity;
    UINT32                          Minimum;
    UINT32                          Maximum;
    UINT32                          TranslationOffset;
    UINT32                          AddressLength;
    ACPI_RESOURCE_SOURCE            ResourceSource;

} ACPI_RESOURCE_ADDRESS32;

typedef struct acpi_resource_address64
{
    ACPI_RESOURCE_ADDRESS_COMMON
    UINT64                          Granularity;
    UINT64                          Minimum;
    UINT64                          Maximum;
    UINT64                          TranslationOffset;
    UINT64                          AddressLength;
    ACPI_RESOURCE_SOURCE            ResourceSource;

} ACPI_RESOURCE_ADDRESS64;

typedef struct acpi_resource_extended_address64
{
    ACPI_RESOURCE_ADDRESS_COMMON
    UINT8                           RevisionID;
    UINT64                          Granularity;
    UINT64                          Minimum;
    UINT64                          Maximum;
    UINT64                          TranslationOffset;
    UINT64                          AddressLength;
    UINT64                          TypeSpecific;

} ACPI_RESOURCE_EXTENDED_ADDRESS64;

typedef struct acpi_resource_extended_irq
{
    UINT8                           ProducerConsumer;
    UINT8                           Triggering;
    UINT8                           Polarity;
    UINT8                           Sharable;
    UINT8                           InterruptCount;
    ACPI_RESOURCE_SOURCE            ResourceSource;
    UINT32                          Interrupts[1];

} ACPI_RESOURCE_EXTENDED_IRQ;

typedef struct acpi_resource_generic_register
{
    UINT8                           SpaceId;
    UINT8                           BitWidth;
    UINT8                           BitOffset;
    UINT8                           AccessSize;
    UINT64                          Address;

} ACPI_RESOURCE_GENERIC_REGISTER;


/* ACPI_RESOURCE_TYPEs */

#define ACPI_RESOURCE_TYPE_IRQ                  0
#define ACPI_RESOURCE_TYPE_DMA                  1
#define ACPI_RESOURCE_TYPE_START_DEPENDENT      2
#define ACPI_RESOURCE_TYPE_END_DEPENDENT        3
#define ACPI_RESOURCE_TYPE_IO                   4
#define ACPI_RESOURCE_TYPE_FIXED_IO             5
#define ACPI_RESOURCE_TYPE_VENDOR               6
#define ACPI_RESOURCE_TYPE_END_TAG              7
#define ACPI_RESOURCE_TYPE_MEMORY24             8
#define ACPI_RESOURCE_TYPE_MEMORY32             9
#define ACPI_RESOURCE_TYPE_FIXED_MEMORY32       10
#define ACPI_RESOURCE_TYPE_ADDRESS16            11
#define ACPI_RESOURCE_TYPE_ADDRESS32            12
#define ACPI_RESOURCE_TYPE_ADDRESS64            13
#define ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64   14  /* ACPI 3.0 */
#define ACPI_RESOURCE_TYPE_EXTENDED_IRQ         15
#define ACPI_RESOURCE_TYPE_GENERIC_REGISTER     16
#define ACPI_RESOURCE_TYPE_MAX                  16


typedef union acpi_resource_data
{
    ACPI_RESOURCE_IRQ                       Irq;
    ACPI_RESOURCE_DMA                       Dma;
    ACPI_RESOURCE_START_DEPENDENT           StartDpf;
    ACPI_RESOURCE_IO                        Io;
    ACPI_RESOURCE_FIXED_IO                  FixedIo;
    ACPI_RESOURCE_VENDOR                    Vendor;
    ACPI_RESOURCE_VENDOR_TYPED              VendorTyped;
    ACPI_RESOURCE_END_TAG                   EndTag;
    ACPI_RESOURCE_MEMORY24                  Memory24;
    ACPI_RESOURCE_MEMORY32                  Memory32;
    ACPI_RESOURCE_FIXED_MEMORY32            FixedMemory32;
    ACPI_RESOURCE_ADDRESS16                 Address16;
    ACPI_RESOURCE_ADDRESS32                 Address32;
    ACPI_RESOURCE_ADDRESS64                 Address64;
    ACPI_RESOURCE_EXTENDED_ADDRESS64        ExtAddress64;
    ACPI_RESOURCE_EXTENDED_IRQ              ExtendedIrq;
    ACPI_RESOURCE_GENERIC_REGISTER          GenericReg;

    /* Common fields */

    ACPI_RESOURCE_ADDRESS                   Address;        /* Common 16/32/64 address fields */

} ACPI_RESOURCE_DATA;


typedef struct acpi_resource
{
    UINT32                          Type;
    UINT32                          Length;
    ACPI_RESOURCE_DATA              Data;

} ACPI_RESOURCE;

/* restore default alignment */

#pragma pack()


#define ACPI_RS_SIZE_MIN                    12
#define ACPI_RS_SIZE_NO_DATA                8       /* Id + Length fields */
#define ACPI_RS_SIZE(Type)                  (UINT32) (ACPI_RS_SIZE_NO_DATA + sizeof (Type))

#define ACPI_NEXT_RESOURCE(Res)             (ACPI_RESOURCE *)((UINT8 *) Res + Res->Length)


typedef struct acpi_pci_routing_table
{
    UINT32                          Length;
    UINT32                          Pin;
    ACPI_INTEGER                    Address;        /* here for 64-bit alignment */
    UINT32                          SourceIndex;
    char                            Source[4];      /* pad to 64 bits so sizeof() works in all cases */

} ACPI_PCI_ROUTING_TABLE;


#endif /* __ACTYPES_H__ */
