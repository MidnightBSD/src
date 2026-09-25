/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Matthew N. Dodd <winter@jurai.net>
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/efi.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#if defined(__amd64__) || defined(__i386__)
#include <machine/pc/bios.h>
#endif
#include <dev/smbios/smbios.h>

/*
 * System Management BIOS Reference Specification, v2.4 Final
 * http://www.dmtf.org/standards/published_documents/DSP0134.pdf
 */

struct smbios_softc {
	device_t		dev;
	struct resource *	res;
	int			rid;
};

#define	RES2EPS(res)	((uint8_t *)rman_get_virtual(res))
#define	SMBIOS_EPS_MAX_SIZE	32
#define	SMBIOS_MAX_TABLE_SIZE	(16 * 1024 * 1024)

static devclass_t	smbios_devclass;
static void		*smbios_eps_data;
static size_t		smbios_eps_len;
static void		*smbios_table_data;
static size_t		smbios_table_len;

static void	smbios_identify	(driver_t *, device_t);
static int	smbios_probe	(device_t);
static int	smbios_attach	(device_t);
static int	smbios_detach	(device_t);
static int	smbios_modevent	(module_t, int, void *);

static int	smbios_cksum(const void *, size_t);
static int	smbios_validate_eps(const uint8_t *, size_t *);

static void
smbios_identify (driver_t *driver, device_t parent)
{
#ifdef ARCH_MAY_USE_EFI
	struct uuid efi_smbios;
	void *addr_efi;
#endif
	uint8_t *eps;
	size_t length, resource_length;
	device_t child;
	vm_paddr_t addr = 0;
	int rid;

	if (!device_is_alive(parent))
		return;

#ifdef ARCH_MAY_USE_EFI
	efi_smbios = (struct uuid)EFI_TABLE_SMBIOS3;
	if (!efi_get_table(&efi_smbios, &addr_efi))
		addr = (vm_paddr_t)addr_efi;
	else {
		efi_smbios = (struct uuid)EFI_TABLE_SMBIOS;
		if (!efi_get_table(&efi_smbios, &addr_efi))
			addr = (vm_paddr_t)addr_efi;
	}
#endif

#if defined(__amd64__) || defined(__i386__)
	if (addr == 0)
		addr = bios_sigsearch(SMBIOS_START, SMBIOS_SIG, SMBIOS_LEN,
		    SMBIOS_STEP, SMBIOS_OFF);
#endif

	if (addr != 0) {
		eps = pmap_mapbios(addr, SMBIOS_EPS_MAX_SIZE);
		rid = 0;
		if (smbios_validate_eps(eps, &length) != 0) {
			pmap_unmapbios((vm_offset_t)eps, SMBIOS_EPS_MAX_SIZE);
			return;
		}
		resource_length = length;
		if (memcmp(eps, SMBIOS_SIG, 4) == 0 && length == 0x1e)
			resource_length = sizeof(struct smbios_eps);

		child = BUS_ADD_CHILD(parent, 5, "smbios", -1);
		device_set_driver(child, driver);
		bus_set_resource(child, SYS_RES_MEMORY, rid, addr,
		    resource_length);
		device_set_desc(child, "System Management BIOS");
		pmap_unmapbios((vm_offset_t)eps, SMBIOS_EPS_MAX_SIZE);
	}

	return;
}

static int
smbios_probe (device_t dev)
{
	struct resource *res;
	size_t length;
	int rid;
	int error;

	error = 0;
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}

	if (smbios_validate_eps(RES2EPS(res), &length) != 0 ||
	    length > rman_get_size(res)) {
		device_printf(dev, "SMBIOS checksum failed.\n");
		error = ENXIO;
		goto bad;
	}

bad:
	if (res)
		bus_release_resource(dev, SYS_RES_MEMORY, rid, res);
	return (error);
}

static int
smbios_attach (device_t dev)
{
	struct smbios_softc *sc;
	struct smbios3_eps *eps3;
	struct smbios_eps *eps;
	void *eps_data, *table, *table_data;
	vm_paddr_t table_addr;
	size_t eps_len, table_len;
	int error;

	sc = device_get_softc(dev);
	eps_data = NULL;
	table_data = NULL;
	error = 0;

	sc->dev = dev;
	sc->rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->rid,
		RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Unable to allocate memory resource.\n");
		error = ENOMEM;
		goto bad;
	}
	if (smbios_validate_eps(RES2EPS(sc->res), &eps_len) != 0) {
		error = ENXIO;
		goto bad;
	}
	eps_data = malloc(eps_len, M_DEVBUF, M_WAITOK);
	memcpy(eps_data, RES2EPS(sc->res), eps_len);

	if (memcmp(eps_data, SMBIOS3_SIG, 5) == 0) {
		eps3 = eps_data;
		table_addr = eps3->structure_table_address;
		table_len = eps3->structure_table_length;
		device_printf(dev, "Version: %u.%u, Doc Revision: %u\n",
		    eps3->major_version, eps3->minor_version,
		    eps3->doc_revision);
	} else {
		eps = eps_data;
		table_addr = eps->structure_table_address;
		table_len = eps->structure_table_length;
		device_printf(dev, "Version: %u.%u",
		    eps->major_version, eps->minor_version);
		if (bcd2bin(eps->BCD_revision))
			printf(", BCD Revision: %u.%u",
			    bcd2bin(eps->BCD_revision >> 4),
			    bcd2bin(eps->BCD_revision & 0x0f));
		printf("\n");
	}
	if (table_len == 0 || table_len > SMBIOS_MAX_TABLE_SIZE ||
	    table_addr > UINT64_MAX - (table_len - 1)) {
		device_printf(dev, "Invalid SMBIOS table range.\n");
		error = EINVAL;
		goto bad;
	}
	table = pmap_mapbios(table_addr, table_len);
	if (table == NULL) {
		error = ENOMEM;
		goto bad;
	}
	table_data = malloc(table_len, M_DEVBUF, M_WAITOK);
	memcpy(table_data, table, table_len);
	pmap_unmapbios((vm_offset_t)table, table_len);
	smbios_eps_data = eps_data;
	smbios_eps_len = eps_len;
	smbios_table_data = table_data;
	smbios_table_len = table_len;

	return (0);
bad:
	if (table_data != NULL)
		free(table_data, M_DEVBUF);
	if (eps_data != NULL)
		free(eps_data, M_DEVBUF);
	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);
	sc->res = NULL;
	return (error);
}

static int
smbios_detach (device_t dev)
{
	struct smbios_softc *sc;

	sc = device_get_softc(dev);

	if (sc->res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->rid, sc->res);

	return (0);
}

static int
smbios_modevent (mod, what, arg)
        module_t        mod;
        int             what;
        void *          arg;
{
	device_t *	devs;
	int		count;
	int		i;

	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		devclass_get_devices(smbios_devclass, &devs, &count);
		for (i = 0; i < count; i++) {
			device_delete_child(device_get_parent(devs[i]), devs[i]);
		}
		free(devs, M_TEMP);
		if (smbios_table_data != NULL)
			free(smbios_table_data, M_DEVBUF);
		if (smbios_eps_data != NULL)
			free(smbios_eps_data, M_DEVBUF);
		smbios_table_data = NULL;
		smbios_eps_data = NULL;
		smbios_table_len = 0;
		smbios_eps_len = 0;
		break;
	default:
		break;
	}

	return (0);
}

static device_method_t smbios_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,      smbios_identify),
	DEVMETHOD(device_probe,         smbios_probe),
	DEVMETHOD(device_attach,        smbios_attach),
	DEVMETHOD(device_detach,        smbios_detach),
	{ 0, 0 }
};

static driver_t smbios_driver = {
	"smbios",
	smbios_methods,
	sizeof(struct smbios_softc),
};

DRIVER_MODULE(smbios, nexus, smbios_driver, smbios_devclass, smbios_modevent, 0);
#ifdef ARCH_MAY_USE_EFI
MODULE_DEPEND(smbios, efirt, 1, 1, 1);
#endif
MODULE_VERSION(smbios, 1);

int
smbios_get_entry_point(const void **data, size_t *length)
{
	if (data == NULL || length == NULL)
		return (EINVAL);
	if (smbios_eps_data == NULL)
		return (ENXIO);
	*data = smbios_eps_data;
	*length = smbios_eps_len;
	return (0);
}

int
smbios_get_structure_table(const void **data, size_t *length)
{
	if (data == NULL || length == NULL)
		return (EINVAL);
	if (smbios_table_data == NULL)
		return (ENXIO);
	*data = smbios_table_data;
	*length = smbios_table_len;
	return (0);
}

static int
smbios_cksum(const void *data, size_t length)
{
	const u_int8_t *ptr;
	u_int8_t cksum;
	size_t i;

	ptr = data;
	cksum = 0;
	for (i = 0; i < length; i++)
		cksum += ptr[i];

	return (cksum);
}

static int
smbios_validate_eps(const uint8_t *eps, size_t *length)
{
	const struct smbios3_eps *eps3;
	const struct smbios_eps *eps2;
	size_t len;

	if (memcmp(eps, SMBIOS3_SIG, 5) == 0) {
		eps3 = (const struct smbios3_eps *)eps;
		len = eps3->length;
		if (len < sizeof(*eps3) || len > SMBIOS_EPS_MAX_SIZE ||
		    smbios_cksum(eps, len) != 0)
			return (EINVAL);
	} else if (memcmp(eps, SMBIOS_SIG, 4) == 0) {
		eps2 = (const struct smbios_eps *)eps;
		len = eps2->length;
		if (len < 0x1e || len > SMBIOS_EPS_MAX_SIZE ||
		    memcmp(eps2->intermediate_anchor_string, "_DMI_", 5) != 0 ||
		    smbios_cksum(eps, len) != 0 ||
		    smbios_cksum(eps2->intermediate_anchor_string, 0x0f) != 0)
			return (EINVAL);
	} else {
		return (EINVAL);
	}
	*length = len;
	return (0);
}
