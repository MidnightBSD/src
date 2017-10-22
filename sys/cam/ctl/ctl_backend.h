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
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend.h#2 $
 * $FreeBSD$
 */
/*
 * CTL backend driver definitions
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_BACKEND_H_
#define	_CTL_BACKEND_H_

/*
 * XXX KDM move this to another header file?
 */
#define	CTL_BE_NAME_LEN		32

/*
 * The ID_REQ flag is used to say that the caller has requested a
 * particular LUN ID in the req_lun_id field.  If we cannot allocate that
 * LUN ID, the ctl_add_lun() call will fail.
 *
 * The POWERED_OFF flag tells us that the LUN should default to the powered
 * off state.  It will return 0x04,0x02 until it is powered up.  ("Logical
 * unit not ready, initializing command required.")
 *
 * The INOPERABLE flag tells us that this LUN is not operable for whatever
 * reason.  This means that user data may have been (or has been?) lost.
 * We will return 0x31,0x00 ("Medium format corrupted") until the host
 * issues a FORMAT UNIT command to clear the error.
 *
 * The PRIMARY flag tells us that this LUN is registered as a Primary LUN
 * which is accessible via the Master shelf controller in an HA. This flag
 * being set indicates a Primary LUN. This flag being reset represents a
 * Secondary LUN controlled by the Secondary controller in an HA
 * configuration. Flag is applicable at this time to T_DIRECT types. 
 *
 * The SERIAL_NUM flag tells us that the serial_num field is filled in and
 * valid for use in SCSI INQUIRY VPD page 0x80.
 *
 * The DEVID flag tells us that the device_id field is filled in and
 * valid for use in SCSI INQUIRY VPD page 0x83.
 *
 * The DEV_TYPE flag tells us that the device_type field is filled in.
 */
typedef enum {
	CTL_LUN_FLAG_ID_REQ		= 0x01,
	CTL_LUN_FLAG_POWERED_OFF	= 0x02,
	CTL_LUN_FLAG_INOPERABLE		= 0x04,
	CTL_LUN_FLAG_PRIMARY		= 0x08,
	CTL_LUN_FLAG_SERIAL_NUM		= 0x10,
	CTL_LUN_FLAG_DEVID		= 0x20,
	CTL_LUN_FLAG_DEV_TYPE		= 0x40
} ctl_backend_lun_flags;

#ifdef _KERNEL

#define CTL_BACKEND_DECLARE(name, driver) \
	static int name ## _modevent(module_t mod, int type, void *data) \
	{ \
		switch (type) { \
		case MOD_LOAD: \
			ctl_backend_register( \
				(struct ctl_backend_driver *)data); \
			break; \
		case MOD_UNLOAD: \
			printf(#name " module unload - not possible for this module type\n"); \
			return EINVAL; \
		default: \
			return EOPNOTSUPP; \
		} \
		return 0; \
	} \
	static moduledata_t name ## _mod = { \
		#name, \
		name ## _modevent, \
		(void *)&driver \
	}; \
	DECLARE_MODULE(name, name ## _mod, SI_SUB_CONFIGURE, SI_ORDER_FOURTH); \
	MODULE_DEPEND(name, ctl, 1, 1, 1); \
	MODULE_DEPEND(name, cam, 1, 1, 1)


typedef enum {
	CTL_LUN_CONFIG_OK,
	CTL_LUN_CONFIG_FAILURE
} ctl_lun_config_status;

typedef void (*be_callback_t)(void *be_lun);
typedef void (*be_lun_config_t)(void *be_lun,
				ctl_lun_config_status status);

/*
 * The lun_type field is the SCSI device type of this particular LUN.  In
 * general, this should be T_DIRECT, although backends will want to create
 * a processor LUN, typically at LUN 0.  See scsi_all.h for the defines for
 * the various SCSI device types.
 *
 * The flags are described above.
 *
 * The be_lun field is the backend driver's own context that will get
 * passsed back so that it can tell which LUN CTL is referencing.
 *
 * maxlba is the maximum accessible LBA on the LUN.  Note that this is
 * different from the capacity of the array.  capacity = maxlba + 1
 *
 * blocksize is the size, in bytes, of each LBA on the LUN.  In general
 * this should be 512.  In theory CTL should be able to handle other block
 * sizes.  Host application software may not deal with it very well, though.
 *
 * req_lun_id is the requested LUN ID.  CTL only pays attention to this
 * field if the CTL_LUN_FLAG_ID_REQ flag is set.  If the requested LUN ID is
 * not available, the LUN addition will fail.  If a particular LUN ID isn't
 * requested, the first available LUN ID will be allocated.
 *
 * serial_num is the device serial number returned in the SCSI INQUIRY VPD
 * page 0x80.  This should be a unique, per-shelf value.  The data inside
 * this field should be ASCII only, left aligned, and any unused space
 * should be padded out with ASCII spaces.  This field should NOT be NULL
 * terminated.
 *
 * device_id is the T10 device identifier returned in the SCSI INQUIRY VPD
 * page 0x83.  This should be a unique, per-LUN value.  The data inside
 * this field should be ASCII only, left aligned, and any unused space
 * should be padded with ASCII spaces.  This field should NOT be NULL
 * terminated.
 *
 * The lun_shutdown() method is the callback for the ctl_invalidate_lun()
 * call.  It is called when all outstanding I/O for that LUN has been
 * completed and CTL has deleted the resources for that LUN.  When the CTL
 * backend gets this call, it can safely free its per-LUN resources.
 *
 * The lun_config_status() method is the callback for the ctl_add_lun()
 * call.  It is called when the LUN is successfully added, or when LUN
 * addition fails.  If the LUN is successfully added, the backend may call
 * the ctl_enable_lun() method to enable the LUN.
 *
 * The be field is a pointer to the ctl_backend_driver structure, which
 * contains the backend methods to be called by CTL.
 *
 * The ctl_lun field is for CTL internal use only, and should not be used
 * by the backend.
 *
 * The links field is for CTL internal use only, and should not be used by
 * the backend.
 */
struct ctl_be_lun {
	uint8_t			lun_type;	/* passed to CTL */
	ctl_backend_lun_flags	flags;		/* passed to CTL */
	void			*be_lun;	/* passed to CTL */
	uint64_t		maxlba;		/* passed to CTL */
	uint32_t		blocksize;	/* passed to CTL */
	uint32_t		req_lun_id;	/* passed to CTL */
	uint32_t		lun_id;		/* returned from CTL */
	uint8_t			serial_num[CTL_SN_LEN];	 /* passed to CTL */
	uint8_t			device_id[CTL_DEVID_LEN];/* passed to CTL */
	be_callback_t		lun_shutdown;	/* passed to CTL */
	be_lun_config_t		lun_config_status; /* passed to CTL */
	struct ctl_backend_driver *be;		/* passed to CTL */
	void			*ctl_lun;	/* used by CTL */
	STAILQ_ENTRY(ctl_be_lun) links;		/* used by CTL */
};

typedef enum {
	CTL_BE_FLAG_NONE	= 0x00,	/* no flags */
	CTL_BE_FLAG_HAS_CONFIG	= 0x01,	/* can do config reads, writes */
	CTL_BE_FLAG_INTERNAL	= 0x02	/* don't inc mod refcount */
} ctl_backend_flags;

typedef int (*be_init_t)(void);
typedef int (*be_func_t)(union ctl_io *io);
typedef void (*be_vfunc_t)(union ctl_io *io);
typedef int (*be_ioctl_t)(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
			  struct thread *td);
typedef int (*be_luninfo_t)(void *be_lun, struct sbuf *sb);

struct ctl_backend_driver {
	char		  name[CTL_BE_NAME_LEN]; /* passed to CTL */
	ctl_backend_flags flags;	         /* passed to CTL */
	be_init_t	  init;			 /* passed to CTL */
	be_func_t	  data_submit;		 /* passed to CTL */
	be_func_t	  data_move_done;	 /* passed to CTL */
	be_func_t	  config_read;		 /* passed to CTL */
	be_func_t	  config_write;		 /* passed to CTL */
	be_ioctl_t	  ioctl;		 /* passed to CTL */
	be_luninfo_t	  lun_info;		 /* passed to CTL */
#ifdef CS_BE_CONFIG_MOVE_DONE_IS_NOT_USED
	be_func_t	  config_move_done;	 /* passed to backend */
#endif
#if 0
	be_vfunc_t	  config_write_done;	 /* passed to backend */
#endif
	u_int		  num_luns;		 /* used by CTL */
	STAILQ_ENTRY(ctl_backend_driver) links;	 /* used by CTL */
};

int ctl_backend_register(struct ctl_backend_driver *be);
int ctl_backend_deregister(struct ctl_backend_driver *be);
struct ctl_backend_driver *ctl_backend_find(char *backend_name);

/*
 * To add a LUN, first call ctl_add_lun().  You will get the lun_config_status()
 * callback when the LUN addition has either succeeded or failed.
 *
 * Once you get that callback, you can then call ctl_enable_lun() to enable
 * the LUN.
 */
int ctl_add_lun(struct ctl_be_lun *be_lun);
int ctl_enable_lun(struct ctl_be_lun *be_lun);

/*
 * To delete a LUN, first call ctl_disable_lun(), then
 * ctl_invalidate_lun().  You will get the lun_shutdown() callback when all
 * I/O to the LUN has completed and the LUN has been deleted.
 */
int ctl_disable_lun(struct ctl_be_lun *be_lun);
int ctl_invalidate_lun(struct ctl_be_lun *be_lun);

/*
 * To start a LUN (transition from powered off to powered on state) call
 * ctl_start_lun().  To stop a LUN (transition from powered on to powered
 * off state) call ctl_stop_lun().
 */
int ctl_start_lun(struct ctl_be_lun *be_lun);
int ctl_stop_lun(struct ctl_be_lun *be_lun);

/*
 * If a LUN is inoperable, call ctl_lun_inoperable().  Generally the LUN
 * will become operable once again when the user issues the SCSI FORMAT UNIT
 * command.  (CTL will automatically clear the inoperable flag.)  If we
 * need to re-enable the LUN, we can call ctl_lun_operable() to enable it
 * without a SCSI command.
 */
int ctl_lun_inoperable(struct ctl_be_lun *be_lun);
int ctl_lun_operable(struct ctl_be_lun *be_lun);

/*
 * If a LUN is locked on or unlocked from a power/APS standpoint, call
 * ctl_lun_power_lock() to update the current status in CTL's APS subpage.
 * Set the lock flag to 1 to lock the LUN, set it to 0 to unlock the LUN.
 */
int ctl_lun_power_lock(struct ctl_be_lun *be_lun, struct ctl_nexus *nexus,
		       int lock);

/*
 * To take a LUN offline, call ctl_lun_offline().  Generally the LUN will
 * be online again once the user sends a SCSI START STOP UNIT command with
 * the start and on/offline bits set.  The backend can bring the LUN back
 * online via the ctl_lun_online() function, if necessary.
 */
int ctl_lun_offline(struct ctl_be_lun *be_lun);
int ctl_lun_online(struct ctl_be_lun *be_lun);

/*
 * Let the backend notify the initiator about changed capacity.
 */
void ctl_lun_capacity_changed(struct ctl_be_lun *be_lun);

#endif /* _KERNEL */
#endif /* _CTL_BACKEND_H_ */

/*
 * vim: ts=8
 */
