/*-
 * Copyright (c) 2004 Lukas Ertl
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
 *
 * $FreeBSD: release/7.0.0/sys/geom/vinum/geom_vinum_raid5.h 161425 2006-08-17 22:50:33Z imp $
 */

#ifndef _GEOM_VINUM_RAID5_H_
#define	_GEOM_VINUM_RAID5_H_

/*
 * A single RAID5 request usually needs more than one I/O transaction,
 * depending on the state of the associated subdisks and the direction of the
 * transaction (read or write).
 */

#define	GV_ENQUEUE(bp, cbp, pbp)				\
	do { 							\
		if (bp->bio_driver1 == NULL) {			\
			bp->bio_driver1 = cbp;			\
		} else {					\
			pbp = bp->bio_driver1;			\
			while (pbp->bio_caller1 != NULL)	\
				pbp = pbp->bio_caller1;		\
			pbp->bio_caller1 = cbp;			\
		}						\
	} while (0)

struct gv_raid5_packet {
	caddr_t	data;		/* Data buffer of this sub-request- */
	off_t	length;		/* Size of data buffer. */
	off_t	lockbase;	/* Deny access to our plex offset. */
	off_t	offset;		/* The drive offset of the subdisk. */
	int	bufmalloc;	/* Flag if data buffer was malloced. */
	int	active;		/* Count of active subrequests. */
	int	rqcount;	/* Count of subrequests. */

	struct bio	*bio;	/* Pointer to the original bio. */
	struct bio	*parity;  /* The bio containing the parity data. */
	struct bio	*waiting; /* A bio that need to wait for other bios. */

	TAILQ_HEAD(,gv_bioq)		bits; /* List of subrequests. */
	TAILQ_ENTRY(gv_raid5_packet)	list; /* Entry in plex's packet list. */
};

int	gv_stripe_active(struct gv_plex *, struct bio *);
int	gv_build_raid5_req(struct gv_plex *, struct gv_raid5_packet *,
	    struct bio *, caddr_t, off_t, off_t);
int	gv_check_raid5(struct gv_plex *, struct gv_raid5_packet *,
	    struct bio *, caddr_t, off_t, off_t);
int	gv_rebuild_raid5(struct gv_plex *, struct gv_raid5_packet *,
	    struct bio *, caddr_t, off_t, off_t);
void	gv_raid5_worker(void *);
void	gv_plex_done(struct bio *);

#endif /* !_GEOM_VINUM_RAID5_H_ */
