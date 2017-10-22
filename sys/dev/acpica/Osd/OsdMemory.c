/*-
 * Copyright (c) 2000 Mitsaru Iwasaki
 * Copyright (c) 2000 Michael Smith
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

/*
 * 6.2 : Memory Management
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/sys/dev/acpica/Osd/OsdMemory.c 167814 2007-03-22 18:16:43Z jkim $");

#include <contrib/dev/acpica/acpi.h>

#include <sys/kernel.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>

MALLOC_DEFINE(M_ACPICA, "acpica", "ACPI CA memory pool");

void *
AcpiOsAllocate(ACPI_SIZE Size)
{
    return (malloc(Size, M_ACPICA, M_NOWAIT));
}

void
AcpiOsFree(void *Memory)
{
    free(Memory, M_ACPICA);
}

void *
AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_NATIVE_UINT Length)
{
    return (pmap_mapbios((vm_offset_t)PhysicalAddress, Length));
}

void
AcpiOsUnmapMemory(void *LogicalAddress, ACPI_SIZE Length)
{
    pmap_unmapbios((vm_offset_t)LogicalAddress, Length);
}

ACPI_STATUS
AcpiOsGetPhysicalAddress(void *LogicalAddress,
    ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
    /* We can't necessarily do this, so cop out. */
    return (AE_BAD_ADDRESS);
}

ACPI_STATUS
AcpiOsValidateInterface (char *Interface)
{
    return (AE_SUPPORT);
}

/*
 * There is no clean way to do this.  We make the charitable assumption
 * that callers will not pass garbage to us.
 */
ACPI_STATUS
AcpiOsValidateAddress (UINT8 SpaceId, ACPI_PHYSICAL_ADDRESS Address,
    ACPI_SIZE Length)
{
    return (AE_OK);
}

BOOLEAN
AcpiOsReadable (void *Pointer, ACPI_SIZE Length)
{
    return (TRUE);
}

BOOLEAN
AcpiOsWritable (void *Pointer, ACPI_SIZE Length)
{
    return (TRUE);
}

ACPI_STATUS
AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
    void	*LogicalAddress;

    LogicalAddress = AcpiOsMapMemory(Address, Width / 8);
    if (LogicalAddress == NULL)
	return (AE_NOT_EXIST);

    switch (Width) {
    case 8:
	*(u_int8_t *)Value = (*(volatile u_int8_t *)LogicalAddress);
	break;
    case 16:
	*(u_int16_t *)Value = (*(volatile u_int16_t *)LogicalAddress);
	break;
    case 32:
	*(u_int32_t *)Value = (*(volatile u_int32_t *)LogicalAddress);
	break;
    case 64:
	*(u_int64_t *)Value = (*(volatile u_int64_t *)LogicalAddress);
	break;
    default:
	/* debug trap goes here */
	break;
    }

    AcpiOsUnmapMemory(LogicalAddress, Width / 8);

    return (AE_OK);
}

ACPI_STATUS
AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    void	*LogicalAddress;

    LogicalAddress = AcpiOsMapMemory(Address, Width / 8);
    if (LogicalAddress == NULL)
	return (AE_NOT_EXIST);

    switch (Width) {
    case 8:
	(*(volatile u_int8_t *)LogicalAddress) = Value & 0xff;
	break;
    case 16:
	(*(volatile u_int16_t *)LogicalAddress) = Value & 0xffff;
	break;
    case 32:
	(*(volatile u_int32_t *)LogicalAddress) = Value & 0xffffffff;
	break;
    case 64:
	(*(volatile u_int64_t *)LogicalAddress) = Value;
	break;
    default:
	/* debug trap goes here */
	break;
    }

    AcpiOsUnmapMemory(LogicalAddress, Width / 8);

    return (AE_OK);
}
