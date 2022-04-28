/*-
 * Copyright (c) 2015 Landon Fuller <landon@landonf.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/bhnd/cores/chipc/chipcreg.h>

#include "nvram/bhnd_nvram.h"

#include "bhnd_chipc_if.h"

#include "bhnd_nvram_if.h"
#include "bhnd_nvram_map.h"

#include "bhndreg.h"
#include "bhndvar.h"

static device_t		find_nvram_child(device_t dev);

/* BHND core device description table. */
static const struct bhnd_core_desc {
	uint16_t	 vendor;
	uint16_t	 device;
	bhnd_devclass_t	 class;
	const char	*desc;
} bhnd_core_descs[] = {
	#define	BHND_CDESC(_mfg, _cid, _cls, _desc)		\
	    { BHND_MFGID_ ## _mfg, BHND_COREID_ ## _cid,	\
		BHND_DEVCLASS_ ## _cls, _desc }

	BHND_CDESC(BCM, CC,		CC,		"ChipCommon I/O Controller"),
	BHND_CDESC(BCM, ILINE20,	OTHER,		"iLine20 HPNA"),
	BHND_CDESC(BCM, SRAM,		RAM,		"SRAM"),
	BHND_CDESC(BCM, SDRAM,		RAM,		"SDRAM"),
	BHND_CDESC(BCM, PCI,		PCI,		"PCI Bridge"),
	BHND_CDESC(BCM, MIPS,		CPU,		"MIPS Core"),
	BHND_CDESC(BCM, ENET,		ENET_MAC,	"Fast Ethernet MAC"),
	BHND_CDESC(BCM, CODEC,		OTHER,		"V.90 Modem Codec"),
	BHND_CDESC(BCM, USB,		OTHER,		"USB 1.1 Device/Host Controller"),
	BHND_CDESC(BCM, ADSL,		OTHER,		"ADSL Core"),
	BHND_CDESC(BCM, ILINE100,	OTHER,		"iLine100 HPNA"),
	BHND_CDESC(BCM, IPSEC,		OTHER,		"IPsec Accelerator"),
	BHND_CDESC(BCM, UTOPIA,		OTHER,		"UTOPIA ATM Core"),
	BHND_CDESC(BCM, PCMCIA,		PCCARD,		"PCMCIA Bridge"),
	BHND_CDESC(BCM, SOCRAM,		RAM,		"Internal Memory"),
	BHND_CDESC(BCM, MEMC,		MEMC,		"MEMC SDRAM Controller"),
	BHND_CDESC(BCM, OFDM,		OTHER,		"OFDM PHY"),
	BHND_CDESC(BCM, EXTIF,		OTHER,		"External Interface"),
	BHND_CDESC(BCM, D11,		WLAN,		"802.11 MAC/PHY/Radio"),
	BHND_CDESC(BCM, APHY,		WLAN_PHY,	"802.11a PHY"),
	BHND_CDESC(BCM, BPHY,		WLAN_PHY,	"802.11b PHY"),
	BHND_CDESC(BCM, GPHY,		WLAN_PHY,	"802.11g PHY"),
	BHND_CDESC(BCM, MIPS33,		CPU,		"MIPS3302 Core"),
	BHND_CDESC(BCM, USB11H,		OTHER,		"USB 1.1 Host Controller"),
	BHND_CDESC(BCM, USB11D,		OTHER,		"USB 1.1 Device Core"),
	BHND_CDESC(BCM, USB20H,		OTHER,		"USB 2.0 Host Controller"),
	BHND_CDESC(BCM, USB20D,		OTHER,		"USB 2.0 Device Core"),
	BHND_CDESC(BCM, SDIOH,		OTHER,		"SDIO Host Controller"),
	BHND_CDESC(BCM, ROBO,		OTHER,		"RoboSwitch"),
	BHND_CDESC(BCM, ATA100,		OTHER,		"Parallel ATA Controller"),
	BHND_CDESC(BCM, SATAXOR,	OTHER,		"SATA DMA/XOR Controller"),
	BHND_CDESC(BCM, GIGETH,		ENET_MAC,	"Gigabit Ethernet MAC"),
	BHND_CDESC(BCM, PCIE,		PCIE,		"PCIe Bridge"),
	BHND_CDESC(BCM, NPHY,		WLAN_PHY,	"802.11n 2x2 PHY"),
	BHND_CDESC(BCM, SRAMC,		MEMC,		"SRAM Controller"),
	BHND_CDESC(BCM, MINIMAC,	OTHER,		"MINI MAC/PHY"),
	BHND_CDESC(BCM, ARM11,		CPU,		"ARM1176 CPU"),
	BHND_CDESC(BCM, ARM7S,		CPU,		"ARM7TDMI-S CPU"),
	BHND_CDESC(BCM, LPPHY,		WLAN_PHY,	"802.11a/b/g PHY"),
	BHND_CDESC(BCM, PMU,		PMU,		"PMU"),
	BHND_CDESC(BCM, SSNPHY,		WLAN_PHY,	"802.11n Single-Stream PHY"),
	BHND_CDESC(BCM, SDIOD,		OTHER,		"SDIO Device Core"),
	BHND_CDESC(BCM, ARMCM3,		CPU,		"ARM Cortex-M3 CPU"),
	BHND_CDESC(BCM, HTPHY,		WLAN_PHY,	"802.11n 4x4 PHY"),
	BHND_CDESC(MIPS,MIPS74K,	CPU,		"MIPS74k CPU"),
	BHND_CDESC(BCM, GMAC,		ENET_MAC,	"Gigabit MAC core"),
	BHND_CDESC(BCM, DMEMC,		MEMC,		"DDR1/DDR2 Memory Controller"),
	BHND_CDESC(BCM, PCIERC,		OTHER,		"PCIe Root Complex"),
	BHND_CDESC(BCM, OCP,		SOC_BRIDGE,	"OCP to OCP Bridge"),
	BHND_CDESC(BCM, SC,		OTHER,		"Shared Common Core"),
	BHND_CDESC(BCM, AHB,		SOC_BRIDGE,	"OCP to AHB Bridge"),
	BHND_CDESC(BCM, SPIH,		OTHER,		"SPI Host Controller"),
	BHND_CDESC(BCM, I2S,		OTHER,		"I2S Digital Audio Interface"),
	BHND_CDESC(BCM, DMEMS,		MEMC,		"SDR/DDR1 Memory Controller"),
	BHND_CDESC(BCM, UBUS_SHIM,	OTHER,		"BCM6362/UBUS WLAN SHIM"),
	BHND_CDESC(BCM, PCIE2,		PCIE,		"PCIe Bridge (Gen2)"),

	BHND_CDESC(ARM, APB_BRIDGE,	SOC_BRIDGE,	"BP135 AMBA3 AXI to APB Bridge"),
	BHND_CDESC(ARM, PL301,		SOC_ROUTER,	"PL301 AMBA3 Interconnect"),
	BHND_CDESC(ARM, EROM,		EROM,		"PL366 Device Enumeration ROM"),
	BHND_CDESC(ARM, OOB_ROUTER,	OTHER,		"PL367 OOB Interrupt Router"),
	BHND_CDESC(ARM, AXI_UNMAPPED,	OTHER,		"Unmapped Address Ranges"),

	BHND_CDESC(BCM, 4706_CC,	CC,		"ChipCommon I/O Controller"),
	BHND_CDESC(BCM, NS_PCIE2,	PCIE,		"PCIe Bridge (Gen2)"),
	BHND_CDESC(BCM, NS_DMA,		OTHER,		"DMA engine"),
	BHND_CDESC(BCM, NS_SDIO,	OTHER,		"SDIO 3.0 Host Controller"),
	BHND_CDESC(BCM, NS_USB20H,	OTHER,		"USB 2.0 Host Controller"),
	BHND_CDESC(BCM, NS_USB30H,	OTHER,		"USB 3.0 Host Controller"),
	BHND_CDESC(BCM, NS_A9JTAG,	OTHER,		"ARM Cortex A9 JTAG Interface"),
	BHND_CDESC(BCM, NS_DDR23_MEMC,	MEMC,		"Denali DDR2/DD3 Memory Controller"),
	BHND_CDESC(BCM, NS_ROM,		NVRAM,		"System ROM"),
	BHND_CDESC(BCM, NS_NAND,	NVRAM,		"NAND Flash Controller"),
	BHND_CDESC(BCM, NS_QSPI,	NVRAM,		"QSPI Flash Controller"),
	BHND_CDESC(BCM, NS_CC_B,	CC_B,		"ChipCommon B Auxiliary I/O Controller"),
	BHND_CDESC(BCM, 4706_SOCRAM,	RAM,		"Internal Memory"),
	BHND_CDESC(BCM, IHOST_ARMCA9,	CPU,		"ARM Cortex A9 CPU"),
	BHND_CDESC(BCM, 4706_GMAC_CMN,	ENET,		"Gigabit MAC (Common)"),
	BHND_CDESC(BCM, 4706_GMAC,	ENET_MAC,	"Gigabit MAC"),
	BHND_CDESC(BCM, AMEMC,		MEMC,		"Denali DDR1/DDR2 Memory Controller"),
#undef	BHND_CDESC

	/* Derived from inspection of the BCM4331 cores that provide PrimeCell
	 * IDs. Due to lack of documentation, the surmised device name/purpose
	 * provided here may be incorrect. */
	{ BHND_MFGID_ARM,	BHND_PRIMEID_EROM,	BHND_DEVCLASS_OTHER,
	    "PL364 Device Enumeration ROM" },
	{ BHND_MFGID_ARM,	BHND_PRIMEID_SWRAP,	BHND_DEVCLASS_OTHER,
	    "PL368 Device Management Interface" },
	{ BHND_MFGID_ARM,	BHND_PRIMEID_MWRAP,	BHND_DEVCLASS_OTHER,
	    "PL369 Device Management Interface" },

	{ 0, 0, 0, NULL }
};

/**
 * Return the name for a given JEP106 manufacturer ID.
 * 
 * @param vendor A JEP106 Manufacturer ID, including the non-standard ARM 4-bit
 * JEP106 continuation code.
 */
const char *
bhnd_vendor_name(uint16_t vendor)
{
	switch (vendor) {
	case BHND_MFGID_ARM:
		return "ARM";
	case BHND_MFGID_BCM:
		return "Broadcom";
	case BHND_MFGID_MIPS:
		return "MIPS";
	default:
		return "unknown";
	}
}

/**
 * Return the name of a port type.
 */
const char *
bhnd_port_type_name(bhnd_port_type port_type)
{
	switch (port_type) {
	case BHND_PORT_DEVICE:
		return ("device");
	case BHND_PORT_BRIDGE:
		return ("bridge");
	case BHND_PORT_AGENT:
		return ("agent");
	default:
		return "unknown";
	}
}


static const struct bhnd_core_desc *
bhnd_find_core_desc(uint16_t vendor, uint16_t device)
{
	for (u_int i = 0; bhnd_core_descs[i].desc != NULL; i++) {
		if (bhnd_core_descs[i].vendor != vendor)
			continue;
		
		if (bhnd_core_descs[i].device != device)
			continue;
		
		return (&bhnd_core_descs[i]);
	}
	
	return (NULL);
}

/**
 * Return a human-readable name for a BHND core.
 * 
 * @param vendor The core designer's JEDEC-106 Manufacturer ID
 * @param device The core identifier.
 */
const char *
bhnd_find_core_name(uint16_t vendor, uint16_t device)
{
	const struct bhnd_core_desc *desc;
	
	if ((desc = bhnd_find_core_desc(vendor, device)) == NULL)
		return ("unknown");

	return desc->desc;
}

/**
 * Return the device class for a BHND core.
 * 
 * @param vendor The core designer's JEDEC-106 Manufacturer ID
 * @param device The core identifier.
 */
bhnd_devclass_t
bhnd_find_core_class(uint16_t vendor, uint16_t device)
{
	const struct bhnd_core_desc *desc;
	
	if ((desc = bhnd_find_core_desc(vendor, device)) == NULL)
		return (BHND_DEVCLASS_OTHER);

	return desc->class;
}

/**
 * Return a human-readable name for a BHND core.
 * 
 * @param ci The core's info record.
 */
const char *
bhnd_core_name(const struct bhnd_core_info *ci)
{
	return bhnd_find_core_name(ci->vendor, ci->device);
}

/**
 * Return the device class for a BHND core.
 * 
 * @param ci The core's info record.
 */
bhnd_devclass_t
bhnd_core_class(const struct bhnd_core_info *ci)
{
	return bhnd_find_core_class(ci->vendor, ci->device);
}

/**
 * Initialize a core info record with data from from a bhnd-attached @p dev.
 * 
 * @param dev A bhnd device.
 * @param core The record to be initialized.
 */
struct bhnd_core_info
bhnd_get_core_info(device_t dev) {
	return (struct bhnd_core_info) {
		.vendor		= bhnd_get_vendor(dev),
		.device		= bhnd_get_device(dev),
		.hwrev		= bhnd_get_hwrev(dev),
		.core_idx	= bhnd_get_core_index(dev),
		.unit		= bhnd_get_core_unit(dev)
	};
}

/**
 * Find a @p class child device with @p unit on @p dev.
 * 
 * @param parent The bhnd-compatible bus to be searched.
 * @param class The device class to match on.
 * @param unit The device unit number; specify -1 to return the first match
 * regardless of unit number.
 * 
 * @retval device_t if a matching child device is found.
 * @retval NULL if no matching child device is found.
 */
device_t
bhnd_find_child(device_t dev, bhnd_devclass_t class, int unit)
{
	struct bhnd_core_match md = {
		BHND_MATCH_CORE_CLASS(class),
		BHND_MATCH_CORE_UNIT(unit)
	};

	if (unit == -1)
		md.m.match.core_unit = 0;

	return bhnd_match_child(dev, &md);
}

/**
 * Find the first child device on @p dev that matches @p desc.
 * 
 * @param parent The bhnd-compatible bus to be searched.
 * @param desc A match descriptor.
 * 
 * @retval device_t if a matching child device is found.
 * @retval NULL if no matching child device is found.
 */
device_t
bhnd_match_child(device_t dev, const struct bhnd_core_match *desc)
{
	device_t	*devlistp;
	device_t	 match;
	int		 devcnt;
	int		 error;

	error = device_get_children(dev, &devlistp, &devcnt);
	if (error != 0)
		return (NULL);

	match = NULL;
	for (int i = 0; i < devcnt; i++) {
		struct bhnd_core_info ci = bhnd_get_core_info(devlistp[i]);

		if (bhnd_core_matches(&ci, desc)) {
			match = devlistp[i];
			goto done;
		}
	}

done:
	free(devlistp, M_TEMP);
	return match;
}

/**
 * Walk up the bhnd device hierarchy to locate the root device
 * to which the bhndb bridge is attached.
 * 
 * This can be used from within bhnd host bridge drivers to locate the
 * actual upstream host device.
 * 
 * @param dev A bhnd device.
 * @param bus_class The expected bus (e.g. "pci") to which the bridge root
 * should be attached.
 * 
 * @retval device_t if a matching parent device is found.
 * @retval NULL @p dev is not attached via a bhndb bus
 * @retval NULL no parent device is attached via @p bus_class.
 */
device_t
bhnd_find_bridge_root(device_t dev, devclass_t bus_class)
{
	devclass_t	bhndb_class;
	device_t	parent;

	KASSERT(device_get_devclass(device_get_parent(dev)) == bhnd_devclass,
	   ("%s not a bhnd device", device_get_nameunit(dev)));

	bhndb_class = devclass_find("bhndb");

	/* Walk the device tree until we hit a bridge */
	parent = dev;
	while ((parent = device_get_parent(parent)) != NULL) {
		if (device_get_devclass(parent) == bhndb_class)
			break;
	}

	/* No bridge? */
	if (parent == NULL)
		return (NULL);

	/* Search for a parent attached to the expected bus class */
	while ((parent = device_get_parent(parent)) != NULL) {
		device_t bus;

		bus = device_get_parent(parent);
		if (bus != NULL && device_get_devclass(bus) == bus_class)
			return (parent);
	}

	/* Not found */
	return (NULL);
}

/**
 * Find the first core in @p cores that matches @p desc.
 * 
 * @param cores The table to search.
 * @param num_cores The length of @p cores.
 * @param desc A match descriptor.
 * 
 * @retval bhnd_core_info if a matching core is found.
 * @retval NULL if no matching core is found.
 */
const struct bhnd_core_info *
bhnd_match_core(const struct bhnd_core_info *cores, u_int num_cores,
    const struct bhnd_core_match *desc)
{
	for (u_int i = 0; i < num_cores; i++) {
		if (bhnd_core_matches(&cores[i], desc))
			return &cores[i];
	}

	return (NULL);
}


/**
 * Find the first core in @p cores with the given @p class.
 * 
 * @param cores The table to search.
 * @param num_cores The length of @p cores.
 * @param desc A match descriptor.
 * 
 * @retval bhnd_core_info if a matching core is found.
 * @retval NULL if no matching core is found.
 */
const struct bhnd_core_info *
bhnd_find_core(const struct bhnd_core_info *cores, u_int num_cores,
    bhnd_devclass_t class)
{
	struct bhnd_core_match md = {
		BHND_MATCH_CORE_CLASS(class)
	};

	return bhnd_match_core(cores, num_cores, &md);
}

/**
 * Return true if the @p core matches @p desc.
 * 
 * @param core A bhnd core descriptor.
 * @param desc A match descriptor to compare against @p core.
 * 
 * @retval true if @p core matches @p match
 * @retval false if @p core does not match @p match.
 */
bool
bhnd_core_matches(const struct bhnd_core_info *core,
    const struct bhnd_core_match *desc)
{
	if (desc->m.match.core_vendor && desc->core_vendor != core->vendor)
		return (false);

	if (desc->m.match.core_id && desc->core_id != core->device)
		return (false);

	if (desc->m.match.core_unit && desc->core_unit != core->unit)
		return (false);

	if (desc->m.match.core_rev && 
	    !bhnd_hwrev_matches(core->hwrev, &desc->core_rev))
		return (false);

	if (desc->m.match.core_class &&
	    desc->core_class != bhnd_core_class(core))
		return (false);

	return true;
}

/**
 * Return true if the @p chip matches @p desc.
 * 
 * @param chip A bhnd chip identifier.
 * @param desc A match descriptor to compare against @p chip.
 * 
 * @retval true if @p chip matches @p match
 * @retval false if @p chip does not match @p match.
 */
bool
bhnd_chip_matches(const struct bhnd_chipid *chip,
    const struct bhnd_chip_match *desc)
{
	if (desc->m.match.chip_id && chip->chip_id != desc->chip_id)
		return (false);

	if (desc->m.match.chip_pkg && chip->chip_pkg != desc->chip_pkg)
		return (false);

	if (desc->m.match.chip_rev &&
	    !bhnd_hwrev_matches(chip->chip_rev, &desc->chip_rev))
		return (false);

	return (true);
}

/**
 * Return true if the @p board matches @p desc.
 * 
 * @param board The bhnd board info.
 * @param desc A match descriptor to compare against @p board.
 * 
 * @retval true if @p chip matches @p match
 * @retval false if @p chip does not match @p match.
 */
bool
bhnd_board_matches(const struct bhnd_board_info *board,
    const struct bhnd_board_match *desc)
{
	if (desc->m.match.board_srom_rev &&
	    !bhnd_hwrev_matches(board->board_srom_rev, &desc->board_srom_rev))
		return (false);

	if (desc->m.match.board_vendor &&
	    board->board_vendor != desc->board_vendor)
		return (false);

	if (desc->m.match.board_type && board->board_type != desc->board_type)
		return (false);

	if (desc->m.match.board_rev &&
	    !bhnd_hwrev_matches(board->board_rev, &desc->board_rev))
		return (false);

	return (true);
}

/**
 * Return true if the @p hwrev matches @p desc.
 * 
 * @param hwrev A bhnd hardware revision.
 * @param desc A match descriptor to compare against @p core.
 * 
 * @retval true if @p hwrev matches @p match
 * @retval false if @p hwrev does not match @p match.
 */
bool
bhnd_hwrev_matches(uint16_t hwrev, const struct bhnd_hwrev_match *desc)
{
	if (desc->start != BHND_HWREV_INVALID &&
	    desc->start > hwrev)
		return false;
		
	if (desc->end != BHND_HWREV_INVALID &&
	    desc->end < hwrev)
		return false;

	return true;
}

/**
 * Return true if the @p dev matches @p desc.
 * 
 * @param dev A bhnd device.
 * @param desc A match descriptor to compare against @p dev.
 * 
 * @retval true if @p dev matches @p match
 * @retval false if @p dev does not match @p match.
 */
bool
bhnd_device_matches(device_t dev, const struct bhnd_device_match *desc)
{
	struct bhnd_core_info		 core;
	const struct bhnd_chipid	*chip;
	struct bhnd_board_info		 board;
	device_t			 parent;
	int				 error;

	/* Construct individual match descriptors */
	struct bhnd_core_match	m_core	= { _BHND_CORE_MATCH_COPY(desc) };
	struct bhnd_chip_match	m_chip	= { _BHND_CHIP_MATCH_COPY(desc) };
	struct bhnd_board_match	m_board	= { _BHND_BOARD_MATCH_COPY(desc) };

	/* Fetch and match core info */
	if (m_core.m.match_flags) {
		/* Only applicable to bhnd-attached cores */
		parent = device_get_parent(dev);
		if (device_get_devclass(parent) != bhnd_devclass) {
			device_printf(dev, "attempting to match core "
			    "attributes against non-core device\n");
			return (false);
		}

		core = bhnd_get_core_info(dev);
		if (!bhnd_core_matches(&core, &m_core))
			return (false);
	}

	/* Fetch and match chip info */
	if (m_chip.m.match_flags) {
		chip = bhnd_get_chipid(dev);

		if (!bhnd_chip_matches(chip, &m_chip))
			return (false);
	}

	/* Fetch and match board info.
	 *
	 * This is not available until  after NVRAM is up; earlier device
	 * matches should not include board requirements */
	if (m_board.m.match_flags) {
		if ((error = bhnd_read_board_info(dev, &board))) {
			device_printf(dev, "failed to read required board info "
			    "during device matching: %d\n", error);
			return (false);
		}

		if (!bhnd_board_matches(&board, &m_board))
			return (false);
	}

	/* All matched */
	return (true);
}

/**
 * Search @p table for an entry matching @p dev.
 * 
 * @param dev A bhnd device to match against @p table.
 * @param table The device table to search.
 * @param entry_size The @p table entry size, in bytes.
 * 
 * @retval bhnd_device the first matching device, if any.
 * @retval NULL if no matching device is found in @p table.
 */
const struct bhnd_device *
bhnd_device_lookup(device_t dev, const struct bhnd_device *table,
    size_t entry_size)
{
	const struct bhnd_device	*entry;
	device_t			 hostb, parent;
	bhnd_attach_type		 attach_type;
	uint32_t			 dflags;

	parent = device_get_parent(dev);
	hostb = bhnd_find_hostb_device(parent);
	attach_type = bhnd_get_attach_type(dev);

	for (entry = table; !BHND_DEVICE_IS_END(entry); entry =
	    (const struct bhnd_device *) ((const char *) entry + entry_size))
	{
		/* match core info */
		if (!bhnd_device_matches(dev, &entry->core))
			continue;

		/* match device flags */
		dflags = entry->device_flags;

		/* hostb implies BHND_ATTACH_ADAPTER requirement */
		if (dflags & BHND_DF_HOSTB)
			dflags |= BHND_DF_ADAPTER;
	
		if (dflags & BHND_DF_ADAPTER)
			if (attach_type != BHND_ATTACH_ADAPTER)
				continue;

		if (dflags & BHND_DF_HOSTB)
			if (dev != hostb)
				continue;

		if (dflags & BHND_DF_SOC)
			if (attach_type != BHND_ATTACH_NATIVE)
				continue;

		/* device found */
		return (entry);
	}

	/* not found */
	return (NULL);
}

/**
 * Scan the device @p table for all quirk flags applicable to @p dev.
 * 
 * @param dev A bhnd device to match against @p table.
 * @param table The device table to search.
 * 
 * @return returns all matching quirk flags.
 */
uint32_t
bhnd_device_quirks(device_t dev, const struct bhnd_device *table,
    size_t entry_size)
{
	const struct bhnd_device	*dent;
	const struct bhnd_device_quirk	*qent, *qtable;
	uint32_t			 quirks;

	/* Locate the device entry */
	if ((dent = bhnd_device_lookup(dev, table, entry_size)) == NULL)
		return (0);

	/* Quirks table is optional */
	qtable = dent->quirks_table;
	if (qtable == NULL)
		return (0);

	/* Collect matching device quirk entries */
	quirks = 0;
	for (qent = qtable; !BHND_DEVICE_QUIRK_IS_END(qent); qent++) {
		if (bhnd_device_matches(dev, &qent->desc))
			quirks |= qent->quirks;
	}

	return (quirks);
}


/**
 * Allocate bhnd(4) resources defined in @p rs from a parent bus.
 * 
 * @param dev The device requesting ownership of the resources.
 * @param rs A standard bus resource specification. This will be updated
 * with the allocated resource's RIDs.
 * @param res On success, the allocated bhnd resources.
 * 
 * @retval 0 success
 * @retval non-zero if allocation of any non-RF_OPTIONAL resource fails,
 * 		    all allocated resources will be released and a regular
 * 		    unix error code will be returned.
 */
int
bhnd_alloc_resources(device_t dev, struct resource_spec *rs,
    struct bhnd_resource **res)
{
	/* Initialize output array */
	for (u_int i = 0; rs[i].type != -1; i++)
		res[i] = NULL;

	for (u_int i = 0; rs[i].type != -1; i++) {
		res[i] = bhnd_alloc_resource_any(dev, rs[i].type, &rs[i].rid,
		    rs[i].flags);

		/* Clean up all allocations on failure */
		if (res[i] == NULL && !(rs[i].flags & RF_OPTIONAL)) {
			bhnd_release_resources(dev, rs, res);
			return (ENXIO);
		}
	}

	return (0);
};

/**
 * Release bhnd(4) resources defined in @p rs from a parent bus.
 * 
 * @param dev The device that owns the resources.
 * @param rs A standard bus resource specification previously initialized
 * by @p bhnd_alloc_resources.
 * @param res The bhnd resources to be released.
 */
void
bhnd_release_resources(device_t dev, const struct resource_spec *rs,
    struct bhnd_resource **res)
{
	for (u_int i = 0; rs[i].type != -1; i++) {
		if (res[i] == NULL)
			continue;

		bhnd_release_resource(dev, rs[i].type, rs[i].rid, res[i]);
		res[i] = NULL;
	}
}

/**
 * Parse the CHIPC_ID_* fields from the ChipCommon CHIPC_ID
 * register, returning its bhnd_chipid representation.
 * 
 * @param idreg The CHIPC_ID register value.
 * @param enum_addr The enumeration address to include in the result.
 *
 * @warning
 * On early siba(4) devices, the ChipCommon core does not provide
 * a valid CHIPC_ID_NUMCORE field. On these ChipCommon revisions
 * (see CHIPC_NCORES_MIN_HWREV()), this function will parse and return
 * an invalid `ncores` value.
 */
struct bhnd_chipid
bhnd_parse_chipid(uint32_t idreg, bhnd_addr_t enum_addr)
{
	struct bhnd_chipid result;

	/* Fetch the basic chip info */
	result.chip_id = CHIPC_GET_BITS(idreg, CHIPC_ID_CHIP);
	result.chip_pkg = CHIPC_GET_BITS(idreg, CHIPC_ID_PKG);
	result.chip_rev = CHIPC_GET_BITS(idreg, CHIPC_ID_REV);
	result.chip_type = CHIPC_GET_BITS(idreg, CHIPC_ID_BUS);
	result.ncores = CHIPC_GET_BITS(idreg, CHIPC_ID_NUMCORE);

	result.enum_addr = enum_addr;

	return (result);
}

/**
 * Allocate the resource defined by @p rs via @p dev, use it
 * to read the ChipCommon ID register relative to @p chipc_offset,
 * then release the resource.
 * 
 * @param dev The device owning @p rs.
 * @param rs A resource spec that encompasses the ChipCommon register block.
 * @param chipc_offset The offset of the ChipCommon registers within @p rs.
 * @param[out] result the chip identification data.
 * 
 * @retval 0 success
 * @retval non-zero if the ChipCommon identification data could not be read.
 */
int
bhnd_read_chipid(device_t dev, struct resource_spec *rs,
    bus_size_t chipc_offset, struct bhnd_chipid *result)
{
	struct resource			*res;
	uint32_t			 reg;
	int				 error, rid, rtype;

	/* Allocate the ChipCommon window resource and fetch the chipid data */
	rid = rs->rid;
	rtype = rs->type;
	res = bus_alloc_resource_any(dev, rtype, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev,
		    "failed to allocate bhnd chipc resource\n");
		return (ENXIO);
	}

	/* Fetch the basic chip info */
	reg = bus_read_4(res, chipc_offset + CHIPC_ID);
	*result = bhnd_parse_chipid(reg, 0x0);

	/* Fetch the enum base address */
	error = 0;
	switch (result->chip_type) {
	case BHND_CHIPTYPE_SIBA:
		result->enum_addr = BHND_DEFAULT_CHIPC_ADDR;
		break;
	case BHND_CHIPTYPE_BCMA:
	case BHND_CHIPTYPE_BCMA_ALT:
		result->enum_addr = bus_read_4(res, chipc_offset +
		    CHIPC_EROMPTR);
		break;
	case BHND_CHIPTYPE_UBUS:
		device_printf(dev, "unsupported ubus/bcm63xx chip type");
		error = ENODEV;
		goto cleanup;
	default:
		device_printf(dev, "unknown chip type %hhu\n",
		    result->chip_type);
		error = ENODEV;
		goto cleanup;
	}

cleanup:
	/* Clean up */
	bus_release_resource(dev, rtype, rid, res);
	return (error);
}

/**
 * Using the bhnd(4) bus-level core information and a custom core name,
 * populate @p dev's device description.
 * 
 * @param dev A bhnd-bus attached device.
 * @param dev_name The core's name (e.g. "SDIO Device Core")
 */
void
bhnd_set_custom_core_desc(device_t dev, const char *dev_name)
{
	const char *vendor_name;
	char *desc;

	vendor_name = bhnd_get_vendor_name(dev);
	asprintf(&desc, M_BHND, "%s %s, rev %hhu", vendor_name, dev_name,
	    bhnd_get_hwrev(dev));

	if (desc != NULL) {
		device_set_desc_copy(dev, desc);
		free(desc, M_BHND);
	} else {
		device_set_desc(dev, dev_name);
	}
}

/**
 * Using the bhnd(4) bus-level core information, populate @p dev's device
 * description.
 * 
 * @param dev A bhnd-bus attached device.
 */
void
bhnd_set_default_core_desc(device_t dev)
{
	bhnd_set_custom_core_desc(dev, bhnd_get_device_name(dev));
}

/**
 * Helper function for implementing BHND_BUS_IS_HW_DISABLED().
 * 
 * If a parent device is available, this implementation delegates the
 * request to the BHND_BUS_IS_HW_DISABLED() method on the parent of @p dev.
 * 
 * If no parent device is available (i.e. on a the bus root), the hardware
 * is assumed to be usable and false is returned.
 */
bool
bhnd_bus_generic_is_hw_disabled(device_t dev, device_t child)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_IS_HW_DISABLED(device_get_parent(dev), child));

	return (false);
}

/**
 * Helper function for implementing BHND_BUS_GET_CHIPID().
 * 
 * This implementation delegates the request to the BHND_BUS_GET_CHIPID()
 * method on the parent of @p dev. If no parent exists, the implementation
 * will panic.
 */
const struct bhnd_chipid *
bhnd_bus_generic_get_chipid(device_t dev, device_t child)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_GET_CHIPID(device_get_parent(dev), child));

	panic("missing BHND_BUS_GET_CHIPID()");
}

/* nvram board_info population macros for bhnd_bus_generic_read_board_info() */
#define	BHND_GV(_dest, _name)	\
	bhnd_nvram_getvar(child, BHND_NVAR_ ## _name, &_dest, sizeof(_dest))

#define	REQ_BHND_GV(_dest, _name)		do {			\
	if ((error = BHND_GV(_dest, _name))) {				\
		device_printf(dev,					\
		    "error reading " __STRING(_name) ": %d\n", error);	\
		return (error);						\
	}								\
} while(0)

#define	OPT_BHND_GV(_dest, _name, _default)	do {			\
	if ((error = BHND_GV(_dest, _name))) {				\
		if (error != ENOENT) {					\
			device_printf(dev,				\
			    "error reading "				\
			       __STRING(_name) ": %d\n", error);	\
			return (error);					\
		}							\
		_dest = _default;					\
	}								\
} while(0)

/**
 * Helper function for implementing BHND_BUS_READ_BOARDINFO().
 * 
 * This implementation populates @p info with information from NVRAM,
 * defaulting board_vendor and board_type fields to 0 if the
 * requested variables cannot be found.
 * 
 * This behavior is correct for most SoCs, but must be overridden on
 * bridged (PCI, PCMCIA, etc) devices to produce a complete bhnd_board_info
 * result.
 */
int
bhnd_bus_generic_read_board_info(device_t dev, device_t child,
    struct bhnd_board_info *info)
{
	int	error;

	OPT_BHND_GV(info->board_vendor,	BOARDVENDOR,	0);
	OPT_BHND_GV(info->board_type,	BOARDTYPE,	0);	/* srom >= 2 */
	REQ_BHND_GV(info->board_rev,	BOARDREV);
	REQ_BHND_GV(info->board_srom_rev,SROMREV);
	REQ_BHND_GV(info->board_flags,	BOARDFLAGS);
	OPT_BHND_GV(info->board_flags2,	BOARDFLAGS2,	0);	/* srom >= 4 */
	OPT_BHND_GV(info->board_flags3,	BOARDFLAGS3,	0);	/* srom >= 11 */

	return (0);
}

#undef	BHND_GV
#undef	BHND_GV_REQ
#undef	BHND_GV_OPT


/**
 * Find an NVRAM child device on @p dev, if any.
 * 
 * @retval device_t An NVRAM device.
 * @retval NULL If no NVRAM device is found.
 */
static device_t
find_nvram_child(device_t dev)
{
	device_t	chipc, nvram;

	/* Look for a directly-attached NVRAM child */
	nvram = device_find_child(dev, "bhnd_nvram", 0);
	if (nvram != NULL)
		return (nvram);

	/* Remaining checks are only applicable when searching a bhnd(4)
	 * bus. */
	if (device_get_devclass(dev) != bhnd_devclass)
		return (NULL);

	/* Look for a ChipCommon-attached NVRAM device */
	if ((chipc = bhnd_find_child(dev, BHND_DEVCLASS_CC, -1)) != NULL) {
		nvram = device_find_child(chipc, "bhnd_nvram", 0);
		if (nvram != NULL)
			return (nvram);
	}

	/* Not found */
	return (NULL);
}

/**
 * Helper function for implementing BHND_BUS_GET_NVRAM_VAR().
 * 
 * This implementation searches @p dev for a usable NVRAM child device:
 * - The first child device implementing the bhnd_nvram devclass is
 *   returned, otherwise
 * - If @p dev is a bhnd(4) bus, a ChipCommon core that advertises an
 *   attached NVRAM source.
 * 
 * If no usable child device is found on @p dev, the request is delegated to
 * the BHND_BUS_GET_NVRAM_VAR() method on the parent of @p dev.
 */
int
bhnd_bus_generic_get_nvram_var(device_t dev, device_t child, const char *name,
    void *buf, size_t *size)
{
	device_t	nvram;
	device_t	parent;

	/* Try to find an NVRAM device applicable to @p child */
	if ((nvram = find_nvram_child(dev)) != NULL)
		return BHND_NVRAM_GETVAR(nvram, name, buf, size);

	/* Try to delegate to parent */
	if ((parent = device_get_parent(dev)) == NULL)
		return (ENODEV);

	return (BHND_BUS_GET_NVRAM_VAR(device_get_parent(dev), child,
	    name, buf, size));
}

/**
 * Helper function for implementing BHND_BUS_ALLOC_RESOURCE().
 * 
 * This implementation of BHND_BUS_ALLOC_RESOURCE() delegates allocation
 * of the underlying resource to BUS_ALLOC_RESOURCE(), and activation
 * to @p dev's BHND_BUS_ACTIVATE_RESOURCE().
 */
struct bhnd_resource *
bhnd_bus_generic_alloc_resource(device_t dev, device_t child, int type,
	int *rid, rman_res_t start, rman_res_t end, rman_res_t count,
	u_int flags)
{
	struct bhnd_resource	*br;
	struct resource		*res;
	int			 error;

	br = NULL;
	res = NULL;

	/* Allocate the real bus resource (without activating it) */
	res = BUS_ALLOC_RESOURCE(dev, child, type, rid, start, end, count,
	    (flags & ~RF_ACTIVE));
	if (res == NULL)
		return (NULL);

	/* Allocate our bhnd resource wrapper. */
	br = malloc(sizeof(struct bhnd_resource), M_BHND, M_NOWAIT);
	if (br == NULL)
		goto failed;
	
	br->direct = false;
	br->res = res;

	/* Attempt activation */
	if (flags & RF_ACTIVE) {
		error = BHND_BUS_ACTIVATE_RESOURCE(dev, child, type, *rid, br);
		if (error)
			goto failed;
	}

	return (br);
	
failed:
	if (res != NULL)
		BUS_RELEASE_RESOURCE(dev, child, type, *rid, res);

	free(br, M_BHND);
	return (NULL);
}

/**
 * Helper function for implementing BHND_BUS_RELEASE_RESOURCE().
 * 
 * This implementation of BHND_BUS_RELEASE_RESOURCE() delegates release of
 * the backing resource to BUS_RELEASE_RESOURCE().
 */
int
bhnd_bus_generic_release_resource(device_t dev, device_t child, int type,
    int rid, struct bhnd_resource *r)
{
	int error;

	if ((error = BUS_RELEASE_RESOURCE(dev, child, type, rid, r->res)))
		return (error);

	free(r, M_BHND);
	return (0);
}


/**
 * Helper function for implementing BHND_BUS_ACTIVATE_RESOURCE().
 * 
 * This implementation of BHND_BUS_ACTIVATE_RESOURCE() simply calls the
 * BHND_BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bhnd_bus_generic_activate_resource(device_t dev, device_t child, int type,
    int rid, struct bhnd_resource *r)
{
	/* Try to delegate to the parent */
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_ACTIVATE_RESOURCE(device_get_parent(dev),
		    child, type, rid, r));

	return (EINVAL);
};

/**
 * Helper function for implementing BHND_BUS_DEACTIVATE_RESOURCE().
 * 
 * This implementation of BHND_BUS_ACTIVATE_RESOURCE() simply calls the
 * BHND_BUS_ACTIVATE_RESOURCE() method of the parent of @p dev.
 */
int
bhnd_bus_generic_deactivate_resource(device_t dev, device_t child,
    int type, int rid, struct bhnd_resource *r)
{
	if (device_get_parent(dev) != NULL)
		return (BHND_BUS_DEACTIVATE_RESOURCE(device_get_parent(dev),
		    child, type, rid, r));

	return (EINVAL);
};

