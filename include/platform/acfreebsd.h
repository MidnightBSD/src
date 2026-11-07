/******************************************************************************
 *
 * Name: acfreebsd.h - OS specific defines, etc.
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef __ACFREEBSD_H__
#define __ACFREEBSD_H__


#include <sys/types.h>

#ifdef __LP64__
#define ACPI_MACHINE_WIDTH      64
#else
#define ACPI_MACHINE_WIDTH      32
#endif

#define COMPILER_DEPENDENT_INT64        int64_t
#define COMPILER_DEPENDENT_UINT64       uint64_t

#define ACPI_UINTPTR_T      uintptr_t

#define ACPI_TO_INTEGER(p)  ((uintptr_t)(p))
#define ACPI_OFFSET(d, f)   __offsetof(d, f)

#define ACPI_USE_DO_WHILE_0
#define ACPI_USE_LOCAL_CACHE
#define ACPI_USE_NATIVE_DIVIDE
#define ACPI_USE_NATIVE_MATH64
#define ACPI_USE_SYSTEM_CLIBRARY

#ifdef _KERNEL

#include <sys/ctype.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/libkern.h>
#include <machine/acpica_machdep.h>
#include <machine/stdarg.h>

#include "opt_acpi.h"

#define ACPI_MUTEX_TYPE     ACPI_OSL_MUTEX

#ifdef ACPI_DEBUG
#define ACPI_DEBUG_OUTPUT   /* for backward compatibility */
#define ACPI_DISASSEMBLER
#endif

#ifdef ACPI_DEBUG_OUTPUT
#include "opt_ddb.h"
#ifdef DDB
#define ACPI_DEBUGGER
#endif /* DDB */
#endif /* ACPI_DEBUG_OUTPUT */

#ifdef DEBUGGER_THREADING
#undef DEBUGGER_THREADING
#endif /* DEBUGGER_THREADING */

#define DEBUGGER_THREADING  0   /* integrated with DDB */

#ifdef INVARIANTS
#define ACPI_MUTEX_DEBUG
#endif

#else /* _KERNEL */

#if __STDC_HOSTED__
#include <ctype.h>
#include <unistd.h>
#endif

#define ACPI_CAST_PTHREAD_T(pthread)    ((ACPI_THREAD_ID) ACPI_TO_INTEGER (pthread))

#define ACPI_USE_STANDARD_HEADERS

#define ACPI_FLUSH_CPU_CACHE()
#define __cdecl

#endif /* _KERNEL */

#endif /* __ACFREEBSD_H__ */
