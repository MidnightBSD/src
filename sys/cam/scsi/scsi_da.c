/*-
 * Implementation of SCSI Direct Access Peripheral driver for CAM.
 *
 * Copyright (c) 1997 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/cons.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>
#endif /* _KERNEL */

#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#endif /* _KERNEL */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_sim.h>

#include <cam/scsi/scsi_message.h>

#ifndef _KERNEL 
#include <cam/scsi/scsi_da.h>
#endif /* !_KERNEL */

#ifdef _KERNEL
typedef enum {
	DA_STATE_PROBE,
	DA_STATE_PROBE2,
	DA_STATE_NORMAL
} da_state;

typedef enum {
	DA_FLAG_PACK_INVALID	= 0x001,
	DA_FLAG_NEW_PACK	= 0x002,
	DA_FLAG_PACK_LOCKED	= 0x004,
	DA_FLAG_PACK_REMOVABLE	= 0x008,
	DA_FLAG_NEED_OTAG	= 0x020,
	DA_FLAG_WENT_IDLE	= 0x040,
	DA_FLAG_RETRY_UA	= 0x080,
	DA_FLAG_OPEN		= 0x100,
	DA_FLAG_SCTX_INIT	= 0x200,
	DA_FLAG_CAN_RC16	= 0x400
} da_flags;

typedef enum {
	DA_Q_NONE		= 0x00,
	DA_Q_NO_SYNC_CACHE	= 0x01,
	DA_Q_NO_6_BYTE		= 0x02,
	DA_Q_NO_PREVENT		= 0x04,
	DA_Q_4K			= 0x08
} da_quirks;

typedef enum {
	DA_CCB_PROBE		= 0x01,
	DA_CCB_PROBE2		= 0x02,
	DA_CCB_BUFFER_IO	= 0x03,
	DA_CCB_WAITING		= 0x04,
	DA_CCB_DUMP		= 0x05,
	DA_CCB_DELETE		= 0x06,
	DA_CCB_TYPE_MASK	= 0x0F,
	DA_CCB_RETRY_UA		= 0x10
} da_ccb_state;

typedef enum {
	DA_DELETE_NONE,
	DA_DELETE_DISABLE,
	DA_DELETE_ZERO,
	DA_DELETE_WS10,
	DA_DELETE_WS16,
	DA_DELETE_UNMAP,
	DA_DELETE_MAX = DA_DELETE_UNMAP
} da_delete_methods;

static const char *da_delete_method_names[] =
    { "NONE", "DISABLE", "ZERO", "WS10", "WS16", "UNMAP" };

/* Offsets into our private area for storing information */
#define ccb_state	ppriv_field0
#define ccb_bp		ppriv_ptr1

struct disk_params {
	u_int8_t  heads;
	u_int32_t cylinders;
	u_int8_t  secs_per_track;
	u_int32_t secsize;	/* Number of bytes/sector */
	u_int64_t sectors;	/* total number sectors */
	u_int     stripesize;
	u_int     stripeoffset;
};

#define UNMAP_MAX_RANGES	512

struct da_softc {
	struct	 bio_queue_head bio_queue;
	struct	 bio_queue_head delete_queue;
	struct	 bio_queue_head delete_run_queue;
	SLIST_ENTRY(da_softc) links;
	LIST_HEAD(, ccb_hdr) pending_ccbs;
	da_state state;
	da_flags flags;	
	da_quirks quirks;
	int	 minimum_cmd_size;
	int	 error_inject;
	int	 ordered_tag_count;
	int	 outstanding_cmds;
	int	 unmap_max_ranges;
	int	 unmap_max_lba;
	int	 delete_running;
	da_delete_methods	 delete_method;
	struct	 disk_params params;
	struct	 disk *disk;
	union	 ccb saved_ccb;
	struct task		sysctl_task;
	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
	struct callout		sendordered_c;
	uint64_t wwpn;
	uint8_t	 unmap_buf[UNMAP_MAX_RANGES * 16 + 8];
};

struct da_quirk_entry {
	struct scsi_inquiry_pattern inq_pat;
	da_quirks quirks;
};

static const char quantum[] = "QUANTUM";
static const char microp[] = "MICROP";

static struct da_quirk_entry da_quirk_table[] =
{
	/* SPI, FC devices */
	{
		/*
		 * Fujitsu M2513A MO drives.
		 * Tested devices: M2513A2 firmware versions 1200 & 1300.
		 * (dip switch selects whether T_DIRECT or T_OPTICAL device)
		 * Reported by: W.Scholten <whs@xs4all.nl>
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FUJITSU", "M2513A", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/* See above. */
		{T_OPTICAL, SIP_MEDIA_REMOVABLE, "FUJITSU", "M2513A", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This particular Fujitsu drive doesn't like the
		 * synchronize cache command.
		 * Reported by: Tom Jackson <toj@gorilla.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "FUJITSU", "M2954*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Matthew Jacob <mjacob@feral.com>
		 * in NetBSD PR kern/6027, August 24, 1998.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2217*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * This drive doesn't like the synchronize cache command
		 * either.  Reported by: Hellmuth Michaelis (hm@kts.org)
		 * (PR 8882).
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, microp, "2112*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: Blaz Zupan <blaz@gold.amis.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "NEC", "D3847*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: Blaz Zupan <blaz@gold.amis.net>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "MAVERICK 540S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "LPS525S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: walter@pelissero.de
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "LPS540S", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Doesn't work correctly with 6 byte reads/writes.
		 * Returns illegal request, and points to byte 9 of the
		 * 6-byte CDB.
		 * Reported by:  Adam McDougall <bsdx@spawnet.com>
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 4*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/* See above. */
		{T_DIRECT, SIP_MEDIA_FIXED, quantum, "VIKING 2*", "*"},
		/*quirks*/ DA_Q_NO_6_BYTE
	},
	{
		/*
		 * Doesn't like the synchronize cache command.
		 * Reported by: walter@pelissero.de
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "CONNER", "CP3500*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * The CISS RAID controllers do not support SYNC_CACHE
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "COMPAQ", "RAID*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	/* USB mass storage devices supported by umass(4) */
	{
		/*
		 * EXATELECOM (Sigmatel) i-Bead 100/105 USB Flash MP3 Player
		 * PR: kern/51675
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "EXATEL", "i-BEAD10*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Power Quotient Int. (PQI) USB flash key
		 * PR: kern/53067
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "USB Flash Disk*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
 	{
 		/*
 		 * Creative Nomad MUVO mp3 player (USB)
 		 * PR: kern/53094
 		 */
 		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CREATIVE", "NOMAD_MUVO", "*"},
 		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
 	},
	{
		/*
		 * Jungsoft NEXDISK USB flash key
		 * PR: kern/54737
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "JUNGSOFT", "NEXDISK*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * FreeDik USB Mini Data Drive
		 * PR: kern/54786
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FreeDik*", "Mini Data Drive",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sigmatel USB Flash MP3 Player
		 * PR: kern/57046
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SigmaTel", "MSCN", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
	},
	{
		/*
		 * Neuros USB Digital Audio Computer
		 * PR: kern/63645
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "NEUROS", "dig. audio comp.",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SEAGRAND NP-900 MP3 Player
		 * PR: kern/64563
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SEAGRAND", "NP-900*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
	},
	{
		/*
		 * iRiver iFP MP3 player (with UMS Firmware)
		 * PR: kern/54881, i386/63941, kern/66124
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iRiver", "iFP*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
 	},
	{
		/*
		 * Frontier Labs NEX IA+ Digital Audio Player, rev 1.10/0.01
		 * PR: kern/70158
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "FL" , "Nex*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * ZICPlay USB MP3 Player with FM
		 * PR: kern/75057
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "ACTIONS*" , "USB DISK*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * TEAC USB floppy mechanisms
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "TEAC" , "FD-05*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Kingston DataTraveler II+ USB Pen-Drive.
		 * Reported by: Pawel Jakub Dawidek <pjd@FreeBSD.org>
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Kingston" , "DataTraveler II+",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Motorola E398 Mobile Phone (TransFlash memory card).
		 * Reported by: Wojciech A. Koszek <dunstan@FreeBSD.czest.pl>
		 * PR: usb/89889
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Motorola" , "Motorola Phone",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Qware BeatZkey! Pro
		 * PR: usb/79164
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "GENERIC", "USB DISK DEVICE",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Time DPA20B 1GB MP3 Player
		 * PR: usb/81846
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB2.0*", "(FS) FLASH DISK*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Samsung USB key 128Mb
		 * PR: usb/90081
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB-DISK", "FreeDik-FlashUsb",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Kingston DataTraveler 2.0 USB Flash memory.
		 * PR: usb/89196
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Kingston", "DataTraveler 2.0",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Creative MUVO Slim mp3 player (USB)
		 * PR: usb/86131
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CREATIVE", "MuVo Slim",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE|DA_Q_NO_PREVENT
		},
	{
		/*
		 * United MP5512 Portable MP3 Player (2-in-1 USB DISK/MP3)
		 * PR: usb/80487
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "MUSIC DISK",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SanDisk Micro Cruzer 128MB
		 * PR: usb/75970
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "SanDisk" , "Micro Cruzer",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * TOSHIBA TransMemory USB sticks
		 * PR: kern/94660
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "TOSHIBA", "TransMemory",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * PNY USB Flash keys
		 * PR: usb/75578, usb/72344, usb/65436 
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "*" , "USB DISK*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Genesys 6-in-1 Card Reader
		 * PR: usb/94647
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Generic*", "STORAGE DEVICE*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Rekam Digital CAMERA
		 * PR: usb/98713
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "CAMERA*", "4MP-9J6*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * iRiver H10 MP3 player
		 * PR: usb/102547
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iriver", "H10*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * iRiver U10 MP3 player
		 * PR: usb/92306
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "iriver", "U10*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * X-Micro Flash Disk
		 * PR: usb/96901
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "X-Micro", "Flash Disk",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * EasyMP3 EM732X USB 2.0 Flash MP3 Player
		 * PR: usb/96546
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "EM732X", "MP3 Player*",
		"1.00"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Denver MP3 player
		 * PR: usb/107101
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "DENVER", "MP3 PLAYER",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Philips USB Key Audio KEY013
		 * PR: usb/68412
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "PHILIPS", "Key*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE | DA_Q_NO_PREVENT
	},
	{
		/*
		 * JNC MP3 Player
		 * PR: usb/94439
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "JNC*" , "MP3 Player*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * SAMSUNG MP0402H
		 * PR: usb/108427
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "MP0402H", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * I/O Magic USB flash - Giga Bank
		 * PR: usb/108810
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "GS-Magic", "stor*", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * JoyFly 128mb USB Flash Drive
		 * PR: 96133
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB 2.0", "Flash Disk*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * ChipsBnk usb stick
		 * PR: 103702
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "ChipsBnk", "USB*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Storcase (Kingston) InfoStation IFS FC2/SATA-R 201A
		 * PR: 129858
		 */
		{T_DIRECT, SIP_MEDIA_FIXED, "IFS", "FC2/SATA-R*",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Samsung YP-U3 mp3-player
		 * PR: 125398
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Samsung", "YP-U3",
		 "*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Netac", "OnlyDisk*",
		 "2000"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Sony Cyber-Shot DSC cameras
		 * PR: usb/137035
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "Sony", "Sony DSC", "*"},
		/*quirks*/ DA_Q_NO_SYNC_CACHE | DA_Q_NO_PREVENT
	},
	/* ATA/SATA devices over SAS/USB/... */
	{
		/* Hitachi Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "Hitachi", "H??????????E3*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG HD155UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "HD155UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "SAMSUNG HD204UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Samsung Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "SAMSUNG", "HD204UI*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST????DL*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST????DL", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST???DM*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST???DM*", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST????DM*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Barracuda Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST????DM", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9500423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST950042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9500424AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST950042", "4AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9640423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST964042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9640424AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST964042", "4AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750420AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "0AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750422AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "2AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST9750423AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST975042", "3AS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Thin Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "ST???LT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* Seagate Momentus Thin Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ST???LT*", "*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "??RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "??RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD??????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "????RS*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD??????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Caviar Green Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "????RX*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD???PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "?PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD?????PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Black Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "???PKT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD???PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "?PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "ATA", "WDC WD?????PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/* WDC Scorpio Blue Advanced Format (4k) drives */
		{ T_DIRECT, SIP_MEDIA_FIXED, "WDC WD??", "???PVT*", "*" },
		/*quirks*/DA_Q_4K
	},
	{
		/*
		 * Olympus FE-210 camera
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "OLYMPUS", "FE210*",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * LG UP3S MP3 player
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "LG", "UP3S",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
	{
		/*
		 * Laser MP3-2GA13 MP3 player
		 */
		{T_DIRECT, SIP_MEDIA_REMOVABLE, "USB 2.0", "(HS) Flash Disk",
		"*"}, /*quirks*/ DA_Q_NO_SYNC_CACHE
	},
};

static	disk_strategy_t	dastrategy;
static	dumper_t	dadump;
static	periph_init_t	dainit;
static	void		daasync(void *callback_arg, u_int32_t code,
				struct cam_path *path, void *arg);
static	void		dasysctlinit(void *context, int pending);
static	int		dacmdsizesysctl(SYSCTL_HANDLER_ARGS);
static	int		dadeletemethodsysctl(SYSCTL_HANDLER_ARGS);
static	periph_ctor_t	daregister;
static	periph_dtor_t	dacleanup;
static	periph_start_t	dastart;
static	periph_oninv_t	daoninvalidate;
static	void		dadone(struct cam_periph *periph,
			       union ccb *done_ccb);
static  int		daerror(union ccb *ccb, u_int32_t cam_flags,
				u_int32_t sense_flags);
static void		daprevent(struct cam_periph *periph, int action);
static int		dagetcapacity(struct cam_periph *periph);
static void		dasetgeom(struct cam_periph *periph, uint32_t block_len,
				  uint64_t maxsector, u_int lbppbe, u_int lalba);
static timeout_t	dasendorderedtag;
static void		dashutdown(void *arg, int howto);

#ifndef DA_DEFAULT_TIMEOUT
#define DA_DEFAULT_TIMEOUT 60	/* Timeout in seconds */
#endif

#ifndef	DA_DEFAULT_RETRY
#define	DA_DEFAULT_RETRY	4
#endif

#ifndef	DA_DEFAULT_SEND_ORDERED
#define	DA_DEFAULT_SEND_ORDERED	1
#endif


static int da_retry_count = DA_DEFAULT_RETRY;
static int da_default_timeout = DA_DEFAULT_TIMEOUT;
static int da_send_ordered = DA_DEFAULT_SEND_ORDERED;

SYSCTL_NODE(_kern_cam, OID_AUTO, da, CTLFLAG_RD, 0,
            "CAM Direct Access Disk driver");
SYSCTL_INT(_kern_cam_da, OID_AUTO, retry_count, CTLFLAG_RW,
           &da_retry_count, 0, "Normal I/O retry count");
TUNABLE_INT("kern.cam.da.retry_count", &da_retry_count);
SYSCTL_INT(_kern_cam_da, OID_AUTO, default_timeout, CTLFLAG_RW,
           &da_default_timeout, 0, "Normal I/O timeout (in seconds)");
TUNABLE_INT("kern.cam.da.default_timeout", &da_default_timeout);
SYSCTL_INT(_kern_cam_da, OID_AUTO, da_send_ordered, CTLFLAG_RW,
           &da_send_ordered, 0, "Send Ordered Tags");
TUNABLE_INT("kern.cam.da.da_send_ordered", &da_send_ordered);

/*
 * DA_ORDEREDTAG_INTERVAL determines how often, relative
 * to the default timeout, we check to see whether an ordered
 * tagged transaction is appropriate to prevent simple tag
 * starvation.  Since we'd like to ensure that there is at least
 * 1/2 of the timeout length left for a starved transaction to
 * complete after we've sent an ordered tag, we must poll at least
 * four times in every timeout period.  This takes care of the worst
 * case where a starved transaction starts during an interval that
 * meets the requirement "don't send an ordered tag" test so it takes
 * us two intervals to determine that a tag must be sent.
 */
#ifndef DA_ORDEREDTAG_INTERVAL
#define DA_ORDEREDTAG_INTERVAL 4
#endif

static struct periph_driver dadriver =
{
	dainit, "da",
	TAILQ_HEAD_INITIALIZER(dadriver.units), /* generation */ 0
};

PERIPHDRIVER_DECLARE(da, dadriver);

MALLOC_DEFINE(M_SCSIDA, "scsi_da", "scsi_da buffers");

static int
daopen(struct disk *dp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int unit;
	int error;

	periph = (struct cam_periph *)dp->d_drv1;
	if (periph == NULL) {
		return (ENXIO);	
	}

	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		return (ENXIO);
	}

	cam_periph_lock(periph);
	if ((error = cam_periph_hold(periph, PRIBIO|PCATCH)) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (error);
	}

	unit = periph->unit_number;
	softc = (struct da_softc *)periph->softc;
	softc->flags |= DA_FLAG_OPEN;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("daopen\n"));

	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		/* Invalidate our pack information. */
		softc->flags &= ~DA_FLAG_PACK_INVALID;
	}

	error = dagetcapacity(periph);

	if (error == 0) {

		softc->disk->d_sectorsize = softc->params.secsize;
		softc->disk->d_mediasize = softc->params.secsize * (off_t)softc->params.sectors;
		softc->disk->d_stripesize = softc->params.stripesize;
		softc->disk->d_stripeoffset = softc->params.stripeoffset;
		/* XXX: these are not actually "firmware" values, so they may be wrong */
		softc->disk->d_fwsectors = softc->params.secs_per_track;
		softc->disk->d_fwheads = softc->params.heads;
		softc->disk->d_devstat->block_size = softc->params.secsize;
		softc->disk->d_devstat->flags &= ~DEVSTAT_BS_UNAVAILABLE;
		if (softc->delete_method > DA_DELETE_DISABLE)
			softc->disk->d_flags |= DISKFLAG_CANDELETE;
		else
			softc->disk->d_flags &= ~DISKFLAG_CANDELETE;

		if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0 &&
		    (softc->quirks & DA_Q_NO_PREVENT) == 0)
			daprevent(periph, PR_PREVENT);
	} else
		softc->flags &= ~DA_FLAG_OPEN;

	cam_periph_unhold(periph);
	cam_periph_unlock(periph);

	if (error != 0) {
		cam_periph_release(periph);
	}
	return (error);
}

static int
daclose(struct disk *dp)
{
	struct	cam_periph *periph;
	struct	da_softc *softc;

	periph = (struct cam_periph *)dp->d_drv1;
	if (periph == NULL)
		return (0);	

	cam_periph_lock(periph);
	if (cam_periph_hold(periph, PRIBIO) != 0) {
		cam_periph_unlock(periph);
		cam_periph_release(periph);
		return (0);
	}

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE | CAM_DEBUG_PERIPH,
	    ("daclose\n"));

	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0
	 && (softc->flags & DA_FLAG_PACK_INVALID) == 0) {
		union	ccb *ccb;

		ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);

		scsi_synchronize_cache(&ccb->csio,
				       /*retries*/1,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);

		cam_periph_runccb(ccb, daerror, /*cam_flags*/0,
				  /*sense_flags*/SF_RETRY_UA | SF_QUIET_IR,
				  softc->disk->d_devstat);
		xpt_release_ccb(ccb);

	}

	if ((softc->flags & DA_FLAG_PACK_REMOVABLE) != 0) {
		if ((softc->quirks & DA_Q_NO_PREVENT) == 0)
			daprevent(periph, PR_ALLOW);
		/*
		 * If we've got removeable media, mark the blocksize as
		 * unavailable, since it could change when new media is
		 * inserted.
		 */
		softc->disk->d_devstat->flags |= DEVSTAT_BS_UNAVAILABLE;
	}

	softc->flags &= ~DA_FLAG_OPEN;
	cam_periph_unhold(periph);
	cam_periph_unlock(periph);
	cam_periph_release(periph);
	return (0);	
}

static void
daschedule(struct cam_periph *periph)
{
	struct da_softc *softc = (struct da_softc *)periph->softc;
	uint32_t prio;

	/* Check if cam_periph_getccb() was called. */
	prio = periph->immediate_priority;

	/* Check if we have more work to do. */
	if (bioq_first(&softc->bio_queue) ||
	    (!softc->delete_running && bioq_first(&softc->delete_queue))) {
		prio = CAM_PRIORITY_NORMAL;
	}

	/* Schedule CCB if any of above is true. */
	if (prio != CAM_PRIORITY_NONE)
		xpt_schedule(periph, prio);
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
static void
dastrategy(struct bio *bp)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	
	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	if (periph == NULL) {
		biofinish(bp, NULL, ENXIO);
		return;
	}
	softc = (struct da_softc *)periph->softc;

	cam_periph_lock(periph);

	/*
	 * If the device has been made invalid, error out
	 */
	if ((softc->flags & DA_FLAG_PACK_INVALID)) {
		cam_periph_unlock(periph);
		biofinish(bp, NULL, ENXIO);
		return;
	}

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dastrategy(%p)\n", bp));

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	if (bp->bio_cmd == BIO_DELETE) {
		if (bp->bio_bcount == 0)
			biodone(bp);
		else
			bioq_disksort(&softc->delete_queue, bp);
	} else
		bioq_disksort(&softc->bio_queue, bp);

	/*
	 * Schedule ourselves for performing the work.
	 */
	daschedule(periph);
	cam_periph_unlock(periph);

	return;
}

static int
dadump(void *arg, void *virtual, vm_offset_t physical, off_t offset, size_t length)
{
	struct	    cam_periph *periph;
	struct	    da_softc *softc;
	u_int	    secsize;
	struct	    ccb_scsiio csio;
	struct	    disk *dp;
	int	    error = 0;

	dp = arg;
	periph = dp->d_drv1;
	if (periph == NULL)
		return (ENXIO);
	softc = (struct da_softc *)periph->softc;
	cam_periph_lock(periph);
	secsize = softc->params.secsize;
	
	if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
		cam_periph_unlock(periph);
		return (ENXIO);
	}

	if (length > 0) {
		xpt_setup_ccb(&csio.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_read_write(&csio,
				/*retries*/0,
				dadone,
				MSG_ORDERED_Q_TAG,
				/*read*/FALSE,
				/*byte2*/0,
				/*minimum_cmd_size*/ softc->minimum_cmd_size,
				offset / secsize,
				length / secsize,
				/*data_ptr*/(u_int8_t *) virtual,
				/*dxfer_len*/length,
				/*sense_len*/SSD_FULL_SIZE,
				da_default_timeout * 1000);
		xpt_polled_action((union ccb *)&csio);

		error = cam_periph_error((union ccb *)&csio,
		    0, SF_NO_RECOVERY | SF_NO_RETRY, NULL);
		if ((csio.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(csio.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			printf("Aborting dump due to I/O error.\n");
		cam_periph_unlock(periph);
		return (error);
	}
		
	/*
	 * Sync the disk cache contents to the physical media.
	 */
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0) {

		xpt_setup_ccb(&csio.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
		csio.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&csio,
				       /*retries*/0,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0,/* Cover the whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       5 * 60 * 1000);
		xpt_polled_action((union ccb *)&csio);

		error = cam_periph_error((union ccb *)&csio,
		    0, SF_NO_RECOVERY | SF_NO_RETRY | SF_QUIET_IR, NULL);
		if ((csio.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(csio.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
	}
	cam_periph_unlock(periph);
	return (error);
}

static int
dagetattr(struct bio *bp)
{
	int ret = -1;
	struct cam_periph *periph;

	if (bp->bio_disk == NULL || bp->bio_disk->d_drv1 == NULL)
		return ENXIO;
	periph = (struct cam_periph *)bp->bio_disk->d_drv1;
	if (periph->path == NULL)
		return ENXIO;

	ret = xpt_getattr(bp->bio_data, bp->bio_length, bp->bio_attribute,
	    periph->path);
	if (ret == 0)
		bp->bio_completed = bp->bio_length;
	return ret;
}

static void
dainit(void)
{
	cam_status status;

	/*
	 * Install a global async callback.  This callback will
	 * receive async callbacks like "new device found".
	 */
	status = xpt_register_async(AC_FOUND_DEVICE, daasync, NULL, NULL);

	if (status != CAM_REQ_CMP) {
		printf("da: Failed to attach master async callback "
		       "due to status 0x%x!\n", status);
	} else if (da_send_ordered) {

		/* Register our shutdown event handler */
		if ((EVENTHANDLER_REGISTER(shutdown_post_sync, dashutdown, 
					   NULL, SHUTDOWN_PRI_DEFAULT)) == NULL)
		    printf("dainit: shutdown event registration failed!\n");
	}
}

/*
 * Callback from GEOM, called when it has finished cleaning up its
 * resources.
 */
static void
dadiskgonecb(struct disk *dp)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)dp->d_drv1;

	cam_periph_release(periph);
}

static void
daoninvalidate(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	/*
	 * De-register any async callbacks.
	 */
	xpt_register_async(0, daasync, periph, periph->path);

	softc->flags |= DA_FLAG_PACK_INVALID;

	/*
	 * Return all queued I/O with ENXIO.
	 * XXX Handle any transactions queued to the card
	 *     with XPT_ABORT_CCB.
	 */
	bioq_flush(&softc->bio_queue, NULL, ENXIO);
	bioq_flush(&softc->delete_queue, NULL, ENXIO);

	/*
	 * Tell GEOM that we've gone away, we'll get a callback when it is
	 * done cleaning up its resources.
	 */
	disk_gone(softc->disk);

	xpt_print(periph->path, "lost device - %d outstanding, %d refs\n",
		  softc->outstanding_cmds, periph->refcount);
}

static void
dacleanup(struct cam_periph *periph)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	xpt_print(periph->path, "removing device entry\n");
	cam_periph_unlock(periph);

	/*
	 * If we can't free the sysctl tree, oh well...
	 */
	if ((softc->flags & DA_FLAG_SCTX_INIT) != 0
	    && sysctl_ctx_free(&softc->sysctl_ctx) != 0) {
		xpt_print(periph->path, "can't remove sysctl context\n");
	}

	disk_destroy(softc->disk);
	callout_drain(&softc->sendordered_c);
	free(softc, M_DEVBUF);
	cam_periph_lock(periph);
}

static void
daasync(void *callback_arg, u_int32_t code,
	struct cam_path *path, void *arg)
{
	struct cam_periph *periph;

	periph = (struct cam_periph *)callback_arg;
	switch (code) {
	case AC_FOUND_DEVICE:
	{
		struct ccb_getdev *cgd;
		cam_status status;
 
		cgd = (struct ccb_getdev *)arg;
		if (cgd == NULL)
			break;

		if (cgd->protocol != PROTO_SCSI)
			break;

		if (SID_TYPE(&cgd->inq_data) != T_DIRECT
		    && SID_TYPE(&cgd->inq_data) != T_RBC
		    && SID_TYPE(&cgd->inq_data) != T_OPTICAL)
			break;

		/*
		 * Allocate a peripheral instance for
		 * this device and start the probe
		 * process.
		 */
		status = cam_periph_alloc(daregister, daoninvalidate,
					  dacleanup, dastart,
					  "da", CAM_PERIPH_BIO,
					  cgd->ccb_h.path, daasync,
					  AC_FOUND_DEVICE, cgd);

		if (status != CAM_REQ_CMP
		 && status != CAM_REQ_INPROG)
			printf("daasync: Unable to attach to new device "
				"due to status 0x%x\n", status);
		return;
	}
	case AC_ADVINFO_CHANGED:
	{
		uintptr_t buftype;

		buftype = (uintptr_t)arg;
		if (buftype == CDAI_TYPE_PHYS_PATH) {
			struct da_softc *softc;

			softc = periph->softc;
			disk_attr_changed(softc->disk, "GEOM::physpath",
					  M_NOWAIT);
		}
		break;
	}
	case AC_SENT_BDR:
	case AC_BUS_RESET:
	{
		struct da_softc *softc;
		struct ccb_hdr *ccbh;

		softc = (struct da_softc *)periph->softc;
		/*
		 * Don't fail on the expected unit attention
		 * that will occur.
		 */
		softc->flags |= DA_FLAG_RETRY_UA;
		LIST_FOREACH(ccbh, &softc->pending_ccbs, periph_links.le)
			ccbh->ccb_state |= DA_CCB_RETRY_UA;
		break;
	}
	default:
		break;
	}
	cam_periph_async(periph, code, path, arg);
}

static void
dasysctlinit(void *context, int pending)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	char tmpstr[80], tmpstr2[80];
	struct ccb_trans_settings cts;

	periph = (struct cam_periph *)context;
	/*
	 * periph was held for us when this task was enqueued
	 */
	if (periph->flags & CAM_PERIPH_INVALID) {
		cam_periph_release(periph);
		return;
	}

	softc = (struct da_softc *)periph->softc;
	snprintf(tmpstr, sizeof(tmpstr), "CAM DA unit %d", periph->unit_number);
	snprintf(tmpstr2, sizeof(tmpstr2), "%d", periph->unit_number);

	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->flags |= DA_FLAG_SCTX_INIT;
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam_da), OID_AUTO, tmpstr2,
		CTLFLAG_RD, 0, tmpstr);
	if (softc->sysctl_tree == NULL) {
		printf("dasysctlinit: unable to allocate sysctl tree\n");
		cam_periph_release(periph);
		return;
	}

	/*
	 * Now register the sysctl handler, so the user can change the value on
	 * the fly.
	 */
	SYSCTL_ADD_PROC(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "delete_method", CTLTYPE_STRING | CTLFLAG_RW,
		&softc->delete_method, 0, dadeletemethodsysctl, "A",
		"BIO_DELETE execution method");
	SYSCTL_ADD_PROC(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
		OID_AUTO, "minimum_cmd_size", CTLTYPE_INT | CTLFLAG_RW,
		&softc->minimum_cmd_size, 0, dacmdsizesysctl, "I",
		"Minimum CDB size");

	SYSCTL_ADD_INT(&softc->sysctl_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_tree),
		       OID_AUTO,
		       "error_inject",
		       CTLFLAG_RW,
		       &softc->error_inject,
		       0,
		       "error_inject leaf");


	/*
	 * Add some addressing info.
	 */
	memset(&cts, 0, sizeof (cts));
	xpt_setup_ccb(&cts.ccb_h, periph->path, /*priority*/1);
	cts.ccb_h.func_code = XPT_GET_TRAN_SETTINGS;
	cts.type = CTS_TYPE_CURRENT_SETTINGS;
	cam_periph_lock(periph);
	xpt_action((union ccb *)&cts);
	cam_periph_unlock(periph);
	if (cts.ccb_h.status != CAM_REQ_CMP) {
		cam_periph_release(periph);
		return;
	}
	if (cts.protocol == PROTO_SCSI && cts.transport == XPORT_FC) {
		struct ccb_trans_settings_fc *fc = &cts.xport_specific.fc;
		if (fc->valid & CTS_FC_VALID_WWPN) {
			softc->wwpn = fc->wwpn;
			SYSCTL_ADD_UQUAD(&softc->sysctl_ctx,
			    SYSCTL_CHILDREN(softc->sysctl_tree),
			    OID_AUTO, "wwpn", CTLFLAG_RD,
			    &softc->wwpn, "World Wide Port Name");
		}
	}
	cam_periph_release(periph);
}

static int
dacmdsizesysctl(SYSCTL_HANDLER_ARGS)
{
	int error, value;

	value = *(int *)arg1;

	error = sysctl_handle_int(oidp, &value, 0, req);

	if ((error != 0)
	 || (req->newptr == NULL))
		return (error);

	/*
	 * Acceptable values here are 6, 10, 12 or 16.
	 */
	if (value < 6)
		value = 6;
	else if ((value > 6)
	      && (value <= 10))
		value = 10;
	else if ((value > 10)
	      && (value <= 12))
		value = 12;
	else if (value > 12)
		value = 16;

	*(int *)arg1 = value;

	return (0);
}

static int
dadeletemethodsysctl(SYSCTL_HANDLER_ARGS)
{
	char buf[16];
	int error;
	const char *p;
	int i, value;

	value = *(int *)arg1;
	if (value < 0 || value > DA_DELETE_MAX)
		p = "UNKNOWN";
	else
		p = da_delete_method_names[value];
	strncpy(buf, p, sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	for (i = 0; i <= DA_DELETE_MAX; i++) {
		if (strcmp(buf, da_delete_method_names[i]) != 0)
			continue;
		*(int *)arg1 = i;
		return (0);
	}
	return (EINVAL);
}

static cam_status
daregister(struct cam_periph *periph, void *arg)
{
	struct da_softc *softc;
	struct ccb_pathinq cpi;
	struct ccb_getdev *cgd;
	char tmpstr[80];
	caddr_t match;

	cgd = (struct ccb_getdev *)arg;
	if (periph == NULL) {
		printf("daregister: periph was NULL!!\n");
		return(CAM_REQ_CMP_ERR);
	}

	if (cgd == NULL) {
		printf("daregister: no getdev CCB, can't register device\n");
		return(CAM_REQ_CMP_ERR);
	}

	softc = (struct da_softc *)malloc(sizeof(*softc), M_DEVBUF,
	    M_NOWAIT|M_ZERO);

	if (softc == NULL) {
		printf("daregister: Unable to probe new device. "
		       "Unable to allocate softc\n");				
		return(CAM_REQ_CMP_ERR);
	}

	LIST_INIT(&softc->pending_ccbs);
	softc->state = DA_STATE_PROBE;
	bioq_init(&softc->bio_queue);
	bioq_init(&softc->delete_queue);
	bioq_init(&softc->delete_run_queue);
	if (SID_IS_REMOVABLE(&cgd->inq_data))
		softc->flags |= DA_FLAG_PACK_REMOVABLE;
	softc->unmap_max_ranges = UNMAP_MAX_RANGES;
	softc->unmap_max_lba = 1024*1024*2;

	periph->softc = softc;

	/*
	 * See if this device has any quirks.
	 */
	match = cam_quirkmatch((caddr_t)&cgd->inq_data,
			       (caddr_t)da_quirk_table,
			       sizeof(da_quirk_table)/sizeof(*da_quirk_table),
			       sizeof(*da_quirk_table), scsi_inquiry_match);

	if (match != NULL)
		softc->quirks = ((struct da_quirk_entry *)match)->quirks;
	else
		softc->quirks = DA_Q_NONE;

	/* Check if the SIM does not want 6 byte commands */
	bzero(&cpi, sizeof(cpi));
	xpt_setup_ccb(&cpi.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
	cpi.ccb_h.func_code = XPT_PATH_INQ;
	xpt_action((union ccb *)&cpi);
	if (cpi.ccb_h.status == CAM_REQ_CMP && (cpi.hba_misc & PIM_NO_6_BYTE))
		softc->quirks |= DA_Q_NO_6_BYTE;

	TASK_INIT(&softc->sysctl_task, 0, dasysctlinit, periph);

	/*
	 * Take an exclusive refcount on the periph while dastart is called
	 * to finish the probe.  The reference will be dropped in dadone at
	 * the end of probe.
	 */
	(void)cam_periph_hold(periph, PRIBIO);

	/*
	 * Schedule a periodic event to occasionally send an
	 * ordered tag to a device.
	 */
	callout_init_mtx(&softc->sendordered_c, periph->sim->mtx, 0);
	callout_reset(&softc->sendordered_c,
	    (da_default_timeout * hz) / DA_ORDEREDTAG_INTERVAL,
	    dasendorderedtag, softc);

	mtx_unlock(periph->sim->mtx);
	/*
	 * RBC devices don't have to support READ(6), only READ(10).
	 */
	if (softc->quirks & DA_Q_NO_6_BYTE || SID_TYPE(&cgd->inq_data) == T_RBC)
		softc->minimum_cmd_size = 10;
	else
		softc->minimum_cmd_size = 6;

	/*
	 * Load the user's default, if any.
	 */
	snprintf(tmpstr, sizeof(tmpstr), "kern.cam.da.%d.minimum_cmd_size",
		 periph->unit_number);
	TUNABLE_INT_FETCH(tmpstr, &softc->minimum_cmd_size);

	/*
	 * 6, 10, 12 and 16 are the currently permissible values.
	 */
	if (softc->minimum_cmd_size < 6)
		softc->minimum_cmd_size = 6;
	else if ((softc->minimum_cmd_size > 6)
	      && (softc->minimum_cmd_size <= 10))
		softc->minimum_cmd_size = 10;
	else if ((softc->minimum_cmd_size > 10)
	      && (softc->minimum_cmd_size <= 12))
		softc->minimum_cmd_size = 12;
	else if (softc->minimum_cmd_size > 12)
		softc->minimum_cmd_size = 16;

	/* Predict whether device may support READ CAPACITY(16). */
	if (SID_ANSI_REV(&cgd->inq_data) >= SCSI_REV_SPC3) {
		softc->flags |= DA_FLAG_CAN_RC16;
		softc->state = DA_STATE_PROBE2;
	}

	/*
	 * Register this media as a disk.
	 */
	softc->disk = disk_alloc();
	softc->disk->d_devstat = devstat_new_entry(periph->periph_name,
			  periph->unit_number, 0,
			  DEVSTAT_BS_UNAVAILABLE,
			  SID_TYPE(&cgd->inq_data) |
			  XPORT_DEVSTAT_TYPE(cpi.transport),
			  DEVSTAT_PRIORITY_DISK);
	softc->disk->d_open = daopen;
	softc->disk->d_close = daclose;
	softc->disk->d_strategy = dastrategy;
	softc->disk->d_dump = dadump;
	softc->disk->d_getattr = dagetattr;
	softc->disk->d_gone = dadiskgonecb;
	softc->disk->d_name = "da";
	softc->disk->d_drv1 = periph;
	if (cpi.maxio == 0)
		softc->disk->d_maxsize = DFLTPHYS;	/* traditional default */
	else if (cpi.maxio > MAXPHYS)
		softc->disk->d_maxsize = MAXPHYS;	/* for safety */
	else
		softc->disk->d_maxsize = cpi.maxio;
	softc->disk->d_unit = periph->unit_number;
	softc->disk->d_flags = 0;
	if ((softc->quirks & DA_Q_NO_SYNC_CACHE) == 0)
		softc->disk->d_flags |= DISKFLAG_CANFLUSHCACHE;
	cam_strvis(softc->disk->d_descr, cgd->inq_data.vendor,
	    sizeof(cgd->inq_data.vendor), sizeof(softc->disk->d_descr));
	strlcat(softc->disk->d_descr, " ", sizeof(softc->disk->d_descr));
	cam_strvis(&softc->disk->d_descr[strlen(softc->disk->d_descr)],
	    cgd->inq_data.product, sizeof(cgd->inq_data.product),
	    sizeof(softc->disk->d_descr) - strlen(softc->disk->d_descr));
	softc->disk->d_hba_vendor = cpi.hba_vendor;
	softc->disk->d_hba_device = cpi.hba_device;
	softc->disk->d_hba_subvendor = cpi.hba_subvendor;
	softc->disk->d_hba_subdevice = cpi.hba_subdevice;

	/*
	 * Acquire a reference to the periph before we register with GEOM.
	 * We'll release this reference once GEOM calls us back (via
	 * dadiskgonecb()) telling us that our provider has been freed.
	 */
	if (cam_periph_acquire(periph) != CAM_REQ_CMP) {
		xpt_print(periph->path, "%s: lost periph during "
			  "registration!\n", __func__);
		mtx_lock(periph->sim->mtx);
		return (CAM_REQ_CMP_ERR);
	}

	disk_create(softc->disk, DISK_VERSION);
	mtx_lock(periph->sim->mtx);

	/*
	 * Add async callbacks for events of interest.
	 * I don't bother checking if this fails as,
	 * in most cases, the system will function just
	 * fine without them and the only alternative
	 * would be to not attach the device on failure.
	 */
	xpt_register_async(AC_SENT_BDR | AC_BUS_RESET
			 | AC_LOST_DEVICE | AC_ADVINFO_CHANGED,
			   daasync, periph, periph->path);

	/*
	 * Emit an attribute changed notification just in case 
	 * physical path information arrived before our async
	 * event handler was registered, but after anyone attaching
	 * to our disk device polled it.
	 */
	disk_attr_changed(softc->disk, "GEOM::physpath", M_NOWAIT);

	xpt_schedule(periph, CAM_PRIORITY_DEV);

	return(CAM_REQ_CMP);
}

static void
dastart(struct cam_periph *periph, union ccb *start_ccb)
{
	struct da_softc *softc;

	softc = (struct da_softc *)periph->softc;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dastart\n"));

	switch (softc->state) {
	case DA_STATE_NORMAL:
	{
		struct bio *bp, *bp1;
		uint8_t tag_code;

		/* Execute immediate CCB if waiting. */
		if (periph->immediate_priority <= periph->pinfo.priority) {
			CAM_DEBUG(periph->path, CAM_DEBUG_SUBTRACE,
					("queuing for immediate ccb\n"));
			start_ccb->ccb_h.ccb_state = DA_CCB_WAITING;
			SLIST_INSERT_HEAD(&periph->ccb_list, &start_ccb->ccb_h,
					  periph_links.sle);
			periph->immediate_priority = CAM_PRIORITY_NONE;
			wakeup(&periph->ccb_list);
			/* May have more work to do, so ensure we stay scheduled */
			daschedule(periph);
			break;
		}

		/* Run BIO_DELETE if not running yet. */
		if (!softc->delete_running &&
		    (bp = bioq_first(&softc->delete_queue)) != NULL) {
		    uint64_t lba;
		    u_int count;

		    if (softc->delete_method == DA_DELETE_UNMAP) {
			uint8_t *buf = softc->unmap_buf;
			uint64_t lastlba = (uint64_t)-1;
			uint32_t lastcount = 0;
			int blocks = 0, off, ranges = 0;

			softc->delete_running = 1;
			bzero(softc->unmap_buf, sizeof(softc->unmap_buf));
			bp1 = bp;
			do {
				bioq_remove(&softc->delete_queue, bp1);
				if (bp1 != bp)
					bioq_insert_tail(&softc->delete_run_queue, bp1);
				lba = bp1->bio_pblkno;
				count = bp1->bio_bcount / softc->params.secsize;

				/* Try to extend the previous range. */
				if (lba == lastlba) {
					lastcount += count;
					off = (ranges - 1) * 16 + 8;
					scsi_ulto4b(lastcount, &buf[off + 8]);
				} else if (count > 0) {
					off = ranges * 16 + 8;
					scsi_u64to8b(lba, &buf[off + 0]);
					scsi_ulto4b(count, &buf[off + 8]);
					lastcount = count;
					ranges++;
				}
				blocks += count;
				lastlba = lba + count;
				bp1 = bioq_first(&softc->delete_queue);
				if (bp1 == NULL ||
				    ranges >= softc->unmap_max_ranges ||
				    blocks + bp1->bio_bcount /
				     softc->params.secsize > softc->unmap_max_lba)
					break;
			} while (1);
			scsi_ulto2b(ranges * 16 + 6, &buf[0]);
			scsi_ulto2b(ranges * 16, &buf[2]);

			scsi_unmap(&start_ccb->csio,
					/*retries*/da_retry_count,
					/*cbfcnp*/dadone,
					/*tag_action*/MSG_SIMPLE_Q_TAG,
					/*byte2*/0,
					/*data_ptr*/ buf,
					/*dxfer_len*/ ranges * 16 + 8,
					/*sense_len*/SSD_FULL_SIZE,
					da_default_timeout * 1000);
			start_ccb->ccb_h.ccb_state = DA_CCB_DELETE;
			goto out;
		    } else if (softc->delete_method == DA_DELETE_ZERO ||
			       softc->delete_method == DA_DELETE_WS10 ||
			       softc->delete_method == DA_DELETE_WS16) {
			softc->delete_running = 1;
			lba = bp->bio_pblkno;
			count = 0;
			bp1 = bp;
			do {
				bioq_remove(&softc->delete_queue, bp1);
				if (bp1 != bp)
					bioq_insert_tail(&softc->delete_run_queue, bp1);
				count += bp1->bio_bcount / softc->params.secsize;
				bp1 = bioq_first(&softc->delete_queue);
				if (bp1 == NULL ||
				    lba + count != bp1->bio_pblkno ||
				    count + bp1->bio_bcount /
				     softc->params.secsize > 0xffff)
					break;
			} while (1);

			scsi_write_same(&start_ccb->csio,
					/*retries*/da_retry_count,
					/*cbfcnp*/dadone,
					/*tag_action*/MSG_SIMPLE_Q_TAG,
					/*byte2*/softc->delete_method ==
					    DA_DELETE_ZERO ? 0 : SWS_UNMAP,
					softc->delete_method ==
					    DA_DELETE_WS16 ? 16 : 10,
					/*lba*/lba,
					/*block_count*/count,
					/*data_ptr*/ __DECONST(void *,
					    zero_region),
					/*dxfer_len*/ softc->params.secsize,
					/*sense_len*/SSD_FULL_SIZE,
					da_default_timeout * 1000);
			start_ccb->ccb_h.ccb_state = DA_CCB_DELETE;
			goto out;
		    } else {
			bioq_flush(&softc->delete_queue, NULL, 0);
			/* FALLTHROUGH */
		    }
		}

		/* Run regular command. */
		bp = bioq_takefirst(&softc->bio_queue);
		if (bp == NULL) {
			xpt_release_ccb(start_ccb);
			break;
		}

		if ((bp->bio_flags & BIO_ORDERED) != 0 ||
		    (softc->flags & DA_FLAG_NEED_OTAG) != 0) {
			softc->flags &= ~DA_FLAG_NEED_OTAG;
			softc->ordered_tag_count++;
			tag_code = MSG_ORDERED_Q_TAG;
		} else {
			tag_code = MSG_SIMPLE_Q_TAG;
		}

		switch (bp->bio_cmd) {
		case BIO_READ:
		case BIO_WRITE:
			scsi_read_write(&start_ccb->csio,
					/*retries*/da_retry_count,
					/*cbfcnp*/dadone,
					/*tag_action*/tag_code,
					/*read_op*/bp->bio_cmd
						== BIO_READ,
					/*byte2*/0,
					softc->minimum_cmd_size,
					/*lba*/bp->bio_pblkno,
					/*block_count*/bp->bio_bcount /
					softc->params.secsize,
					/*data_ptr*/ bp->bio_data,
					/*dxfer_len*/ bp->bio_bcount,
					/*sense_len*/SSD_FULL_SIZE,
					da_default_timeout * 1000);
			break;
		case BIO_FLUSH:
			/*
			 * BIO_FLUSH doesn't currently communicate
			 * range data, so we synchronize the cache
			 * over the whole disk.  We also force
			 * ordered tag semantics the flush applies
			 * to all previously queued I/O.
			 */
			scsi_synchronize_cache(&start_ccb->csio,
					       /*retries*/1,
					       /*cbfcnp*/dadone,
					       MSG_ORDERED_Q_TAG,
					       /*begin_lba*/0,
					       /*lb_count*/0,
					       SSD_FULL_SIZE,
					       da_default_timeout*1000);
			break;
		}
		start_ccb->ccb_h.ccb_state = DA_CCB_BUFFER_IO;

out:
		/*
		 * Block out any asyncronous callbacks
		 * while we touch the pending ccb list.
		 */
		LIST_INSERT_HEAD(&softc->pending_ccbs,
				 &start_ccb->ccb_h, periph_links.le);
		softc->outstanding_cmds++;

		/* We expect a unit attention from this device */
		if ((softc->flags & DA_FLAG_RETRY_UA) != 0) {
			start_ccb->ccb_h.ccb_state |= DA_CCB_RETRY_UA;
			softc->flags &= ~DA_FLAG_RETRY_UA;
		}

		start_ccb->ccb_h.ccb_bp = bp;
		xpt_action(start_ccb);

		/* May have more work to do, so ensure we stay scheduled */
		daschedule(periph);
		break;
	}
	case DA_STATE_PROBE:
	{
		struct ccb_scsiio *csio;
		struct scsi_read_capacity_data *rcap;

		rcap = (struct scsi_read_capacity_data *)
		    malloc(sizeof(*rcap), M_SCSIDA, M_NOWAIT|M_ZERO);
		if (rcap == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		csio = &start_ccb->csio;
		scsi_read_capacity(csio,
				   /*retries*/4,
				   dadone,
				   MSG_SIMPLE_Q_TAG,
				   rcap,
				   SSD_FULL_SIZE,
				   /*timeout*/5000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE;
		xpt_action(start_ccb);
		break;
	}
	case DA_STATE_PROBE2:
	{
		struct ccb_scsiio *csio;
		struct scsi_read_capacity_data_long *rcaplong;

		rcaplong = (struct scsi_read_capacity_data_long *)
			malloc(sizeof(*rcaplong), M_SCSIDA, M_NOWAIT|M_ZERO);
		if (rcaplong == NULL) {
			printf("dastart: Couldn't malloc read_capacity data\n");
			/* da_free_periph??? */
			break;
		}
		csio = &start_ccb->csio;
		scsi_read_capacity_16(csio,
				      /*retries*/ 4,
				      /*cbfcnp*/ dadone,
				      /*tag_action*/ MSG_SIMPLE_Q_TAG,
				      /*lba*/ 0,
				      /*reladr*/ 0,
				      /*pmi*/ 0,
				      rcaplong,
				      /*sense_len*/ SSD_FULL_SIZE,
				      /*timeout*/ 60000);
		start_ccb->ccb_h.ccb_bp = NULL;
		start_ccb->ccb_h.ccb_state = DA_CCB_PROBE2;
		xpt_action(start_ccb);	
		break;
	}
	}
}

static int
cmd6workaround(union ccb *ccb)
{
	struct scsi_rw_6 cmd6;
	struct scsi_rw_10 *cmd10;
	struct da_softc *softc;
	u_int8_t *cdb;
	struct bio *bp;
	int frozen;

	cdb = ccb->csio.cdb_io.cdb_bytes;
	softc = (struct da_softc *)xpt_path_periph(ccb->ccb_h.path)->softc;

	if (ccb->ccb_h.ccb_state == DA_CCB_DELETE) {
		if (softc->delete_method == DA_DELETE_UNMAP) {
			xpt_print(ccb->ccb_h.path, "UNMAP is not supported, "
			    "switching to WRITE SAME(16) with UNMAP.\n");
			softc->delete_method = DA_DELETE_WS16;
		} else if (softc->delete_method == DA_DELETE_WS16) {
			xpt_print(ccb->ccb_h.path,
			    "WRITE SAME(16) with UNMAP is not supported, "
			    "disabling BIO_DELETE.\n");
			softc->delete_method = DA_DELETE_DISABLE;
		} else if (softc->delete_method == DA_DELETE_WS10) {
			xpt_print(ccb->ccb_h.path,
			    "WRITE SAME(10) with UNMAP is not supported, "
			    "disabling BIO_DELETE.\n");
			softc->delete_method = DA_DELETE_DISABLE;
		} else if (softc->delete_method == DA_DELETE_ZERO) {
			xpt_print(ccb->ccb_h.path,
			    "WRITE SAME(10) is not supported, "
			    "disabling BIO_DELETE.\n");
			softc->delete_method = DA_DELETE_DISABLE;
		} else
			softc->delete_method = DA_DELETE_DISABLE;
		while ((bp = bioq_takefirst(&softc->delete_run_queue))
		    != NULL)
			bioq_disksort(&softc->delete_queue, bp);
		bioq_insert_tail(&softc->delete_queue,
		    (struct bio *)ccb->ccb_h.ccb_bp);
		ccb->ccb_h.ccb_bp = NULL;
		return (0);
	}

	/* Translation only possible if CDB is an array and cmd is R/W6 */
	if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0 ||
	    (*cdb != READ_6 && *cdb != WRITE_6))
		return 0;

	xpt_print(ccb->ccb_h.path, "READ(6)/WRITE(6) not supported, "
	    "increasing minimum_cmd_size to 10.\n");
 	softc->minimum_cmd_size = 10;

	bcopy(cdb, &cmd6, sizeof(struct scsi_rw_6));
	cmd10 = (struct scsi_rw_10 *)cdb;
	cmd10->opcode = (cmd6.opcode == READ_6) ? READ_10 : WRITE_10;
	cmd10->byte2 = 0;
	scsi_ulto4b(scsi_3btoul(cmd6.addr), cmd10->addr);
	cmd10->reserved = 0;
	scsi_ulto2b(cmd6.length, cmd10->length);
	cmd10->control = cmd6.control;
	ccb->csio.cdb_len = sizeof(*cmd10);

	/* Requeue request, unfreezing queue if necessary */
	frozen = (ccb->ccb_h.status & CAM_DEV_QFRZN) != 0;
 	ccb->ccb_h.status = CAM_REQUEUE_REQ;
	xpt_action(ccb);
	if (frozen) {
		cam_release_devq(ccb->ccb_h.path,
				 /*relsim_flags*/0,
				 /*reduction*/0,
				 /*timeout*/0,
				 /*getcount_only*/0);
	}
	return (ERESTART);
}

static void
dadone(struct cam_periph *periph, union ccb *done_ccb)
{
	struct da_softc *softc;
	struct ccb_scsiio *csio;
	u_int32_t  priority;

	softc = (struct da_softc *)periph->softc;
	priority = done_ccb->ccb_h.pinfo.priority;

	CAM_DEBUG(periph->path, CAM_DEBUG_TRACE, ("dadone\n"));

	csio = &done_ccb->csio;
	switch (csio->ccb_h.ccb_state & DA_CCB_TYPE_MASK) {
	case DA_CCB_BUFFER_IO:
	case DA_CCB_DELETE:
	{
		struct bio *bp, *bp1;

		bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
		if ((done_ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			int error;
			int sf;

			if ((csio->ccb_h.ccb_state & DA_CCB_RETRY_UA) != 0)
				sf = SF_RETRY_UA;
			else
				sf = 0;

			error = daerror(done_ccb, CAM_RETRY_SELTO, sf);
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			}
			bp = (struct bio *)done_ccb->ccb_h.ccb_bp;
			if (error != 0) {
				int queued_error;

				/*
				 * return all queued I/O with EIO, so that
				 * the client can retry these I/Os in the
				 * proper order should it attempt to recover.
				 */
				queued_error = EIO;

				if (error == ENXIO
				 && (softc->flags & DA_FLAG_PACK_INVALID)== 0) {
					/*
					 * Catastrophic error.  Mark our pack as
					 * invalid.
					 */
					/*
					 * XXX See if this is really a media
					 * XXX change first?
					 */
					xpt_print(periph->path,
					    "Invalidating pack\n");
					softc->flags |= DA_FLAG_PACK_INVALID;
					queued_error = ENXIO;
				}
				bioq_flush(&softc->bio_queue, NULL,
					   queued_error);
				if (bp != NULL) {
					bp->bio_error = error;
					bp->bio_resid = bp->bio_bcount;
					bp->bio_flags |= BIO_ERROR;
				}
			} else if (bp != NULL) {
				bp->bio_resid = csio->resid;
				bp->bio_error = 0;
				if (bp->bio_resid != 0)
					bp->bio_flags |= BIO_ERROR;
			}
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				cam_release_devq(done_ccb->ccb_h.path,
						 /*relsim_flags*/0,
						 /*reduction*/0,
						 /*timeout*/0,
						 /*getcount_only*/0);
		} else if (bp != NULL) {
			if ((done_ccb->ccb_h.status & CAM_DEV_QFRZN) != 0)
				panic("REQ_CMP with QFRZN");
			bp->bio_resid = csio->resid;
			if (csio->resid > 0)
				bp->bio_flags |= BIO_ERROR;
			if (softc->error_inject != 0) {
				bp->bio_error = softc->error_inject;
				bp->bio_resid = bp->bio_bcount;
				bp->bio_flags |= BIO_ERROR;
				softc->error_inject = 0;
			}

		}

		/*
		 * Block out any asyncronous callbacks
		 * while we touch the pending ccb list.
		 */
		LIST_REMOVE(&done_ccb->ccb_h, periph_links.le);
		softc->outstanding_cmds--;
		if (softc->outstanding_cmds == 0)
			softc->flags |= DA_FLAG_WENT_IDLE;

		if ((softc->flags & DA_FLAG_PACK_INVALID) != 0) {
			xpt_print(periph->path, "oustanding %d\n",
				  softc->outstanding_cmds);
		}

		if ((csio->ccb_h.ccb_state & DA_CCB_TYPE_MASK) ==
		    DA_CCB_DELETE) {
			while ((bp1 = bioq_takefirst(&softc->delete_run_queue))
			    != NULL) {
				bp1->bio_resid = bp->bio_resid;
				bp1->bio_error = bp->bio_error;
				if (bp->bio_flags & BIO_ERROR)
					bp1->bio_flags |= BIO_ERROR;
				biodone(bp1);
			}
			softc->delete_running = 0;
			if (bp != NULL)
				biodone(bp);
			daschedule(periph);
		} else if (bp != NULL)
			biodone(bp);
		break;
	}
	case DA_CCB_PROBE:
	case DA_CCB_PROBE2:
	{
		struct	   scsi_read_capacity_data *rdcap;
		struct     scsi_read_capacity_data_long *rcaplong;
		char	   announce_buf[80];

		rdcap = NULL;
		rcaplong = NULL;
		if (softc->state == DA_STATE_PROBE)
			rdcap =(struct scsi_read_capacity_data *)csio->data_ptr;
		else
			rcaplong = (struct scsi_read_capacity_data_long *)
				csio->data_ptr;

		if ((csio->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP) {
			struct disk_params *dp;
			uint32_t block_size;
			uint64_t maxsector;
			u_int lbppbe;	/* LB per physical block exponent. */
			u_int lalba;	/* Lowest aligned LBA. */

			if (softc->state == DA_STATE_PROBE) {
				block_size = scsi_4btoul(rdcap->length);
				maxsector = scsi_4btoul(rdcap->addr);
				lbppbe = 0;
				lalba = 0;

				/*
				 * According to SBC-2, if the standard 10
				 * byte READ CAPACITY command returns 2^32,
				 * we should issue the 16 byte version of
				 * the command, since the device in question
				 * has more sectors than can be represented
				 * with the short version of the command.
				 */
				if (maxsector == 0xffffffff) {
					softc->state = DA_STATE_PROBE2;
					free(rdcap, M_SCSIDA);
					xpt_release_ccb(done_ccb);
					xpt_schedule(periph, priority);
					return;
				}
			} else {
				block_size = scsi_4btoul(rcaplong->length);
				maxsector = scsi_8btou64(rcaplong->addr);
				lbppbe = rcaplong->prot_lbppbe & SRC16_LBPPBE;
				lalba = scsi_2btoul(rcaplong->lalba_lbp);
			}

			/*
			 * Because GEOM code just will panic us if we
			 * give them an 'illegal' value we'll avoid that
			 * here.
			 */
			if (block_size == 0 && maxsector == 0) {
				snprintf(announce_buf, sizeof(announce_buf),
				        "0MB (no media?)");
			} else if (block_size >= MAXPHYS || block_size == 0) {
				xpt_print(periph->path,
				    "unsupportable block size %ju\n",
				    (uintmax_t) block_size);
				announce_buf[0] = '\0';
				cam_periph_invalidate(periph);
			} else {
				dasetgeom(periph, block_size, maxsector,
				    lbppbe, lalba & SRC16_LALBA);
				if ((lalba & SRC16_LBPME) &&
				    softc->delete_method == DA_DELETE_NONE)
					softc->delete_method = DA_DELETE_UNMAP;
				dp = &softc->params;
				snprintf(announce_buf, sizeof(announce_buf),
				        "%juMB (%ju %u byte sectors: %dH %dS/T "
                                        "%dC)", (uintmax_t)
	                                (((uintmax_t)dp->secsize *
				        dp->sectors) / (1024*1024)),
			                (uintmax_t)dp->sectors,
				        dp->secsize, dp->heads,
                                        dp->secs_per_track, dp->cylinders);
			}
		} else {
			int	error;

			announce_buf[0] = '\0';

			/*
			 * Retry any UNIT ATTENTION type errors.  They
			 * are expected at boot.
			 */
			error = daerror(done_ccb, CAM_RETRY_SELTO,
					SF_RETRY_UA|SF_NO_PRINT);
			if (error == ERESTART) {
				/*
				 * A retry was scheuled, so
				 * just return.
				 */
				return;
			} else if (error != 0) {
				struct scsi_sense_data *sense;
				int asc, ascq;
				int sense_key, error_code;
				int have_sense;
				cam_status status;
				struct ccb_getdev cgd;

				/* Don't wedge this device's queue */
				status = done_ccb->ccb_h.status;
				if ((status & CAM_DEV_QFRZN) != 0)
					cam_release_devq(done_ccb->ccb_h.path,
							 /*relsim_flags*/0,
							 /*reduction*/0,
							 /*timeout*/0,
							 /*getcount_only*/0);


				xpt_setup_ccb(&cgd.ccb_h, 
					      done_ccb->ccb_h.path,
					      CAM_PRIORITY_NORMAL);
				cgd.ccb_h.func_code = XPT_GDEV_TYPE;
				xpt_action((union ccb *)&cgd);

				if (((csio->ccb_h.flags & CAM_SENSE_PHYS) != 0)
				 || ((csio->ccb_h.flags & CAM_SENSE_PTR) != 0)
				 || ((status & CAM_AUTOSNS_VALID) == 0))
					have_sense = FALSE;
				else
					have_sense = TRUE;

				if (have_sense) {
					sense = &csio->sense_data;
					scsi_extract_sense_len(sense,
					    csio->sense_len - csio->sense_resid,
					    &error_code, &sense_key, &asc,
					    &ascq, /*show_errors*/ 1);
				}
				/*
				 * If we tried READ CAPACITY(16) and failed,
				 * fallback to READ CAPACITY(10).
				 */
				if ((softc->state == DA_STATE_PROBE2) &&
				    (softc->flags & DA_FLAG_CAN_RC16) &&
				    (((csio->ccb_h.status & CAM_STATUS_MASK) ==
					CAM_REQ_INVALID) ||
				     ((have_sense) &&
				      (error_code == SSD_CURRENT_ERROR) &&
				      (sense_key == SSD_KEY_ILLEGAL_REQUEST)))) {
					softc->flags &= ~DA_FLAG_CAN_RC16;
					softc->state = DA_STATE_PROBE;
					free(rdcap, M_SCSIDA);
					xpt_release_ccb(done_ccb);
					xpt_schedule(periph, priority);
					return;
				} else
				/*
				 * Attach to anything that claims to be a
				 * direct access or optical disk device,
				 * as long as it doesn't return a "Logical
				 * unit not supported" (0x25) error.
				 */
				if ((have_sense) && (asc != 0x25)
				 && (error_code == SSD_CURRENT_ERROR)) {
					const char *sense_key_desc;
					const char *asc_desc;

					scsi_sense_desc(sense_key, asc, ascq,
							&cgd.inq_data,
							&sense_key_desc,
							&asc_desc);
					snprintf(announce_buf,
					    sizeof(announce_buf),
						"Attempt to query device "
						"size failed: %s, %s",
						sense_key_desc,
						asc_desc);
				} else { 
					if (have_sense)
						scsi_sense_print(
							&done_ccb->csio);
					else {
						xpt_print(periph->path,
						    "got CAM status %#x\n",
						    done_ccb->ccb_h.status);
					}

					xpt_print(periph->path, "fatal error, "
					    "failed to attach to device\n");

					/*
					 * Free up resources.
					 */
					cam_periph_invalidate(periph);
				} 
			}
		}
		free(csio->data_ptr, M_SCSIDA);
		if (announce_buf[0] != '\0') {
			/*
			 * Create our sysctl variables, now that we know
			 * we have successfully attached.
			 */
			/* increase the refcount */
			if (cam_periph_acquire(periph) == CAM_REQ_CMP) {
				taskqueue_enqueue(taskqueue_thread,
						  &softc->sysctl_task);
				xpt_announce_periph(periph, announce_buf);
			} else {
				xpt_print(periph->path, "fatal error, "
				    "could not acquire reference count\n");
			}
				
		}
		softc->state = DA_STATE_NORMAL;	
		/*
		 * Since our peripheral may be invalidated by an error
		 * above or an external event, we must release our CCB
		 * before releasing the probe lock on the peripheral.
		 * The peripheral will only go away once the last lock
		 * is removed, and we need it around for the CCB release
		 * operation.
		 */
		xpt_release_ccb(done_ccb);
		cam_periph_unhold(periph);
		return;
	}
	case DA_CCB_WAITING:
	{
		/* Caller will release the CCB */
		wakeup(&done_ccb->ccb_h.cbfcnp);
		return;
	}
	case DA_CCB_DUMP:
		/* No-op.  We're polling */
		return;
	default:
		break;
	}
	xpt_release_ccb(done_ccb);
}

static int
daerror(union ccb *ccb, u_int32_t cam_flags, u_int32_t sense_flags)
{
	struct da_softc	  *softc;
	struct cam_periph *periph;
	int error;

	periph = xpt_path_periph(ccb->ccb_h.path);
	softc = (struct da_softc *)periph->softc;

 	/*
	 * Automatically detect devices that do not support
 	 * READ(6)/WRITE(6) and upgrade to using 10 byte cdbs.
 	 */
	error = 0;
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INVALID) {
		error = cmd6workaround(ccb);
	} else if (((ccb->ccb_h.status & CAM_STATUS_MASK) ==
		   CAM_SCSI_STATUS_ERROR)
	 && (ccb->ccb_h.status & CAM_AUTOSNS_VALID)
	 && (ccb->csio.scsi_status == SCSI_STATUS_CHECK_COND)
	 && ((ccb->ccb_h.flags & CAM_SENSE_PHYS) == 0)
	 && ((ccb->ccb_h.flags & CAM_SENSE_PTR) == 0)) {
		int sense_key, error_code, asc, ascq;

 		scsi_extract_sense(&ccb->csio.sense_data,
				   &error_code, &sense_key, &asc, &ascq);
		if (sense_key == SSD_KEY_ILLEGAL_REQUEST)
 			error = cmd6workaround(ccb);
	}
	if (error == ERESTART)
		return (ERESTART);

	/*
	 * XXX
	 * Until we have a better way of doing pack validation,
	 * don't treat UAs as errors.
	 */
	sense_flags |= SF_RETRY_UA;
	return(cam_periph_error(ccb, cam_flags, sense_flags,
				&softc->saved_ccb));
}

static void
daprevent(struct cam_periph *periph, int action)
{
	struct	da_softc *softc;
	union	ccb *ccb;		
	int	error;
		
	softc = (struct da_softc *)periph->softc;

	if (((action == PR_ALLOW)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) == 0)
	 || ((action == PR_PREVENT)
	  && (softc->flags & DA_FLAG_PACK_LOCKED) != 0)) {
		return;
	}

	ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);

	scsi_prevent(&ccb->csio,
		     /*retries*/1,
		     /*cbcfp*/dadone,
		     MSG_SIMPLE_Q_TAG,
		     action,
		     SSD_FULL_SIZE,
		     5000);

	error = cam_periph_runccb(ccb, daerror, CAM_RETRY_SELTO,
	    SF_RETRY_UA | SF_QUIET_IR, softc->disk->d_devstat);

	if (error == 0) {
		if (action == PR_ALLOW)
			softc->flags &= ~DA_FLAG_PACK_LOCKED;
		else
			softc->flags |= DA_FLAG_PACK_LOCKED;
	}

	xpt_release_ccb(ccb);
}

static int
dagetcapacity(struct cam_periph *periph)
{
	struct da_softc *softc;
	union ccb *ccb;
	struct scsi_read_capacity_data *rcap;
	struct scsi_read_capacity_data_long *rcaplong;
	uint32_t block_len;
	uint64_t maxsector;
	int error, rc16failed;
	u_int32_t sense_flags;
	u_int lbppbe;	/* Logical blocks per physical block exponent. */
	u_int lalba;	/* Lowest aligned LBA. */

	softc = (struct da_softc *)periph->softc;
	block_len = 0;
	maxsector = 0;
	lbppbe = 0;
	lalba = 0;
	error = 0;
	rc16failed = 0;
	sense_flags = SF_RETRY_UA;
	if (softc->flags & DA_FLAG_PACK_REMOVABLE)
		sense_flags |= SF_NO_PRINT;

	/* Do a read capacity */
	rcap = (struct scsi_read_capacity_data *)malloc(sizeof(*rcaplong),
							M_SCSIDA,
							M_NOWAIT | M_ZERO);
	if (rcap == NULL)
		return (ENOMEM);
	rcaplong = (struct scsi_read_capacity_data_long *)rcap;

	ccb = cam_periph_getccb(periph, CAM_PRIORITY_NORMAL);

	/* Try READ CAPACITY(16) first if we think it should work. */
	if (softc->flags & DA_FLAG_CAN_RC16) {
		scsi_read_capacity_16(&ccb->csio,
			      /*retries*/ 4,
			      /*cbfcnp*/ dadone,
			      /*tag_action*/ MSG_SIMPLE_Q_TAG,
			      /*lba*/ 0,
			      /*reladr*/ 0,
			      /*pmi*/ 0,
			      rcaplong,
			      /*sense_len*/ SSD_FULL_SIZE,
			      /*timeout*/ 60000);
		ccb->ccb_h.ccb_bp = NULL;

		error = cam_periph_runccb(ccb, daerror,
				  /*cam_flags*/CAM_RETRY_SELTO,
				  sense_flags,
				  softc->disk->d_devstat);
		if (error == 0)
			goto rc16ok;

		/* If we got ILLEGAL REQUEST, do not prefer RC16 any more. */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) ==
		     CAM_REQ_INVALID) {
			softc->flags &= ~DA_FLAG_CAN_RC16;
		} else if (((ccb->ccb_h.status & CAM_STATUS_MASK) ==
		     CAM_SCSI_STATUS_ERROR) &&
		    (ccb->csio.scsi_status == SCSI_STATUS_CHECK_COND) &&
		    (ccb->ccb_h.status & CAM_AUTOSNS_VALID) &&
		    ((ccb->ccb_h.flags & CAM_SENSE_PHYS) == 0) &&
		    ((ccb->ccb_h.flags & CAM_SENSE_PTR) == 0)) {
			int sense_key, error_code, asc, ascq;

			scsi_extract_sense(&ccb->csio.sense_data,
				   &error_code, &sense_key, &asc, &ascq);
			if (sense_key == SSD_KEY_ILLEGAL_REQUEST)
				softc->flags &= ~DA_FLAG_CAN_RC16;
		}
		rc16failed = 1;
	}

	/* Do READ CAPACITY(10). */
	scsi_read_capacity(&ccb->csio,
			   /*retries*/4,
			   /*cbfncp*/dadone,
			   MSG_SIMPLE_Q_TAG,
			   rcap,
			   SSD_FULL_SIZE,
			   /*timeout*/60000);
	ccb->ccb_h.ccb_bp = NULL;

	error = cam_periph_runccb(ccb, daerror,
				  /*cam_flags*/CAM_RETRY_SELTO,
				  sense_flags,
				  softc->disk->d_devstat);
	if (error == 0) {
		block_len = scsi_4btoul(rcap->length);
		maxsector = scsi_4btoul(rcap->addr);

		if (maxsector != 0xffffffff || rc16failed)
			goto done;
	} else
		goto done;

	/* If READ CAPACITY(10) returned overflow, use READ CAPACITY(16) */
	scsi_read_capacity_16(&ccb->csio,
			      /*retries*/ 4,
			      /*cbfcnp*/ dadone,
			      /*tag_action*/ MSG_SIMPLE_Q_TAG,
			      /*lba*/ 0,
			      /*reladr*/ 0,
			      /*pmi*/ 0,
			      rcaplong,
			      /*sense_len*/ SSD_FULL_SIZE,
			      /*timeout*/ 60000);
	ccb->ccb_h.ccb_bp = NULL;

	error = cam_periph_runccb(ccb, daerror,
				  /*cam_flags*/CAM_RETRY_SELTO,
				  sense_flags,
				  softc->disk->d_devstat);
	if (error == 0) {
rc16ok:
		block_len = scsi_4btoul(rcaplong->length);
		maxsector = scsi_8btou64(rcaplong->addr);
		lbppbe = rcaplong->prot_lbppbe & SRC16_LBPPBE;
		lalba = scsi_2btoul(rcaplong->lalba_lbp);
	}

done:

	if (error == 0) {
		if (block_len >= MAXPHYS || block_len == 0) {
			xpt_print(periph->path,
			    "unsupportable block size %ju\n",
			    (uintmax_t) block_len);
			error = EINVAL;
		} else {
			dasetgeom(periph, block_len, maxsector,
			    lbppbe, lalba & SRC16_LALBA);
			if ((lalba & SRC16_LBPME) &&
			    softc->delete_method == DA_DELETE_NONE)
				softc->delete_method = DA_DELETE_UNMAP;
		}
	}

	xpt_release_ccb(ccb);

	free(rcap, M_SCSIDA);

	return (error);
}

static void
dasetgeom(struct cam_periph *periph, uint32_t block_len, uint64_t maxsector,
    u_int lbppbe, u_int lalba)
{
	struct ccb_calc_geometry ccg;
	struct da_softc *softc;
	struct disk_params *dp;

	softc = (struct da_softc *)periph->softc;

	dp = &softc->params;
	dp->secsize = block_len;
	dp->sectors = maxsector + 1;
	if (lbppbe > 0) {
		dp->stripesize = block_len << lbppbe;
		dp->stripeoffset = (dp->stripesize - block_len * lalba) %
		    dp->stripesize;
	} else if (softc->quirks & DA_Q_4K) {
		dp->stripesize = 4096;
		dp->stripeoffset = 0;
	} else {
		dp->stripesize = 0;
		dp->stripeoffset = 0;
	}
	/*
	 * Have the controller provide us with a geometry
	 * for this disk.  The only time the geometry
	 * matters is when we boot and the controller
	 * is the only one knowledgeable enough to come
	 * up with something that will make this a bootable
	 * device.
	 */
	xpt_setup_ccb(&ccg.ccb_h, periph->path, CAM_PRIORITY_NORMAL);
	ccg.ccb_h.func_code = XPT_CALC_GEOMETRY;
	ccg.block_size = dp->secsize;
	ccg.volume_size = dp->sectors;
	ccg.heads = 0;
	ccg.secs_per_track = 0;
	ccg.cylinders = 0;
	xpt_action((union ccb*)&ccg);
	if ((ccg.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		/*
		 * We don't know what went wrong here- but just pick
		 * a geometry so we don't have nasty things like divide
		 * by zero.
		 */
		dp->heads = 255;
		dp->secs_per_track = 255;
		dp->cylinders = dp->sectors / (255 * 255);
		if (dp->cylinders == 0) {
			dp->cylinders = 1;
		}
	} else {
		dp->heads = ccg.heads;
		dp->secs_per_track = ccg.secs_per_track;
		dp->cylinders = ccg.cylinders;
	}
}

static void
dasendorderedtag(void *arg)
{
	struct da_softc *softc = arg;

	if (da_send_ordered) {
		if ((softc->ordered_tag_count == 0) 
		 && ((softc->flags & DA_FLAG_WENT_IDLE) == 0)) {
			softc->flags |= DA_FLAG_NEED_OTAG;
		}
		if (softc->outstanding_cmds > 0)
			softc->flags &= ~DA_FLAG_WENT_IDLE;

		softc->ordered_tag_count = 0;
	}
	/* Queue us up again */
	callout_reset(&softc->sendordered_c,
	    (da_default_timeout * hz) / DA_ORDEREDTAG_INTERVAL,
	    dasendorderedtag, softc);
}

/*
 * Step through all DA peripheral drivers, and if the device is still open,
 * sync the disk cache to physical media.
 */
static void
dashutdown(void * arg, int howto)
{
	struct cam_periph *periph;
	struct da_softc *softc;
	int error;

	TAILQ_FOREACH(periph, &dadriver.units, unit_links) {
		union ccb ccb;

		cam_periph_lock(periph);
		softc = (struct da_softc *)periph->softc;

		/*
		 * We only sync the cache if the drive is still open, and
		 * if the drive is capable of it..
		 */
		if (((softc->flags & DA_FLAG_OPEN) == 0)
		 || (softc->quirks & DA_Q_NO_SYNC_CACHE)) {
			cam_periph_unlock(periph);
			continue;
		}

		xpt_setup_ccb(&ccb.ccb_h, periph->path, CAM_PRIORITY_NORMAL);

		ccb.ccb_h.ccb_state = DA_CCB_DUMP;
		scsi_synchronize_cache(&ccb.csio,
				       /*retries*/0,
				       /*cbfcnp*/dadone,
				       MSG_SIMPLE_Q_TAG,
				       /*begin_lba*/0, /* whole disk */
				       /*lb_count*/0,
				       SSD_FULL_SIZE,
				       60 * 60 * 1000);

		xpt_polled_action(&ccb);

		error = cam_periph_error(&ccb,
		    0, SF_NO_RECOVERY | SF_NO_RETRY | SF_QUIET_IR, NULL);
		if ((ccb.ccb_h.status & CAM_DEV_QFRZN) != 0)
			cam_release_devq(ccb.ccb_h.path, /*relsim_flags*/0,
			    /*reduction*/0, /*timeout*/0, /*getcount_only*/0);
		if (error != 0)
			xpt_print(periph->path, "Synchronize cache failed\n");
		cam_periph_unlock(periph);
	}
}

#else /* !_KERNEL */

/*
 * XXX This is only left out of the kernel build to silence warnings.  If,
 * for some reason this function is used in the kernel, the ifdefs should
 * be moved so it is included both in the kernel and userland.
 */
void
scsi_format_unit(struct ccb_scsiio *csio, u_int32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 u_int8_t tag_action, u_int8_t byte2, u_int16_t ileave,
		 u_int8_t *data_ptr, u_int32_t dxfer_len, u_int8_t sense_len,
		 u_int32_t timeout)
{
	struct scsi_format_unit *scsi_cmd;

	scsi_cmd = (struct scsi_format_unit *)&csio->cdb_io.cdb_bytes;
	scsi_cmd->opcode = FORMAT_UNIT;
	scsi_cmd->byte2 = byte2;
	scsi_ulto2b(ileave, scsi_cmd->interleave);

	cam_fill_csio(csio,
		      retries,
		      cbfcnp,
		      /*flags*/ (dxfer_len > 0) ? CAM_DIR_OUT : CAM_DIR_NONE,
		      tag_action,
		      data_ptr,
		      dxfer_len,
		      sense_len,
		      sizeof(*scsi_cmd),
		      timeout);
}

#endif /* _KERNEL */
