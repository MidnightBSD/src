/*
 * Copyright (c) 2026 Albert Song <albb0920@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/firmware.h>

#include "aq_common.h"
#include "aq_dbg.h"
#include "aq_hostboot.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"

#define AQ_HOSTBOOT_LEGACY_MAC_IRAM_MAX_SIZE 0x60000u
#define AQ_HOSTBOOT_LEGACY_MAC_DRAM_MAX_SIZE 0x40000u

#define AQ_HOSTBOOT_LEGACY_RESET_SCRPAD_BASE 0x300u
#define AQ_HOSTBOOT_LEGACY_FW_STATUS_REG 0x340u

#define AQ_HOSTBOOT_LEGACY_RBL_ADDR_IDX 0u
#define AQ_HOSTBOOT_LEGACY_RBL_CMD_IDX 1u
#define AQ_HOSTBOOT_LEGACY_RBL_STATUS_IDX 2u

#define AQ_HOSTBOOT_LEGACY_RBL_EXEC 0x80000000u
#define AQ_HOSTBOOT_LEGACY_RBL_LAST_DWORD 0x40000000u
#define AQ_HOSTBOOT_LEGACY_RBL_COMPLETE 0x55555555u
#define AQ_HOSTBOOT_LEGACY_RBL_WRONG_ADDR 0x66666666u
#define AQ_HOSTBOOT_LEGACY_RBL_CHUNK_DONE 0xAAAAAAAau
#define AQ_HOSTBOOT_LEGACY_RBL_DONE 0xABBAu

#define AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_READY BIT(0)
#define AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_REQ BIT(9)
#define AQ_HOSTBOOT_LEGACY_FW_LOADER_MAC_BDP_REQ BIT(15)
#define AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_BDP_REQ BIT(16)
#define AQ_HOSTBOOT_LEGACY_CTL2_FW_LOADER_START 0x000000e0u

#define AQ_HOSTBOOT_LEGACY_MAC_MAX_CHUNK 0x20u
#define AQ_HOSTBOOT_LEGACY_PHY_MAX_CHUNK 0x10u
#define AQ_HOSTBOOT_LEGACY_BDP_MAX_CHUNK 0x10u
#define AQ_HOSTBOOT_LEGACY_UPLOAD_POLL_US 50u
#define AQ_HOSTBOOT_LEGACY_UPLOAD_TIMEOUT_US 250000u
#define AQ_HOSTBOOT_LEGACY_COMPLETE_POLL_US 1000u
#define AQ_HOSTBOOT_LEGACY_COMPLETE_TIMEOUT_US 1000000u
#define AQ_HOSTBOOT_LEGACY_STAGE_POLL_US 100u
#define AQ_HOSTBOOT_LEGACY_STAGE_TIMEOUT_US 1000000u

#define AQ_HOSTBOOT_LEGACY_MAC_IRAM_ADDR 0x1FC00000u
#define AQ_HOSTBOOT_LEGACY_MAC_DRAM_ADDR 0x1FB00000u
#define AQ_HOSTBOOT_LEGACY_PHY_IRAM_ADDR 0x40000000u
#define AQ_HOSTBOOT_LEGACY_PHY_DRAM_ADDR 0x3FFE0000u
#define AQ_HOSTBOOT_LEGACY_FW_BDP_FROM_HOST 0x1FB0FC08u

#define AQ_HOSTBOOT_LEGACY_MAC_IRAM_SKIP 0x4000u
#define AQ_HOSTBOOT_LEGACY_MAC_DRAM_SKIP 0xFC00u

struct aq_hostboot_legacy_header {
	uint32_t mac_iram_offset;
	uint32_t mac_iram_size;
	uint32_t mac_dram_offset;
	uint32_t mac_dram_size;
	uint32_t phy_iram_offset;
	uint32_t phy_iram_size;
	uint32_t phy_dram_offset;
	uint32_t phy_dram_size;
	uint32_t configuration_offset;
	uint32_t configuration_size;
	uint32_t conf_record_cnt;
	uint32_t mac_crc;
	uint32_t phy_crc;
	uint32_t conf_crc;
} __packed;

struct aq_hostboot_legacy_record {
	uint32_t sub_system_id;
	uint32_t mac_bdp_offset;
	uint32_t mac_bdp_size;
	uint32_t phy_bdp_offset;
	uint32_t phy_bdp_size;
} __packed;

struct aq_hostboot_legacy_image {
	const uint8_t *mac_iram;
	uint32_t mac_iram_size;
	const uint8_t *mac_dram;
	uint32_t mac_dram_size;
	const uint8_t *phy_iram;
	uint32_t phy_iram_size;
	const uint8_t *phy_dram;
	uint32_t phy_dram_size;
	const uint8_t *mac_bdp;
	uint32_t mac_bdp_size;
	const uint8_t *phy_bdp;
	uint32_t phy_bdp_size;
};

static uint16_t
aq_hostboot_legacy_crc_itu_t(uint16_t crc, const void *buffer, size_t len)
{
	const uint8_t *p;
	unsigned int bit;

	p = buffer;
	while (len-- != 0) {
		crc ^= (uint16_t)(*p++) << 8;
		for (bit = 0; bit < 8; bit++) {
			if ((crc & 0x8000u) != 0)
				crc = (uint16_t)((crc << 1) ^ 0x1021u);
			else
				crc <<= 1;
		}
	}

	return (crc);
}

static bool
aq_hostboot_legacy_range_valid(size_t total, uint32_t offset, uint32_t size)
{
	return (offset <= total && size <= total - offset);
}

static uint32_t
aq_hostboot_legacy_read_le32(const uint8_t *data, size_t size)
{
	uint32_t val = 0;

	if (size > sizeof(val))
		size = sizeof(val);

	memcpy(&val, data, size);
	return (le32toh(val));
}

static int
aq_hostboot_legacy_parse(struct aq_hw *hw, struct aq_hostboot_legacy_image *img)
{
	const struct firmware *fw;
	const struct aq_hostboot_legacy_header *hdr;
	const struct aq_hostboot_legacy_record *record;
	const uint8_t *data;
	size_t total;
	uint16_t crc;
	uint32_t selector;
	uint32_t conf_off;
	uint32_t conf_cnt;
	uint32_t i;

	fw = hw->hostboot_fw;
	if (fw == NULL)
		return (ENOENT);

	data = fw->data;
	total = fw->datasize;
	if (total < sizeof(*hdr))
		return (EINVAL);

	memset(img, 0, sizeof(*img));
	hdr = (const struct aq_hostboot_legacy_header *)data;

	img->mac_iram_size = le32toh(hdr->mac_iram_size);
	img->mac_dram_size = le32toh(hdr->mac_dram_size);
	img->phy_iram_size = le32toh(hdr->phy_iram_size);
	img->phy_dram_size = le32toh(hdr->phy_dram_size);
	conf_off = le32toh(hdr->configuration_offset);
	conf_cnt = le32toh(hdr->conf_record_cnt);

	if (img->mac_iram_size == 0 ||
	    img->mac_iram_size > AQ_HOSTBOOT_LEGACY_MAC_IRAM_MAX_SIZE ||
	    img->mac_iram_size < AQ_HOSTBOOT_LEGACY_MAC_IRAM_SKIP)
		return (EINVAL);
	if (img->mac_dram_size == 0 ||
	    img->mac_dram_size > AQ_HOSTBOOT_LEGACY_MAC_DRAM_MAX_SIZE ||
	    img->mac_dram_size < AQ_HOSTBOOT_LEGACY_MAC_DRAM_SKIP)
		return (EINVAL);
	if (img->phy_iram_size == 0 || img->phy_dram_size == 0)
		return (EINVAL);

	if (!aq_hostboot_legacy_range_valid(total,
		le32toh(hdr->mac_iram_offset), img->mac_iram_size) ||
	    !aq_hostboot_legacy_range_valid(total,
		le32toh(hdr->mac_dram_offset), img->mac_dram_size) ||
	    !aq_hostboot_legacy_range_valid(total,
		le32toh(hdr->phy_iram_offset), img->phy_iram_size) ||
	    !aq_hostboot_legacy_range_valid(total,
		le32toh(hdr->phy_dram_offset), img->phy_dram_size))
		return (EINVAL);

	if (conf_off > total || conf_cnt > (total - conf_off) / sizeof(*record))
		return (EINVAL);

	img->mac_iram = data + le32toh(hdr->mac_iram_offset);
	img->mac_dram = data + le32toh(hdr->mac_dram_offset);
	img->phy_iram = data + le32toh(hdr->phy_iram_offset);
	img->phy_dram = data + le32toh(hdr->phy_dram_offset);

	crc = aq_hostboot_legacy_crc_itu_t(0, img->mac_iram,
	    img->mac_iram_size);
	crc = aq_hostboot_legacy_crc_itu_t(crc, img->mac_dram,
	    img->mac_dram_size);
	if (crc != (uint16_t)le32toh(hdr->mac_crc))
		return (EINVAL);

	crc = aq_hostboot_legacy_crc_itu_t(0, img->phy_iram,
	    img->phy_iram_size);
	crc = aq_hostboot_legacy_crc_itu_t(crc, img->phy_dram,
	    img->phy_dram_size);
	if (crc != (uint16_t)le32toh(hdr->phy_crc))
		return (EINVAL);

	selector = aq_hostboot_provisioning_selector(hw);
	record = (const struct aq_hostboot_legacy_record *)(data + conf_off);
	crc = 0;
	for (i = 0; i < conf_cnt; i++, record++) {
		uint32_t mac_bdp_off;
		uint32_t mac_bdp_size;
		uint32_t phy_bdp_off;
		uint32_t phy_bdp_size;

		mac_bdp_off = le32toh(record->mac_bdp_offset);
		mac_bdp_size = le32toh(record->mac_bdp_size);
		phy_bdp_off = le32toh(record->phy_bdp_offset);
		phy_bdp_size = le32toh(record->phy_bdp_size);

		if (!aq_hostboot_legacy_range_valid(total, mac_bdp_off,
			mac_bdp_size) ||
		    !aq_hostboot_legacy_range_valid(total, phy_bdp_off,
			phy_bdp_size))
			return (EINVAL);

		crc = aq_hostboot_legacy_crc_itu_t(crc, record,
		    sizeof(*record));
		crc = aq_hostboot_legacy_crc_itu_t(crc, data + mac_bdp_off,
		    mac_bdp_size);
		crc = aq_hostboot_legacy_crc_itu_t(crc, data + phy_bdp_off,
		    phy_bdp_size);

		if (le32toh(record->sub_system_id) != selector)
			continue;

		img->mac_bdp = data + mac_bdp_off;
		img->mac_bdp_size = mac_bdp_size;
		img->phy_bdp = data + phy_bdp_off;
		img->phy_bdp_size = phy_bdp_size;
	}

	if (crc != (uint16_t)le32toh(hdr->conf_crc))
		return (EINVAL);

	return (0);
}

static void
aq_hostboot_legacy_clear_payload(struct aq_hw *hw, uint32_t dword_count)
{
	uint32_t i;

	for (i = 0; i < dword_count; i++)
		AQ_WRITE_REG(hw,
		    AQ_HOSTBOOT_LEGACY_RESET_SCRPAD_BASE + i * sizeof(uint32_t),
		    0);
}

static int
aq_hostboot_legacy_wait_mailbox(struct aq_hw *hw, uint32_t cmd,
    uint32_t poll_us, uint32_t timeout_us, uint32_t *response)
{
	uint32_t count;
	uint32_t i;
	uint32_t val;

	count = timeout_us / poll_us;
	if (count == 0)
		count = 1;

	for (i = 0; i < count; i++) {
		val = reg_glb_cpu_no_reset_scratchpad_get(hw,
		    AQ_HOSTBOOT_LEGACY_RBL_CMD_IDX);
		if (val != cmd) {
			*response = val;
			return (0);
		}
		usec_delay(poll_us);
	}

	return (ETIMEDOUT);
}

static int
aq_hostboot_legacy_upload(struct aq_hw *hw, uint32_t addr, const uint8_t *data,
    uint32_t size, uint32_t max_chunk, bool rewrite_addr, bool mark_last)
{
	uint32_t offset;

	offset = 0;
	if (!rewrite_addr)
		reg_glb_cpu_no_reset_scratchpad_set(hw, addr,
		    AQ_HOSTBOOT_LEGACY_RBL_ADDR_IDX);

	while (offset < size) {
		uint32_t chunk;
		uint32_t cmd;
		uint32_t response;

		if (rewrite_addr)
			reg_glb_cpu_no_reset_scratchpad_set(hw, addr,
			    AQ_HOSTBOOT_LEGACY_RBL_ADDR_IDX);

		cmd = AQ_HOSTBOOT_LEGACY_RBL_EXEC;
		for (chunk = 0; chunk < max_chunk && offset < size; chunk++) {
			uint32_t val;
			size_t remaining;
			size_t copy_len;

			remaining = size - offset;
			copy_len = remaining >= sizeof(uint32_t) ?
			    sizeof(uint32_t) :
			    remaining;
			val = aq_hostboot_legacy_read_le32(data + offset,
			    copy_len);
			if (copy_len < sizeof(uint32_t) && mark_last)
				cmd |= AQ_HOSTBOOT_LEGACY_RBL_LAST_DWORD;

			AQ_WRITE_REG(hw,
			    AQ_HOSTBOOT_LEGACY_RESET_SCRPAD_BASE +
				chunk * sizeof(uint32_t),
			    val);
			offset += (uint32_t)copy_len;
		}

		cmd |= chunk;
		reg_glb_cpu_no_reset_scratchpad_set(hw, cmd,
		    AQ_HOSTBOOT_LEGACY_RBL_CMD_IDX);
		if (aq_hostboot_legacy_wait_mailbox(hw, cmd,
			AQ_HOSTBOOT_LEGACY_UPLOAD_POLL_US,
			AQ_HOSTBOOT_LEGACY_UPLOAD_TIMEOUT_US, &response) != 0) {
			aq_hostboot_legacy_clear_payload(hw, max_chunk);
			return (ETIMEDOUT);
		}

		if (response == AQ_HOSTBOOT_LEGACY_RBL_WRONG_ADDR) {
			aq_hostboot_legacy_clear_payload(hw, max_chunk);
			return (EIO);
		}
		if (offset < size) {
			if (response != AQ_HOSTBOOT_LEGACY_RBL_CHUNK_DONE) {
				aq_hostboot_legacy_clear_payload(hw, max_chunk);
				return (EIO);
			}
		} else if (response != AQ_HOSTBOOT_LEGACY_RBL_COMPLETE) {
			aq_hostboot_legacy_clear_payload(hw, max_chunk);
			return (EIO);
		}

		addr += chunk * sizeof(uint32_t);
	}

	aq_hostboot_legacy_clear_payload(hw, max_chunk);
	return (0);
}

static int
aq_hostboot_legacy_write_complete(struct aq_hw *hw)
{
	uint32_t response;
	uint32_t cmd;

	cmd = AQ_HOSTBOOT_LEGACY_RBL_EXEC;
	reg_glb_cpu_no_reset_scratchpad_set(hw, cmd,
	    AQ_HOSTBOOT_LEGACY_RBL_CMD_IDX);
	if (aq_hostboot_legacy_wait_mailbox(hw, cmd,
		AQ_HOSTBOOT_LEGACY_COMPLETE_POLL_US,
		AQ_HOSTBOOT_LEGACY_COMPLETE_TIMEOUT_US, &response) != 0)
		return (ETIMEDOUT);
	if (response != AQ_HOSTBOOT_LEGACY_RBL_COMPLETE)
		return (EIO);

	return (0);
}

static int
aq_hostboot_legacy_wait_rbl_done(struct aq_hw *hw)
{
	uint32_t i;
	uint16_t rbl_status;

	for (i = 0; i < 10000; i++) {
		rbl_status = LOWORD(reg_glb_cpu_no_reset_scratchpad_get(hw,
		    AQ_HOSTBOOT_LEGACY_RBL_STATUS_IDX));
		if (rbl_status == AQ_HOSTBOOT_LEGACY_RBL_DONE)
			return (0);
		msec_delay(1);
	}

	return (ETIMEDOUT);
}

static int
aq_hostboot_legacy_load_bdp(struct aq_hw *hw, const uint8_t *data,
    uint32_t size)
{
	int err;

	err = aq_hostboot_legacy_upload(hw, 0, data, size,
	    AQ_HOSTBOOT_LEGACY_BDP_MAX_CHUNK, false, true);
	if (err != 0)
		return (err);

	return (aq_hostboot_legacy_write_complete(hw));
}

int
aq_hostboot_legacy(struct aq_hw *hw)
{
	struct aq_hostboot_legacy_image img;
	const uint32_t timeout_ticks = AQ_HOSTBOOT_LEGACY_STAGE_TIMEOUT_US /
	    AQ_HOSTBOOT_LEGACY_STAGE_POLL_US;
	/* Account for the for-loop post-decrement on continue paths. */
	const uint32_t timeout_reset_ticks = timeout_ticks + 1;
	uint32_t loader_status;
	uint32_t serviced;
	uint32_t timeout_left;
	int err;

	err = aq_hostboot_legacy_parse(hw, &img);
	if (err != 0) {
		aq_log_error("AQ1 host boot image validation failed: %d", err);
		return (err);
	}

	err = aq_hostboot_legacy_upload(hw,
	    AQ_HOSTBOOT_LEGACY_MAC_IRAM_ADDR + AQ_HOSTBOOT_LEGACY_MAC_IRAM_SKIP,
	    img.mac_iram + AQ_HOSTBOOT_LEGACY_MAC_IRAM_SKIP,
	    img.mac_iram_size - AQ_HOSTBOOT_LEGACY_MAC_IRAM_SKIP,
	    AQ_HOSTBOOT_LEGACY_MAC_MAX_CHUNK, true, false);
	if (err != 0) {
		aq_log_error("AQ1 host boot MAC IRAM upload failed: %d", err);
		return (err);
	}

	err = aq_hostboot_legacy_upload(hw,
	    AQ_HOSTBOOT_LEGACY_MAC_DRAM_ADDR + AQ_HOSTBOOT_LEGACY_MAC_DRAM_SKIP,
	    img.mac_dram + AQ_HOSTBOOT_LEGACY_MAC_DRAM_SKIP,
	    img.mac_dram_size - AQ_HOSTBOOT_LEGACY_MAC_DRAM_SKIP,
	    AQ_HOSTBOOT_LEGACY_MAC_MAX_CHUNK, true, false);
	if (err != 0) {
		aq_log_error("AQ1 host boot MAC DRAM upload failed: %d", err);
		return (err);
	}

	if (img.mac_bdp_size != 0 || img.phy_bdp_size != 0)
		reg_glb_cpu_no_reset_scratchpad_set(hw,
		    AQ_HOSTBOOT_LEGACY_FW_BDP_FROM_HOST,
		    AQ_HOSTBOOT_LEGACY_RBL_ADDR_IDX);

	AQ_WRITE_REG(hw, AQ_HOSTBOOT_LEGACY_FW_STATUS_REG, 0);
	err = aq_hostboot_legacy_write_complete(hw);
	if (err != 0) {
		aq_log_error("AQ1 host boot mailbox completion failed: %d",
		    err);
		return (err);
	}

	err = aq_hostboot_legacy_wait_rbl_done(hw);
	if (err != 0) {
		aq_log_error("AQ1 host boot RBL completion timed out");
		return (err);
	}

	reg_global_ctl2_set(hw, AQ_HOSTBOOT_LEGACY_CTL2_FW_LOADER_START);

	serviced = 0;
	for (timeout_left = timeout_ticks;; timeout_left--) {
		loader_status = AQ_READ_REG(hw,
		    AQ_HOSTBOOT_LEGACY_FW_STATUS_REG);
		if ((loader_status & AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_READY) !=
		    0)
			return (0);

		if ((loader_status & AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_REQ) !=
			0 &&
		    (serviced & AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_REQ) == 0) {
			err = aq_hostboot_legacy_upload(hw,
			    AQ_HOSTBOOT_LEGACY_PHY_IRAM_ADDR, img.phy_iram,
			    img.phy_iram_size, AQ_HOSTBOOT_LEGACY_PHY_MAX_CHUNK,
			    false, false);
			if (err == 0)
				err = aq_hostboot_legacy_upload(hw,
				    AQ_HOSTBOOT_LEGACY_PHY_DRAM_ADDR,
				    img.phy_dram, img.phy_dram_size,
				    AQ_HOSTBOOT_LEGACY_PHY_MAX_CHUNK, false,
				    false);
			if (err == 0)
				err = aq_hostboot_legacy_write_complete(hw);
			if (err != 0) {
				aq_log_error(
				    "AQ1 host boot PHY upload failed: %d", err);
				return (err);
			}
			serviced |= AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_REQ;
			timeout_left = timeout_reset_ticks;
			continue;
		}

		if ((loader_status &
			AQ_HOSTBOOT_LEGACY_FW_LOADER_MAC_BDP_REQ) != 0 &&
		    (serviced & AQ_HOSTBOOT_LEGACY_FW_LOADER_MAC_BDP_REQ) ==
			0) {
			err = aq_hostboot_legacy_load_bdp(hw, img.mac_bdp,
			    img.mac_bdp_size);
			if (err != 0) {
				aq_log_error(
				    "AQ1 host boot MAC BDP load failed: %d",
				    err);
				return (err);
			}
			serviced |= AQ_HOSTBOOT_LEGACY_FW_LOADER_MAC_BDP_REQ;
			timeout_left = timeout_reset_ticks;
			continue;
		}

		if ((loader_status &
			AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_BDP_REQ) != 0 &&
		    (serviced & AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_BDP_REQ) ==
			0) {
			err = aq_hostboot_legacy_load_bdp(hw, img.phy_bdp,
			    img.phy_bdp_size);
			if (err != 0) {
				aq_log_error(
				    "AQ1 host boot PHY BDP load failed: %d",
				    err);
				return (err);
			}
			serviced |= AQ_HOSTBOOT_LEGACY_FW_LOADER_PHY_BDP_REQ;
			timeout_left = timeout_reset_ticks;
			continue;
		}

		if (timeout_left == 0)
			break;

		usec_delay(AQ_HOSTBOOT_LEGACY_STAGE_POLL_US);
	}

	aq_log_error("AQ1 host boot stage timed out");
	return (ETIMEDOUT);
}
