/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * $FreeBSD: release/10.0.0/sys/dev/hyperv/storvsc/hv_vstorage.h 256276 2013-10-10 16:25:53Z dim $
 */

#ifndef __HV_VSTORAGE_H__
#define __HV_VSTORAGE_H__

/*
 * Major/minor macros.  Minor version is in LSB, meaning that earlier flat
 * version numbers will be interpreted as "0.x" (i.e., 1 becomes 0.1).
 */

#define VMSTOR_PROTOCOL_MAJOR(VERSION_)         (((VERSION_) >> 8) & 0xff)
#define VMSTOR_PROTOCOL_MINOR(VERSION_)         (((VERSION_)     ) & 0xff)
#define VMSTOR_PROTOCOL_VERSION(MAJOR_, MINOR_) ((((MAJOR_) & 0xff) << 8) | \
                                                 (((MINOR_) & 0xff)     ))

/*
 * Invalid version.
 */
#define VMSTOR_INVALID_PROTOCOL_VERSION  -1

/*
 * Version history:
 * V1 Beta                    0.1
 * V1 RC < 2008/1/31          1.0
 * V1 RC > 2008/1/31          2.0
 */

#define VMSTOR_PROTOCOL_VERSION_CURRENT	VMSTOR_PROTOCOL_VERSION(2, 0)

/**
 *  Packet structure ops describing virtual storage requests.
 */
enum vstor_packet_ops {
	VSTOR_OPERATION_COMPLETEIO            = 1,
	VSTOR_OPERATION_REMOVEDEVICE          = 2,
	VSTOR_OPERATION_EXECUTESRB            = 3,
	VSTOR_OPERATION_RESETLUN              = 4,
	VSTOR_OPERATION_RESETADAPTER          = 5,
	VSTOR_OPERATION_RESETBUS              = 6,
	VSTOR_OPERATION_BEGININITIALIZATION   = 7,
	VSTOR_OPERATION_ENDINITIALIZATION     = 8,
	VSTOR_OPERATION_QUERYPROTOCOLVERSION  = 9,
	VSTOR_OPERATION_QUERYPROPERTIES       = 10,
	VSTOR_OPERATION_MAXIMUM               = 10
};


/*
 *  Platform neutral description of a scsi request -
 *  this remains the same across the write regardless of 32/64 bit
 *  note: it's patterned off the Windows DDK SCSI_PASS_THROUGH structure
 */

#define CDB16GENERIC_LENGTH			0x10
#define SENSE_BUFFER_SIZE			0x12
#define MAX_DATA_BUFFER_LENGTH_WITH_PADDING	0x14

struct vmscsi_req {
	uint16_t length;
	uint8_t  srb_status;
	uint8_t  scsi_status;

	/* HBA number, set to the order number detected by initiator. */
	uint8_t  port;
	/* SCSI bus number or bus_id, different from CAM's path_id. */
	uint8_t  path_id;

	uint8_t  target_id;
	uint8_t  lun;

	uint8_t  cdb_len;
	uint8_t  sense_info_len;
	uint8_t  data_in;
	uint8_t  reserved;

	uint32_t transfer_len;

	union {
	    uint8_t cdb[CDB16GENERIC_LENGTH];

	    uint8_t sense_data[SENSE_BUFFER_SIZE];

	    uint8_t reserved_array[MAX_DATA_BUFFER_LENGTH_WITH_PADDING];
	} u;

} __packed;

/**
 *  This structure is sent during the initialization phase to get the different
 *  properties of the channel.
 */

struct vmstor_chan_props {
	uint16_t proto_ver;
	uint8_t  path_id;
	uint8_t  target_id;

	/**
	 * Note: port number is only really known on the client side
	 */
	uint32_t port;
	uint32_t flags;
	uint32_t max_transfer_bytes;

	/**
	 *  This id is unique for each channel and will correspond with
	 *  vendor specific data in the inquiry_ata
	 */
	uint64_t unique_id;

} __packed;

/**
 *  This structure is sent during the storage protocol negotiations.
 */

struct vmstor_proto_ver
{
	/**
	 * Major (MSW) and minor (LSW) version numbers.
	 */
	uint16_t major_minor;

	uint16_t revision;			/* always zero */
} __packed;

/**
 * Channel Property Flags
 */

#define STORAGE_CHANNEL_REMOVABLE_FLAG                  0x1
#define STORAGE_CHANNEL_EMULATED_IDE_FLAG               0x2


struct vstor_packet {
	/**
	 * Requested operation type
	 */
	enum vstor_packet_ops operation;

	/*
	 * Flags - see below for values
	 */
	uint32_t flags;

	/**
	 * Status of the request returned from the server side.
	 */
	uint32_t status;

	union
	{
	    /**
	     * Structure used to forward SCSI commands from the client to
	     * the server.
	     */
	    struct vmscsi_req vm_srb;

	    /**
	     * Structure used to query channel properties.
	     */
	    struct vmstor_chan_props chan_props;

	    /**
	     * Used during version negotiations.
	     */
	    struct vmstor_proto_ver version;
	} u;

} __packed;


/**
 * SRB (SCSI Request Block) Status Codes
 */
#define SRB_STATUS_PENDING		0x00
#define SRB_STATUS_SUCCESS		0x01
#define SRB_STATUS_ABORTED		0x02
#define SRB_STATUS_ABORT_FAILED	0x03
#define SRB_STATUS_ERROR 		0x04
#define SRB_STATUS_BUSY			0x05

/**
 * SRB Status Masks (can be combined with above status codes)
 */
#define SRB_STATUS_QUEUE_FROZEN		0x40
#define SRB_STATUS_AUTOSENSE_VALID	0x80


/**
 *  Packet flags
 */

/**
 *  This flag indicates that the server should send back a completion for this
 *  packet.
 */
#define REQUEST_COMPLETION_FLAG	0x1

/**
 *  This is the set of flags that the vsc can set in any packets it sends
 */
#define VSC_LEGAL_FLAGS (REQUEST_COMPLETION_FLAG)

#endif /* __HV_VSTORAGE_H__ */
