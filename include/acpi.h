/******************************************************************************
 *
 * Name: acpi.h - Master public include file used to interface to ACPICA
 *
 *****************************************************************************/

/******************************************************************************
 *
 * Copyright (c) 1999 - 2026, Intel Corp.
 * SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0-only
 *
 *****************************************************************************/

#ifndef __ACPI_H__
#define __ACPI_H__

/*
 * Public include files for use by code that will interface to ACPICA.
 *
 * Information includes the ACPICA data types, names, exceptions, and
 * external interface prototypes. Also included are the definitions for
 * all ACPI tables (FADT, MADT, etc.)
 *
 * Note: The order of these include files is important.
 */
#include <contrib/dev/acpica/include/platform/acenv.h>     /* Environment-specific items */
#include <contrib/dev/acpica/include/actypes.h>            /* ACPICA data types and structures */
#include <contrib/dev/acpica/include/platform/acenvex.h>   /* Extra environment-specific items */
#include <contrib/dev/acpica/include/acnames.h>            /* Common ACPI names and strings */
#include <contrib/dev/acpica/include/acexcep.h>            /* ACPICA exceptions */
#include <contrib/dev/acpica/include/actbl.h>              /* ACPI table definitions */
#include <contrib/dev/acpica/include/acoutput.h>           /* Error output and Debug macros */
#include <contrib/dev/acpica/include/acrestyp.h>           /* Resource Descriptor structs */
#include <contrib/dev/acpica/include/acpiosxf.h>           /* OSL interfaces (ACPICA-to-OS) */
#include <contrib/dev/acpica/include/acpixf.h>             /* ACPI core subsystem external interfaces */

#endif /* __ACPI_H__ */
