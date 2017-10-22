/*-
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl.h#5 $
 * $FreeBSD$
 */
/*
 * Function definitions used both within CTL and potentially in various CTL
 * clients.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_H_
#define	_CTL_H_

#define	ctl_min(x,y)	(((x) < (y)) ? (x) : (y))
#define	CTL_RETVAL_COMPLETE	0
#define	CTL_RETVAL_QUEUED	1
#define	CTL_RETVAL_ALLOCATED	2
#define	CTL_RETVAL_ERROR	3

typedef enum {
	CTL_PORT_NONE		= 0x00,
	CTL_PORT_FC		= 0x01,
	CTL_PORT_SCSI		= 0x02,
	CTL_PORT_IOCTL		= 0x04,
	CTL_PORT_INTERNAL	= 0x08,
	CTL_PORT_ALL		= 0xff,
	CTL_PORT_ISC		= 0x100 // FC port for inter-shelf communication
} ctl_port_type;

struct ctl_port_entry {
	ctl_port_type		port_type;
	char			port_name[64];
	int32_t			targ_port;
	int			physical_port;
	int			virtual_port;
	u_int			flags;
#define	CTL_PORT_WWNN_VALID	0x01
#define	CTL_PORT_WWPN_VALID	0x02
	uint64_t		wwnn;
	uint64_t		wwpn;
	int			online;
};

struct ctl_modepage_header {
	uint8_t page_code;
	uint8_t subpage;
	int32_t len_used;
	int32_t len_left;
};

struct ctl_modepage_aps {
	struct ctl_modepage_header header;
	uint8_t lock_active;
};

union ctl_modepage_info {
	struct ctl_modepage_header header;
	struct ctl_modepage_aps aps;
};

/*
 * Serial number length, for VPD page 0x80.
 */
#define	CTL_SN_LEN	16

/*
 * Device ID length, for VPD page 0x83.
 */
#define	CTL_DEVID_LEN	16
/*
 * WWPN length, for VPD page 0x83.
 */
#define CTL_WWPN_LEN   8

/*
 * Unit attention types. ASC/ASCQ values for these should be placed in
 * ctl_build_ua.  These are also listed in order of reporting priority.
 * i.e. a poweron UA is reported first, bus reset second, etc.
 */
typedef enum {
	CTL_UA_NONE		= 0x0000,
	CTL_UA_POWERON		= 0x0001,
	CTL_UA_BUS_RESET	= 0x0002,
	CTL_UA_TARG_RESET	= 0x0004,
	CTL_UA_LUN_RESET	= 0x0008,
	CTL_UA_LUN_CHANGE	= 0x0010,
	CTL_UA_MODE_CHANGE	= 0x0020,
	CTL_UA_LOG_CHANGE	= 0x0040,
	CTL_UA_LVD		= 0x0080,
	CTL_UA_SE		= 0x0100,
	CTL_UA_RES_PREEMPT	= 0x0200,
	CTL_UA_RES_RELEASE	= 0x0400,
	CTL_UA_REG_PREEMPT  	= 0x0800,
	CTL_UA_ASYM_ACC_CHANGE  = 0x1000,
	CTL_UA_CAPACITY_CHANGED = 0x2000
} ctl_ua_type;

#ifdef	_KERNEL

MALLOC_DECLARE(M_CTL);

typedef enum {
	CTL_THREAD_NONE		= 0x00,
	CTL_THREAD_WAKEUP	= 0x01
} ctl_thread_flags;

struct ctl_thread {
	void			(*thread_func)(void *arg);
	void			*arg;
	struct cv		wait_queue;
	const char		*thread_name;
	ctl_thread_flags	thread_flags;
	struct completion	*thread_event;
	struct task_struct	*task;
};

struct ctl_page_index;

#ifdef SYSCTL_DECL	/* from sysctl.h */
SYSCTL_DECL(_kern_cam_ctl);
#endif

/*
 * Call these routines to enable or disable front end ports.
 */
int ctl_port_enable(ctl_port_type port_type);
int ctl_port_disable(ctl_port_type port_type);
/*
 * This routine grabs a list of frontend ports.
 */
int ctl_port_list(struct ctl_port_entry *entries, int num_entries_alloced,
		  int *num_entries_filled, int *num_entries_dropped,
		  ctl_port_type port_type, int no_virtual);

/*
 * Put a string into an sbuf, escaping characters that are illegal or not
 * recommended in XML.  Note this doesn't escape everything, just > < and &.
 */
int ctl_sbuf_printf_esc(struct sbuf *sb, char *str);

int ctl_ffz(uint32_t *mask, uint32_t size);
int ctl_set_mask(uint32_t *mask, uint32_t bit);
int ctl_clear_mask(uint32_t *mask, uint32_t bit);
int ctl_is_set(uint32_t *mask, uint32_t bit);
int ctl_control_page_handler(struct ctl_scsiio *ctsio,
			     struct ctl_page_index *page_index,
			     uint8_t *page_ptr);
/**
int ctl_failover_sp_handler(struct ctl_scsiio *ctsio,
			    struct ctl_page_index *page_index,
			    uint8_t *page_ptr);
**/
int ctl_power_sp_handler(struct ctl_scsiio *ctsio,
			 struct ctl_page_index *page_index, uint8_t *page_ptr);
int ctl_power_sp_sense_handler(struct ctl_scsiio *ctsio,
			       struct ctl_page_index *page_index, int pc);
int ctl_aps_sp_handler(struct ctl_scsiio *ctsio,
		       struct ctl_page_index *page_index, uint8_t *page_ptr);
int ctl_debugconf_sp_sense_handler(struct ctl_scsiio *ctsio,
				   struct ctl_page_index *page_index,
				   int pc);
int ctl_debugconf_sp_select_handler(struct ctl_scsiio *ctsio,
				    struct ctl_page_index *page_index,
				    uint8_t *page_ptr);
int ctl_config_move_done(union ctl_io *io);
void ctl_datamove(union ctl_io *io);
void ctl_done(union ctl_io *io);
void ctl_config_write_done(union ctl_io *io);
#if 0
int ctl_thread(void *arg);
#endif
void ctl_wakeup_thread(void);
#if 0
struct ctl_thread *ctl_create_thread(void (*thread_func)
	(void *thread_arg), void *thread_arg, const char *thread_name);
void ctl_signal_thread(struct ctl_thread *thread);
void ctl_shutdown_thread(struct ctl_thread *thread);
#endif
void ctl_portDB_changed(int portnum);
void ctl_init_isc_msg(void);

#endif	/* _KERNEL */

#endif	/* _CTL_H_ */

/*
 * vim: ts=8
 */
