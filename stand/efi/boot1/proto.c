/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/disk/gpt.h>
#include <machine/elf.h>
#include <machine/stdarg.h>
#include <stand.h>

#include <efi.h>
#include <eficonsctl.h>
#include <efichar.h>

#include "boot_module.h"
#include "paths.h"
#include "proto.h"

static EFI_GUID BlockIoProtocolGUID = BLOCK_IO_PROTOCOL;
static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;

#ifndef EFI_DEBUG
static const char *prio_str[] = {
	"error",
	"not supported",
	"good",
	"better"
};
#endif

static const gpt_uuid_t freebsd_ufs_uuid = GPT_ENT_TYPE_FREEBSD_UFS;
static const gpt_uuid_t freebsd_zfs_uuid = GPT_ENT_TYPE_FREEBSD_ZFS;

/*
 * is_fallback_part returns true if devpath is a GPT partition of type
 * freebsd-ufs or freebsd-zfs. Such partitions are only tried after all
 * others so that MidnightBSD partitions are preferred. If the type cannot
 * be determined, false is returned and the partition keeps its place.
 */
static bool
is_fallback_part(EFI_DEVICE_PATH *devpath)
{
	HARDDRIVE_DEVICE_PATH *hd;
	EFI_DEVICE_PATH *diskpath, *rem;
	EFI_BLOCK_IO *blkio;
	EFI_HANDLE h;
	struct gpt_hdr hdr;
	struct gpt_ent ent;
	uint64_t off;
	UINT32 bsize;
	char *buf;
	bool ret;

	hd = (HARDDRIVE_DEVICE_PATH *)efi_devpath_last_node(devpath);
	if (hd == NULL ||
	    DevicePathType(&hd->Header) != MEDIA_DEVICE_PATH ||
	    DevicePathSubType(&hd->Header) != MEDIA_HARDDRIVE_DP ||
	    DevicePathNodeLength(&hd->Header) < sizeof(*hd) ||
	    hd->MBRType != MBR_TYPE_EFI_PARTITION_TABLE_HEADER ||
	    hd->SignatureType != SIGNATURE_TYPE_GUID)
		return (false);

	/* Find the whole disk the partition is on. */
	diskpath = efi_devpath_trim(devpath);
	if (diskpath == NULL)
		return (false);
	rem = diskpath;
	if (BS->LocateDevicePath(&BlockIoProtocolGUID, &rem, &h) !=
	    EFI_SUCCESS || !IsDevicePathEnd(rem)) {
		free(diskpath);
		return (false);
	}
	free(diskpath);
	if (OpenProtocolByHandle(h, &BlockIoProtocolGUID, (void **)&blkio) !=
	    EFI_SUCCESS || blkio->Media->LogicalPartition)
		return (false);
	bsize = blkio->Media->BlockSize;
	if (bsize < sizeof(hdr))
		return (false);

	/* Read the primary GPT header and the partition's entry. */
	buf = malloc(bsize);
	if (buf == NULL)
		return (false);
	ret = false;
	if (blkio->ReadBlocks(blkio, blkio->Media->MediaId, 1, bsize, buf) !=
	    EFI_SUCCESS)
		goto out;
	memcpy(&hdr, buf, sizeof(hdr));
	if (memcmp(hdr.hdr_sig, GPT_HDR_SIG, sizeof(hdr.hdr_sig)) != 0 ||
	    hdr.hdr_entsz < sizeof(ent) || bsize % hdr.hdr_entsz != 0 ||
	    hd->PartitionNumber < 1 || hd->PartitionNumber > hdr.hdr_entries)
		goto out;
	off = (uint64_t)(hd->PartitionNumber - 1) * hdr.hdr_entsz;
	if (blkio->ReadBlocks(blkio, blkio->Media->MediaId,
	    hdr.hdr_lba_table + off / bsize, bsize, buf) != EFI_SUCCESS)
		goto out;
	memcpy(&ent, buf + off % bsize, sizeof(ent));

	/* Make sure the entry is the partition the firmware gave us. */
	if (memcmp(&ent.ent_uuid, hd->Signature, sizeof(ent.ent_uuid)) != 0 ||
	    ent.ent_lba_start != hd->PartitionStart)
		goto out;
	ret = memcmp(&ent.ent_type, &freebsd_ufs_uuid,
	    sizeof(ent.ent_type)) == 0 ||
	    memcmp(&ent.ent_type, &freebsd_zfs_uuid,
	    sizeof(ent.ent_type)) == 0;
out:
	free(buf);
	return (ret);
}

/*
 * probe_handle determines if the passed handle represents a logical partition
 * if it does it uses each module in order to probe it and if successful it
 * returns EFI_SUCCESS.
 */
static int
probe_handle(EFI_HANDLE h, EFI_DEVICE_PATH *imgpath)
{
	dev_info_t *devinfo;
	EFI_BLOCK_IO *blkio;
	EFI_DEVICE_PATH *devpath;
	EFI_STATUS status;
	UINTN i;
	int preferred;

	/* Figure out if we're dealing with an actual partition. */
	status = OpenProtocolByHandle(h, &DevicePathGUID, (void **)&devpath);
	if (status == EFI_UNSUPPORTED)
		return (0);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query DevicePath (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (-1);
	}
#ifdef EFI_DEBUG
	{
		CHAR16 *text = efi_devpath_name(devpath);
		DPRINTF("probing: %S ", text);
		efi_free_devpath_name(text);
	}
#endif
	status = OpenProtocolByHandle(h, &BlockIoProtocolGUID, (void **)&blkio);
	if (status == EFI_UNSUPPORTED)
		return (0);

	if (status != EFI_SUCCESS) {
		DPRINTF("\nFailed to query BlockIoProtocol (%lu)\n",
		    EFI_ERROR_CODE(status));
		return (-1);
	}

	if (!blkio->Media->LogicalPartition)
		return (0);

	preferred = efi_devpath_same_disk(imgpath, devpath);

	/* Run through each module, see if it can load this partition */
	devinfo = malloc(sizeof(*devinfo));
	if (devinfo == NULL) {
		DPRINTF("\nFailed to allocate devinfo\n");
		return (-1);
	}
	devinfo->dev = blkio;
	devinfo->devpath = devpath;
	devinfo->devhandle = h;
	devinfo->preferred = preferred;
	devinfo->fallback = is_fallback_part(devpath);
	devinfo->next = NULL;

	for (i = 0; i < num_boot_modules; i++) {
		devinfo->devdata = NULL;

		status = boot_modules[i]->probe(devinfo);
		if (status == EFI_SUCCESS)
			return (preferred + 1);
	}
	free(devinfo);

	return (0);
}

/*
 * load_loader attempts to load the loader image data.
 *
 * It tries each module and its respective devices, identified by mod->probe,
 * in order until a successful load occurs at which point it returns EFI_SUCCESS
 * and EFI_NOT_FOUND otherwise.
 *
 * Only devices which have preferred and fallback matching the preferred and
 * fallback parameters are tried.
 */
static EFI_STATUS
load_loader(const boot_module_t **modp, dev_info_t **devinfop, void **bufp,
    size_t *bufsize, int preferred, int fallback)
{
	UINTN i;
	dev_info_t *dev;
	const boot_module_t *mod;

	for (i = 0; i < num_boot_modules; i++) {
		mod = boot_modules[i];
		for (dev = mod->devices(); dev != NULL; dev = dev->next) {
			if (dev->preferred != preferred ||
			    dev->fallback != fallback)
				continue;

			if (mod->load(PATH_LOADER_EFI, dev, bufp, bufsize) ==
			    EFI_SUCCESS) {
				*devinfop = dev;
				*modp = mod;
				return (EFI_SUCCESS);
			}
		}
	}

	return (EFI_NOT_FOUND);
}

void
choice_protocol(EFI_HANDLE *handles, UINTN nhandles, EFI_DEVICE_PATH *imgpath)
{
	UINT16 boot_current;
	size_t sz;
	UINT16 boot_order[100];
	unsigned i;
	int pass, rv;
	EFI_STATUS status;
	const boot_module_t *mod;
	dev_info_t *dev;
	void *loaderbuf;
	size_t loadersize;

	/* Report UEFI Boot Manager Protocol details */
	boot_current = 0;
	sz = sizeof(boot_current);
	if (efi_global_getenv("BootCurrent", &boot_current, &sz) == EFI_SUCCESS) {
		printf("   BootCurrent: %04x\n", boot_current);

		sz = sizeof(boot_order);
		if (efi_global_getenv("BootOrder", &boot_order, &sz) == EFI_SUCCESS) {
			printf("   BootOrder:");
			for (i = 0; i < sz / sizeof(boot_order[0]); i++)
				printf(" %04x%s", boot_order[i],
				    boot_order[i] == boot_current ? "[*]" : "");
			printf("\n");
		}
	}

#ifdef TEST_FAILURE
	/*
	 * For testing failover scenarios, it's nice to be able to fail fast.
	 * Define TEST_FAILURE to create a boot1.efi that always fails after
	 * reporting the boot manager protocol details.
	 */
	BS->Exit(IH, EFI_OUT_OF_RESOURCES, 0, NULL);
#endif

	/* Scan all partitions, probing with all modules. */
	printf("   Probing %zu block devices...", nhandles);
	DPRINTF("\n");
	for (i = 0; i < nhandles; i++) {
		rv = probe_handle(handles[i], imgpath);
#ifdef EFI_DEBUG
		printf("%c", "x.+*"[rv + 1]);
#else
		printf("%s\n", prio_str[rv + 1]);
#endif
	}
	printf(" done\n");


	/* Status summary. */
	for (i = 0; i < num_boot_modules; i++) {
		printf("    ");
		boot_modules[i]->status();
	}

	/*
	 * Try partitions on the disk we were loaded from first, then the
	 * others. In each group, FreeBSD partitions are tried last.
	 */
	status = EFI_NOT_FOUND;
	for (pass = 0; pass < 4 && status != EFI_SUCCESS; pass++)
		status = load_loader(&mod, &dev, &loaderbuf, &loadersize,
		    pass < 2, pass % 2);
	if (status != EFI_SUCCESS) {
		printf("Failed to load '%s'\n", PATH_LOADER_EFI);
		return;
	}

	try_boot(mod, dev, loaderbuf, loadersize);
}
