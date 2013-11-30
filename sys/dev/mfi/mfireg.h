/*-
 * Copyright (c) 2006 IronPort Systems
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

#ifndef _MFIREG_H
#define _MFIREG_H

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/mfi/mfireg.h,v 1.1.2.1 2006/04/04 03:24:48 scottl Exp $");
__MBSDID("$MidnightBSD$");

/*
 * MegaRAID SAS MFI firmware definitions
 *
 * Calling this driver 'MegaRAID SAS' is a bit misleading.  It's a completely
 * new firmware interface from the old AMI MegaRAID one, and there is no
 * reason why this interface should be limited to just SAS.  In any case, LSI
 * seems to also call this interface 'MFI', so that will be used here.
 */

/*
 * Start with the register set.  All registers are 32 bits wide.
 * The usual Intel IOP style setup.
 */
#define MFI_IMSG0	0x10	/* Inbound message 0 */
#define MFI_IMSG1	0x14	/* Inbound message 1 */
#define MFI_OMSG0	0x18	/* Outbound message 0 */
#define MFI_OMSG1	0x1c	/* Outbound message 1 */
#define MFI_IDB		0x20	/* Inbound doorbell */
#define MFI_ISTS	0x24	/* Inbound interrupt status */
#define MFI_IMSK	0x28	/* Inbound interrupt mask */
#define MFI_ODB		0x2c	/* Outbound doorbell */
#define MFI_OSTS	0x30	/* Outbound interrupt status */
#define MFI_OMSK	0x34	/* Outbound interrupt mask */
#define MFI_IQP		0x40	/* Inbound queue port */
#define MFI_OQP		0x44	/* Outbound queue port */

/* Bits for MFI_OSTS */
#define MFI_OSTS_INTR_VALID	0x00000002

/*
 * Firmware state values.  Found in OMSG0 during initialization.
 */
#define MFI_FWSTATE_MASK		0xf0000000
#define MFI_FWSTATE_UNDEFINED		0x00000000
#define MFI_FWSTATE_BB_INIT		0x10000000
#define MFI_FWSTATE_FW_INIT		0x40000000
#define MFI_FWSTATE_WAIT_HANDSHAKE	0x60000000
#define MFI_FWSTATE_FW_INIT_2		0x70000000
#define MFI_FWSTATE_DEVICE_SCAN		0x80000000
#define MFI_FWSTATE_FLUSH_CACHE		0xa0000000
#define MFI_FWSTATE_READY		0xb0000000
#define MFI_FWSTATE_OPERATIONAL		0xc0000000
#define MFI_FWSTATE_FAULT		0xf0000000
#define MFI_FWSTATE_MAXSGL_MASK		0x00ff0000
#define MFI_FWSTATE_MAXCMD_MASK		0x0000ffff

/*
 * Control bits to drive the card to ready state.  These go into the IDB
 * register.
 */
#define MFI_FWINIT_ABORT	0x00000000 /* Abort all pending commands */
#define MFI_FWINIT_READY	0x00000002 /* Move from operational to ready */
#define MFI_FWINIT_MFIMODE	0x00000004 /* unknown */
#define MFI_FWINIT_CLEAR_HANDSHAKE 0x00000008 /* Respond to WAIT_HANDSHAKE */

/* MFI Commands */
typedef enum {
	MFI_CMD_INIT =		0x00,
	MFI_CMD_LD_READ,
	MFI_CMD_LD_WRITE,
	MFI_CMD_LD_SCSI_IO,
	MFI_CMD_PD_SCSI_IO,
	MFI_CMD_DCMD,
	MFI_CMD_ABORT,
	MFI_CMD_SMP,
	MFI_CMD_STP
} mfi_cmd_t;

/* Direct commands */
typedef enum {
	MFI_DCMD_CTRL_GETINFO =		0x01010000,
	MFI_DCMD_CTRL_FLUSHCACHE =	0x01101000,
	MFI_DCMD_CTRL_SHUTDOWN =	0x01050000,
	MFI_DCMD_CTRL_EVENT_GETINFO =	0x01040100,
	MFI_DCMD_CTRL_EVENT_GET =	0x01040300,
	MFI_DCMD_CTRL_EVENT_WAIT =	0x01040500,
	MFI_DCMD_LD_GET_PROP =		0x03030000,
	MFI_DCMD_CLUSTER =		0x08000000,
	MFI_DCMD_CLUSTER_RESET_ALL =	0x08010100,
	MFI_DCMD_CLUSTER_RESET_LD =	0x08010200
} mfi_dcmd_t;

/* Modifiers for MFI_DCMD_CTRL_FLUSHCACHE */
#define MFI_FLUSHCACHE_CTRL	0x01
#define MFI_FLUSHCACHE_DISK	0x02

/* Modifiers for MFI_DCMD_CTRL_SHUTDOWN */
#define MFI_SHUTDOWN_SPINDOWN	0x01

/*
 * MFI Frmae flags
 */
#define MFI_FRAME_POST_IN_REPLY_QUEUE		0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE	0x0001
#define MFI_FRAME_SGL32				0x0000
#define MFI_FRAME_SGL64				0x0002
#define MFI_FRAME_SENSE32			0x0000
#define MFI_FRAME_SENSE64			0x0004
#define MFI_FRAME_DIR_NONE			0x0000
#define MFI_FRAME_DIR_WRITE			0x0008
#define MFI_FRAME_DIR_READ			0x0010
#define MFI_FRAME_DIR_BOTH			0x0018

/* MFI Status codes */
typedef enum {
	MFI_STAT_OK =			0x00,
	MFI_STAT_INVALID_CMD,
	MFI_STAT_INVALID_DCMD,
	MFI_STAT_INVALID_PARAMETER,
	MFI_STAT_INVALID_SEQUENCE_NUMBER,
	MFI_STAT_ABORT_NOT_POSSIBLE,
	MFI_STAT_APP_HOST_CODE_NOT_FOUND,
	MFI_STAT_APP_IN_USE,
	MFI_STAT_APP_NOT_INITIALIZED,
	MFI_STAT_ARRAY_INDEX_INVALID,
	MFI_STAT_ARRAY_ROW_NOT_EMPTY,
	MFI_STAT_CONFIG_RESOURCE_CONFLICT,
	MFI_STAT_DEVICE_NOT_FOUND,
	MFI_STAT_DRIVE_TOO_SMALL,
	MFI_STAT_FLASH_ALLOC_FAIL,
	MFI_STAT_FLASH_BUSY,
	MFI_STAT_FLASH_ERROR =		0x10,
	MFI_STAT_FLASH_IMAGE_BAD,
	MFI_STAT_FLASH_IMAGE_INCOMPLETE,
	MFI_STAT_FLASH_NOT_OPEN,
	MFI_STAT_FLASH_NOT_STARTED,
	MFI_STAT_FLUSH_FAILED,
	MFI_STAT_HOST_CODE_NOT_FOUNT,
	MFI_STAT_LD_CC_IN_PROGRESS,
	MFI_STAT_LD_INIT_IN_PROGRESS,
	MFI_STAT_LD_LBA_OUT_OF_RANGE,
	MFI_STAT_LD_MAX_CONFIGURED,
	MFI_STAT_LD_NOT_OPTIMAL,
	MFI_STAT_LD_RBLD_IN_PROGRESS,
	MFI_STAT_LD_RECON_IN_PROGRESS,
	MFI_STAT_LD_WRONG_RAID_LEVEL,
	MFI_STAT_MAX_SPARES_EXCEEDED,
	MFI_STAT_MEMORY_NOT_AVAILABLE =	0x20,
	MFI_STAT_MFC_HW_ERROR,
	MFI_STAT_NO_HW_PRESENT,
	MFI_STAT_NOT_FOUND,
	MFI_STAT_NOT_IN_ENCL,
	MFI_STAT_PD_CLEAR_IN_PROGRESS,
	MFI_STAT_PD_TYPE_WRONG,
	MFI_STAT_PR_DISABLED,
	MFI_STAT_ROW_INDEX_INVALID,
	MFI_STAT_SAS_CONFIG_INVALID_ACTION,
	MFI_STAT_SAS_CONFIG_INVALID_DATA,
	MFI_STAT_SAS_CONFIG_INVALID_PAGE,
	MFI_STAT_SAS_CONFIG_INVALID_TYPE,
	MFI_STAT_SCSI_DONE_WITH_ERROR,
	MFI_STAT_SCSI_IO_FAILED,
	MFI_STAT_SCSI_RESERVATION_CONFLICT,
	MFI_STAT_SHUTDOWN_FAILED =	0x30,
	MFI_STAT_TIME_NOT_SET,
	MFI_STAT_WRONG_STATE,
	MFI_STAT_LD_OFFLINE,
	MFI_STAT_PEER_NOTIFICATION_REJECTED,
	MFI_STAT_PEER_NOTIFICATION_FAILED,
	MFI_STAT_RESERVATION_IN_PROGRESS,
	MFI_STAT_I2C_ERRORS_DETECTED,
	MFI_STAT_PCI_ERRORS_DETECTED,
	MFI_STAT_INVALID_STATUS =	0xFF
} mfi_status_t;

typedef enum {
	MFI_EVT_CLASS_DEBUG =		-2,
	MFI_EVT_CLASS_PROGRESS =	-1,
	MFI_EVT_CLASS_INFO =		0,
	MFI_EVT_CLASS_WARNING =		1,
	MFI_EVT_CLASS_CRITICAL =	2,
	MFI_EVT_CLASS_FATAL =		3,
	MFI_EVT_CLASS_DEAD =		4
} mfi_evt_class_t;

typedef enum {
	MFI_EVT_LOCALE_LD =		0x0001,
	MFI_EVT_LOCALE_PD =		0x0002,
	MFI_EVT_LOCALE_ENCL =		0x0004,
	MFI_EVT_LOCALE_BBU =		0x0008,
	MFI_EVT_LOCALE_SAS =		0x0010,
	MFI_EVT_LOCALE_CTRL =		0x0020,
	MFI_EVT_LOCALE_CONFIG =		0x0040,
	MFI_EVT_LOCALE_CLUSTER =	0x0080,
	MFI_EVT_LOCALE_ALL =		0xffff
} mfi_evt_locale_t;

typedef enum {
        MR_EVT_ARGS_NONE =		0x00,
        MR_EVT_ARGS_CDB_SENSE,
        MR_EVT_ARGS_LD,
        MR_EVT_ARGS_LD_COUNT,
        MR_EVT_ARGS_LD_LBA,
        MR_EVT_ARGS_LD_OWNER,
        MR_EVT_ARGS_LD_LBA_PD_LBA,
        MR_EVT_ARGS_LD_PROG,
        MR_EVT_ARGS_LD_STATE,
        MR_EVT_ARGS_LD_STRIP,
        MR_EVT_ARGS_PD,
        MR_EVT_ARGS_PD_ERR,
        MR_EVT_ARGS_PD_LBA,
        MR_EVT_ARGS_PD_LBA_LD,
        MR_EVT_ARGS_PD_PROG,
        MR_EVT_ARGS_PD_STATE,
        MR_EVT_ARGS_PCI,
        MR_EVT_ARGS_RATE,
        MR_EVT_ARGS_STR,
        MR_EVT_ARGS_TIME,
        MR_EVT_ARGS_ECC
} mfi_evt_args;

/*
 * Other propertities and definitions
 */
#define MFI_MAX_PD_CHANNELS	2
#define MFI_MAX_LD_CHANNELS	2
#define MFI_MAX_CHANNELS	(MFI_MAX_PD_CHANNELS + MFI_MAX_LD_CHANNELS)
#define MFI_MAX_CHANNEL_DEVS	128
#define MFI_DEFAULT_ID		-1
#define MFI_MAX_LUN		8
#define MFI_MAX_LD		64

#define MFI_FRAME_SIZE		64
#define MFI_MBOX_SIZE		12

#define MFI_POLL_TIMEOUT_SECS	10

/* Allow for speedier math calculations */
#define MFI_SECTOR_LEN		512

/* Scatter Gather elements */
struct mfi_sg32 {
	uint32_t	addr;
	uint32_t	len;
} __packed;

struct mfi_sg64 {
	uint64_t	addr;
	uint32_t	len;
} __packed;

union mfi_sgl {
	struct mfi_sg32	sg32[1];
	struct mfi_sg64	sg64[1];
} __packed;

/* Message frames.  All messages have a common header */
struct mfi_frame_header {
	uint8_t		cmd;
	uint8_t		sense_len;
	uint8_t		cmd_status;
	uint8_t		scsi_status;
	uint8_t		target_id;
	uint8_t		lun_id;
	uint8_t		cdb_len;
	uint8_t		sg_count;
	uint32_t	context;
	uint32_t	pad0;
	uint16_t	flags;
	uint16_t	timeout;
	uint32_t	data_len;
} __packed;

struct mfi_init_frame {
	struct mfi_frame_header	header;
	uint32_t	qinfo_new_addr_lo;
	uint32_t	qinfo_new_addr_hi;
	uint32_t	qinfo_old_addr_lo;
	uint32_t	qinfo_old_addr_hi;
	uint32_t	reserved[6];
} __packed;

#define MFI_IO_FRAME_SIZE 40
struct mfi_io_frame {
	struct mfi_frame_header	header;
	uint32_t	sense_addr_lo;
	uint32_t	sense_addr_hi;
	uint32_t	lba_lo;
	uint32_t	lba_hi;
	union mfi_sgl	sgl;
} __packed;

#define MFI_PASS_FRAME_SIZE 48
struct mfi_pass_frame {
	struct mfi_frame_header header;
	uint32_t	sense_addr_lo;
	uint32_t	sense_addr_hi;
	uint8_t		cdb[16];
	union mfi_sgl	sgl;
} __packed;

#define MFI_DCMD_FRAME_SIZE 40
struct mfi_dcmd_frame {
	struct mfi_frame_header header;
	uint32_t	opcode;
	uint8_t		mbox[MFI_MBOX_SIZE];
	union mfi_sgl	sgl;
} __packed;

struct mfi_abort_frame {
	struct mfi_frame_header header;
	uint32_t	abort_context;
	uint32_t	pad;
	uint32_t	abort_mfi_addr_lo;
	uint32_t	abort_mfi_addr_hi;
	uint32_t	reserved[6];
} __packed;

struct mfi_smp_frame {
	struct mfi_frame_header header;
	uint64_t	sas_addr;
	union {
		struct mfi_sg32 sg32[2];
		struct mfi_sg64 sg64[2];
	} sgl;
} __packed;

struct mfi_stp_frame {
	struct mfi_frame_header header;
	uint16_t	fis[10];
	uint32_t	stp_flags;
	union {
		struct mfi_sg32 sg32[2];
		struct mfi_sg64 sg64[2];
	} sgl;
} __packed;

union mfi_frame {
	struct mfi_frame_header header;
	struct mfi_init_frame	init;
	struct mfi_io_frame	io;
	struct mfi_pass_frame	pass;
	struct mfi_dcmd_frame	dcmd;
	struct mfi_abort_frame	abort;
	struct mfi_smp_frame	smp;
	struct mfi_stp_frame	stp;
	uint8_t			bytes[MFI_FRAME_SIZE];
};

#define MFI_SENSE_LEN 128
struct mfi_sense {
	uint8_t		data[MFI_SENSE_LEN];
};

/* The queue init structure that is passed with the init message */
struct mfi_init_qinfo {
	uint32_t	flags;
	uint32_t	rq_entries;
	uint32_t	rq_addr_lo;
	uint32_t	rq_addr_hi;
	uint32_t	pi_addr_lo;
	uint32_t	pi_addr_hi;
	uint32_t	ci_addr_lo;
	uint32_t	ci_addr_hi;
} __packed;

/* SAS (?) controller properties, part of mfi_ctrl_info */
struct mfi_ctrl_props {
	uint16_t	seq_num;
	uint16_t	pred_fail_poll_interval;
	uint16_t	intr_throttle_cnt;
	uint16_t	intr_throttle_timeout;
	uint8_t		rebuild_rate;
	uint8_t		patrol_read_rate;
	uint8_t		bgi_rate;
	uint8_t		cc_rate;
	uint8_t		recon_rate;
	uint8_t		cache_flush_interval;
	uint8_t		spinup_drv_cnt;
	uint8_t		spinup_delay;
	uint8_t		cluster_enable;
	uint8_t		coercion_mode;
	uint8_t		alarm_enable;
	uint8_t		disable_auto_rebuild;
	uint8_t		disable_battery_warn;
	uint8_t		ecc_bucket_size;
	uint16_t	ecc_bucket_leak_rate;
	uint8_t		restore_hotspare_on_insertion;
	uint8_t		expose_encl_devices;
	uint8_t		reserved[38];
} __packed;

/* PCI information about the card. */
struct mfi_info_pci {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	uint8_t		reserved[24];
} __packed;

/* Host (front end) interface information */
struct mfi_info_host {
	uint8_t		type;
#define MFI_INFO_HOST_PCIX	0x01
#define MFI_INFO_HOST_PCIE	0x02
#define MFI_INFO_HOST_ISCSI	0x04
#define MFI_INFO_HOST_SAS3G	0x08
	uint8_t		reserved[6];
	uint8_t		port_count;
	uint64_t	port_addr[8];
} __packed;

/* Device (back end) interface information */
struct mfi_info_device {
	uint8_t		type;
#define MFI_INFO_DEV_SPI	0x01
#define MFI_INFO_DEV_SAS3G	0x02
#define MFI_INFO_DEV_SATA1	0x04
#define MFI_INFO_DEV_SATA3G	0x08
	uint8_t		reserved[6];
	uint8_t		port_count;
	uint64_t	port_addr[8];
} __packed;

/* Firmware component information */
struct mfi_info_component {
	char		 name[8];
	char		 version[32];
	char		 build_date[16];
	char		 build_time[16];
} __packed;


/* SAS (?) controller info, returned from MFI_DCMD_CTRL_GETINFO. */
struct mfi_ctrl_info {
	struct mfi_info_pci	pci;
	struct mfi_info_host	host;
	struct mfi_info_device	device;

	/* Firmware components that are present and active. */
	uint32_t		image_check_word;
	uint32_t		image_component_count;
	struct mfi_info_component image_component[8];

	/* Firmware components that have been flashed but are inactive */
	uint32_t		pending_image_component_count;
	struct mfi_info_component pending_image_component[8];

	uint8_t			max_arms;
	uint8_t			max_spans;
	uint8_t			max_arrays;
	uint8_t			max_lds;
	char			product_name[80];
	char			serial_number[32];
	uint32_t		hw_present;
#define MFI_INFO_HW_BBU		0x01
#define MFI_INFO_HW_ALARM	0x02
#define MFI_INFO_HW_NVRAM	0x04
#define MFI_INFO_HW_UART	0x08
	uint32_t		current_fw_time;
	uint16_t		max_cmds;
	uint16_t		max_sg_elements;
	uint32_t		max_request_size;
	uint16_t		lds_present;
	uint16_t		lds_degraded;
	uint16_t		lds_offline;
	uint16_t		pd_present;
	uint16_t		pd_disks_present;
	uint16_t		pd_disks_pred_failure;
	uint16_t		pd_disks_failed;
	uint16_t		nvram_size;
	uint16_t		memory_size;
	uint16_t		flash_size;
	uint16_t		ram_correctable_errors;
	uint16_t		ram_uncorrectable_errors;
	uint8_t			cluster_allowed;
	uint8_t			cluster_active;
	uint16_t		max_strips_per_io;

	uint32_t		raid_levels;
#define MFI_INFO_RAID_0		0x01
#define MFI_INFO_RAID_1		0x02
#define MFI_INFO_RAID_5		0x04
#define MFI_INFO_RAID_1E	0x08
#define MFI_INFO_RAID_6		0x10

	uint32_t		adapter_ops;
#define MFI_INFO_AOPS_RBLD_RATE		0x0001		
#define MFI_INFO_AOPS_CC_RATE		0x0002
#define MFI_INFO_AOPS_BGI_RATE		0x0004
#define MFI_INFO_AOPS_RECON_RATE	0x0008
#define MFI_INFO_AOPS_PATROL_RATE	0x0010
#define MFI_INFO_AOPS_ALARM_CONTROL	0x0020
#define MFI_INFO_AOPS_CLUSTER_SUPPORTED	0x0040
#define MFI_INFO_AOPS_BBU		0x0080
#define MFI_INFO_AOPS_SPANNING_ALLOWED	0x0100
#define MFI_INFO_AOPS_DEDICATED_SPARES	0x0200
#define MFI_INFO_AOPS_REVERTIBLE_SPARES	0x0400
#define MFI_INFO_AOPS_FOREIGN_IMPORT	0x0800
#define MFI_INFO_AOPS_SELF_DIAGNOSTIC	0x1000
#define MFI_INFO_AOPS_MIXED_ARRAY	0x2000
#define MFI_INFO_AOPS_GLOBAL_SPARES	0x4000

	uint32_t		ld_ops;
#define MFI_INFO_LDOPS_READ_POLICY	0x01
#define MFI_INFO_LDOPS_WRITE_POLICY	0x02
#define MFI_INFO_LDOPS_IO_POLICY	0x04
#define MFI_INFO_LDOPS_ACCESS_POLICY	0x08
#define MFI_INFO_LDOPS_DISK_CACHE_POLICY 0x10

	struct {
		uint8_t		min;
		uint8_t		max;
		uint8_t		reserved[2];
	} __packed stripe_sz_ops;

	uint32_t		pd_ops;
#define MFI_INFO_PDOPS_FORCE_ONLINE	0x01
#define MFI_INFO_PDOPS_FORCE_OFFLINE	0x02
#define MFI_INFO_PDOPS_FORCE_REBUILD	0x04

	uint32_t		pd_mix_support;
#define MFI_INFO_PDMIX_SAS		0x01
#define MFI_INFO_PDMIX_SATA		0x02
#define MFI_INFO_PDMIX_ENCL		0x04
#define MFI_INFO_PDMIX_LD		0x08
#define MFI_INFO_PDMIX_SATA_CLUSTER	0x10

	uint8_t			ecc_bucket_count;
	uint8_t			reserved2[11];
	struct mfi_ctrl_props	properties;
	char			package_version[0x60];
	uint8_t			pad[0x800 - 0x6a0];
} __packed;

#endif /* _MFIREG_H */
