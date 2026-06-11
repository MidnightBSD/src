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
#include "aq_fw.h"
#include "aq_hostboot.h"
#include "aq_hw.h"
#include "aq_hw_aq2.h"

enum aq_hostboot_aq2_req {
	AQ2_HOSTBOOT_REQ_UNKNOWN = 0,
	AQ2_HOSTBOOT_REQ_IMAGE,
	AQ2_HOSTBOOT_REQ_CMD,
	AQ2_HOSTBOOT_REQ_ITI,
};

static enum aq_hostboot_aq2_req
aq_hostboot_aq2_classify(uint32_t offset, uint32_t length)
{
	if (offset < AQ2_HOSTBOOT_MAX_OFFSET &&
	    length <= AQ2_HOSTBOOT_MAX_LENGTH)
		return (AQ2_HOSTBOOT_REQ_IMAGE);

	if ((offset & AQ2_HOSTBOOT_CMD_REQ_MASK) != 0 &&
	    offset < (AQ2_HOSTBOOT_CMD_REQ_MASK | AQ2_HOSTBOOT_MAX_OFFSET) &&
	    length <= AQ2_HOSTBOOT_MAX_LENGTH)
		return (AQ2_HOSTBOOT_REQ_CMD);

	if ((offset & AQ2_HOSTBOOT_ITI_REQ_MASK) != 0 &&
	    offset < (AQ2_HOSTBOOT_ITI_REQ_MASK | AQ2_HOSTBOOT_MAX_OFFSET) &&
	    length <= AQ2_HOSTBOOT_MAX_LENGTH)
		return (AQ2_HOSTBOOT_REQ_ITI);

	return (AQ2_HOSTBOOT_REQ_UNKNOWN);
}

static void
aq_hostboot_aq2_copy_dwords(struct aq_hw *hw, uint32_t reg, const uint8_t *data,
    uint32_t dword_count)
{
	uint32_t i;
	uint32_t val;

	for (i = 0; i < dword_count; i++) {
		memcpy(&val, data + i * sizeof(uint32_t), sizeof(val));
		AQ_WRITE_REG(hw, reg + i * sizeof(uint32_t), le32toh(val));
	}
}

static int
aq_hostboot_aq2_service_request(struct aq_hw *hw)
{
	const struct firmware *fw;
	enum aq_hostboot_aq2_req req;
	uint32_t offset;
	uint32_t length;
	uint32_t offset_dwords;
	uint32_t length_dwords;
	uint32_t actual_length_dwords;
	uint32_t start_byte;
	uint32_t remaining_dwords;

	fw = hw->hostboot_fw;
	if (fw == NULL)
		return (ENOENT);

	offset = AQ_READ_REG(hw, AQ2_HOSTBOOT_REQ_OFFSET_REG);
	length = AQ_READ_REG(hw, AQ2_HOSTBOOT_REQ_LENGTH_REG);
	req = aq_hostboot_aq2_classify(offset, length);
	if (req != AQ2_HOSTBOOT_REQ_IMAGE) {
		aq_log_error("AQ2 host boot received invalid request 0x%x/0x%x",
		    offset, length);
		return (EINVAL);
	}

	offset_dwords = offset & AQ2_HOSTBOOT_OFFSET_MASK;
	length_dwords = length & AQ2_HOSTBOOT_LENGTH_MASK;
	start_byte = offset_dwords * sizeof(uint32_t);
	if (start_byte >= fw->datasize) {
		aq_log_error(
		    "AQ2 host boot request offset 0x%x exceeds image size %zu",
		    start_byte, fw->datasize);
		return (EINVAL);
	}

	remaining_dwords = (uint32_t)((fw->datasize - start_byte) /
	    sizeof(uint32_t));
	actual_length_dwords = min(length_dwords, remaining_dwords);

	AQ_WRITE_REG(hw, AQ2_MCP_HOST_REQ_INT_CLR_REG,
	    AQ2_MCP_HOST_REQ_INT_READY);
	AQ_WRITE_REG(hw, AQ2_HOSTBOOT_DATA_OFFSET_REG, offset);
	AQ_WRITE_REG(hw, AQ2_HOSTBOOT_DATA_LENGTH_REG, length);

	aq_hostboot_aq2_copy_dwords(hw, AQ2_HOSTBOOT_SHARED_IMAGE_BUF,
	    (const uint8_t *)fw->data + start_byte, actual_length_dwords);

	AQ_WRITE_REG(hw, AQ2_MIF_HOST_FINISHED_STATUS_WRITE_REG,
	    AQ2_MIF_HOST_FINISHED_STATUS_ACK);

	return (0);
}

int
aq_hostboot_aq2(struct aq_hw *hw)
{
	uint32_t boot;
	uint32_t i;
	int err;

	for (i = 0; i < 13000; i++) {
		boot = AQ_READ_REG(hw, AQ2_MIF_BOOT_REG);
		if ((boot & AQ2_MIF_BOOT_FW_FAIL_MASK) != 0) {
			aq2_log_boot_fail(boot, "AQ2 host boot failed");
			return (EIO);
		}

		if ((AQ_READ_REG(hw, AQ2_MCP_HOST_REQ_INT_REG) &
			AQ2_MCP_HOST_REQ_INT_READY) != 0) {
			err = aq_hostboot_aq2_service_request(hw);
			if (err != 0)
				return (err);
		}

		if ((boot & AQ2_MIF_BOOT_FW_INIT_COMP_SUCCESS) != 0)
			break;

		msec_delay(1);
	}
	if (i == 13000) {
		aq_log_error(
		    "AQ2 host boot timed out waiting for firmware ready");
		return (ETIMEDOUT);
	}

	for (i = 0; i < 10000; i++) {
		boot = AQ_READ_REG(hw, AQ2_MIF_BOOT_REG);
		if ((boot & AQ2_MIF_BOOT_FW_FAIL_MASK) != 0) {
			aq2_log_boot_fail(boot, "AQ2 host boot failed");
			return (EIO);
		}

		if ((AQ_READ_REG(hw, AQ2_MCP_HOST_REQ_INT_REG) &
			AQ2_MCP_HOST_REQ_INT_READY) != 0) {
			err = aq_hostboot_aq2_service_request(hw);
			if (err != 0)
				return (err);
		}

		if ((boot & AQ2_MIF_BOOT_HOST_DATA_LOADED) != 0)
			return (0);

		msec_delay(1);
	}

	aq_log_error("AQ2 host boot timed out waiting for MAC firmware ready");
	return (ETIMEDOUT);
}
