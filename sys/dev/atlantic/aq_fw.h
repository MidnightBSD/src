/**
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2017 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3) The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef AQ_FW_H
#define AQ_FW_H

struct aq_hw;
struct aq_stats_s;

typedef enum aq_fw_link_speed {
	aq_fw_none = 0,
	aq_fw_100M = (1 << 0),
	aq_fw_1G = (1 << 1),
	aq_fw_2G5 = (1 << 2),
	aq_fw_5G = (1 << 3),
	aq_fw_10G = (1 << 4),
	aq_fw_10M = (1 << 5),
} aq_fw_link_speed_t;

typedef enum aq_fw_link_fc {
	aq_fw_fc_none = 0,
	aq_fw_fc_ENABLE_RX = BIT(0),
	aq_fw_fc_ENABLE_TX = BIT(1),
	aq_fw_fc_ENABLE_ALL = aq_fw_fc_ENABLE_RX | aq_fw_fc_ENABLE_TX,
} aq_fw_link_fc_t;

#define aq_fw_speed_auto \
	(aq_fw_10M | aq_fw_100M | aq_fw_1G | aq_fw_2G5 | aq_fw_5G | aq_fw_10G)

#define AQ_DOWNSHIFT_MAX 15U

#define AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_A0 0u
#define AQ2_FW_INTERFACE_OUT_VERSION_IFACE_VER_B0 1u

#ifndef AQ_HW_FW_MPI_STATE_E_DEFINED
#define AQ_HW_FW_MPI_STATE_E_DEFINED
enum aq_hw_fw_mpi_state_e {
	MPI_DEINIT = 0,
	MPI_RESET = 1,
	MPI_INIT = 2,
	MPI_POWER = 4,
};
#endif

struct aq_firmware_ops {
	int (*reset)(struct aq_hw *hal);

	int (*set_mode)(struct aq_hw *hal, enum aq_hw_fw_mpi_state_e mode,
	    aq_fw_link_speed_t speed);
	int (*get_mode)(struct aq_hw *hal, enum aq_hw_fw_mpi_state_e *mode,
	    aq_fw_link_speed_t *speed, aq_fw_link_fc_t *fc);

	int (*get_mac_addr)(struct aq_hw *hal, uint8_t *mac_addr);
	int (*get_stats)(struct aq_hw *hal, struct aq_stats_s *stats);

	int (*led_control)(struct aq_hw *hal, uint32_t mode);
	int (*get_phy_temp)(struct aq_hw *hal, int *temp_c);
	int (*get_cable_len)(struct aq_hw *hal, uint8_t *len);
	int (*get_cable_diag)(struct aq_hw *hal, uint32_t lane_data[4]);
	int (*set_downshift)(struct aq_hw *hal, uint32_t counter);
	int (*set_eee_rate)(struct aq_hw *hal, uint32_t rate);
	int (*get_eee_rate)(struct aq_hw *hal, uint32_t *rate,
	    uint32_t *supported, uint32_t *lp_rate);
};

int aq_fw_reset(struct aq_hw *hw);
int aq_fw_ops_init(struct aq_hw *hw);
void aq2_log_boot_fail(uint32_t status, const char *message);
int aq2_fw_reboot(struct aq_hw *hw);
int aq2_fw_set_wol(struct aq_hw *hw, uint32_t wol_flags, const uint8_t *mac);
int fw2x_set_wol(struct aq_hw *hw, uint32_t wol_flags, const uint8_t *mac);
int fw2x_read_settings_addr(struct aq_hw *hw);
int fw2x_set_media_detect(struct aq_hw *hw, bool enable);
int fw2x_set_loopback(struct aq_hw *hw, int mode);
extern struct aq_firmware_ops aq_fw2x_ops;

#endif // AQ_FW_H
