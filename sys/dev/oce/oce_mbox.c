/*-
 * Copyright (C) 2012 Emulex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Emulex Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information:
 * freebsd-drivers@emulex.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */



/* $FreeBSD$ */


#include "oce_if.h"


/**
 * @brief Reset (firmware) common function
 * @param sc		software handle to the device
 * @returns		0 on success, ETIMEDOUT on failure
 */
int
oce_reset_fun(POCE_SOFTC sc)
{
	struct oce_mbx *mbx;
	struct oce_bmbx *mb;
	struct ioctl_common_function_reset *fwcmd;
	int rc = 0;

	if (sc->flags & OCE_FLAGS_FUNCRESET_RQD) {
		mb = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
		mbx = &mb->mbx;
		bzero(mbx, sizeof(struct oce_mbx));

		fwcmd = (struct ioctl_common_function_reset *)&mbx->payload;
		mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
					MBX_SUBSYSTEM_COMMON,
					OPCODE_COMMON_FUNCTION_RESET,
					10,	/* MBX_TIMEOUT_SEC */
					sizeof(struct
					    ioctl_common_function_reset),
					OCE_MBX_VER_V0);

		mbx->u0.s.embedded = 1;
		mbx->payload_length =
		    sizeof(struct ioctl_common_function_reset);

		rc = oce_mbox_dispatch(sc, 2);
	}

	return rc;
}


/**
 * @brief  		This funtions tells firmware we are
 *			done with commands.
 * @param sc            software handle to the device
 * @returns             0 on success, ETIMEDOUT on failure
 */
int
oce_fw_clean(POCE_SOFTC sc)
{
	struct oce_bmbx *mbx;
	uint8_t *ptr;
	int ret = 0;

	mbx = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
	ptr = (uint8_t *) &mbx->mbx;

	/* Endian Signature */
	*ptr++ = 0xff;
	*ptr++ = 0xaa;
	*ptr++ = 0xbb;
	*ptr++ = 0xff;
	*ptr++ = 0xff;
	*ptr++ = 0xcc;
	*ptr++ = 0xdd;
	*ptr = 0xff;

	ret = oce_mbox_dispatch(sc, 2);
	
	return ret;
}


/**
 * @brief Mailbox wait
 * @param sc		software handle to the device
 * @param tmo_sec	timeout in seconds
 */
static int
oce_mbox_wait(POCE_SOFTC sc, uint32_t tmo_sec)
{
	tmo_sec *= 10000;
	pd_mpu_mbox_db_t mbox_db;

	for (;;) {
		if (tmo_sec != 0) {
			if (--tmo_sec == 0)
				break;
		}

		mbox_db.dw0 = OCE_READ_REG32(sc, db, PD_MPU_MBOX_DB);

		if (mbox_db.bits.ready)
			return 0;

		DELAY(100);
	}

	device_printf(sc->dev, "Mailbox timed out\n");

	return ETIMEDOUT;
}



/**
 * @brief Mailbox dispatch
 * @param sc		software handle to the device
 * @param tmo_sec	timeout in seconds
 */
int
oce_mbox_dispatch(POCE_SOFTC sc, uint32_t tmo_sec)
{
	pd_mpu_mbox_db_t mbox_db;
	uint32_t pa;
	int rc;

	oce_dma_sync(&sc->bsmbx, BUS_DMASYNC_PREWRITE);
	pa = (uint32_t) ((uint64_t) sc->bsmbx.paddr >> 34);
	bzero(&mbox_db, sizeof(pd_mpu_mbox_db_t));
	mbox_db.bits.ready = 0;
	mbox_db.bits.hi = 1;
	mbox_db.bits.address = pa;

	rc = oce_mbox_wait(sc, tmo_sec);
	if (rc == 0) {
		OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

		pa = (uint32_t) ((uint64_t) sc->bsmbx.paddr >> 4) & 0x3fffffff;
		mbox_db.bits.ready = 0;
		mbox_db.bits.hi = 0;
		mbox_db.bits.address = pa;

		rc = oce_mbox_wait(sc, tmo_sec);

		if (rc == 0) {
			OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

			rc = oce_mbox_wait(sc, tmo_sec);

			oce_dma_sync(&sc->bsmbx, BUS_DMASYNC_POSTWRITE);
		}
	}

	return rc;
}



/**
 * @brief 		Mailbox common request header initialization
 * @param hdr		mailbox header
 * @param dom		domain
 * @param port		port
 * @param subsys	subsystem
 * @param opcode	opcode
 * @param timeout	timeout
 * @param pyld_len	payload length
 */
void
mbx_common_req_hdr_init(struct mbx_hdr *hdr,
			uint8_t dom, uint8_t port,
			uint8_t subsys, uint8_t opcode,
			uint32_t timeout, uint32_t pyld_len,
			uint8_t version)
{
	hdr->u0.req.opcode = opcode;
	hdr->u0.req.subsystem = subsys;
	hdr->u0.req.port_number = port;
	hdr->u0.req.domain = dom;

	hdr->u0.req.timeout = timeout;
	hdr->u0.req.request_length = pyld_len - sizeof(struct mbx_hdr);
	hdr->u0.req.version = version;
}



/**
 * @brief Function to initialize the hw with host endian information
 * @param sc		software handle to the device
 * @returns		0 on success, ETIMEDOUT on failure
 */
int
oce_mbox_init(POCE_SOFTC sc)
{
	struct oce_bmbx *mbx;
	uint8_t *ptr;
	int ret = 0;

	if (sc->flags & OCE_FLAGS_MBOX_ENDIAN_RQD) {
		mbx = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
		ptr = (uint8_t *) &mbx->mbx;

		/* Endian Signature */
		*ptr++ = 0xff;
		*ptr++ = 0x12;
		*ptr++ = 0x34;
		*ptr++ = 0xff;
		*ptr++ = 0xff;
		*ptr++ = 0x56;
		*ptr++ = 0x78;
		*ptr = 0xff;

		ret = oce_mbox_dispatch(sc, 0);
	}
	
	return ret;
}


/**
 * @brief 		Function to get the firmware version
 * @param sc		software handle to the device
 * @returns		0 on success, EIO on failure
 */
int
oce_get_fw_version(POCE_SOFTC sc)
{
	struct oce_mbx mbx;
	struct mbx_get_common_fw_version *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_get_common_fw_version *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_GET_FW_VERSION,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_common_fw_version),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_get_common_fw_version);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	bcopy(fwcmd->params.rsp.fw_ver_str, sc->fw_version, 32);
	
	return 0;
}


/**
 * @brief		Function to post a MBX to the mbox
 * @param sc		software handle to the device
 * @param mbx 		pointer to the MBX to send
 * @param mbxctx	pointer to the mbx context structure
 * @returns		0 on success, error on failure
 */
int
oce_mbox_post(POCE_SOFTC sc, struct oce_mbx *mbx, struct oce_mbx_ctx *mbxctx)
{
	struct oce_mbx *mb_mbx = NULL;
	struct oce_mq_cqe *mb_cqe = NULL;
	struct oce_bmbx *mb = NULL;
	int rc = 0;
	uint32_t tmo = 0;
	uint32_t cstatus = 0;
	uint32_t xstatus = 0;

	LOCK(&sc->bmbx_lock);

	mb = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
	mb_mbx = &mb->mbx;

	/* get the tmo */
	tmo = mbx->tag[0];
	mbx->tag[0] = 0;

	/* copy mbx into mbox */
	bcopy(mbx, mb_mbx, sizeof(struct oce_mbx));

	/* now dispatch */
	rc = oce_mbox_dispatch(sc, tmo);
	if (rc == 0) {
		/*
		 * the command completed successfully. Now get the
		 * completion queue entry
		 */
		mb_cqe = &mb->cqe;
		DW_SWAP(u32ptr(&mb_cqe->u0.dw[0]), sizeof(struct oce_mq_cqe));
	
		/* copy mbox mbx back */
		bcopy(mb_mbx, mbx, sizeof(struct oce_mbx));

		/* pick up the mailbox status */
		cstatus = mb_cqe->u0.s.completion_status;
		xstatus = mb_cqe->u0.s.extended_status;

		/*
		 * store the mbx context in the cqe tag section so that
		 * the upper layer handling the cqe can associate the mbx
		 * with the response
		 */
		if (cstatus == 0 && mbxctx) {
			/* save context */
			mbxctx->mbx = mb_mbx;
			bcopy(&mbxctx, mb_cqe->u0.s.mq_tag,
				sizeof(struct oce_mbx_ctx *));
		}
	}

	UNLOCK(&sc->bmbx_lock);

	return rc;
}

/**
 * @brief Function to read the mac address associated with an interface
 * @param sc		software handle to the device
 * @param if_id 	interface id to read the address from
 * @param perm 		set to 1 if reading the factory mac address.
 *			In this case if_id is ignored
 * @param type 		type of the mac address, whether network or storage
 * @param[out] mac 	[OUTPUT] pointer to a buffer containing the
 *			mac address when the command succeeds.
 * @returns		0 on success, EIO on failure
 */
int
oce_read_mac_addr(POCE_SOFTC sc, uint32_t if_id,
		uint8_t perm, uint8_t type, struct mac_address_format *mac)
{
	struct oce_mbx mbx;
	struct mbx_query_common_iface_mac *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_query_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_query_common_iface_mac),
				OCE_MBX_VER_V0);

	fwcmd->params.req.permanent = perm;
	if (!perm)
		fwcmd->params.req.if_id = (uint16_t) if_id;
	else
		fwcmd->params.req.if_id = 0;

	fwcmd->params.req.type = type;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_query_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	/* copy the mac addres in the output parameter */
	mac->size_of_struct = fwcmd->params.rsp.mac.size_of_struct;
	bcopy(&fwcmd->params.rsp.mac.mac_addr[0], &mac->mac_addr[0],
		mac->size_of_struct);

	return 0;
}

/**
 * @brief Function to query the fw attributes from the hw
 * @param sc		software handle to the device
 * @returns		0 on success, EIO on failure
 */
int 
oce_get_fw_config(POCE_SOFTC sc)
{
	struct oce_mbx mbx;
	struct mbx_common_query_fw_config *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_query_fw_config *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_FIRMWARE_CONFIG,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_query_fw_config),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_query_fw_config);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	DW_SWAP(u32ptr(fwcmd), sizeof(struct mbx_common_query_fw_config));

	sc->config_number = fwcmd->params.rsp.config_number;
	sc->asic_revision = fwcmd->params.rsp.asic_revision;
	sc->port_id	  = fwcmd->params.rsp.port_id;
	sc->function_mode = fwcmd->params.rsp.function_mode;
	sc->function_caps = fwcmd->params.rsp.function_caps;

	if (fwcmd->params.rsp.ulp[0].ulp_mode & ULP_NIC_MODE) {
		sc->max_tx_rings = fwcmd->params.rsp.ulp[0].nic_wq_tot;
		sc->max_rx_rings = fwcmd->params.rsp.ulp[0].lro_rqid_tot;
	} else {
		sc->max_tx_rings = fwcmd->params.rsp.ulp[1].nic_wq_tot;
		sc->max_rx_rings = fwcmd->params.rsp.ulp[1].lro_rqid_tot;
	}
	
	return 0;

}

/**
 *
 * @brief function to create a device interface 
 * @param sc		software handle to the device
 * @param cap_flags	capability flags
 * @param en_flags	enable capability flags
 * @param vlan_tag	optional vlan tag to associate with the if
 * @param mac_addr	pointer to a buffer containing the mac address
 * @param[out] if_id	[OUTPUT] pointer to an integer to hold the ID of the
 interface created
 * @returns		0 on success, EIO on failure
 */
int
oce_if_create(POCE_SOFTC sc,
		uint32_t cap_flags,
		uint32_t en_flags,
		uint16_t vlan_tag,
		uint8_t *mac_addr,
		uint32_t *if_id)
{
	struct oce_mbx mbx;
	struct mbx_create_common_iface *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_iface *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_IFACE,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_iface),
				OCE_MBX_VER_V0);
	DW_SWAP(u32ptr(&fwcmd->hdr), sizeof(struct mbx_hdr));

	fwcmd->params.req.version = 0;
	fwcmd->params.req.cap_flags = LE_32(cap_flags);
	fwcmd->params.req.enable_flags = LE_32(en_flags);
	if (mac_addr != NULL) {
		bcopy(mac_addr, &fwcmd->params.req.mac_addr[0], 6);
		fwcmd->params.req.vlan_tag.u0.normal.vtag = LE_16(vlan_tag);
		fwcmd->params.req.mac_invalid = 0;
	} else {
		fwcmd->params.req.mac_invalid = 1;
	}

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_iface);
	DW_SWAP(u32ptr(&mbx), OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		return rc;

	*if_id = LE_32(fwcmd->params.rsp.if_id);

	if (mac_addr != NULL)
		sc->pmac_id = LE_32(fwcmd->params.rsp.pmac_id);

	return 0;
}

/**
 * @brief		Function to delete an interface
 * @param sc 		software handle to the device
 * @param if_id		ID of the interface to delete
 * @returns		0 on success, EIO on failure
 */
int
oce_if_del(POCE_SOFTC sc, uint32_t if_id)
{
	struct oce_mbx mbx;
	struct mbx_destroy_common_iface *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_destroy_common_iface *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_DESTROY_IFACE,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_destroy_common_iface),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = if_id;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_destroy_common_iface);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}

/**
 * @brief Function to send the mbx command to configure vlan
 * @param sc 		software handle to the device
 * @param if_id 	interface identifier index
 * @param vtag_arr	array of vlan tags
 * @param vtag_cnt	number of elements in array
 * @param untagged	boolean TRUE/FLASE
 * @param enable_promisc flag to enable/disable VLAN promiscuous mode
 * @returns		0 on success, EIO on failure
 */
int
oce_config_vlan(POCE_SOFTC sc,
		uint32_t if_id,
		struct normal_vlan *vtag_arr,
		uint8_t vtag_cnt, uint32_t untagged, uint32_t enable_promisc)
{
	struct oce_mbx mbx;
	struct mbx_common_config_vlan *fwcmd;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = (struct mbx_common_config_vlan *)&mbx.payload;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CONFIG_IFACE_VLAN,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_config_vlan),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = (uint8_t) if_id;
	fwcmd->params.req.promisc = (uint8_t) enable_promisc;
	fwcmd->params.req.untagged = (uint8_t) untagged;
	fwcmd->params.req.num_vlans = vtag_cnt;

	if (!enable_promisc) {
		bcopy(vtag_arr, fwcmd->params.req.tags.normal_vlans,
			vtag_cnt * sizeof(struct normal_vlan));
	}
	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_config_vlan);
	DW_SWAP(u32ptr(&mbx), (OCE_BMBX_RHDR_SZ + mbx.payload_length));

	rc = oce_mbox_post(sc, &mbx, NULL);

	return rc;

}

/**
 * @brief Function to set flow control capability in the hardware
 * @param sc 		software handle to the device
 * @param flow_control	flow control flags to set
 * @returns		0 on success, EIO on failure
 */
int
oce_set_flow_control(POCE_SOFTC sc, uint32_t flow_control)
{
	struct oce_mbx mbx;
	struct mbx_common_get_set_flow_control *fwcmd =
		(struct mbx_common_get_set_flow_control *)&mbx.payload;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_FLOW_CONTROL,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_get_set_flow_control),
				OCE_MBX_VER_V0);

	if (flow_control & OCE_FC_TX)
		fwcmd->tx_flow_control = 1;

	if (flow_control & OCE_FC_RX)
		fwcmd->rx_flow_control = 1;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_get_set_flow_control);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	
	return rc;
}

/**
 * @brief Initialize the RSS CPU indirection table
 *
 * The table is used to choose the queue to place the incomming packets.
 * Incomming packets are hashed.  The lowest bits in the hash result
 * are used as the index into the CPU indirection table.
 * Each entry in the table contains the RSS CPU-ID returned by the NIC
 * create.  Based on the CPU ID, the receive completion is routed to
 * the corresponding RSS CQs.  (Non-RSS packets are always completed
 * on the default (0) CQ).
 *
 * @param sc 		software handle to the device
 * @param *fwcmd	pointer to the rss mbox command
 * @returns		none
 */
static int
oce_rss_itbl_init(POCE_SOFTC sc, struct mbx_config_nic_rss *fwcmd)
{
	int i = 0, j = 0, rc = 0;
	uint8_t *tbl = fwcmd->params.req.cputable;


	for (j = 0; j < sc->nrqs; j++) {
		if (sc->rq[j]->cfg.is_rss_queue) {
			tbl[i] = sc->rq[j]->rss_cpuid;
			i = i + 1;
		}
	}
	if (i == 0) {
		device_printf(sc->dev, "error: Invalid number of RSS RQ's\n");
		rc = ENXIO;

	} 
	
	/* fill log2 value indicating the size of the CPU table */
	if (rc == 0)
		fwcmd->params.req.cpu_tbl_sz_log2 = LE_16(OCE_LOG2(i));

	return rc;
}

/**
 * @brief Function to set flow control capability in the hardware
 * @param sc 		software handle to the device
 * @param if_id 	interface id to read the address from
 * @param enable_rss	0=disable, RSS_ENABLE_xxx flags otherwise
 * @returns		0 on success, EIO on failure
 */
int
oce_config_nic_rss(POCE_SOFTC sc, uint32_t if_id, uint16_t enable_rss)
{
	int rc;
	struct oce_mbx mbx;
	struct mbx_config_nic_rss *fwcmd =
				(struct mbx_config_nic_rss *)&mbx.payload;

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_CONFIG_RSS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_config_nic_rss),
				OCE_MBX_VER_V0);
	if (enable_rss)
		fwcmd->params.req.enable_rss = (RSS_ENABLE_IPV4 |
						RSS_ENABLE_TCP_IPV4 |
						RSS_ENABLE_IPV6 |
						RSS_ENABLE_TCP_IPV6);
	fwcmd->params.req.flush = OCE_FLUSH;
	fwcmd->params.req.if_id = LE_32(if_id);

	srandom(arc4random());	/* random entropy seed */
	read_random(fwcmd->params.req.hash, sizeof(fwcmd->params.req.hash));
	
	rc = oce_rss_itbl_init(sc, fwcmd);
	if (rc == 0) {
		mbx.u0.s.embedded = 1;
		mbx.payload_length = sizeof(struct mbx_config_nic_rss);
		DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

		rc = oce_mbox_post(sc, &mbx, NULL);

	}
	
	return rc;
}

/**
 * @brief 		RXF function to enable/disable device promiscuous mode
 * @param sc		software handle to the device
 * @param enable	enable/disable flag
 * @returns		0 on success, EIO on failure
 * @note
 *	The NIC_CONFIG_PROMISCUOUS command deprecated for Lancer.
 *	This function uses the COMMON_SET_IFACE_RX_FILTER command instead.
 */
int
oce_rxf_set_promiscuous(POCE_SOFTC sc, uint32_t enable)
{
	struct mbx_set_common_iface_rx_filter *fwcmd;
	int sz = sizeof(struct mbx_set_common_iface_rx_filter);
	iface_rx_filter_ctx_t *req;
	OCE_DMA_MEM sgl;
	int rc;

	/* allocate mbx payload's dma scatter/gather memory */
	rc = oce_dma_alloc(sc, sz, &sgl, 0);
	if (rc)
		return rc;

	fwcmd = OCE_DMAPTR(&sgl, struct mbx_set_common_iface_rx_filter);

	req =  &fwcmd->params.req;
	req->iface_flags_mask = MBX_RX_IFACE_FLAGS_PROMISCUOUS |
				MBX_RX_IFACE_FLAGS_VLAN_PROMISCUOUS;
	if (enable) {
		req->iface_flags = MBX_RX_IFACE_FLAGS_PROMISCUOUS |
				   MBX_RX_IFACE_FLAGS_VLAN_PROMISCUOUS;
	}
	req->if_id = sc->if_id;

	rc = oce_set_common_iface_rx_filter(sc, &sgl);
	oce_dma_free(sc, &sgl);
	
	return rc;
}


/**
 * @brief 			Function modify and select rx filter options
 * @param sc			software handle to the device
 * @param sgl			scatter/gather request/response
 * @returns			0 on success, error code on failure
 */
int
oce_set_common_iface_rx_filter(POCE_SOFTC sc, POCE_DMA_MEM sgl)
{
	struct oce_mbx mbx;
	int mbx_sz = sizeof(struct mbx_set_common_iface_rx_filter);
	struct mbx_set_common_iface_rx_filter *fwcmd;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(sgl, struct mbx_set_common_iface_rx_filter);

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_IFACE_RX_FILTER,
				MBX_TIMEOUT_SEC,
				mbx_sz,
				OCE_MBX_VER_V0);

	oce_dma_sync(sgl, BUS_DMASYNC_PREWRITE);
	mbx.u0.s.embedded = 0;
	mbx.u0.s.sge_count = 1;
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(sgl->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(sgl->paddr);
	mbx.payload.u0.u1.sgl[0].length = mbx_sz;
	mbx.payload_length = mbx_sz;
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}

/**
 * @brief Function to query the link status from the hardware
 * @param sc 		software handle to the device
 * @param[out] link	pointer to the structure returning link attributes
 * @returns		0 on success, EIO on failure
 */
int
oce_get_link_status(POCE_SOFTC sc, struct link_status *link)
{
	struct oce_mbx mbx;
	struct mbx_query_common_link_config *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_query_common_link_config *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_LINK_CONFIG,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_query_common_link_config),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_query_common_link_config);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	if (rc) {
		device_printf(sc->dev, "Could not get link speed: %d\n", rc);
	} else {
		/* interpret response */
		bcopy(&fwcmd->params.rsp, link, sizeof(struct link_status));
		link->logical_link_status = LE_32(link->logical_link_status);
		link->qos_link_speed = LE_16(link->qos_link_speed);
	}

	return rc;
}



int
oce_mbox_get_nic_stats_v0(POCE_SOFTC sc, POCE_DMA_MEM pstats_dma_mem)
{
	struct oce_mbx mbx;
	struct mbx_get_nic_stats_v0 *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_nic_stats_v0);
	bzero(fwcmd, sizeof(struct mbx_get_nic_stats_v0));
	
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_GET_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_nic_stats_v0),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 0;
	mbx.u0.s.sge_count = 1;

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);

	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_nic_stats_v0);
	
	mbx.payload_length = sizeof(struct mbx_get_nic_stats_v0);

	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);
	
	rc = oce_mbox_post(sc, &mbx, NULL);

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);

	if (rc) {
		device_printf(sc->dev, 
			"Could not get nic statistics: %d\n", rc);
	}
	
	return rc; 
}



/**
 * @brief Function to get NIC statistics
 * @param sc 		software handle to the device
 * @param *stats	pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 * @note		command depricated in Lancer
 */
int
oce_mbox_get_nic_stats(POCE_SOFTC sc, POCE_DMA_MEM pstats_dma_mem)
{
	struct oce_mbx mbx;
	struct mbx_get_nic_stats *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_nic_stats);
	bzero(fwcmd, sizeof(struct mbx_get_nic_stats));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_GET_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_nic_stats),
				OCE_MBX_VER_V1);


	mbx.u0.s.embedded = 0;  /* stats too large for embedded mbx rsp */
	mbx.u0.s.sge_count = 1; /* using scatter gather instead */

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_nic_stats);

	mbx.payload_length = sizeof(struct mbx_get_nic_stats);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);
	if (rc) {
		device_printf(sc->dev, 
			"Could not get nic statistics: %d\n", rc);
	}
	return rc; 
}


/**
 * @brief Function to get pport (physical port) statistics
 * @param sc 		software handle to the device
 * @param *stats	pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 */
int
oce_mbox_get_pport_stats(POCE_SOFTC sc, POCE_DMA_MEM pstats_dma_mem,
				uint32_t reset_stats)
{
	struct oce_mbx mbx;
	struct mbx_get_pport_stats *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_pport_stats);
	bzero(fwcmd, sizeof(struct mbx_get_pport_stats));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_GET_PPORT_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_pport_stats),
				OCE_MBX_VER_V0);

	fwcmd->params.req.reset_stats = reset_stats;
	fwcmd->params.req.port_number = sc->if_id;
	
	mbx.u0.s.embedded = 0;	/* stats too large for embedded mbx rsp */
	mbx.u0.s.sge_count = 1; /* using scatter gather instead */

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_pport_stats);

	mbx.payload_length = sizeof(struct mbx_get_pport_stats);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);

	if (rc != 0) {
		device_printf(sc->dev,
			  "Could not get physical port statistics: %d\n", rc);
	}

	return rc;
}


/**
 * @brief Function to get vport (virtual port) statistics
 * @param sc 		software handle to the device
 * @param *stats	pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 */
int
oce_mbox_get_vport_stats(POCE_SOFTC sc, POCE_DMA_MEM pstats_dma_mem,
				uint32_t req_size, uint32_t reset_stats)
{
	struct oce_mbx mbx;
	struct mbx_get_vport_stats *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_vport_stats);
	bzero(fwcmd, sizeof(struct mbx_get_vport_stats));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_GET_VPORT_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_vport_stats),
				OCE_MBX_VER_V0);

	fwcmd->params.req.reset_stats = reset_stats;
	fwcmd->params.req.vport_number = sc->if_id;
	
	mbx.u0.s.embedded = 0;	/* stats too large for embedded mbx rsp */
	mbx.u0.s.sge_count = 1; /* using scatter gather instead */

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_vport_stats);

	mbx.payload_length = sizeof(struct mbx_get_vport_stats);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);

	if (rc != 0) {
		device_printf(sc->dev,
			  "Could not get physical port statistics: %d\n", rc);
	}

	return rc;
}


/**
 * @brief               Function to update the muticast filter with 
 *                      values in dma_mem 
 * @param sc            software handle to the device
 * @param dma_mem       pointer to dma memory region
 * @returns             0 on success, EIO on failure
 */
int
oce_update_multicast(POCE_SOFTC sc, POCE_DMA_MEM pdma_mem)
{
	struct oce_mbx mbx;
	struct oce_mq_sge *sgl;
	struct mbx_set_common_iface_multicast *req = NULL;
	int rc = 0;

	req = OCE_DMAPTR(pdma_mem, struct mbx_set_common_iface_multicast);
	mbx_common_req_hdr_init(&req->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_IFACE_MULTICAST,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_set_common_iface_multicast),
				OCE_MBX_VER_V0);

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx.u0.s.embedded = 0; /*Non embeded*/
	mbx.payload_length = sizeof(struct mbx_set_common_iface_multicast);
	mbx.u0.s.sge_count = 1;
	sgl = &mbx.payload.u0.u1.sgl[0];
	sgl->pa_hi = htole32(upper_32_bits(pdma_mem->paddr));
	sgl->pa_lo = htole32((pdma_mem->paddr) & 0xFFFFFFFF);
	sgl->length = htole32(mbx.payload_length);

	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	
	return rc;
}


/**
 * @brief               Function to send passthrough Ioctls
 * @param sc            software handle to the device
 * @param dma_mem       pointer to dma memory region
 * @param req_size      size of dma_mem
 * @returns             0 on success, EIO on failure
 */
int
oce_pass_through_mbox(POCE_SOFTC sc, POCE_DMA_MEM dma_mem, uint32_t req_size)
{
	struct oce_mbx mbx;
	struct oce_mq_sge *sgl;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx.u0.s.embedded  = 0; /*Non embeded*/
	mbx.payload_length = req_size;
	mbx.u0.s.sge_count = 1;
	sgl = &mbx.payload.u0.u1.sgl[0];
	sgl->pa_hi = htole32(upper_32_bits(dma_mem->paddr));
	sgl->pa_lo = htole32((dma_mem->paddr) & 0xFFFFFFFF);
	sgl->length = htole32(req_size);

	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}


int
oce_mbox_macaddr_add(POCE_SOFTC sc, uint8_t *mac_addr,
		 uint32_t if_id, uint32_t *pmac_id)
{
	struct oce_mbx mbx;
	struct mbx_add_common_iface_mac *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_add_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_ADD_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_add_common_iface_mac),
				OCE_MBX_VER_V0);
				
	fwcmd->params.req.if_id = (uint16_t) if_id;
	bcopy(mac_addr, fwcmd->params.req.mac_address, 6);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_add_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		return rc;

	*pmac_id = fwcmd->params.rsp.pmac_id;

	return rc;
}


int
oce_mbox_macaddr_del(POCE_SOFTC sc, uint32_t if_id, uint32_t pmac_id)
{
	struct oce_mbx mbx;
	struct mbx_del_common_iface_mac *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_del_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_DEL_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_del_common_iface_mac),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = (uint16_t)if_id;
	fwcmd->params.req.pmac_id = pmac_id;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_del_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}



int
oce_mbox_check_native_mode(POCE_SOFTC sc)
{
	struct oce_mbx mbx;
	struct mbx_common_set_function_cap *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_set_function_cap *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_FUNCTIONAL_CAPS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_set_function_cap),
				OCE_MBX_VER_V0);

	fwcmd->params.req.valid_capability_flags = CAP_SW_TIMESTAMPS | 
							CAP_BE3_NATIVE_ERX_API;

	fwcmd->params.req.capability_flags = CAP_BE3_NATIVE_ERX_API;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_set_function_cap);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	//if (rc != 0)		This can fail in legacy mode. So skip
	//	FN_LEAVE(rc);

	sc->be3_native = fwcmd->params.rsp.capability_flags
			& CAP_BE3_NATIVE_ERX_API;

	return 0;
}



int
oce_mbox_cmd_set_loopback(POCE_SOFTC sc, uint8_t port_num,
		uint8_t loopback_type, uint8_t enable)
{
	struct oce_mbx mbx;
	struct mbx_lowlevel_set_loopback_mode *fwcmd;
	int rc = 0;


	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_lowlevel_set_loopback_mode *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_LOWLEVEL,
				OPCODE_LOWLEVEL_SET_LOOPBACK_MODE,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_lowlevel_set_loopback_mode),
				OCE_MBX_VER_V0);

	fwcmd->params.req.src_port = port_num;
	fwcmd->params.req.dest_port = port_num;
	fwcmd->params.req.loopback_type = loopback_type;
	fwcmd->params.req.loopback_state = enable;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_lowlevel_set_loopback_mode);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	return rc;

}

int
oce_mbox_cmd_test_loopback(POCE_SOFTC sc, uint32_t port_num,
	uint32_t loopback_type, uint32_t pkt_size, uint32_t num_pkts,
	uint64_t pattern)
{
	
	struct oce_mbx mbx;
	struct mbx_lowlevel_test_loopback_mode *fwcmd;
	int rc = 0;


	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_lowlevel_test_loopback_mode *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_LOWLEVEL,
				OPCODE_LOWLEVEL_TEST_LOOPBACK,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_lowlevel_test_loopback_mode),
				OCE_MBX_VER_V0);

	fwcmd->params.req.pattern = pattern;
	fwcmd->params.req.src_port = port_num;
	fwcmd->params.req.dest_port = port_num;
	fwcmd->params.req.pkt_size = pkt_size;
	fwcmd->params.req.num_pkts = num_pkts;
	fwcmd->params.req.loopback_type = loopback_type;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_lowlevel_test_loopback_mode);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		return rc;
	
	return(fwcmd->params.rsp.status);
}

int
oce_mbox_write_flashrom(POCE_SOFTC sc, uint32_t optype,uint32_t opcode,
				POCE_DMA_MEM pdma_mem, uint32_t num_bytes)
{

	struct oce_mbx mbx;
	struct oce_mq_sge *sgl = NULL;
	struct mbx_common_read_write_flashrom *fwcmd = NULL;
	int rc = 0, payload_len = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(pdma_mem, struct mbx_common_read_write_flashrom);
	payload_len = sizeof(struct mbx_common_read_write_flashrom) + 32*1024;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_WRITE_FLASHROM,
				LONG_TIMEOUT,
				payload_len,
				OCE_MBX_VER_V0);

	fwcmd->flash_op_type = optype;
	fwcmd->flash_op_code = opcode;
	fwcmd->data_buffer_size = num_bytes;

	mbx.u0.s.embedded  = 0; /*Non embeded*/
	mbx.payload_length = payload_len;
	mbx.u0.s.sge_count = 1;

	sgl = &mbx.payload.u0.u1.sgl[0];
	sgl->pa_hi = upper_32_bits(pdma_mem->paddr);
	sgl->pa_lo = pdma_mem->paddr & 0xFFFFFFFF;
	sgl->length = payload_len;

	/* post the command */
	if (rc) {
		device_printf(sc->dev, "Write FlashROM mbox post failed\n");
	} else {
		rc = fwcmd->hdr.u0.rsp.status;
	}
	
	return rc;

}

int
oce_mbox_get_flashrom_crc(POCE_SOFTC sc, uint8_t *flash_crc,
				uint32_t offset, uint32_t optype)
{

	int rc = 0, payload_len = 0;
	struct oce_mbx mbx;
	struct mbx_common_read_write_flashrom *fwcmd;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_read_write_flashrom *)&mbx.payload;

	/* Firmware requires extra 4 bytes with this ioctl. Since there
	   is enough room in the mbx payload it should be good enough
	   Reference: Bug 14853
	*/
	payload_len = sizeof(struct mbx_common_read_write_flashrom) + 4;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_READ_FLASHROM,
				MBX_TIMEOUT_SEC,
				payload_len,
				OCE_MBX_VER_V0);

	fwcmd->flash_op_type = optype;
	fwcmd->flash_op_code = FLASHROM_OPER_REPORT;
	fwcmd->data_offset = offset;
	fwcmd->data_buffer_size = 0x4;

	mbx.u0.s.embedded  = 1;
	mbx.payload_length = payload_len;

	/* post the command */
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc) {
		device_printf(sc->dev, "Read FlashROM CRC mbox post failed\n");
	} else {
		bcopy(fwcmd->data_buffer, flash_crc, 4);
		rc = fwcmd->hdr.u0.rsp.status;
	}
	return rc;
}

int
oce_mbox_get_phy_info(POCE_SOFTC sc, struct oce_phy_info *phy_info)
{

	struct oce_mbx mbx;
	struct mbx_common_phy_info *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_phy_info *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_GET_PHY_CONFIG,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_phy_info),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_common_phy_info);

	/* now post the command */
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc) {
		device_printf(sc->dev, "Read PHY info  mbox post failed\n");
	} else {
		rc = fwcmd->hdr.u0.rsp.status;
		phy_info->phy_type = fwcmd->params.rsp.phy_info.phy_type;
		phy_info->interface_type =
				fwcmd->params.rsp.phy_info.interface_type;
		phy_info->auto_speeds_supported =
			fwcmd->params.rsp.phy_info.auto_speeds_supported;
		phy_info->fixed_speeds_supported =
			fwcmd->params.rsp.phy_info.fixed_speeds_supported;
		phy_info->misc_params =fwcmd->params.rsp.phy_info.misc_params;

	}
	return rc;

}


int
oce_mbox_lancer_write_flashrom(POCE_SOFTC sc, uint32_t data_size,
			uint32_t data_offset, POCE_DMA_MEM pdma_mem,
			uint32_t *written_data, uint32_t *additional_status)
{

	struct oce_mbx mbx;
	struct mbx_lancer_common_write_object *fwcmd = NULL;
	int rc = 0, payload_len = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	payload_len = sizeof(struct mbx_lancer_common_write_object);

	mbx.u0.s.embedded  = 1;/* Embedded */
	mbx.payload_length = payload_len;
	fwcmd = (struct mbx_lancer_common_write_object *)&mbx.payload;

	/* initialize the ioctl header */
	mbx_common_req_hdr_init(&fwcmd->params.req.hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_WRITE_OBJECT,
				LONG_TIMEOUT,
				payload_len,
				OCE_MBX_VER_V0);

	fwcmd->params.req.write_length = data_size;
	if (data_size == 0)
		fwcmd->params.req.eof = 1;
	else
		fwcmd->params.req.eof = 0;

	strcpy(fwcmd->params.req.object_name, "/prg");
	fwcmd->params.req.descriptor_count = 1;
	fwcmd->params.req.write_offset = data_offset;
	fwcmd->params.req.buffer_length = data_size;
	fwcmd->params.req.address_lower = pdma_mem->paddr & 0xFFFFFFFF;
	fwcmd->params.req.address_upper = upper_32_bits(pdma_mem->paddr);

	/* post the command */
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc) {
		device_printf(sc->dev,
			"Write Lancer FlashROM mbox post failed\n");
	} else {
		*written_data = fwcmd->params.rsp.actual_write_length;
		*additional_status = fwcmd->params.rsp.additional_status;
		rc = fwcmd->params.rsp.status;
	}
	return rc;

}



int
oce_mbox_create_rq(struct oce_rq *rq)
{

	struct oce_mbx mbx;
	struct mbx_create_nic_rq *fwcmd;
	POCE_SOFTC sc = rq->parent;
	int rc, num_pages = 0;

	if (rq->qstate == QCREATED)
		return 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_nic_rq *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_CREATE_RQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_nic_rq),
				OCE_MBX_VER_V0);

	/* oce_page_list will also prepare pages */
	num_pages = oce_page_list(rq->ring, &fwcmd->params.req.pages[0]);
	
	if (IS_XE201(sc)) {
		fwcmd->params.req.frag_size = rq->cfg.frag_size/2048;
		fwcmd->params.req.page_size = 1;
		fwcmd->hdr.u0.req.version = OCE_MBX_VER_V1;
	} else
		fwcmd->params.req.frag_size = OCE_LOG2(rq->cfg.frag_size);
	fwcmd->params.req.num_pages = num_pages;
	fwcmd->params.req.cq_id = rq->cq->cq_id;
	fwcmd->params.req.if_id = sc->if_id;
	fwcmd->params.req.max_frame_size = rq->cfg.mtu;
	fwcmd->params.req.is_rss_queue = rq->cfg.is_rss_queue;
	
	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_nic_rq);
	
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto error;
 
	rq->rq_id = fwcmd->params.rsp.rq_id;
	rq->rss_cpuid = fwcmd->params.rsp.rss_cpuid;

	return 0;
error:
	device_printf(sc->dev, "Mbox Create RQ failed\n");
	return rc;

}



int
oce_mbox_create_wq(struct oce_wq *wq)
{
	struct oce_mbx mbx;
	struct mbx_create_nic_wq *fwcmd;
	POCE_SOFTC sc = wq->parent;
	int rc = 0, version, num_pages;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_nic_wq *)&mbx.payload;
	if (IS_XE201(sc)) {
		version = OCE_MBX_VER_V1;
		fwcmd->params.req.if_id = sc->if_id;
	} else
		version = OCE_MBX_VER_V0;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				NIC_CREATE_WQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_nic_wq),
				version);

	num_pages = oce_page_list(wq->ring, &fwcmd->params.req.pages[0]);

	fwcmd->params.req.nic_wq_type = wq->cfg.wq_type;
	fwcmd->params.req.num_pages = num_pages;
	fwcmd->params.req.wq_size = OCE_LOG2(wq->cfg.q_len) + 1;
	fwcmd->params.req.cq_id = wq->cq->cq_id;
	fwcmd->params.req.ulp_num = 1;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_nic_wq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc) 
		goto error;

	wq->wq_id = LE_16(fwcmd->params.rsp.wq_id);

	return 0;
error:
	device_printf(sc->dev, "Mbox Create WQ failed\n");
	return rc;

}



int
oce_mbox_create_eq(struct oce_eq *eq)
{
	struct oce_mbx mbx;
	struct mbx_create_common_eq *fwcmd;
	POCE_SOFTC sc = eq->parent;
	int rc = 0;
	uint32_t num_pages;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_eq *)&mbx.payload;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_EQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_eq),
				OCE_MBX_VER_V0);

	num_pages = oce_page_list(eq->ring, &fwcmd->params.req.pages[0]);
	fwcmd->params.req.ctx.num_pages = num_pages;
	fwcmd->params.req.ctx.valid = 1;
	fwcmd->params.req.ctx.size = (eq->eq_cfg.item_size == 4) ? 0 : 1;
	fwcmd->params.req.ctx.count = OCE_LOG2(eq->eq_cfg.q_len / 256);
	fwcmd->params.req.ctx.armed = 0;
	fwcmd->params.req.ctx.delay_mult = eq->eq_cfg.cur_eqd;


	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_eq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto error;

	eq->eq_id = LE_16(fwcmd->params.rsp.eq_id);

	return 0;
error:
	device_printf(sc->dev, "Mbox Create EQ failed\n");
	return rc;
}



int
oce_mbox_cq_create(struct oce_cq *cq, uint32_t ncoalesce, uint32_t is_eventable)
{
	struct oce_mbx mbx;
	struct mbx_create_common_cq *fwcmd;
	POCE_SOFTC sc = cq->parent;
	uint8_t version;
	oce_cq_ctx_t *ctx;
	uint32_t num_pages, page_size;
	int rc = 0;

	
	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_cq *)&mbx.payload;

	if (IS_XE201(sc))
		version = OCE_MBX_VER_V2;
	else
		version = OCE_MBX_VER_V0;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_CQ,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_cq),
				version);

	ctx = &fwcmd->params.req.cq_ctx;

	num_pages = oce_page_list(cq->ring, &fwcmd->params.req.pages[0]);
	page_size =  1;  /* 1 for 4K */

	if (version == OCE_MBX_VER_V2) {
		ctx->v2.num_pages = LE_16(num_pages);
		ctx->v2.page_size = page_size;
		ctx->v2.eventable = is_eventable;
		ctx->v2.valid = 1;
		ctx->v2.count = OCE_LOG2(cq->cq_cfg.q_len / 256);
		ctx->v2.nodelay = cq->cq_cfg.nodelay;
		ctx->v2.coalesce_wm = ncoalesce;
		ctx->v2.armed = 0;
		ctx->v2.eq_id = cq->eq->eq_id;
		if (ctx->v2.count == 3) {
			if (cq->cq_cfg.q_len > (4*1024)-1)
				ctx->v2.cqe_count = (4*1024)-1;
			else
				ctx->v2.cqe_count = cq->cq_cfg.q_len;
		}
	} else {
		ctx->v0.num_pages = LE_16(num_pages);
		ctx->v0.eventable = is_eventable;
		ctx->v0.valid = 1;
		ctx->v0.count = OCE_LOG2(cq->cq_cfg.q_len / 256);
		ctx->v0.nodelay = cq->cq_cfg.nodelay;
		ctx->v0.coalesce_wm = ncoalesce;
		ctx->v0.armed = 0;
		ctx->v0.eq_id = cq->eq->eq_id;
	}

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_cq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto error;

	cq->cq_id = LE_16(fwcmd->params.rsp.cq_id);

	return 0;
error:
	device_printf(sc->dev, "Mbox Create CQ failed\n");
	return rc;

}
