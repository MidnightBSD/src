/*-
 * Copyright (c) 2008-2010 Rui Paulo
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include <efi.h>
#include <efilib.h>

#include <bootstrap.h>
#include "../libi386/libi386.h"

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

struct devdesc currdev;		/* our current device */
struct arch_switch archsw;	/* MI/MD interface boundary */

EFI_GUID acpi = ACPI_TABLE_GUID;
EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
EFI_GUID devid = DEVICE_PATH_PROTOCOL;
EFI_GUID imgid = LOADED_IMAGE_PROTOCOL;
EFI_GUID mps = MPS_TABLE_GUID;
EFI_GUID netid = EFI_SIMPLE_NETWORK_PROTOCOL;
EFI_GUID smbios = SMBIOS_TABLE_GUID;

EFI_STATUS
main(int argc, CHAR16 *argv[])
{
	char vendor[128];
	EFI_LOADED_IMAGE *img;
	int i;

	/* 
	 * XXX Chicken-and-egg problem; we want to have console output
	 * early, but some console attributes may depend on reading from
	 * eg. the boot device, which we can't do yet.  We can use
	 * printf() etc. once this is done.
	 */
	cons_probe();

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	/* Get our loaded image protocol interface structure. */
	BS->HandleProtocol(IH, &imgid, (VOID**)&img);

	printf("Image base: 0x%lx\n", (u_long)img->ImageBase);
	printf("EFI version: %d.%02d\n", ST->Hdr.Revision >> 16,
	    ST->Hdr.Revision & 0xffff);
	printf("EFI Firmware: ");
	/* printf doesn't understand EFI Unicode */
	ST->ConOut->OutputString(ST->ConOut, ST->FirmwareVendor);
	printf(" (rev %d.%02d)\n", ST->FirmwareRevision >> 16,
	    ST->FirmwareRevision & 0xffff);

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);

	efi_handle_lookup(img->DeviceHandle, &currdev.d_dev, &currdev.d_unit);
	currdev.d_type = currdev.d_dev->dv_type;

	/*
	 * Disable the watchdog timer. By default the boot manager sets
	 * the timer to 5 minutes before invoking a boot option. If we
	 * want to return to the boot manager, we have to disable the
	 * watchdog timer and since we're an interactive program, we don't
	 * want to wait until the user types "quit". The timer may have
	 * fired by then. We don't care if this fails. It does not prevent
	 * normal functioning in any way...
	 */
	BS->SetWatchdogTimer(0, 0, 0, NULL);

	env_setenv("currdev", EV_VOLATILE, i386_fmtdev(&currdev),
	    i386_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, i386_fmtdev(&currdev), env_noset,
	    env_nounset);

	setenv("LINES", "24", 1);	/* optional */
    
	archsw.arch_autoload = i386_autoload;
	archsw.arch_getdev = i386_getdev;
	archsw.arch_copyin = i386_copyin;
	archsw.arch_copyout = i386_copyout;
	archsw.arch_readin = i386_readin;

	interact();			/* doesn't return */

	return (EFI_SUCCESS);		/* keep compiler happy */
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	RS->ResetSystem(EfiResetCold, EFI_SUCCESS, 23,
	    (CHAR16 *)"Reboot from the loader");

	/* NOTREACHED */
	return (CMD_ERROR);
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{
	exit(0);
	return (CMD_OK);
}

COMMAND_SET(memmap, "memmap", "print memory map", command_memmap);

static int
command_memmap(int argc, char *argv[])
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	static char *types[] = {
	    "Reserved",
	    "LoaderCode",
	    "LoaderData",
	    "BootServicesCode",
	    "BootServicesData",
	    "RuntimeServicesCode",
	    "RuntimeServicesData",
	    "ConventionalMemory",
	    "UnusableMemory",
	    "ACPIReclaimMemory",
	    "ACPIMemoryNVS",
	    "MemoryMappedIO",
	    "MemoryMappedIOPortSpace",
	    "PalCode"
	};

	sz = 0;
	status = BS->GetMemoryMap(&sz, 0, &key, &dsz, &dver);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't determine memory map size\n");
		return CMD_ERROR;
	}
	map = malloc(sz);
	status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
	if (EFI_ERROR(status)) {
		printf("Can't read memory map\n");
		return CMD_ERROR;
	}

	ndesc = sz / dsz;
	printf("%23s %12s %12s %8s %4s\n",
	       "Type", "Physical", "Virtual", "#Pages", "Attr");
	       
	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
	    printf("%23s %012lx %012lx %08lx ",
		   types[p->Type],
		   p->PhysicalStart,
		   p->VirtualStart,
		   p->NumberOfPages);
	    if (p->Attribute & EFI_MEMORY_UC)
		printf("UC ");
	    if (p->Attribute & EFI_MEMORY_WC)
		printf("WC ");
	    if (p->Attribute & EFI_MEMORY_WT)
		printf("WT ");
	    if (p->Attribute & EFI_MEMORY_WB)
		printf("WB ");
	    if (p->Attribute & EFI_MEMORY_UCE)
		printf("UCE ");
	    if (p->Attribute & EFI_MEMORY_WP)
		printf("WP ");
	    if (p->Attribute & EFI_MEMORY_RP)
		printf("RP ");
	    if (p->Attribute & EFI_MEMORY_XP)
		printf("XP ");
	    printf("\n");
	}

	return CMD_OK;
}

COMMAND_SET(configuration, "configuration",
	    "print configuration tables", command_configuration);

static const char *
guid_to_string(EFI_GUID *guid)
{
	static char buf[40];

	sprintf(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    guid->Data1, guid->Data2, guid->Data3, guid->Data4[0],
	    guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4],
	    guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	return (buf);
}

static int
command_configuration(int argc, char *argv[])
{
	int i;

	printf("NumberOfTableEntries=%ld\n", ST->NumberOfTableEntries);
	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		EFI_GUID *guid;

		printf("  ");
		guid = &ST->ConfigurationTable[i].VendorGuid;
		if (!memcmp(guid, &mps, sizeof(EFI_GUID)))
			printf("MPS Table");
		else if (!memcmp(guid, &acpi, sizeof(EFI_GUID)))
			printf("ACPI Table");
		else if (!memcmp(guid, &acpi20, sizeof(EFI_GUID)))
			printf("ACPI 2.0 Table");
		else if (!memcmp(guid, &smbios, sizeof(EFI_GUID)))
			printf("SMBIOS Table");
		else
			printf("Unknown Table (%s)", guid_to_string(guid));
		printf(" at %p\n", ST->ConfigurationTable[i].VendorTable);
	}

	return CMD_OK;
}    


COMMAND_SET(mode, "mode", "change or display text modes", command_mode);

static int
command_mode(int argc, char *argv[])
{
	unsigned int cols, rows, mode;
	int i;
	char *cp;
	char rowenv[8];
	EFI_STATUS status;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;

	conout = ST->ConOut;

	if (argc > 1) {
		mode = strtol(argv[1], &cp, 0);
		if (cp[0] != '\0') {
			printf("Invalid mode\n");
			return (CMD_ERROR);
		}
		status = conout->QueryMode(conout, mode, &cols, &rows);
		if (EFI_ERROR(status)) {
			printf("invalid mode %d\n", mode);
			return (CMD_ERROR);
		}
		status = conout->SetMode(conout, mode);
		if (EFI_ERROR(status)) {
			printf("couldn't set mode %d\n", mode);
			return (CMD_ERROR);
		}
		sprintf(rowenv, "%d", rows);
		setenv("LINES", rowenv, 1);

		return (CMD_OK);
	}

	for (i = 0; ; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			break;
		printf("Mode %d: %d columns, %d rows\n", i, cols, rows);
	}

	if (i != 0)
		printf("Choose the mode with \"col <mode number>\"\n");	

	return (CMD_OK);
}


COMMAND_SET(nvram, "nvram", "get or set NVRAM variables", command_nvram);

static int
command_nvram(int argc, char *argv[])
{
	CHAR16 var[128];
	CHAR16 *data;
	EFI_STATUS status;
	EFI_GUID varguid = { 0,0,0,{0,0,0,0,0,0,0,0} };
	unsigned int varsz;
	unsigned int datasz;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;
	int i;

	conout = ST->ConOut;

	/* Initiate the search */
	status = RS->GetNextVariableName(&varsz, NULL, NULL);

	for (; status != EFI_NOT_FOUND; ) {
		status = RS->GetNextVariableName(&varsz, var,
		    &varguid);
		//if (EFI_ERROR(status))
			//break;

		conout->OutputString(conout, var);
		printf("=");
		datasz = 0;
		status = RS->GetVariable(var, &varguid, NULL, &datasz,
		    NULL);
		/* XXX: check status */
		data = malloc(datasz);
		status = RS->GetVariable(var, &varguid, NULL, &datasz,
		    data);
		if (EFI_ERROR(status))
			printf("<error retrieving variable>");
		else {
			for (i = 0; i < datasz; i++) {
				if (isalnum(data[i]) || isspace(data[i]))
					printf("%c", data[i]);
				else
					printf("\\x%02x", data[i]);
			}
		}
		/* XXX */
		pager_output("\n");
		free(data);
	}

	return (CMD_OK);
}
