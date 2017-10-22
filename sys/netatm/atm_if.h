/*-
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD: release/7.0.0/sys/netatm/atm_if.h 147256 2005-06-10 16:49:24Z brooks $
 *
 */

/*
 * Core ATM Services
 * -----------------
 *
 * ATM Physical and Network Interface definitions 
 *
 */

#ifndef _NETATM_ATM_IF_H
#define _NETATM_ATM_IF_H

/*
 * Handy constants
 */
#define ATM_NIF_MTU	9180		/* Default network interface MTU */

#define ATM_PCR_25	59111		/* Peak Cell Rate for 25.6 Mbs */
#define ATM_PCR_DS3	(12*8000)	/* 12 cells in 1/8000 sec */
#define ATM_PCR_TAXI100	227273		/* Peak Cell Rate for 100 Mbs TAXI */
#define ATM_PCR_TAXI140	318181		/* Peak Cell Rate for 140 Mbs TAXI */
#define ATM_PCR_OC3C	353207		/* Peak Cell Rate for OC3c */
#define ATM_PCR_OC12C	1416905		/* Peak Cell Rate for OC12c */


/*
 * Media Access Control (MAC) address
 */
struct mac_addr	{
	u_char		ma_data[6];	/* MAC address */
};
typedef struct mac_addr	Mac_addr;


/*
 * Adapter vendor identifiers
 */
enum atm_vendor {
	VENDOR_UNKNOWN,			/* Unknown vendor */
	VENDOR_FORE,			/* FORE Systems, Inc. */
	VENDOR_ENI,			/* Efficient Networks, Inc. */
	VENDOR_IDT,			/* Integrated Device Technology, Inc. */
	VENDOR_PROSUM,			/* Prosum, Inc. */
	VENDOR_NETGRAPH			/* Netgraph device */
};
typedef enum atm_vendor	Atm_vendor;


/*
 * Adapter vendor interface identifiers
 */
enum atm_vendapi {
	VENDAPI_UNKNOWN,		/* Unknown interface */
	VENDAPI_FORE_1,			/* FORE - 200 Series */
	VENDAPI_ENI_1,			/* ENI - Midway */
	VENDAPI_IDT_1,			/* IDT - NICStAR */
	VENDAPI_IDT_2,			/* IDT 77252 */
	VENDAPI_NETGRAPH_1,		/* Netgraph API v1 */
	VENDAPI_FORE_2,			/* FORE - HE Series */
};
typedef enum atm_vendapi	Atm_vendapi;


/*
 * Adapter device model identifiers
 */
enum atm_device {
	DEV_UNKNOWN,			/* Unknown device */
	DEV_FORE_SBA200E,		/* FORE SBA-200E */
	DEV_FORE_SBA200,		/* FORE SBA-200 */
	DEV_FORE_PCA200E,		/* FORE PCA-200E */
	DEV_FORE_ESA200E,		/* FORE ESA-200E */
	DEV_ENI_155P,			/* ENI-155p */
	DEV_IDT_155,			/* IDT NICStAR */
	DEV_PROATM_25,			/* Prosum boards based on IDT 77252 */
	DEV_PROATM_155,			/* Prosum boards based on IDT 77252 */
	DEV_VATMPIF,			/* Virtual ATM Physical IF */
	DEV_FORE_LE25,			/* ForeLE-25 */
	DEV_FORE_LE155,			/* ForeLE-155 */
	DEV_IDT_25,			/* IDT NICStAR */
	DEV_IDTABR_25,			/* IDT 77252 evaluation board */
	DEV_IDTABR_155,			/* IDT 77252 evaluation board */
	DEV_FORE_HE155,			/* ForeRunnerHE-155 */
	DEV_FORE_HE622,			/* ForeRunnerHE-622 */
};
typedef enum atm_device	Atm_device;


/*
 * Adapter media identifiers
 */
enum atm_media {
	MEDIA_UNKNOWN,			/* Unknown media type */
	MEDIA_TAXI_100,			/* TAXI - 100 Mbps */
	MEDIA_TAXI_140,			/* TAXI - 140 Mbps */
	MEDIA_OC3C,			/* OC-3C */
	MEDIA_OC12C,			/* OC-12C */
	MEDIA_UTP155,			/* UTP-155 */
	MEDIA_UTP25,			/* UTP-25.6 */
	MEDIA_VIRTUAL,			/* Virtual Link */
	MEDIA_DSL			/* xDSL */
};
typedef enum atm_media	Atm_media;


/*
 * Bus type identifiers
 */
enum atm_bus {
	BUS_UNKNOWN,			/* Unknown bus type */
	BUS_SBUS_B16,			/* SBus: 16 byte (4 word) max burst */
	BUS_SBUS_B32,			/* SBus: 32 byte (8 word) max burst */
	BUS_PCI,			/* PCI */
	BUS_EISA,			/* EISA */
	BUS_USB,			/* USB */
	BUS_VIRTUAL			/* Virtual Bus */
};
typedef enum atm_bus	Atm_bus;


#define	VERSION_LEN	16		/* Length of version info string */


/*
 * ATM adapter configuration information structure
 */
struct atm_config {
	Atm_vendor	ac_vendor;	/* Vendor */
	Atm_vendapi	ac_vendapi;	/* Vendor interface */
	Atm_device	ac_device;	/* Device model */
	Atm_media	ac_media;	/* Media type */
	u_long		ac_serial;	/* Serial number */
	Atm_bus		ac_bustype;	/* Bus type */
	u_long		ac_busslot;	/* Bus slot info (bus type dependent) */
	u_long		ac_ram;		/* Device ram offset */
	u_long		ac_ramsize;	/* Device ram size */
	Mac_addr	ac_macaddr;	/* MAC address */
	char		ac_hard_vers[VERSION_LEN];	/* Hardware version */
	char		ac_firm_vers[VERSION_LEN];	/* Firmware version */
};
typedef struct atm_config	Atm_config;


#ifdef _KERNEL

#include <vm/uma.h>

/*
 * Common structure used to define each physical ATM device interface.
 * This structure will (normally) be embedded at the top of each driver's 
 * device-specific interface structure.  
 */
struct	atm_pif {
	struct atm_pif	*pif_next;	/* Next registered atm interface */
	const char	*pif_name;	/* Device name */
	short		pif_unit;	/* Device unit number */
	u_char		pif_flags;	/* Interface flags (see below) */
	struct sigmgr	*pif_sigmgr;	/* Signalling Manager for interface */
	struct siginst	*pif_siginst;	/* Signalling protocol instance */
	struct stack_defn	*pif_services;	/* Interface's stack services */
	struct mac_addr	pif_macaddr;	/* Interface's MAC address */
	struct atm_nif	*pif_nif;	/* List of network interfaces */
	struct atm_pif	*pif_grnext;	/* Next atm device in group */

/* Exported functions */
	int		(*pif_ioctl)	/* Interface ioctl handler */
				(int, caddr_t, caddr_t);

/* Interface statistics */
	u_quad_t	pif_ipdus;	/* PDUs received from interface */
	u_quad_t	pif_opdus;	/* PDUs sent to interface */
	u_quad_t	pif_ibytes;	/* Bytes received from interface */
	u_quad_t	pif_obytes;	/* Bytes sent to interface */
	u_quad_t	pif_ierrors;	/* Errors receiving from interface */
	u_quad_t	pif_oerrors;	/* Errors sending to interface */
	u_quad_t	pif_cmderrors;	/* Interface command errors */
	caddr_t		pif_cardstats;	/* Card specific statistics */

/* Interface capabilities */
	u_short		pif_maxvpi;	/* Maximum VPI value supported */
	u_short		pif_maxvci;	/* Maximum VCI value supported */
	u_int		pif_pcr;	/* Peak Cell Rate */
};

/*
 * Physical interface flags
 */
#define	PIF_UP		0x01		/* Interface is up */
#define	PIF_LOOPBACK	0x02		/* Loopback local packets */


/*
 * Structure defining an ATM network interface.  This structure is used as 
 * the hook between the standard BSD network layer interface mechanism and 
 * the ATM device layer.  There may be one or more network interfaces for 
 * each physical ATM interface.
 */
struct	atm_nif {
	struct ifnet	*nif_ifp;	/* Network interface */
	struct atm_pif	*nif_pif;	/* Our physical interface */
	char		nif_name[IFNAMSIZ];/* Network interface name */
	u_char		nif_sel;	/* Interface's address selector */
	struct atm_nif	*nif_pnext;	/* Next net interface on phys i/f */

/* Interface statistics (in addition to ifnet stats) */
	long		nif_ibytes;	/* Bytes received from interface */
	long		nif_obytes;	/* Bytes sent to interface */
};
#define ANIF2IFP(an)	((an)->nif_ifp)
#define IFP2ANIF(ifp)	((struct atm_nif *)(ifp)->if_softc)

/*
 * Common Device VCC Entry
 *
 * Contains the common information for each VCC which is opened
 * through a particular device.
 */
struct cmn_vcc {
	struct cmn_vcc	*cv_next;	/* Next in list */
	void		*cv_toku;	/* Upper layer's token */
	void		(*cv_upper)	/* Upper layer's interface */
				(int, void *, intptr_t, intptr_t);
	Atm_connvc	*cv_connvc;	/* Associated connection VCC */
	u_char		cv_state;	/* VCC state (see below) */
	u_char		cv_flags;	/* VCC flags (see below) */
};
typedef struct cmn_vcc	Cmn_vcc;

/*
 * VCC States
 */
#define	CVS_FREE	0		/* Not allocated */
#define	CVS_INST	1		/* Instantiated, waiting for INIT */
#define	CVS_INITED	2		/* Initialized, waiting for driver */
#define	CVS_ACTIVE	3		/* Device activated by driver */
#define	CVS_PTERM	4		/* Waiting for TERM */
#define	CVS_TERM	5		/* Terminated */

/*
 * VCC Flags
 */
#define	CVF_RSVD	0x0f		/* Reserved for device-specific use */


/*
 * Common Device Unit Structure
 *
 * Contains the common information for a single device (adapter).
 */
struct cmn_unit {
	struct atm_pif	cu_pif;		/* Physical interface */
	u_int		cu_unit;	/* Local unit number */
	u_char		cu_flags;	/* Device flags (see below) */
	u_int		cu_mtu;		/* Interface MTU */

	u_int		cu_open_vcc;	/* Open VCC count */
	Cmn_vcc		*cu_vcc;	/* List of VCC's on interface */

	u_int		cu_intrpri;	/* Highest unit interrupt priority */
	int		cu_savepri;	/* Saved priority for locking device */

	uma_zone_t	cu_vcc_zone;	/* Device VCC zone */
	uma_zone_t	cu_nif_zone;	/* Device NIF zone */

	int		(*cu_ioctl)	/* Interface ioctl handler */
				(int, caddr_t, caddr_t);
	int		(*cu_instvcc)	/* VCC stack instantion handler */
				(struct cmn_unit *, Cmn_vcc *);
	int		(*cu_openvcc)	/* Open VCC handler */
				(struct cmn_unit *, Cmn_vcc *);
	int		(*cu_closevcc)	/* Close VCC handler */
				(struct cmn_unit *, Cmn_vcc *);
	void		(*cu_output)	/* Data output handler */
				(struct cmn_unit *, Cmn_vcc *, KBuffer *);

	Atm_config	cu_config;	/* Device configuration data */

	void *		cu_softc;	/* pointer to driver state */
};
typedef struct cmn_unit	Cmn_unit;

/*
 * Device flags
 */
#define	CUF_REGISTER	0x01		/* Device is registered */
#define	CUF_INITED	0x02		/* Device is initialized */


/*
 * Structure used to define a network convergence module and its associated
 * entry points.  A convergence module is used to provide the interface
 * translations necessary between the ATM system and the BSD network layer
 * interface mechanism.  There will be one network convergence module for
 * each protocol address family supporting ATM connections.
 */
struct atm_ncm {
	struct atm_ncm	*ncm_next;	/* Next in registry list */
	u_short		ncm_family;	/* Protocol family */
/* Exported functions */
	int		(*ncm_ifoutput)	/* Interface if_output handler */
				(struct ifnet *, KBuffer *, struct sockaddr *);
	int		(*ncm_stat)	/* Network i/f status handler */
				(int, struct atm_nif *, intptr_t);
};

/*
 * ncm_stat() commands
 */
#define	NCM_ATTACH	1		/* Attaching a new net i/f */
#define	NCM_DETACH	2		/* Detaching a current net i/f */
#define	NCM_SETADDR	3		/* Net i/f address change */
#define	NCM_SIGATTACH	4		/* Attaching a signalling manager */
#define	NCM_SIGDETACH	5		/* Detaching a signalling manager */


/*
 * atm_dev_alloc() parameters
 */
#define	ATM_DEV_NONCACHE	1	/* Allocate non-cacheable memory */

/*
 * atm_dev_compress() buffer allocation sizes
 */
#define	ATM_DEV_CMPR_LG	MCLBYTES	/* Size of large buffers */
#define	ATM_DEV_CMPR_SM	MLEN		/* Size of small buffers */

/*
 * Macros to lock out device interrupts
 */
#define	DEVICE_LOCK(u)		((u)->cu_savepri = splimp())
#define	DEVICE_UNLOCK(u)	((void) splx((u)->cu_savepri))

/*
 * Macro to schedule the ATM interrupt queue handler
 */
typedef	void (atm_intr_t)(void *, KBuffer *); /* Callback function type */
typedef	atm_intr_t	*atm_intr_func_t; /* Pointer to callback function */

#endif /* _KERNEL */

#endif	/* _NETATM_ATM_IF_H */
