/*-
 * Copyright (c) 2000,2001 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/i386/acpica/OsdEnvironment.c 167814 2007-03-22 18:16:43Z jkim $");

/*
 * 6.1 : Environmental support
 */
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/linker_set.h>
#include <sys/sysctl.h>

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/actables.h>

static u_long i386_acpi_root;

SYSCTL_ULONG(_machdep, OID_AUTO, acpi_root, CTLFLAG_RD, &i386_acpi_root, 0,
	     "The physical address of the RSDP");

ACPI_STATUS
AcpiOsInitialize(void)
{
	return(0);
}

ACPI_STATUS
AcpiOsTerminate(void)
{
	return(0);
}

ACPI_PHYSICAL_ADDRESS
AcpiOsGetRootPointer(void)
{
	u_long	ptr;

	if (i386_acpi_root == 0 &&
	    (resource_long_value("acpi", 0, "rsdp", (long *)&ptr) == 0 ||
	    AcpiFindRootPointer((ACPI_NATIVE_UINT *)&ptr) == AE_OK) &&
	    ptr != 0)
		i386_acpi_root = ptr;

	return (i386_acpi_root);
}
