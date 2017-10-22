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
 *	@(#) $FreeBSD: release/7.0.0/sys/netatm/uni/sscf_uni_var.h 139823 2005-01-07 01:45:51Z imp $
 *
 */

/*
 * ATM Forum UNI Support
 * ---------------------
 *
 * SSCF UNI protocol control blocks
 *
 */

#ifndef _UNI_SSCF_UNI_VAR_H
#define _UNI_SSCF_UNI_VAR_H

/*
 * Structure containing information for each SSCF UNI connection.
 */
struct univcc {
	u_char		uv_ustate;	/* SSCF-User state (see below) */
	u_char		uv_lstate;	/* SSCF-SSCOP state (see below) */
	u_short		uv_flags;	/* Connection flags (see below) */
	enum uni_vers	uv_vers;	/* UNI version */

	/* Stack variables */
	Atm_connvc	*uv_connvc;	/* Connection vcc for this stack */
	void		*uv_toku;	/* Stack upper layer's token */
	void		*uv_tokl;	/* Stack lower layer's token */
	void		(*uv_upper)	/* Stack upper layer's interface */
				(int, void *, intptr_t, intptr_t);
	void		(*uv_lower)	/* Stack lower layer's interface */
				(int, void *, intptr_t, intptr_t);
};

/*
 * SSCF to SAAL User (Q.2931) Interface States
 */
#define	UVU_INST	0		/* Instantiated, waiting for INIT */
#define	UVU_RELEASED	1		/* Connection released */
#define	UVU_PACTIVE	2		/* Awaiting connection establishment */
#define	UVU_PRELEASE	3		/* Awaiting connection release */
#define	UVU_ACTIVE	4		/* Connection established */
#define	UVU_TERM	5		/* Waiting for TERM */

/*
 * SSCF to SSCOP Interface States
 */
#define	UVL_INST	0		/* Instantiated, waiting for INIT */
#define	UVL_IDLE	1		/* Idle */
#define	UVL_OUTCONN	2		/* Outgoing connection pending */
#define	UVL_INCONN	3		/* Incoming connection pending */
#define	UVL_OUTDISC	4		/* Outgoing disconnection pending */
#define	UVL_OUTRESYN	5		/* Outgoing resynchronization pending */
#define	UVL_INRESYN	6		/* Incoming resynchornization pending */
#define	UVL_RECOVERY	8		/* Recovery pending */
#define	UVL_READY	10		/* Data transfer ready */
#define	UVL_TERM	11		/* Waiting for TERM */

/*
 * Connection Flags
 */
#define	UVF_NOESTIND	0x0001		/* Don't process ESTABLISH_IND */


#ifdef _KERNEL
/*
 * Global function declarations
 */
	/* sscf_uni.c */
int		sscf_uni_start(void);
int		sscf_uni_stop(void);
void		sscf_uni_abort(struct univcc *, char *);
void		sscf_uni_pdu_print(const struct univcc *,
		    const KBuffer *, const char *);

	/* sscf_uni_lower.c */
void		sscf_uni_lower(int, void *, intptr_t, intptr_t);

	/* sscf_uni_upper.c */
void		sscf_uni_upper(int, void *, intptr_t, intptr_t);


/*
 * External variables
 */
extern int		sscf_uni_vccnt;

#endif	/* _KERNEL */

#endif	/* _UNI_SSCF_UNI_VAR_H */
