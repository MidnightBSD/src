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
 * 
 * $FreeBSD$
 */

#ifndef _BCMA_BCMAVAR_H_
#define _BCMA_BCMAVAR_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/limits.h>

#include <machine/bus.h>
#include <sys/rman.h>

#include "bcma.h"

/*
 * Internal definitions shared by bcma(4) driver implementations.
 */

/** BCMA port identifier. */
typedef u_int		bcma_pid_t;
#define BCMA_PID_MAX	UINT_MAX	/**< Maximum bcma_pid_t value */

/** BCMA per-port region map identifier. */
typedef u_int		bcma_rmid_t;
#define	BCMA_RMID_MAX	UINT_MAX	/**< Maximum bcma_rmid_t value */

struct bcma_devinfo;
struct bcma_corecfg;
struct bcma_map;
struct bcma_mport;
struct bcma_sport;

int			 bcma_probe(device_t dev);
int			 bcma_attach(device_t dev);
int			 bcma_detach(device_t dev);

int			 bcma_add_children(device_t bus,
			     struct resource *erom_res, bus_size_t erom_offset);

struct bcma_sport_list	*bcma_corecfg_get_port_list(struct bcma_corecfg *cfg,
			     bhnd_port_type type);

struct bcma_devinfo	*bcma_alloc_dinfo(device_t bus);
int			 bcma_init_dinfo(device_t bus,
			     struct bcma_devinfo *dinfo,
			     struct bcma_corecfg *corecfg);
void			 bcma_free_dinfo(device_t bus,
			     struct bcma_devinfo *dinfo);

struct bcma_corecfg	*bcma_alloc_corecfg(u_int core_index, int core_unit,
			     uint16_t vendor, uint16_t device, uint8_t hwrev);
void			 bcma_free_corecfg(struct bcma_corecfg *corecfg);

struct bcma_sport	*bcma_alloc_sport(bcma_pid_t port_num, bhnd_port_type port_type);
void			 bcma_free_sport(struct bcma_sport *sport);

/** BCMA master port descriptor */
struct bcma_mport {
	bcma_pid_t	mp_num;		/**< AXI port identifier (bus-unique) */
	bcma_pid_t	mp_vid;		/**< AXI master virtual ID (core-unique) */
	STAILQ_ENTRY(bcma_mport) mp_link;
};

/** BCMA memory region descriptor */
struct bcma_map {
	bcma_rmid_t	m_region_num;	/**< region identifier (port-unique). */
	bhnd_addr_t	m_base;		/**< base address */
	bhnd_size_t	m_size;		/**< size */
	int		m_rid;		/**< bus resource id, or -1. */

	STAILQ_ENTRY(bcma_map) m_link;
};

/** BCMA slave port descriptor */
struct bcma_sport {
	bcma_pid_t	sp_num;		/**< slave port number (core-unique) */
	bhnd_port_type	sp_type;	/**< port type */

	u_long		sp_num_maps;	/**< number of regions mapped to this port */
	STAILQ_HEAD(, bcma_map) sp_maps;
	STAILQ_ENTRY(bcma_sport) sp_link;
};

STAILQ_HEAD(bcma_mport_list, bcma_mport);
STAILQ_HEAD(bcma_sport_list, bcma_sport);

/** BCMA IP core/block configuration */
struct bcma_corecfg {
	struct bhnd_core_info	core_info;	/**< standard core info */

	u_long		num_master_ports;	/**< number of master port descriptors. */
	struct bcma_mport_list	master_ports;	/**< master port descriptors */

	u_long		num_dev_ports;		/**< number of device slave port descriptors. */
	struct bcma_sport_list	dev_ports;	/**< device port descriptors */
	
	u_long		num_bridge_ports;	/**< number of bridge slave port descriptors. */
	struct bcma_sport_list	bridge_ports;	/**< bridge port descriptors */
	
	u_long		num_wrapper_ports;	/**< number of wrapper slave port descriptors. */	
	struct bcma_sport_list	wrapper_ports;	/**< wrapper port descriptors */	
};

/**
 * BCMA per-device info
 */
struct bcma_devinfo {
	struct bhnd_devinfo	 bhnd_dinfo;	/**< superclass device info. */

	struct resource_list	 resources;	/**< Slave port memory regions. */
	struct bcma_corecfg	*corecfg;	/**< IP core/block config */

	struct bhnd_resource	*res_agent;	/**< Agent (wrapper) resource, or NULL. Not
						  *  all bcma(4) cores have or require an agent. */
	int			 rid_agent;	/**< Agent resource ID, or -1 */
};


/** BMCA per-instance state */
struct bcma_softc {
	struct bhnd_softc	bhnd_sc;	/**< bhnd state */
	device_t		hostb_dev;	/**< host bridge core, or NULL */
};

#endif /* _BCMA_BCMAVAR_H_ */
