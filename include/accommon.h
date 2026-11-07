/******************************************************************************
 *
 * Name: accommon.h - Common include files for generation of ACPICA source
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef __ACCOMMON_H__
#define __ACCOMMON_H__

/*
 * Common set of includes for all ACPICA source files.
 * We put them here because we don't want to duplicate them
 * in the source code again and again.
 *
 * Note: The order of these include files is important.
 */
#include <contrib/dev/acpica/include/acconfig.h>           /* Global configuration constants */
#include <contrib/dev/acpica/include/acmacros.h>           /* C macros */
#include <contrib/dev/acpica/include/aclocal.h>            /* Internal data types */
#include <contrib/dev/acpica/include/acobject.h>           /* ACPI internal object */
#include <contrib/dev/acpica/include/acstruct.h>           /* Common structures */
#include <contrib/dev/acpica/include/acglobal.h>           /* All global variables */
#include <contrib/dev/acpica/include/achware.h>            /* Hardware defines and interfaces */
#include <contrib/dev/acpica/include/acutils.h>            /* Utility interfaces */
#ifndef ACPI_USE_SYSTEM_CLIBRARY
#include <contrib/dev/acpica/include/acclib.h>             /* C library interfaces */
#endif /* !ACPI_USE_SYSTEM_CLIBRARY */


#endif /* __ACCOMMON_H__ */
