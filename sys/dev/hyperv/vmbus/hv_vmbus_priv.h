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
 * $FreeBSD: release/10.0.0/sys/dev/hyperv/vmbus/hv_vmbus_priv.h 256276 2013-10-10 16:25:53Z dim $
 */

#ifndef __HYPERV_PRIV_H__
#define __HYPERV_PRIV_H__

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>

#include <dev/hyperv/include/hyperv.h>


/*
 *  Status codes for hypervisor operations.
 */

typedef uint16_t hv_vmbus_status;

#define HV_MESSAGE_SIZE                 (256)
#define HV_MESSAGE_PAYLOAD_BYTE_COUNT   (240)
#define HV_MESSAGE_PAYLOAD_QWORD_COUNT  (30)
#define HV_ANY_VP                       (0xFFFFFFFF)

/*
 * Synthetic interrupt controller flag constants.
 */

#define HV_EVENT_FLAGS_COUNT        (256 * 8)
#define HV_EVENT_FLAGS_BYTE_COUNT   (256)
#define HV_EVENT_FLAGS_DWORD_COUNT  (256 / sizeof(uint32_t))

/*
 * MessageId: HV_STATUS_INSUFFICIENT_BUFFERS
 * MessageText:
 *    You did not supply enough message buffers to send a message.
 */

#define HV_STATUS_INSUFFICIENT_BUFFERS   ((uint16_t)0x0013)

typedef void (*hv_vmbus_channel_callback)(void *context);

typedef struct {
	void*		data;
	uint32_t	length;
} hv_vmbus_sg_buffer_list;

typedef struct {
	uint32_t	current_interrupt_mask;
	uint32_t	current_read_index;
	uint32_t	current_write_index;
	uint32_t	bytes_avail_to_read;
	uint32_t	bytes_avail_to_write;
} hv_vmbus_ring_buffer_debug_info;

typedef struct {
	uint32_t 		rel_id;
	hv_vmbus_channel_state	state;
	hv_guid			interface_type;
	hv_guid			interface_instance;
	uint32_t		monitor_id;
	uint32_t		server_monitor_pending;
	uint32_t		server_monitor_latency;
	uint32_t		server_monitor_connection_id;
	uint32_t		client_monitor_pending;
	uint32_t		client_monitor_latency;
	uint32_t		client_monitor_connection_id;
	hv_vmbus_ring_buffer_debug_info	inbound;
	hv_vmbus_ring_buffer_debug_info	outbound;
} hv_vmbus_channel_debug_info;

typedef union {
	hv_vmbus_channel_version_supported	version_supported;
	hv_vmbus_channel_open_result		open_result;
	hv_vmbus_channel_gpadl_torndown		gpadl_torndown;
	hv_vmbus_channel_gpadl_created		gpadl_created;
	hv_vmbus_channel_version_response	version_response;
} hv_vmbus_channel_msg_response;

/*
 * Represents each channel msg on the vmbus connection
 * This is a variable-size data structure depending on
 * the msg type itself
 */
typedef struct hv_vmbus_channel_msg_info {
	/*
	 * Bookkeeping stuff
	 */
	TAILQ_ENTRY(hv_vmbus_channel_msg_info)  msg_list_entry;
	/*
	 * So far, this is only used to handle
	 * gpadl body message
	 */
	TAILQ_HEAD(, hv_vmbus_channel_msg_info) sub_msg_list_anchor;
	/*
	 * Synchronize the request/response if
	 * needed.
	 * KYS: Use a semaphore for now.
	 * Not perf critical.
	 */
	struct sema				wait_sema;
	hv_vmbus_channel_msg_response		response;
	uint32_t				message_size;
	/**
	 * The channel message that goes out on
	 *  the "wire". It will contain at
	 *  minimum the
	 *  hv_vmbus_channel_msg_header
	 * header.
	 */
	unsigned char 				msg[0];
} hv_vmbus_channel_msg_info;

/*
 * The format must be the same as hv_vm_data_gpa_direct
 */
typedef struct hv_vmbus_channel_packet_page_buffer {
	uint16_t		type;
	uint16_t		data_offset8;
	uint16_t		length8;
	uint16_t		flags;
	uint64_t		transaction_id;
	uint32_t		reserved;
	uint32_t		range_count;
	hv_vmbus_page_buffer	range[HV_MAX_PAGE_BUFFER_COUNT];
} __packed hv_vmbus_channel_packet_page_buffer;

/*
 * The format must be the same as hv_vm_data_gpa_direct
 */
typedef struct hv_vmbus_channel_packet_multipage_buffer {
	uint16_t 			type;
	uint16_t 			data_offset8;
	uint16_t 			length8;
	uint16_t 			flags;
	uint64_t			transaction_id;
	uint32_t 			reserved;
	uint32_t			range_count; /* Always 1 in this case */
	hv_vmbus_multipage_buffer	range;
} __packed hv_vmbus_channel_packet_multipage_buffer;

enum {
	HV_VMBUS_MESSAGE_CONNECTION_ID	= 1,
	HV_VMBUS_MESSAGE_PORT_ID	= 1,
	HV_VMBUS_EVENT_CONNECTION_ID	= 2,
	HV_VMBUS_EVENT_PORT_ID		= 2,
	HV_VMBUS_MONITOR_CONNECTION_ID	= 3,
	HV_VMBUS_MONITOR_PORT_ID	= 3,
	HV_VMBUS_MESSAGE_SINT		= 2
};

#define HV_PRESENT_BIT		0x80000000

#define HV_HYPERCALL_PARAM_ALIGN sizeof(uint64_t)

/*
 *  Connection identifier type
 */
typedef union {
	uint32_t		as_uint32_t;
	struct {
		uint32_t	id:24;
		uint32_t	reserved:8;
	} u;

} __packed hv_vmbus_connection_id;

/*
 * Definition of the hv_vmbus_signal_event hypercall input structure
 */
typedef struct {
	hv_vmbus_connection_id	connection_id;
	uint16_t		flag_number;
	uint16_t		rsvd_z;
} __packed hv_vmbus_input_signal_event;

typedef struct {
	uint64_t			align8;
	hv_vmbus_input_signal_event	event;
} __packed hv_vmbus_input_signal_event_buffer;

typedef struct {
	uint64_t	guest_id;
	void*		hypercall_page;
	hv_bool_uint8_t	syn_ic_initialized;
	/*
	 * This is used as an input param to HV_CALL_SIGNAL_EVENT hypercall.
	 * The input param is immutable  in our usage and
	 * must be dynamic mem (vs stack or global).
	 */
	hv_vmbus_input_signal_event_buffer	*signal_event_buffer;
	/*
	 * 8-bytes aligned of the buffer above
	 */
	hv_vmbus_input_signal_event		*signal_event_param;

	hv_vmbus_handle	syn_ic_msg_page[MAXCPU];
	hv_vmbus_handle	syn_ic_event_page[MAXCPU];
} hv_vmbus_context;

/*
 * Define hypervisor message types
 */
typedef enum {

	HV_MESSAGE_TYPE_NONE				= 0x00000000,

	/*
	 * Memory access messages
	 */
	HV_MESSAGE_TYPE_UNMAPPED_GPA			= 0x80000000,
	HV_MESSAGE_TYPE_GPA_INTERCEPT			= 0x80000001,

	/*
	 * Timer notification messages
	 */
	HV_MESSAGE_TIMER_EXPIRED			= 0x80000010,

	/*
	 * Error messages
	 */
	HV_MESSAGE_TYPE_INVALID_VP_REGISTER_VALUE	= 0x80000020,
	HV_MESSAGE_TYPE_UNRECOVERABLE_EXCEPTION		= 0x80000021,
	HV_MESSAGE_TYPE_UNSUPPORTED_FEATURE		= 0x80000022,

	/*
	 * Trace buffer complete messages
	 */
	HV_MESSAGE_TYPE_EVENT_LOG_BUFFER_COMPLETE	= 0x80000040,

	/*
	 * Platform-specific processor intercept messages
	 */
	HV_MESSAGE_TYPE_X64_IO_PORT_INTERCEPT		= 0x80010000,
	HV_MESSAGE_TYPE_X64_MSR_INTERCEPT		= 0x80010001,
	HV_MESSAGE_TYPE_X64_CPU_INTERCEPT		= 0x80010002,
	HV_MESSAGE_TYPE_X64_EXCEPTION_INTERCEPT		= 0x80010003,
	HV_MESSAGE_TYPE_X64_APIC_EOI			= 0x80010004,
	HV_MESSAGE_TYPE_X64_LEGACY_FP_ERROR		= 0x80010005

} hv_vmbus_msg_type;

/*
 * Define port identifier type
 */
typedef union _hv_vmbus_port_id {
	uint32_t	as_uint32_t;
	struct {
		uint32_t	id:24;
		uint32_t	reserved:8;
	} u ;
} hv_vmbus_port_id;

/*
 * Define synthetic interrupt controller message flag
 */
typedef union {
	uint8_t	as_uint8_t;
	struct {
		uint8_t	message_pending:1;
		uint8_t	reserved:7;
	} u;
} hv_vmbus_msg_flags;

typedef uint64_t hv_vmbus_partition_id;

/*
 * Define synthetic interrupt controller message header
 */
typedef struct {
	hv_vmbus_msg_type	message_type;
	uint8_t			payload_size;
	hv_vmbus_msg_flags	message_flags;
	uint8_t			reserved[2];
	union {
		hv_vmbus_partition_id	sender;
		hv_vmbus_port_id	port;
	} u;
} hv_vmbus_msg_header;

/*
 *  Define synthetic interrupt controller message format
 */
typedef struct {
	hv_vmbus_msg_header	header;
	union {
		uint64_t	payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
	} u ;
} hv_vmbus_message;

/*
 *  Maximum channels is determined by the size of the interrupt
 *  page which is PAGE_SIZE. 1/2 of PAGE_SIZE is for
 *  send endpoint interrupt and the other is receive
 *  endpoint interrupt.
 *
 *   Note: (PAGE_SIZE >> 1) << 3 allocates 16348 channels
 */
#define HV_MAX_NUM_CHANNELS			(PAGE_SIZE >> 1) << 3

/*
 * (The value here must be in multiple of 32)
 */
#define HV_MAX_NUM_CHANNELS_SUPPORTED		256

/*
 * VM Bus connection states
 */
typedef enum {
	HV_DISCONNECTED,
	HV_CONNECTING,
	HV_CONNECTED,
	HV_DISCONNECTING
} hv_vmbus_connect_state;

#define HV_MAX_SIZE_CHANNEL_MESSAGE	HV_MESSAGE_PAYLOAD_BYTE_COUNT


typedef struct {
	hv_vmbus_connect_state			connect_state;
	uint32_t				next_gpadl_handle;
	/**
	 * Represents channel interrupts. Each bit position
	 * represents a channel.
	 * When a channel sends an interrupt via VMBUS, it
	 * finds its bit in the send_interrupt_page, set it and
	 * calls Hv to generate a port event. The other end
	 * receives the port event and parse the
	 * recv_interrupt_page to see which bit is set
	 */
	void					*interrupt_page;
	void					*send_interrupt_page;
	void					*recv_interrupt_page;
	/*
	 * 2 pages - 1st page for parent->child
	 * notification and 2nd is child->parent
	 * notification
	 */
	void					*monitor_pages;
	TAILQ_HEAD(, hv_vmbus_channel_msg_info)	channel_msg_anchor;
	struct mtx				channel_msg_lock;
	/**
	 * List of channels
	 */
	TAILQ_HEAD(, hv_vmbus_channel)		channel_anchor;
	struct mtx				channel_lock;

	hv_vmbus_handle				work_queue;
	struct sema				control_sema;
} hv_vmbus_connection;

/*
 * Declare the MSR used to identify the guest OS
 */
#define HV_X64_MSR_GUEST_OS_ID	0x40000000

typedef union {
	uint64_t as_uint64_t;
	struct {
		uint64_t build_number		: 16;
		uint64_t service_version	: 8; /* Service Pack, etc. */
		uint64_t minor_version		: 8;
		uint64_t major_version		: 8;
		/*
		 * HV_GUEST_OS_MICROSOFT_IDS (If Vendor=MS)
		 * HV_GUEST_OS_VENDOR
		 */
		uint64_t os_id			: 8;
		uint64_t vendor_id		: 16;
	} u;
} hv_vmbus_x64_msr_guest_os_id_contents;

/*
 *  Declare the MSR used to setup pages used to communicate with the hypervisor
 */
#define HV_X64_MSR_HYPERCALL	0x40000001

typedef union {
	uint64_t as_uint64_t;
	struct {
		uint64_t enable :1;
		uint64_t reserved :11;
		uint64_t guest_physical_address :52;
	} u;
} hv_vmbus_x64_msr_hypercall_contents;

typedef union {
	uint32_t as_uint32_t;
	struct {
		uint32_t group_enable :4;
		uint32_t rsvd_z :28;
	} u;
} hv_vmbus_monitor_trigger_state;

typedef union {
	uint64_t as_uint64_t;
	struct {
		uint32_t pending;
		uint32_t armed;
	} u;
} hv_vmbus_monitor_trigger_group;

typedef struct {
	hv_vmbus_connection_id	connection_id;
	uint16_t		flag_number;
	uint16_t		rsvd_z;
} hv_vmbus_monitor_parameter;

/*
 * hv_vmbus_monitor_page Layout
 * ------------------------------------------------------
 * | 0   | trigger_state (4 bytes) | Rsvd1 (4 bytes)     |
 * | 8   | trigger_group[0]                              |
 * | 10  | trigger_group[1]                              |
 * | 18  | trigger_group[2]                              |
 * | 20  | trigger_group[3]                              |
 * | 28  | Rsvd2[0]                                      |
 * | 30  | Rsvd2[1]                                      |
 * | 38  | Rsvd2[2]                                      |
 * | 40  | next_check_time[0][0] | next_check_time[0][1] |
 * | ...                                                 |
 * | 240 | latency[0][0..3]                              |
 * | 340 | Rsvz3[0]                                      |
 * | 440 | parameter[0][0]                               |
 * | 448 | parameter[0][1]                               |
 * | ...                                                 |
 * | 840 | Rsvd4[0]                                      |
 * ------------------------------------------------------
 */

typedef struct {
	hv_vmbus_monitor_trigger_state	trigger_state;
	uint32_t			rsvd_z1;

	hv_vmbus_monitor_trigger_group	trigger_group[4];
	uint64_t			rsvd_z2[3];

	int32_t				next_check_time[4][32];

	uint16_t			latency[4][32];
	uint64_t			rsvd_z3[32];

	hv_vmbus_monitor_parameter	parameter[4][32];

	uint8_t				rsvd_z4[1984];
} hv_vmbus_monitor_page;

/*
 * The below CPUID leaves are present if VersionAndFeatures.HypervisorPresent
 * is set by CPUID(HV_CPU_ID_FUNCTION_VERSION_AND_FEATURES).
 */
typedef enum {
	HV_CPU_ID_FUNCTION_VERSION_AND_FEATURES			= 0x00000001,
	HV_CPU_ID_FUNCTION_HV_VENDOR_AND_MAX_FUNCTION		= 0x40000000,
	HV_CPU_ID_FUNCTION_HV_INTERFACE				= 0x40000001,
	/*
	 * The remaining functions depend on the value
	 * of hv_cpu_id_function_interface
	 */
	HV_CPU_ID_FUNCTION_MS_HV_VERSION			= 0x40000002,
	HV_CPU_ID_FUNCTION_MS_HV_FEATURES			= 0x40000003,
	HV_CPU_ID_FUNCTION_MS_HV_ENLIGHTENMENT_INFORMATION	= 0x40000004,
	HV_CPU_ID_FUNCTION_MS_HV_IMPLEMENTATION_LIMITS		= 0x40000005

} hv_vmbus_cpuid_function;

/*
 * Define the format of the SIMP register
 */
typedef union {
	uint64_t as_uint64_t;
	struct {
		uint64_t simp_enabled	: 1;
		uint64_t preserved	: 11;
		uint64_t base_simp_gpa	: 52;
	} u;
} hv_vmbus_synic_simp;

/*
 * Define the format of the SIEFP register
 */
typedef union {
	uint64_t as_uint64_t;
	struct {
		uint64_t siefp_enabled	: 1;
		uint64_t preserved	: 11;
		uint64_t base_siefp_gpa	: 52;
	} u;
} hv_vmbus_synic_siefp;

/*
 * Define synthetic interrupt source
 */
typedef union {
	uint64_t as_uint64_t;
	struct {
		uint64_t vector		: 8;
		uint64_t reserved1	: 8;
		uint64_t masked		: 1;
		uint64_t auto_eoi	: 1;
		uint64_t reserved2	: 46;
	} u;
} hv_vmbus_synic_sint;

/*
 * Define syn_ic control register
 */
typedef union _hv_vmbus_synic_scontrol {
    uint64_t as_uint64_t;
    struct {
        uint64_t enable		: 1;
        uint64_t reserved	: 63;
    } u;
} hv_vmbus_synic_scontrol;

/*
 *  Define the hv_vmbus_post_message hypercall input structure
 */
typedef struct {
	hv_vmbus_connection_id	connection_id;
	uint32_t		reserved;
	hv_vmbus_msg_type	message_type;
	uint32_t		payload_size;
	uint64_t		payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
} hv_vmbus_input_post_message;

/*
 * Define the synthetic interrupt controller event flags format
 */
typedef union {
	uint8_t		flags8[HV_EVENT_FLAGS_BYTE_COUNT];
	uint32_t	flags32[HV_EVENT_FLAGS_DWORD_COUNT];
} hv_vmbus_synic_event_flags;


/*
 * Define synthetic interrupt controller model specific registers
 */
#define HV_X64_MSR_SCONTROL   (0x40000080)
#define HV_X64_MSR_SVERSION   (0x40000081)
#define HV_X64_MSR_SIEFP      (0x40000082)
#define HV_X64_MSR_SIMP       (0x40000083)
#define HV_X64_MSR_EOM        (0x40000084)

#define HV_X64_MSR_SINT0      (0x40000090)
#define HV_X64_MSR_SINT1      (0x40000091)
#define HV_X64_MSR_SINT2      (0x40000092)
#define HV_X64_MSR_SINT3      (0x40000093)
#define HV_X64_MSR_SINT4      (0x40000094)
#define HV_X64_MSR_SINT5      (0x40000095)
#define HV_X64_MSR_SINT6      (0x40000096)
#define HV_X64_MSR_SINT7      (0x40000097)
#define HV_X64_MSR_SINT8      (0x40000098)
#define HV_X64_MSR_SINT9      (0x40000099)
#define HV_X64_MSR_SINT10     (0x4000009A)
#define HV_X64_MSR_SINT11     (0x4000009B)
#define HV_X64_MSR_SINT12     (0x4000009C)
#define HV_X64_MSR_SINT13     (0x4000009D)
#define HV_X64_MSR_SINT14     (0x4000009E)
#define HV_X64_MSR_SINT15     (0x4000009F)

/*
 * Declare the various hypercall operations
 */
typedef enum {
	HV_CALL_POST_MESSAGE	= 0x005c,
	HV_CALL_SIGNAL_EVENT	= 0x005d,
} hv_vmbus_call_code;

/**
 * Global variables
 */

extern hv_vmbus_context		hv_vmbus_g_context;
extern hv_vmbus_connection	hv_vmbus_g_connection;


/*
 * Private, VM Bus functions
 */

int			hv_vmbus_ring_buffer_init(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

void			hv_ring_buffer_cleanup(
				hv_vmbus_ring_buffer_info	*ring_info);

int			hv_ring_buffer_write(
				hv_vmbus_ring_buffer_info	*ring_info,
				hv_vmbus_sg_buffer_list		sg_buffers[],
				uint32_t			sg_buff_count);

int			hv_ring_buffer_peek(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len);

int			hv_ring_buffer_read(
				hv_vmbus_ring_buffer_info	*ring_info,
				void				*buffer,
				uint32_t			buffer_len,
				uint32_t			offset);

uint32_t		hv_vmbus_get_ring_buffer_interrupt_mask(
				hv_vmbus_ring_buffer_info	*ring_info);

void			hv_vmbus_dump_ring_info(
				hv_vmbus_ring_buffer_info	*ring_info,
				char				*prefix);

hv_vmbus_channel*	hv_vmbus_allocate_channel(void);
void			hv_vmbus_free_vmbus_channel(hv_vmbus_channel *channel);
void			hv_vmbus_on_channel_message(void *context);
int			hv_vmbus_request_channel_offers(void);
void			hv_vmbus_release_unattached_channels(void);
int			hv_vmbus_init(void);
void			hv_vmbus_cleanup(void);

uint16_t		hv_vmbus_post_msg_via_msg_ipc(
				hv_vmbus_connection_id	connection_id,
				hv_vmbus_msg_type	message_type,
				void			*payload,
				size_t			payload_size);

uint16_t		hv_vmbus_signal_event(void);
void			hv_vmbus_synic_init(void *irq_arg);
void			hv_vmbus_synic_cleanup(void *arg);
int			hv_vmbus_query_hypervisor_presence(void);

struct hv_device*	hv_vmbus_child_device_create(
				hv_guid			device_type,
				hv_guid			device_instance,
				hv_vmbus_channel	*channel);

int			hv_vmbus_child_device_register(
					struct hv_device *child_dev);
int			hv_vmbus_child_device_unregister(
					struct hv_device *child_dev);
hv_vmbus_channel*	hv_vmbus_get_channel_from_rel_id(uint32_t rel_id);

/**
 * Connection interfaces
 */
int			hv_vmbus_connect(void);
int			hv_vmbus_disconnect(void);
int			hv_vmbus_post_message(void *buffer, size_t buf_size);
int			hv_vmbus_set_event(uint32_t child_rel_id);
void			hv_vmbus_on_events(void *);


/*
 * The guest OS needs to register the guest ID with the hypervisor.
 * The guest ID is a 64 bit entity and the structure of this ID is
 * specified in the Hyper-V specification:
 *
 * http://msdn.microsoft.com/en-us/library/windows/
 * hardware/ff542653%28v=vs.85%29.aspx
 *
 * While the current guideline does not specify how FreeBSD guest ID(s)
 * need to be generated, our plan is to publish the guidelines for
 * FreeBSD and other guest operating systems that currently are hosted
 * on Hyper-V. The implementation here conforms to this yet
 * unpublished guidelines.
 *
 * Bit(s)
 * 63 - Indicates if the OS is Open Source or not; 1 is Open Source
 * 62:56 - Os Type; Linux is 0x100, FreeBSD is 0x200
 * 55:48 - Distro specific identification
 * 47:16 - FreeBSD kernel version number
 * 15:0  - Distro specific identification
 *
 */

#define HV_FREEBSD_VENDOR_ID	0x8200
#define HV_FREEBSD_GUEST_ID	hv_generate_guest_id(0,0)

static inline  uint64_t hv_generate_guest_id(
	uint8_t distro_id_part1,
	uint16_t distro_id_part2)
{
	uint64_t guest_id;
	guest_id =  (((uint64_t)HV_FREEBSD_VENDOR_ID) << 48);
	guest_id |= (((uint64_t)(distro_id_part1)) << 48);
	guest_id |= (((uint64_t)(__FreeBSD_version)) << 16); /* in param.h */
	guest_id |= ((uint64_t)(distro_id_part2));
	return guest_id;
}

typedef struct {
	unsigned int	vector;
	void		*page_buffers[2];
} hv_setup_args;

#endif  /* __HYPERV_PRIV_H__ */
