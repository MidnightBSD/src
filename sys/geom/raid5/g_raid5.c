/*-
 * Copyright (c) 2010 Lev Serebryakov <lev@FreeBSD.org>
 * Copyright (c) 2006 Arne Wörner <arne_woerner@yahoo.com>
 * testing + tuning-tricks: veronica@fluffles.net
 * derived from gstripe/gmirror (Pawel Jakub Dawidek <pjd@FreeBSD.org>)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/* Historic Id: g_raid5.c,v 1.271.1.120.1.11 2008/02/11 18:09:27 aw Exp aw */
__MBSDID("$MidnightBSD$");

/*ARNE*/
#ifdef KASSERT
#define MYKASSERT(a,b) KASSERT(a,b)
#else
#define MYKASSERT(a,b) do {if (!(a)) { G_RAID5_DEBUG(0,"KASSERT in line %d.",__LINE__); panic b;}} while (0)
#endif
#define ORDER(a,b) do {if (a > b) { int tmp = a; a = b; b = tmp; }} while(0)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/eventhandler.h>
#include <sys/sched.h>
#include <geom/geom.h>

#include "g_raid5.h"

static char graid5_version[]  = "1.0.20101212.29";
static char graid5_revision[] = "78b102ab875d";

SYSCTL_DECL(_kern_geom);
SYSCTL_NODE(_kern_geom, OID_AUTO, raid5, CTLFLAG_RW, 0, "GEOM_RAID5 stuff");
static u_int g_raid5_debug = 0;
TUNABLE_INT("kern.geom.raid5.debug", &g_raid5_debug);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, debug, CTLFLAG_RW, &g_raid5_debug, 0,
    "Debug level");
static u_int g_raid5_tooc = 5;
TUNABLE_INT("kern.geom.raid5.tooc", &g_raid5_tooc);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, tooc, CTLFLAG_RW, &g_raid5_tooc, 0,
    "timeout on create (in order to avoid unnecessary rebuilds on reboot)");
static u_int g_raid5_wdt = 3;
TUNABLE_INT("kern.geom.raid5.wdt", &g_raid5_wdt);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wdt, CTLFLAG_RW, &g_raid5_wdt, 0,
    "write request delay (in seconds)");
static u_int g_raid5_maxwql = 50;
TUNABLE_INT("kern.geom.raid5.maxwql", &g_raid5_maxwql);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, maxwql, CTLFLAG_RW, &g_raid5_maxwql, 0,
    "max wait queue length");
static u_int g_raid5_maxmem = 8000000;
TUNABLE_INT("kern.geom.raid5.maxmem", &g_raid5_maxmem);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, maxmem, CTLFLAG_RW, &g_raid5_maxmem, 0,
    "max memory used for malloc hamster");
static u_int g_raid5_veri_fac = 25;
TUNABLE_INT("kern.geom.raid5.veri_fac", &g_raid5_veri_fac);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri_fac, CTLFLAG_RW, &g_raid5_veri_fac,
    0, "veri brake factor in case of veri_min * X < veri_max");
static u_int g_raid5_veri_nice = 100;
TUNABLE_INT("kern.geom.raid5.veri_nice", &g_raid5_veri_nice);
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO,veri_nice, CTLFLAG_RW,&g_raid5_veri_nice,
    0, "wait this many milli seconds after last user-read (less than 1sec)");
static u_int g_raid5_vsc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri, CTLFLAG_RD, &g_raid5_vsc, 0,
    "verify stripe count");
static u_int g_raid5_vwc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, veri_w, CTLFLAG_RD, &g_raid5_vwc, 0,
    "verify write count");
static u_int g_raid5_coca = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, coca, CTLFLAG_RD, &g_raid5_coca,0,
    "combine catastrophe count");
static u_int g_raid5_mhh = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, mhh, CTLFLAG_RD, &g_raid5_mhh,0,
    "malloc hamster hit");
static u_int g_raid5_mhm = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, mhm, CTLFLAG_RD, &g_raid5_mhm,0,
    "malloc hamster miss");
static u_int g_raid5_rrc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, rreq_cnt, CTLFLAG_RD, &g_raid5_rrc, 0,
    "read request count");
static u_int g_raid5_wrc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq_cnt, CTLFLAG_RD, &g_raid5_wrc, 0,
    "write request count");
static u_int g_raid5_w1rc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq1_cnt, CTLFLAG_RD, &g_raid5_w1rc, 0,
    "write request count (1-phase)");
static u_int g_raid5_w2rc = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wreq2_cnt, CTLFLAG_RD, &g_raid5_w2rc, 0,
    "write request count (2-phase)");
static u_int g_raid5_disks_ok = 50;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, dsk_ok, CTLFLAG_RD, &g_raid5_disks_ok,0,
    "repeat EIO'ed request?");
static u_int g_raid5_blked1 = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, blked1, CTLFLAG_RD, &g_raid5_blked1,0,
    "1. kind block count");
static u_int g_raid5_blked2 = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, blked2, CTLFLAG_RD, &g_raid5_blked2,0,
    "2. kind block count");
static u_int g_raid5_wqp = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wqp, CTLFLAG_RD, &g_raid5_wqp,0,
    "max. write queue length");
static u_int g_raid5_wqf = 0;
SYSCTL_UINT(_kern_geom_raid5, OID_AUTO, wqf, CTLFLAG_RD, &g_raid5_wqf,0,
    "forced write by read request");

static MALLOC_DEFINE(M_RAID5, "raid5_data", "GEOM_RAID5 Data");

static int g_raid5_destroy(struct g_raid5_softc *sc,
                           boolean_t force, boolean_t noyoyo);
static int g_raid5_destroy_geom(struct gctl_req *req, struct g_class *mp,
                                struct g_geom *gp);

static g_taste_t g_raid5_taste;
static g_ctl_req_t g_raid5_config;
static g_dumpconf_t g_raid5_dumpconf;

static eventhandler_tag g_raid5_post_sync = NULL;

static void g_raid5_init(struct g_class *mp);
static void g_raid5_fini(struct g_class *mp);

struct g_class g_raid5_class = {
	.name = G_RAID5_CLASS_NAME,
	.version = G_VERSION,
	.ctlreq = g_raid5_config,
	.taste = g_raid5_taste,
	.destroy_geom = g_raid5_destroy_geom,
	.init = g_raid5_init,
	.fini = g_raid5_fini
};

/*
 * Greatest Common Divisor.
 */
static __inline u_int
gcd(u_int a, u_int b)
{
	u_int c;

	while (b != 0) {
		c = a;
		a = b;
		b = (c % b);
	}
	return (a);
}

/*
 * Least Common Multiple.
 */
static __inline u_int
g_raid5_lcm(u_int a, u_int b)
{
	return ((a * b) / gcd(a, b));
}

static __inline int
g_raid5_bintime_cmp(struct bintime *a, struct bintime *b)
{
	if (a->sec == b->sec) {
		if (a->frac == b->frac)
			return 0;
		else if (a->frac > b->frac)
			return 1;
	} else if (a->sec > b->sec)
		return 1;
	return -1;
}

static __inline int64_t
g_raid5_bintime2micro(struct bintime *a)
{
	return (a->sec*1000000) + (((a->frac>>32)*1000000)>>32);
}

static __inline u_int
g_raid5_disk_good(struct g_raid5_softc *sc, int i)
{
	return sc->sc_disks[i] != NULL && sc->sc_disk_states[i] == 0;
}

static __inline u_int
g_raid5_allgood(struct g_raid5_softc *sc)
{
	for (int i=sc->sc_ndisks-1; i>=0; i--)
		if (!g_raid5_disk_good(sc, i))
			return 0;
	return 1;
}

static __inline u_int
g_raid5_data_good(struct g_raid5_softc *sc, int i, off_t end)
{
	if (!g_raid5_disk_good(sc, i))
		return 0;
	if (!g_raid5_allgood(sc))
		return 1;
	if (sc->newest == i && sc->verified >= 0 && end > sc->verified)
		return 0;
	return 1;
}

static __inline u_int
g_raid5_parity_good(struct g_raid5_softc *sc, int pno, off_t end)
{
	if (!g_raid5_disk_good(sc, pno))
		return 0;
	if (!g_raid5_allgood(sc))
		return 1;
	if (sc->newest != -1 && sc->newest != pno)
		return 1;
	if (sc->verified >= 0 && end > sc->verified)
		return 0;
	return 1;
}

/*
 * Return the number of valid disks.
 */
static __inline u_int
g_raid5_nvalid(struct g_raid5_softc *sc)
{
	int no = 0;
	for (int i = 0; i < sc->sc_ndisks; i++)
		if (g_raid5_disk_good(sc,i))
			no++;

	return no;
}

static __inline int
g_raid5_find_disk(struct g_raid5_softc * sc, struct g_consumer * cp)
{
	struct g_consumer **cpp = cp->private;
	if (cpp == NULL)
		return -1;
	struct g_consumer *rcp = *cpp;
	if (rcp == NULL)
		return -1;
	int dn = cpp - sc->sc_disks;
	MYKASSERT(dn >= 0 && dn < sc->sc_ndisks, ("dn out of range."));
	return dn;
}

static int
g_raid5_write_metadata(struct g_consumer **cpp, struct g_raid5_metadata *md,
                       struct bio *ur)
{
	off_t offset;
	int length;
	u_char *sector;
	int error = 0;
	struct g_consumer *cp = *cpp;

	g_topology_assert_not();

	length = cp->provider->sectorsize;
	MYKASSERT(length >= sizeof(*md), ("sector size too low (%d %d).",
	                                  length,(int)sizeof(*md)));
	offset = cp->provider->mediasize - length;

	sector = malloc(length, M_RAID5, M_WAITOK | M_ZERO);
	raid5_metadata_encode(md, sector);

	if (ur != NULL) {
		bzero(ur,sizeof(*ur));
		ur->bio_cmd = BIO_WRITE;
		ur->bio_done = NULL;
		ur->bio_offset = offset;
		ur->bio_length = length;
		ur->bio_data = sector;
		g_io_request(ur, cp);
		error = 0;
	} else {
		error = g_write_data(cp, offset, sector, length);
		free(sector, M_RAID5);
	}

	return error;
}

static int
g_raid5_read_metadata(struct g_consumer **cpp, struct g_raid5_metadata *md)
{
	struct g_consumer *cp = *cpp;
	struct g_provider *pp;
	u_char *buf;
	int error;

	g_topology_assert();

	error = g_access(cp, 1,0,0);
	if (error) {
		G_RAID5_DEBUG(2, "g_access error %d", error); 
		return error;
	}
	/* Make santiy checks */
	pp = cp->provider;
	if (pp->error != 0) {
		G_RAID5_DEBUG(2, "Provider have error %d", pp->error); 
		g_access(cp, -1,0,0);
		return pp->error;
        }
	if (pp->sectorsize == 0) {
		G_RAID5_DEBUG(2, "Sector size iz 0"); 
		g_access(cp, -1,0,0);
		return ENXIO;
	}
	MYKASSERT(pp->sectorsize >= sizeof(*md), ("sector size too low (%d %d).",
	                                          pp->sectorsize,(int)sizeof(*md)));

	g_topology_unlock();
	buf = g_read_data(cp, pp->mediasize - pp->sectorsize, pp->sectorsize,
	                  &error);
	g_topology_lock();
	if ((*cpp) != NULL)
		g_access(cp, -1,0,0);
	if (buf == NULL) {
		G_RAID5_DEBUG(2, "g_read_data error %d", error); 
		return (error);
	}

	/* Decode metadata. */
	raid5_metadata_decode(buf, md);
	g_free(buf);

	return 0;
}

static int
g_raid5_update_metadata(struct g_raid5_softc *sc, struct g_consumer ** cpp,
                        int state, int di_no, struct bio *ur)
{
	struct g_raid5_metadata md;
	struct g_consumer *cp = *cpp;

	if (cp == NULL || sc == NULL || sc->sc_provider == NULL)
		return EINVAL;

	g_topology_assert_not();

	bzero(&md,sizeof(md));

	if (state >= 0) {
		if (sc->no_hot && (state & G_RAID5_STATE_HOT))
			state &= ~G_RAID5_STATE_HOT;

		strlcpy(md.md_magic,G_RAID5_MAGIC,sizeof(md.md_magic));
		md.md_version = G_RAID5_VERSION;
		strlcpy(md.md_name,sc->sc_name,sizeof(md.md_name));
		if (sc->hardcoded)
			strlcpy(md.md_provider,cp->provider->name,sizeof(md.md_provider));
		else
			bzero(md.md_provider,sizeof(md.md_provider));
		md.md_id = sc->sc_id;
		md.md_no = di_no;
		md.md_all = sc->sc_ndisks;
		md.md_no_hot = sc->no_hot;
		md.md_provsize = cp->provider->mediasize;
		md.md_stripesize = sc->stripesize;

		if (sc->state&G_RAID5_STATE_SAFEOP)
			state |= G_RAID5_STATE_SAFEOP;
		if (sc->state&G_RAID5_STATE_COWOP)
			state |= G_RAID5_STATE_COWOP;
		if (sc->state&G_RAID5_STATE_VERIFY)
			state |= G_RAID5_STATE_VERIFY;
		md.md_state = state;
		if (state&G_RAID5_STATE_VERIFY) {
			md.md_verified = sc->verified;
			md.md_newest = sc->newest < 0 ? md.md_no : sc->newest;
		} else {
			md.md_verified = -1;
			md.md_newest = -1;
		}
	}

	G_RAID5_DEBUG(1, "%s: %s: update meta data: state%d",
	              sc->sc_name, cp->provider->name, md.md_state);
	return g_raid5_write_metadata(cpp, &md, ur);
}

static __inline void
g_raid5_wakeup(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->sleep_mtx);
	if (!sc->no_sleep)
		sc->no_sleep = 1;
	mtx_unlock(&sc->sleep_mtx);
	wakeup(&sc->worker);
}

static __inline void
g_raid5_sleep(struct g_raid5_softc *sc, int *wt)
{
	mtx_lock(&sc->sleep_mtx);

	if (wt != NULL) {
		if (*wt)
			(*wt) = 0;
		else if (!sc->no_sleep)
			sc->no_sleep = 1;
	}

	if (sc->no_sleep) {
		sc->no_sleep = 0;
		mtx_unlock(&sc->sleep_mtx);
	} else
		msleep(&sc->worker, &sc->sleep_mtx, PRIBIO | PDROP, "gr5ma", hz);
}

static __inline void
g_raid5_detach(struct g_consumer *cp)
{
	g_detach(cp);
	g_destroy_consumer(cp);
}
static void
g_raid5_detachE(void *arg, int flags __unused)
{
	g_topology_assert();
	g_raid5_detach( (struct g_consumer*) arg);
}

static __inline void
g_raid5_no_more_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	while (sc->no_more_open)
		msleep(&sc->no_more_open, &sc->open_mtx, PRIBIO, "gr5nm", hz);
	sc->no_more_open = 1;
	while (sc->open_count > 0)
		msleep(&sc->open_count, &sc->open_mtx, PRIBIO, "gr5mn", hz);
	MYKASSERT(sc->open_count == 0, ("open count must be zero here."));
	mtx_unlock(&sc->open_mtx);
}
static __inline void
g_raid5_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	while (sc->no_more_open)
		msleep(&sc->no_more_open, &sc->open_mtx, PRIBIO, "gr5op", hz);
	sc->open_count++;
	mtx_unlock(&sc->open_mtx);
}
static __inline int
g_raid5_try_open(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	int can_do = !sc->no_more_open;
	if (can_do)
		sc->open_count++;
	mtx_unlock(&sc->open_mtx);
	return can_do;
}
static __inline void
g_raid5_more_open(struct g_raid5_softc *sc)
{
	MYKASSERT(sc->open_count == 0, ("open count must be zero here."));
	MYKASSERT(sc->no_more_open, ("no more open must be activated."));
	sc->no_more_open = 0;
	wakeup(&sc->no_more_open);
}
static __inline void
g_raid5_unopen(struct g_raid5_softc *sc)
{
	mtx_lock(&sc->open_mtx);
	MYKASSERT(sc->open_count > 0, ("open count must be positive here."));
	sc->open_count--;
	wakeup(&sc->open_count);
	mtx_unlock(&sc->open_mtx);
}

static void
g_raid5_remove_disk(struct g_raid5_softc *sc, struct g_consumer ** cpp,
                    int clear_md, int noyoyo)
{
	struct g_consumer * cp;

	g_topology_assert();

	cp = *cpp;
	if (cp == NULL)
		return;

	g_topology_unlock();
	g_raid5_no_more_open(sc);
	g_topology_lock();

	g_raid5_disks_ok = 50;
	int dn = g_raid5_find_disk(sc, cp);
	MYKASSERT(dn >= 0, ("unknown disk"));
	sc->sc_disk_states[dn] = 0;
	if (sc->state & G_RAID5_STATE_VERIFY)
		G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: removed while %d is missing.",
		              sc->sc_name, cp->provider->name, dn, sc->newest);

	G_RAID5_DEBUG(0, "%s: %s(%d): disk removed.",
	              sc->sc_name, cp->provider->name, dn);

	*cpp = NULL;

	if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC) {
		g_topology_unlock();
		g_raid5_update_metadata(sc,&cp,clear_md?-1:G_RAID5_STATE_CALM,dn,NULL);
		g_topology_lock();
		if (clear_md)
			sc->state |= G_RAID5_STATE_VERIFY;
	}

	if (cp->acr || cp->acw || cp->ace)
		g_access(cp, -cp->acr, -cp->acw, -cp->ace);

	if (noyoyo)
		g_post_event(g_raid5_detachE, cp, M_WAITOK, NULL);
	else
		g_raid5_detach(cp);

	g_raid5_more_open(sc);
	g_raid5_wakeup(sc);
}

static void
g_raid5_orphan(struct g_consumer *cp)
{
	struct g_raid5_softc *sc;
	struct g_consumer **cpp;
	struct g_geom *gp;

	g_topology_assert();
	gp = cp->geom;
	sc = gp->softc;
	if (sc == NULL)
		return;

	cpp = cp->private;
	if ((*cpp) == NULL)	/* Possible? */
		return;
	g_raid5_remove_disk(sc, cpp, 0, 0);
}

static __inline void
g_raid5_free(struct g_raid5_softc *sc, struct bio *bp)
{
	MYKASSERT(bp->bio_length >= 0, ("length must be positive."));
	mtx_lock(&sc->msq_mtx);
	bioq_insert_tail(&sc->msq, bp);
	sc->msl++;
	sc->memuse += bp->bio_length;
	mtx_unlock(&sc->msq_mtx);
}

static __inline int
g_raid5_mem_mdl(void)
{ return sizeof(int) + sizeof(caddr_t); }

static __inline caddr_t
g_raid5_mem_gethead(struct bio *bp)
{ return ((caddr_t*)bp->bio_data)[-1]; }

static __inline void
g_raid5_mem_sethead(struct bio *bp, caddr_t na)
{ ((caddr_t*)bp->bio_data)[-1] = na; }

static __inline void
g_raid5_freex(struct g_raid5_softc *sc, struct bio *bp)
{
	MYKASSERT(bp->bio_length >= 0, ("length must be positive."));
	bp->bio_data = g_raid5_mem_gethead(bp);
	bp->bio_length = * (int*) bp->bio_data;
	g_raid5_free(sc, bp);
}

static __inline int
g_raid5_find_alloc(struct g_raid5_softc *sc,
                   struct bio **fit, struct bio **nxt, off_t len)
{
	struct bio *bp;
	TAILQ_FOREACH(bp, &sc->msq.queue, bio_queue) {
		if (bp->bio_length < len) {
			if ((*nxt) == NULL || (*nxt)->bio_length < bp->bio_length)
				(*nxt) = bp;
		} else if (bp->bio_length > len) {
			if ((*fit) == NULL || (*fit)->bio_length > bp->bio_length)
				(*fit) = bp;
		} else {
			(*fit) = bp;
			break;
		}
	}
	if ((*nxt) == NULL && (*fit) != NULL)
		(*nxt) = *fit;
	int found = (*fit) != NULL && (*fit)->bio_length <= len*4;
	if (found)
		g_raid5_mhh++;
	else
		g_raid5_mhm++;
	return found;
}

static struct bio *
g_raid5_malloc(struct g_raid5_softc *sc, off_t len, int force)
{
	struct bio *nxt = NULL;
	struct bio *fit = NULL;
	mtx_lock(&sc->msq_mtx);
	if (g_raid5_find_alloc(sc, &fit, &nxt, len)) {
		bioq_remove(&sc->msq, fit);
		sc->msl--;
		sc->memuse -= fit->bio_length;
		mtx_unlock(&sc->msq_mtx);

		char *tmp = fit->bio_data;
		bzero(fit, sizeof(*fit));
		fit->bio_data = tmp;
	} else {
		if (nxt == NULL) {
			mtx_unlock(&sc->msq_mtx);
			fit = force ? g_alloc_bio() : g_new_bio();
		} else {
			bioq_remove(&sc->msq, nxt);
			sc->msl--;
			sc->memuse -= nxt->bio_length;
			mtx_unlock(&sc->msq_mtx);

			free(nxt->bio_data, M_RAID5);
			fit = nxt;
			bzero(fit, sizeof(*fit));
		}
		if (fit != NULL) {
			fit->bio_data = malloc(len, M_RAID5, force ? M_WAITOK : M_NOWAIT);
			if (fit->bio_data == NULL) {
				g_destroy_bio(fit);
				fit = NULL;
			}
		}
	}
	return fit;
}

static struct bio *
g_raid5_mallocx(struct g_raid5_softc *sc, off_t off, int mile)
{
	int fsl = (sc->sc_ndisks-1) << sc->stripebits;
	int p = g_raid5_mem_mdl();
	int len = fsl*3 + MAXPHYS*2 + p;
	struct bio *bp = g_raid5_malloc(sc,len,0);
	if (bp == NULL) {
G_RAID5_DEBUG(1, "hitting kmem limit %d/%d.", len,mile+p);
		len = mile + p;
		bp = g_raid5_malloc(sc,len,1);
	} else
		p += MAXPHYS + fsl + off%fsl;
	((int*)bp->bio_data)[0] = len;
	char *d = bp->bio_data;
	bp->bio_data = d + p;
	g_raid5_mem_sethead(bp, d);
	return bp;
}

static void
g_raid5_combine(struct g_raid5_softc *sc,
                struct bio *bp, struct bio *wbp, off_t mend)
{
	caddr_t head = g_raid5_mem_gethead(bp);
	int len = * (int*) head;
	int off = ((char*)bp->bio_data) - (char*)head;
	off_t noff = MIN(bp->bio_offset, wbp->bio_offset);
	off_t nlen = mend - noff;
	int ok = 1;
	if (bp->bio_offset > wbp->bio_offset) {
		int doff = bp->bio_offset - wbp->bio_offset;
		off -= doff;
		if (len >= off + nlen && off >= g_raid5_mem_mdl()) {
			bp->bio_data = ((char*)bp->bio_data) - doff;
			g_raid5_mem_sethead(bp, head);
			bp->bio_offset = wbp->bio_offset;
		} else
			ok = 0;
	} else if (len < off + nlen)
		ok = 0;
	if (!ok) {
		off = g_raid5_mem_mdl();
		len = nlen + off;
		char *nd = malloc(len, M_RAID5, M_WAITOK);
		((int*)nd)[0] = len;
		char *od = bp->bio_data;
		bp->bio_data = nd + off;
		bcopy(od, bp->bio_data + (bp->bio_offset - noff), bp->bio_length);
		free(head, M_RAID5);
		head = nd;
		g_raid5_mem_sethead(bp, head);
		bp->bio_offset = noff;
		ok = 1;
		g_raid5_coca++;
	}
	bp->bio_length = nlen;
	bp->bio_t0.sec = wbp->bio_t0.sec;
	bcopy(wbp->bio_data, bp->bio_data+(wbp->bio_offset - noff), wbp->bio_length);
}

static int
g_raid5_stripe_conflict(struct g_raid5_softc *sc, struct bio *bbp)
{
	MYKASSERT(bbp->bio_parent == NULL, ("must not be issued."));
	MYKASSERT(bbp->bio_caller1 == NULL, ("must not be blocked."));

	if (bbp->bio_length == 0)
		return 0;
	off_t bbsno = bbp->bio_offset >> sc->stripebits;
	int blow = bbp->bio_offset & (sc->stripesize - 1);
	bbsno /= sc->sc_ndisks - 1;
	off_t besno = bbp->bio_offset + bbp->bio_length - 1;
	int bhih = besno & (sc->stripesize - 1);
	ORDER(blow,bhih);
	besno >>= sc->stripebits;
	besno /= sc->sc_ndisks - 1;

	struct bio *bp;
	TAILQ_FOREACH(bp, &sc->wq.queue, bio_queue) {
		if (bp == bbp)
			continue;
		if (bp->bio_parent == NULL)
			continue;

		if (bp->bio_length == 0)
			continue;
		off_t bsno = bp->bio_offset >> sc->stripebits;
		int low = bp->bio_offset & (sc->stripesize - 1);
		bsno /= sc->sc_ndisks - 1;
		off_t esno = bp->bio_offset + bp->bio_length - 1;
		int hih = esno & (sc->stripesize - 1);
		ORDER(low,hih);
		esno >>= sc->stripebits;
		esno /= sc->sc_ndisks - 1;

		if (besno >= bsno && esno >= bbsno && bhih >= low && hih >= blow)
			return 1;
	}

	return 0;
}

static __inline int
g_raid5_still_blocked(struct g_raid5_softc *sc, struct bio *bbp)
{
	MYKASSERT(bbp->bio_parent == NULL, ("must not be issued."));
	MYKASSERT(bbp->bio_caller1 != NULL, ("must be blocked."));

	struct bio *bp;
	TAILQ_FOREACH(bp, &sc->wq.queue, bio_queue) {
		if (bp == bbp)
			continue;

		off_t end = bp->bio_offset + bp->bio_length;
		off_t bend = bbp->bio_offset + bbp->bio_length;
		int overlapf = end > bbp->bio_offset && bend > bp->bio_offset;
		if (overlapf) {
			MYKASSERT(bp->bio_parent == (void*) sc,
			          ("combo error found with %p/%p", bp,bbp));
			return 1;
		}
	}

	return 0;
}

static int
g_raid5_check_full(struct g_raid5_softc *sc, struct bio *bp)
{
	if (bp->bio_parent == (void*) sc)
		return 0;
	MYKASSERT(bp->bio_parent == NULL, ("corrupted request"));

	int r = bp->bio_offset & (sc->stripesize - 1);
	off_t bsno = bp->bio_offset >> sc->stripebits;
	int m = bsno % (sc->sc_ndisks - 1);
	if (m == 0 && r > 0) {
		bsno += sc->sc_ndisks - 1;
		m++;
	} else if (m > 0)
		bsno += (sc->sc_ndisks - 1) - m;
	off_t mend = bp->bio_offset + bp->bio_length;
	off_t esno = mend >> sc->stripebits;
	if (esno - bsno < sc->sc_ndisks - 1)
		return 0; /* no 1-phase-write possible ==> wait */

	if (m > 0) {
		off_t clen = (bsno<<sc->stripebits) - bp->bio_offset;
		struct bio *cbp = g_raid5_mallocx(sc, bp->bio_offset, clen);
		char *nd = cbp->bio_data;
		bcopy(bp, cbp, sizeof(*cbp));
		cbp->bio_length = clen;
		cbp->bio_data = nd;
		bcopy(bp->bio_data, cbp->bio_data, cbp->bio_length);

		bp->bio_length -= cbp->bio_length;
		bp->bio_offset += cbp->bio_length;
		bcopy(bp->bio_data + cbp->bio_length, bp->bio_data, bp->bio_length);

		cbp->bio_parent = NULL;
		cbp->bio_caller2 = NULL;

		sc->wqp++;
		bioq_insert_tail(&sc->wq, cbp);
	}

	off_t coff = (esno - (esno % (sc->sc_ndisks - 1))) << sc->stripebits;
	off_t clen = mend - coff;
	if (clen > 0) {
		struct bio *cbp = g_raid5_mallocx(sc, coff, clen);
		char *nd = cbp->bio_data;
		bcopy(bp,cbp,sizeof(*cbp));
		cbp->bio_length = clen;
		cbp->bio_data = nd;

		bp->bio_length -= clen;

		cbp->bio_offset = coff;
		bcopy(bp->bio_data+bp->bio_length, cbp->bio_data, clen);

		cbp->bio_parent = NULL;
		cbp->bio_caller2 = NULL;

		sc->wqp++;
		bioq_insert_tail(&sc->wq, cbp);
	}

	return 1;
}

static void g_raid5_dispatch(struct g_raid5_softc *sc, struct bio *bp);
static void
g_raid5_issue(struct g_raid5_softc *sc, struct bio *bp)
{
	bp->bio_done = NULL;
	bp->bio_children = 0;
	bp->bio_inbed = 0;
	MYKASSERT(bp->bio_caller1 == NULL, ("still dependent."));
	MYKASSERT(bp->bio_caller2 == NULL, ("still error."));
	bp->bio_driver1 = NULL;
	g_raid5_dispatch(sc, bp);
}

static int
g_raid5_issue_check(struct g_raid5_softc *sc, struct bio *bp, int *blked)
{
	/* pending/issued/blocked write request found */
	if (!*blked)
		(*blked) = 1;

	/* already issued? */
	if (bp->bio_parent == (void*) sc)
		return 0;
	MYKASSERT(bp->bio_parent == NULL, ("wrong parent."));

	/* blocked by already issued? */
	if (bp->bio_caller1 != NULL) {
		if (g_raid5_still_blocked(sc, bp))
			return 0;
		bp->bio_caller1 = NULL;
	}

	off_t bend = bp->bio_offset + bp->bio_length;
	if (bp->bio_offset >> sc->stripebits < (bend-1) >> sc->stripebits) {
		off_t soff = bp->bio_offset & (sc->stripesize - 1);
		off_t loff = bp->bio_length & (sc->stripesize - 1);
		if (soff > 0 || loff > 0 ||
		    (bp->bio_offset >> sc->stripebits) % (sc->sc_ndisks - 1) != 0 ||
		    (bp->bio_length >> sc->stripebits) % (sc->sc_ndisks - 1) != 0) {
			int len = sc->stripesize - soff;
			off_t noff = bp->bio_offset + len;
			off_t nlen = bp->bio_length - len;
			bp->bio_length = len;

			struct bio *cbp = g_raid5_mallocx(sc, noff, nlen);
			char *nd = cbp->bio_data;
			bcopy(bp, cbp, sizeof(*cbp));
			bcopy(bp->bio_data + len, nd, nlen);
			cbp->bio_data = nd;
			cbp->bio_offset = noff;
			cbp->bio_length = nlen;
			bioq_insert_tail(&sc->wq, cbp);
			sc->wqp++;
		}
	}

	/* possible 2-phase stripe conflict? */
	if (g_raid5_stripe_conflict(sc, bp)) {
		if (bp->bio_driver2 == NULL) {
			g_raid5_blked2++;
			bp->bio_driver2 = bp;
		}
		return 0;
	} else if (bp->bio_driver2 != NULL)
		bp->bio_driver2 = NULL;

	bp->bio_parent = (void*) sc;

	g_raid5_issue(sc,bp);

	sc->wqpi++;
	return 1;
}

static __inline void
g_raid5_wqp_dec(struct g_raid5_softc *sc, int prot)
{
	if (prot)
		mtx_lock(&sc->dq_mtx);
	sc->wqp--;
	if (prot)
		mtx_unlock(&sc->dq_mtx);
}
static __inline void
g_raid5_wqp_inc(struct g_raid5_softc *sc, int prot)
{
	if (prot)
		mtx_lock(&sc->dq_mtx);
	sc->wqp++;
	if (prot)
		mtx_unlock(&sc->dq_mtx);
}

static int
g_raid5_write(struct g_raid5_softc *sc, struct bio *wbp, int flush,
              struct bintime *now)
{
	if (sc->sc_ndisks == 2) {
		if (wbp != NULL && wbp->bio_cmd == BIO_WRITE)
			g_raid5_dispatch(sc, wbp);
		return 0;
	}
	MYKASSERT(wbp == NULL || wbp->bio_parent != (void*) sc ||
	          wbp->bio_cmd != BIO_WRITE, ("no way!"));

	off_t wend;
	if (wbp == NULL) {
		wend = -1ULL;
		if (g_raid5_wdt < 0)
			g_raid5_wdt = 0;
		else if (g_raid5_wdt > 50)
			g_raid5_wdt = 50;
	} else
		wend = wbp->bio_offset + wbp->bio_length;

	struct bio *combo = NULL;
	struct bio *issued = NULL;
	int blked = 0;
	int emptied = sc->wqpi;

	struct bio *bp, *bp2;
	TAILQ_FOREACH_SAFE(bp, &sc->wq.queue, bio_queue, bp2) {
		if (bp->bio_parent == (void*) sc && bp->bio_driver1 == bp) { /* done? */
			bioq_remove(&sc->wq, bp);
			g_raid5_wqp_dec(sc,0);
			sc->wqpi--;
			g_raid5_freex(sc,bp);
			continue;
		}

		if (bp->bio_parent == (void*)sc && bp->bio_caller2 != NULL) { /* retry? */
			MYKASSERT(bp->bio_caller1 == NULL, ("impossible."));
			bp->bio_caller2 = NULL;
			if (bp->bio_error == ENOMEM) {
				bp->bio_error = 0;
				bp->bio_parent = NULL;
				sc->wqpi--;
				g_raid5_issue_check(sc,bp,&blked);
			} else {
				G_RAID5_DEBUG(0,"%s: %p: aborted.",sc->sc_name,bp);
				bp->bio_driver1 = bp;
				g_error_provider(sc->sc_provider, bp->bio_error);
			}
		}

		if (wbp == NULL) {
			int st = !flush && sc->wqp - emptied <= g_raid5_maxwql / 4 &&
						now->sec - bp->bio_t0.sec <= g_raid5_wdt &&
			         !g_raid5_check_full(sc, bp); /* still time? */
			if (!st) {
				if (g_raid5_issue_check(sc,bp,&blked))
					emptied++;
			}
			continue;
		}

		off_t end = bp->bio_offset + bp->bio_length;
		int overlapf = end > wbp->bio_offset && wend > bp->bio_offset;
		int overlap = overlapf ||
		              end == wbp->bio_offset || wend == bp->bio_offset;

		if (wbp->bio_cmd == BIO_READ) {
			/* chk for write req that might interfere with this read req? */
			if (overlapf) { /* bp is independent? */
				g_raid5_check_full(sc, bp);
				g_raid5_issue_check(sc,bp,&blked);
				g_raid5_wqf++;
			}
			continue;
		}

		/* if bp has been already issued,
		     then no combo but look for another bp that matches and mark wbp */
		if (bp->bio_parent == (void*) sc) {
			if (overlapf)
				issued = bp;
			continue;
		}
		MYKASSERT(bp->bio_parent == NULL, ("parent must be NULL (no issue)."));
		MYKASSERT(bp->bio_caller2 == NULL, ("caller2 must be NULL (no error)."));

		if (!overlap)
			continue;

		if (combo == NULL) {
			g_raid5_combine(sc, bp, wbp, MAX(end, wend));
			combo = bp;
		} else { /* combine bp with combo and remove bp */
			MYKASSERT(combo->bio_parent == NULL, ("oops something started?"));

			off_t mend = MAX(end, combo->bio_offset+combo->bio_length);
			g_raid5_combine(sc, bp, combo, mend);

			bioq_remove(&sc->wq, bp);
			sc->wqp--;
			char *tmpD = combo->bio_data;
			int tmpL = combo->bio_length;
			combo->bio_data = bp->bio_data;
			combo->bio_length = bp->bio_length;
			combo->bio_offset = bp->bio_offset;
			if (!combo->bio_caller1 && bp->bio_caller1)
				combo->bio_caller1 = bp->bio_caller1;
			bp->bio_data = tmpD;
			bp->bio_length = tmpL;
			g_raid5_freex(sc, bp);
		}
	}

	if (wbp != NULL && wbp->bio_cmd == BIO_WRITE) {
		if (combo == NULL) {
			struct bio *cbp = g_raid5_mallocx(sc, wbp->bio_offset,wbp->bio_length);
			char *nd = cbp->bio_data;
			bcopy(wbp, cbp, sizeof(*cbp));
			cbp->bio_data = nd;
			bcopy(wbp->bio_data, cbp->bio_data, cbp->bio_length);
			cbp->bio_parent = NULL;
			cbp->bio_caller1 = issued;
			if (issued)
				g_raid5_blked1++;
			cbp->bio_caller2 = NULL;
			cbp->bio_driver2 = NULL;
			bioq_insert_tail(&sc->wq, cbp);
			g_raid5_wqp_inc(sc,0);
		} else {
			bioq_remove(&sc->wq, combo);
			bioq_insert_tail(&sc->wq, combo);
			if (g_raid5_check_full(sc, combo))
				g_raid5_issue_check(sc, combo, &blked);
		}
		bioq_insert_tail(&sc->wdq, wbp);
	}

	if (sc->wqp > g_raid5_wqp)
		g_raid5_wqp = sc->wqp;

	if (wbp != NULL && wbp->bio_cmd == BIO_READ)
		return blked;
	return 0;
}

static int
g_raid5_access(struct g_provider *pp, int dr, int dw, int de)
{
	struct g_consumer *cp1, *cp2;
	struct g_raid5_softc *sc;
	struct g_geom *gp;
	int error;

	gp = pp->geom;
	sc = gp->softc;

	g_topology_assert();

	if (sc == NULL) {
		/*
		 * It looks like geom is being withered.
		 * In that case we allow only negative requests.
		 */
		MYKASSERT(dr <= 0 && dw <= 0 && de <= 0,
		    ("Positive access request (device=%s).", pp->name));
		if ((pp->acr + dr) == 0 && (pp->acw + dw) == 0 &&
		    (pp->ace + de) == 0) {
			G_RAID5_DEBUG(0, "%s: device definitely destroyed.", gp->name);
		}
		return 0;
	}

	error = ENXIO;
	LIST_FOREACH(cp1, &gp->consumer, consumer) {
		if (g_raid5_find_disk(sc, cp1) >= 0) {
			error = g_access(cp1, dr, dw, de);
			if (error)
				break;
		}
	}
	if (error) {
		/* if we fail here, backout all previous changes. */
		LIST_FOREACH(cp2, &gp->consumer, consumer) {
			if (cp1 == cp2)
				break;
			if (g_raid5_find_disk(sc, cp2) >= 0)
				g_access(cp2, -dr, -dw, -de);
		}
	} else {
		LIST_FOREACH(cp1, &gp->consumer, consumer) {
			if (g_raid5_find_disk(sc, cp1) < 0)
				continue;
			if (cp1->acr < 0) g_access(cp1, -cp1->acr,0,0);
			if (cp1->acw < 0) g_access(cp1, 0,-cp1->acw,0);
			if (cp1->ace < 0) g_access(cp1, 0,0,-cp1->ace);
		}
		if (sc->destroy_on_za &&
		    pp->acr+dr == 0 && pp->acw+dw == 0 && pp->ace+de == 0) {
			sc->destroy_on_za = 0;
			g_raid5_destroy(sc,1,0);
		}
	}

	return error;
}

static __inline void
g_raid5_xor(caddr_t a, caddr_t b, int len)
{
	u_register_t *aa = (u_register_t*) a;
	u_register_t *bb = (u_register_t*) b;
	MYKASSERT(len % sizeof(*aa) == 0, ("len has wrong modul."));
	len /= sizeof(*aa);
	while (len > 0) {
		(*aa) ^= (*bb);
		aa++;
		bb++;
		len--;
	}
}

static __inline int
g_raid5_neq(caddr_t a, caddr_t b, int len)
{
	u_register_t *aa = (u_register_t*) a;
	u_register_t *bb = (u_register_t*) b;
	MYKASSERT(len % sizeof(*aa) == 0, ("len has wrong modul."));
	len /= sizeof(*aa);
	while (len > 0) {
		if ((*aa) != (*bb))
			return 1;
		aa++;
		bb++;
		len--;
	}
	return 0;
}

static __inline void
g_raid5_io_req(struct g_raid5_softc *sc, struct bio *cbp)
{
	struct g_consumer *disk = cbp->bio_driver2;
	MYKASSERT(disk != NULL, ("disk must be present"));
	cbp->bio_driver2 = NULL;
	if (g_raid5_find_disk(sc, disk) >= 0) {
		if (cbp->bio_children) {
			cbp->bio_children = 0;
			cbp->bio_inbed = 0;
		}
		g_io_request(cbp, disk);
	} else {
		cbp->bio_error = EIO;
		g_io_deliver(cbp, cbp->bio_error);
	}
}

static __inline int
g_raid5_extra_mem(struct bio *a, caddr_t b)
{
	if (a->bio_data == NULL)
		return 1;
	register_t delta = ((uint8_t*)b) - ((uint8_t*)a->bio_data);
	return delta < 0 || delta >= a->bio_length;
}

static void
g_raid5_ready(struct g_raid5_softc *sc, struct bio *bp)
{
	MYKASSERT(bp != NULL, ("BIO must not be zero here."));

	struct bio *pbp = bp->bio_parent;
	MYKASSERT(pbp != NULL, ("pbp must not be NULL."));

	struct bio *obp;
	if (pbp->bio_offset < 0 && pbp->bio_parent != NULL)
		obp = pbp->bio_parent;
	else
		obp = pbp;

	/*XXX: inefficient in case of _cmp and _xor (one thread for each request would be better)*/

	pbp->bio_inbed++;
	MYKASSERT(pbp->bio_inbed<=pbp->bio_children, ("more inbed than children."));

	if (bp->bio_error) {
		if (!pbp->bio_error) {
			pbp->bio_error = bp->bio_error;
			if (!obp->bio_error)
				obp->bio_error = pbp->bio_error;
		}
		if (obp->bio_error == EIO || obp->bio_error == ENXIO) {
			struct g_consumer *cp = bp->bio_from;
			int dn = g_raid5_find_disk(sc, cp);
			if (g_raid5_disks_ok < 40) {
				for (int i=0; i<sc->sc_ndisks; i++)
					sc->sc_disk_states[i] = i==dn ? 1 : 0;
				G_RAID5_DEBUG(0,"%s: %s(%d): pre-remove disk due to errors.",
				              sc->sc_name, cp->provider->name,dn);
			} else if (g_raid5_disks_ok > 0 && sc->sc_disk_states[dn] == 0)
				g_raid5_disks_ok--;
		}
		G_RAID5_DEBUG(0,"%s: %p: cmd%c off%jd len%jd err:%d/%d c%d",
		              sc->sc_name, obp, obp->bio_cmd==BIO_READ?'R':'W',
		              obp->bio_offset, obp->bio_length,
		              bp->bio_error,obp->bio_error,g_raid5_disks_ok);
	}

	int saved = 0;
	int extra = g_raid5_extra_mem(obp,bp->bio_data);
	if (bp->bio_cmd == BIO_READ) {
		if (obp == pbp) {
			/* best case read */
			MYKASSERT(obp->bio_cmd == BIO_READ, ("need BIO_READ here."));
			MYKASSERT(!extra, ("wrong memory area."));
			obp->bio_completed += bp->bio_completed;
		} else if (obp->bio_cmd == BIO_READ &&
		           pbp->bio_children == sc->sc_ndisks) {
			/* verify read */
			MYKASSERT(pbp->bio_cmd == BIO_WRITE, ("unkown conf."));
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1,
					  ("offsets must correspond"));
			MYKASSERT(bp->bio_length*2 == pbp->bio_length,
					  ("lengths must correspond"));
			if (pbp->bio_data+bp->bio_length != bp->bio_data) {
				/* not the stripe in question */
				g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
				if (extra)
					saved = 1;
			}
			if (pbp->bio_inbed == pbp->bio_children) {
				g_raid5_vsc++;
				if (pbp->bio_driver1 != NULL) {
					MYKASSERT(!g_raid5_extra_mem(obp,pbp->bio_driver1),("bad addr"));
					bcopy(pbp->bio_data,pbp->bio_driver1,bp->bio_length);
					pbp->bio_driver1 = NULL;
				}
				if (obp->bio_error ||
				    !g_raid5_neq(pbp->bio_data,
				                 pbp->bio_data+bp->bio_length,bp->bio_length)) {
					pbp->bio_offset = -pbp->bio_offset-1;
					obp->bio_completed += bp->bio_length;
					obp->bio_inbed++;
					MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
					g_raid5_free(sc, pbp);
				} else if (sc->state&G_RAID5_STATE_VERIFY) {
					/* corrective write */
					MYKASSERT(pbp->bio_caller1 == NULL, ("no counting"));
					g_raid5_vwc++;
					pbp->bio_offset = -pbp->bio_offset-1;
					MYKASSERT(bp->bio_offset == pbp->bio_offset,
							  ("offsets must correspond"));
					pbp->bio_length /= 2;
					pbp->bio_children = 0;
					pbp->bio_inbed = 0;
					pbp->bio_caller1 = obp;
					g_raid5_io_req(sc, pbp);
				} else {
					if (!obp->bio_error)
						obp->bio_error = EIO;
					obp->bio_inbed++;
					MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
					int pos;
					for (pos=0; pos<bp->bio_length; pos++)
						if (((char*)pbp->bio_data)[pos] !=
						    ((char*)pbp->bio_data)[pos+bp->bio_length])
							break;
					G_RAID5_DEBUG(0,"%s: %p: parity mismatch: %jd+%jd@%d.",
					              sc->sc_name,obp,bp->bio_offset,bp->bio_length,pos);
					g_raid5_free(sc, pbp);
				}
			}
		} else if (obp->bio_cmd == BIO_WRITE &&
		           pbp->bio_children == sc->sc_ndisks-2 &&
		           g_raid5_extra_mem(obp,pbp->bio_data)) {
			/* preparative read for degraded case write */
			MYKASSERT(extra, ("wrong memory area."));
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1,
			          ("offsets must correspond"));
			MYKASSERT(bp->bio_length == pbp->bio_length,
			          ("length must correspond"));
			g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				pbp->bio_offset = -pbp->bio_offset-1;
				MYKASSERT(g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
				if (pbp->bio_error) {
					obp->bio_inbed++;
					g_raid5_free(sc, pbp);
				} else
					g_raid5_io_req(sc,pbp);
			}
		} else if ( obp->bio_cmd == BIO_WRITE &&
		            (pbp->bio_children == 2 ||
		             (sc->sc_ndisks == 3 && pbp->bio_children == 1)) ) {
			/* preparative read for best case 2-phase write */
			MYKASSERT(extra, ("wrong memory area."));
			MYKASSERT(bp->bio_offset == -pbp->bio_offset-1,
					  ("offsets must correspond"));
			MYKASSERT(bp->bio_length == pbp->bio_length,
					  ("length must correspond"));
			struct bio *pab = pbp->bio_caller2;
			g_raid5_xor(pab->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				pbp->bio_offset = -pbp->bio_offset-1;
				pab->bio_offset = -pab->bio_offset-1;
				MYKASSERT(pab->bio_length == pbp->bio_length,
				        ("lengths must correspond"));
				MYKASSERT(pbp->bio_offset == pab->bio_offset,
				        ("offsets must correspond"));
				MYKASSERT(pbp->bio_driver2 != pab->bio_driver2,
				        ("disks must be different"));
				MYKASSERT(g_raid5_extra_mem(obp,pab->bio_data), ("bad addr"));
				MYKASSERT(!g_raid5_extra_mem(obp,pbp->bio_data), ("bad addr"));
				if (pbp->bio_error) {
					obp->bio_inbed += 2;
					g_raid5_free(sc, pab);
					g_destroy_bio(pbp);
				} else {
					g_raid5_io_req(sc, pab);
					g_raid5_io_req(sc, pbp);
				}
			}
		} else {
			/* read degraded stripe */
			MYKASSERT(obp->bio_cmd == BIO_READ, ("need BIO_READ here."));
			MYKASSERT(pbp->bio_children == sc->sc_ndisks-1,
			        ("must have %d children here.", sc->sc_ndisks-1));
			MYKASSERT(extra, ("wrong memory area."));
			MYKASSERT(bp->bio_length==pbp->bio_length,("length must correspond."));
			g_raid5_xor(pbp->bio_data,bp->bio_data,bp->bio_length);
			saved = 1;
			if (pbp->bio_inbed == pbp->bio_children) {
				obp->bio_completed += bp->bio_completed;
				obp->bio_inbed++;
				g_destroy_bio(pbp);
			}
		}
	} else {
		MYKASSERT(bp->bio_cmd == BIO_WRITE, ("bio_cmd must be BIO_WRITE here."));
		MYKASSERT(obp == pbp, ("invalid write request configuration"));
		if (extra)
			saved = 1;
		if (bp->bio_caller1 == obp)
			obp->bio_completed += bp->bio_completed;
	}

	if (saved)
		g_raid5_free(sc, bp);
	else
		g_destroy_bio(bp);

	MYKASSERT(obp->bio_inbed<=obp->bio_children, ("more inbed than children."));
	if (obp->bio_inbed == obp->bio_children) {
		if (obp->bio_driver2 != NULL) {
			MYKASSERT(sc->worker == NULL || obp->bio_driver2 == sc->worker,
			          ("driver2 is misused."));
			/* corrective verify read requested by worker() */
			obp->bio_driver2 = NULL;
		} else {
			if (g_raid5_disks_ok > 0 &&
			    (obp->bio_error == EIO || obp->bio_error == ENXIO)) {
				obp->bio_error = ENOMEM;
				obp->bio_completed = 0;
				obp->bio_children = 0;
				obp->bio_inbed = 0;
			} else if (obp->bio_error == 0 && g_raid5_disks_ok < 30)
				g_raid5_disks_ok = 50;
			if (obp->bio_parent == (void*) sc && obp->bio_cmd == BIO_WRITE) {
				if (obp->bio_error == ENOMEM)
					obp->bio_caller2 = obp; /* retry! */
				else {
					if (obp->bio_error) {
						G_RAID5_DEBUG(0,"%s: %p: lost data: off%jd len%jd error%d.",
										  sc->sc_name,obp,
						              obp->bio_offset,obp->bio_length,obp->bio_error);
						g_error_provider(sc->sc_provider, obp->bio_error);
							/* cancels all pending write requests */
					}
					obp->bio_driver1 = obp;
					wakeup(&sc->wqp);
				}
			} else {
				if (obp->bio_cmd == BIO_WRITE) {
					MYKASSERT(sc->sc_ndisks==2,
					          ("incompetent for BIO_WRITE %jd/%jd %d %p/%p.",
					           obp->bio_length,obp->bio_offset,
					           sc->sc_ndisks,obp->bio_parent,sc));
					g_raid5_wqp_dec(sc,1);
				}
				g_io_deliver(obp, obp->bio_error);
			}
		}
		g_raid5_wakeup(sc);

		g_raid5_unopen(sc);
	}
}

static void
g_raid5_done(struct bio *bp)
{
	struct g_raid5_softc *sc = bp->bio_from->geom->softc;
	MYKASSERT(sc != NULL, ("SC must not be zero here."));
	G_RAID5_LOGREQ(bp, "[done err:%d dat:%02x adr:%p]",
	               bp->bio_error,*(unsigned char*)bp->bio_data,bp->bio_data);

	if (sc->workerD == NULL)
		g_raid5_ready(sc, bp);
	else {
		mtx_lock(&sc->dq_mtx);
		bioq_insert_tail(&sc->dq, bp);
		wakeup(&sc->workerD);
		mtx_unlock(&sc->dq_mtx);
	}
}

static struct bio*
g_raid5_prep(struct g_raid5_softc *sc,
             struct bio_queue_head *queue, struct bio* bp, uint8_t cmd,
             off_t offset, int len, u_char *data, struct g_consumer *disk)
{
	MYKASSERT(disk!=NULL || (offset<0 && cmd==BIO_READ),
	          ("disk shall be not NULL here."));
	struct bio *cbp = data == NULL ? g_raid5_malloc(sc,len,0) : g_new_bio();
	if (cbp == NULL)
		return NULL;
	bioq_insert_tail(queue, cbp);

	bp->bio_children++;

	cbp->bio_parent = bp;
	cbp->bio_cmd = cmd;
	cbp->bio_length = len;
	cbp->bio_offset = offset;
	cbp->bio_attribute = bp->bio_attribute;
	cbp->bio_done = g_raid5_done;
	cbp->bio_driver2 = disk;
	if (data != NULL)
		cbp->bio_data = data;

	return cbp;
}

static int
g_raid5_conv_veri_read(struct bio_queue_head *queue, struct g_raid5_softc *sc,
                       struct bio *obp, off_t loff, int len, int dno, int pno,
                       caddr_t data)
{
	struct bio * root;
	if (g_raid5_nvalid(sc) < sc->sc_ndisks)
		return EIO;
	/* READ ALL parts and build XOR and cmp with zero and save data */
	int qno;
	if (data == NULL)
		qno = sc->newest >= 0 ? sc->newest : pno;
	else
		qno = dno;
	if (!g_raid5_disk_good(sc,qno))
		return EIO;
	root= g_raid5_prep(sc,queue,obp,BIO_WRITE,-loff-1,len*2,0,sc->sc_disks[qno]);
	if (root == NULL)
		return ENOMEM;
	bzero(root->bio_data, len);
	root->bio_driver1 = data;
	for (int i=0; i<sc->sc_ndisks; i++) {
		u_char *d = i == qno ? root->bio_data + len : NULL;
		struct bio *son;
		if (!g_raid5_disk_good(sc,i))
			return EIO;
		son = g_raid5_prep(sc,queue,root,BIO_READ,loff,len,d,sc->sc_disks[i]);
		if (son == NULL)
			return ENOMEM;
	}

	return 0;
}

static void
g_raid5_req_sq(struct g_raid5_softc *sc, struct bio_queue_head *queue)
{
	struct bio *prv = 0;
	for (;;) {
		struct bio *cbp = bioq_first(queue);
		if (cbp == NULL)
			break;
		bioq_remove(queue, cbp);

		cbp->bio_caller2 = prv;
		prv = cbp;

		if (cbp->bio_offset < 0)
			continue;

		if (cbp->bio_cmd == BIO_WRITE)
			cbp->bio_caller2 = NULL;
		g_raid5_io_req(sc, cbp);
	}
}

static void
g_raid5_des_sq(struct g_raid5_softc *sc,
               struct bio_queue_head *queue, struct bio *bp)
{
	for (;;) {
		struct bio *cbp = bioq_first(queue);
		if (cbp == NULL)
			break;
		bioq_remove(queue, cbp);

		if (cbp->bio_data != NULL &&
		    g_raid5_extra_mem(cbp->bio_parent,cbp->bio_data) &&
		    g_raid5_extra_mem(bp,cbp->bio_data))
			g_raid5_free(sc, cbp);
		else
			g_destroy_bio(cbp);
	}
}

static int
g_raid5_order_full(struct g_raid5_softc *sc, struct bio_queue_head *queue,
                   struct bio *bp, off_t sno, char *data)
{
	off_t loff = (sno / (sc->sc_ndisks-1)) << sc->stripebits;
	int pno = (sno / (sc->sc_ndisks-1)) % sc->sc_ndisks;
	int failed = 0;
	struct bio *pari;
	if (g_raid5_disk_good(sc,pno)) {
		pari = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                    loff,sc->stripesize,0,sc->sc_disks[pno]);
		if (pari == NULL)
			return ENOMEM;
		pari->bio_caller1 = pari;
		bzero(pari->bio_data, pari->bio_length);
	} else {
		pari = NULL;
		failed++;
	}
	for (int i=0; i<sc->sc_ndisks; i++) {
		if (i == pno)
			continue;
		if (pari != NULL)
			g_raid5_xor(pari->bio_data, data, pari->bio_length);
		if (!g_raid5_disk_good(sc,i)) {
			failed++;
			if (failed > 1)
				return EIO;
			data += sc->stripesize;
			continue;
		}
		struct bio *cbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                               loff,sc->stripesize,data,sc->sc_disks[i]);
		if (cbp == NULL)
			return ENOMEM;
		cbp->bio_caller1 = bp;
		data += sc->stripesize;
	}
	if (failed > 0) {
		if (sc->verified < 0 || sc->verified > loff)
			sc->verified = loff;
	}
	g_raid5_w1rc++;
	return 0;
}

static int
g_raid5_order(struct g_raid5_softc *sc, struct bio_queue_head *queue,
              struct bio *bp, off_t sno, int dno, int soff, int len, char *data)
{
	off_t loff = ( (sno/(sc->sc_ndisks-1)) << sc->stripebits ) + soff;
	off_t lend = loff + len;
	int dead = 0;
	int pno = (sno / (sc->sc_ndisks-1)) % sc->sc_ndisks;
	if (dno >= pno)
		dno++;
	MYKASSERT(dno < sc->sc_ndisks, ("dno: out of range (%d,%d)",pno,dno));
	if (lend > sc->disksize)
		return EIO;

	int datdead = g_raid5_data_good(sc, dno, lend) ? 0 : 1;
	int pardead = g_raid5_parity_good(sc, pno, lend) ? 0 : 1;
	dead = pardead + datdead;
	if (dead != 0) {
		if (dead == 2 || (sc->state&G_RAID5_STATE_SAFEOP))
			return EIO; /* RAID is too badly degraded */
	}

	if (bp->bio_cmd == BIO_READ) {
		if (sc->state&G_RAID5_STATE_SAFEOP)
			return g_raid5_conv_veri_read(queue,sc,bp,loff,len,dno,pno,data);
		if (datdead) {
			int i;
			struct bio*root= g_raid5_prep(sc,queue,bp,BIO_READ,-loff-1,len,data,0);
			if (root == NULL)
				return ENOMEM;
			for (i=0; i < sc->sc_ndisks; i++) {
				if (i == dno)
					continue;
				if (!g_raid5_disk_good(sc,i))
					return EIO;
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[i]) == NULL)
					return ENOMEM;
			}
			bzero(data,len);
			return 0;
		}
		if (sc->sc_ndisks == 2 && !pardead) {
			if (sc->lstdno) {
				sc->lstdno = 0;
				dno = pno;
			} else
				sc->lstdno = 1;
		}
		if (g_raid5_prep(sc,queue,bp,BIO_READ,
		                 loff,len,data,sc->sc_disks[dno]) == NULL)
			return ENOMEM;
	} else {
		MYKASSERT(bp->bio_cmd == BIO_WRITE, ("bio_cmd WRITE expected"));

		if (dead != 0) {
			if (sc->state&G_RAID5_STATE_COWOP)
				return EPERM;
			if (sc->verified < 0 || sc->verified > loff) {
				sc->verified = loff - soff;
				g_raid5_wakeup(sc);
			}
		}

		if (pardead) { /* parity disk dead-dead */
			struct bio *cbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                               loff,len,data,sc->sc_disks[dno]);
			if (cbp == NULL)
				return ENOMEM;
			cbp->bio_caller1 = bp;
			g_raid5_w1rc++;
			return 0;
		}

		if (sc->sc_ndisks == 2) {
			struct bio *pbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,loff,
			                               len,data,sc->sc_disks[pno]);
			if (pbp == NULL)
				return ENOMEM;
			pbp->bio_caller1 = bp;

			if (g_raid5_disk_good(sc, dno)) {
				struct bio *dbp;
				dbp = g_raid5_prep(sc,queue,bp,BIO_WRITE,loff,
				                   len,data,sc->sc_disks[dno]);
				if (dbp == NULL)
					return ENOMEM;
				dbp->bio_caller1 = dbp;
			}

			g_raid5_w1rc++;
			return 0;
		}

		if (!datdead) {
			struct bio * root;
			struct bio * pari;

			pari = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                    -loff-1,len,0,sc->sc_disks[pno]);
			if (pari == NULL)
				return ENOMEM;
			root = g_raid5_prep(sc,queue,bp,BIO_WRITE,
			                    -loff-1,len,data,sc->sc_disks[dno]);
			if (root == NULL)
				return ENOMEM;
			if (sc->sc_ndisks == 3 &&
			    g_raid5_data_good(sc, 3-dno-pno, lend)) {
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[3-dno-pno]) == NULL)
					return ENOMEM;
			} else {
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[dno]) == NULL)
					return ENOMEM;
				if (g_raid5_prep(sc,queue,root,BIO_READ,
				                 loff,len,0,sc->sc_disks[pno]) == NULL)
					return ENOMEM;
			}
			bcopy(data, pari->bio_data, len);
			root->bio_caller1 = bp;
			g_raid5_w2rc++;
			return 0;
		}

		MYKASSERT(datdead, ("no valid data disk permissible here"));
		MYKASSERT(!pardead, ("need parity disk."));

		/* read all but dno and pno*/
		int i;
		struct bio *root = g_raid5_prep(sc,queue,bp,BIO_WRITE,
		                                -loff-1,len,0,sc->sc_disks[pno]);
		if (root == NULL)
			return ENOMEM;
		for (i=0; i < sc->sc_ndisks; i++) {
			if (i == dno || i == pno)
				continue;
			if (!g_raid5_data_good(sc, i, lend))
				return EIO;
			if (g_raid5_prep(sc,queue,root,BIO_READ,
			                 loff,len,0,sc->sc_disks[i]) == NULL)
				return ENOMEM;
		}
		bcopy(data, root->bio_data, len);
		root->bio_caller1 = bp;
		g_raid5_w2rc++;
		return 0;
	}
	return 0;
}

static void
g_raid5_flush_wq(struct g_raid5_softc *sc)
{
	while (sc->wqp > 0) {
		g_raid5_write(sc, NULL, 1, NULL);
		tsleep(&sc->wqp, PRIBIO, "gr5fl", hz);
	}
}

static void
g_raid5_flush(struct g_raid5_softc *sc, struct bio* bp)
{
G_RAID5_DEBUG(0,"g_raid5_flush() begin %d.", __LINE__);
	g_raid5_flush_wq(sc);

	g_raid5_open(sc);

G_RAID5_DEBUG(0,"g_raid5_flush() middle %d.", __LINE__);
	struct bio *cbps[sc->sc_ndisks];
	int i;
	for (i=0; i<sc->sc_ndisks; i++) {
		if (sc->sc_disks[i] == NULL)
			continue;
		cbps[i] = g_clone_bio(bp);
		if (cbps[i] == NULL)
			break;
		cbps[i]->bio_driver2 = sc->sc_disks[i];
		cbps[i]->bio_offset = sc->sc_disks[i]->provider->mediasize;
		cbps[i]->bio_length = 0;
		cbps[i]->bio_done = NULL;
		cbps[i]->bio_attribute = NULL;
		cbps[i]->bio_data = NULL;
		g_raid5_io_req(sc,cbps[i]);
	}
	int err = i < sc->sc_ndisks ? ENOMEM : 0;
	for (int j=0; j<i; j++) {
		if (sc->sc_disks[j] == NULL)
			continue;
		int e = biowait(cbps[j], "gr5bw");
		if (e && !err)
			err = e;
		g_destroy_bio(cbps[j]);
	}
	g_io_deliver(bp, err);
	g_raid5_unopen(sc);
G_RAID5_DEBUG(0,"g_raid5_flush() end %d.", __LINE__);
}

static void
g_raid5_start(struct bio *bp)
{
	struct g_raid5_softc *sc;
	struct g_provider *pp;

	pp = bp->bio_to;
	sc = pp->geom->softc;
	/*
	 * If sc == NULL, provider's error should be set and g_raid5_start()
	 * should not be called at all.
	 */
	MYKASSERT(sc != NULL,
	          ("Provider's error should be set (error=%d)(device=%s).",
	           pp->error, pp->name));
	MYKASSERT(bp->bio_parent != (void*) sc, ("no cycles!"));

	g_topology_assert_not();

	G_RAID5_LOGREQ(bp, "[start]");

	if (sc->sc_provider->error != 0) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	switch (bp->bio_cmd) {
	case BIO_WRITE:
		g_raid5_wrc++;
		break;
	case BIO_READ:
		g_raid5_rrc++;
		break;
	case BIO_FLUSH:
		break;
	case BIO_DELETE:
		/* delete? what? */
	case BIO_GETATTR:
		/* To which provider it should be delivered? */
	default:
		g_io_deliver(bp, EOPNOTSUPP);
		return;
	}

	if (bp->bio_length < 0 ||
	    bp->bio_offset < 0 || bp->bio_offset > sc->sc_provider->mediasize) {
		g_io_deliver(bp,EINVAL);
		return;
	}
	if (bp->bio_length + bp->bio_offset > sc->sc_provider->mediasize)
		bp->bio_length = sc->sc_provider->mediasize - bp->bio_offset;
	if (bp->bio_length == 0) {
		g_io_deliver(bp,0);
		return;
	}

	mtx_lock(&sc->sq_mtx);
	if (sc->worker == NULL)
		g_io_deliver(bp, ENXIO);
	else
		bioq_insert_tail(&sc->sq, bp);
	mtx_unlock(&sc->sq_mtx);
	g_raid5_wakeup(sc);
}

static void
g_raid5_dispatch(struct g_raid5_softc *sc, struct bio *bp)
{
	int error = sc->sc_provider->error;
	off_t offset = bp->bio_offset;
	off_t length = bp->bio_length;
	u_char *data = bp->bio_data;

	MYKASSERT(bp->bio_children==0, ("no children."));
	MYKASSERT(bp->bio_inbed==0, ("no inbed."));

	g_topology_assert_not();
	g_raid5_open(sc);

	struct bio_queue_head queue;
	bioq_init(&queue);
	while (length > 0 && !error) {
		int soff = offset & (sc->stripesize-1);
		int len = MIN(length, sc->stripesize - soff);
		off_t sno = offset >> sc->stripebits;
		int dno = sno % (sc->sc_ndisks-1);
		if ( dno == 0 && soff == 0 && bp->bio_cmd == BIO_WRITE &&
		     length >= ((sc->sc_ndisks-1) << sc->stripebits) &&
		     g_raid5_nvalid(sc) >= sc->sc_ndisks - 1) {
			error = g_raid5_order_full(sc,&queue,bp,sno,data);
			len *= sc->sc_ndisks - 1;
		} else
			error = g_raid5_order(sc,&queue,bp,sno,dno,soff,len,data);
		length -= len;
		offset += len;
		data += len;
	}

	if (error) {
		g_raid5_des_sq(sc,&queue,bp);
		if (!bp->bio_error)
			bp->bio_error = error;
		bp->bio_completed = 0;
		bp->bio_children = 0;
		bp->bio_inbed = 0;
		if (bp->bio_cmd == BIO_WRITE && bp->bio_parent == (void*) sc) {
			bp->bio_caller2 = bp; /* retry! */
			g_raid5_wakeup(sc);
		} else
{
G_RAID5_DEBUG(0,"%s: request failed: %c.",
              sc->sc_name, bp->bio_cmd==BIO_READ?'R':'W');
			g_io_deliver(bp, bp->bio_error);
}
		g_raid5_unopen(sc);
	} else {
		MYKASSERT(length == 0,
		          ("Length is still greater than 0 (class=%s, name=%s).",
		           bp->bio_to->geom->class->name, bp->bio_to->geom->name));
		g_raid5_req_sq(sc, &queue);
		if (bp->bio_cmd == BIO_WRITE && sc->sc_ndisks == 2)
			g_raid5_wqp_inc(sc,1);
	}
}

static void
g_raid5_update_metadatas(struct g_raid5_softc *sc)
{
	int state = sc->state & G_RAID5_STATE_HOT;

	g_topology_assert_not();

	struct bio urs[sc->sc_ndisks];
	for (int no = 0; no < sc->sc_ndisks; no++)
		if (g_raid5_disk_good(sc, no))
			g_raid5_update_metadata(sc, &sc->sc_disks[no], state, no, urs+no);
		else
			urs[no].bio_cmd = BIO_READ;
	for (int no = 0; no < sc->sc_ndisks; no++) {
		if (urs[no].bio_cmd == BIO_READ)
			continue;
		int error = biowait(urs+no, "gr5wr");
		free(urs[no].bio_data, M_RAID5);
		if (error)
			G_RAID5_DEBUG(0,"%s: %s(%d): meta data update failed: error:%d.",
			              sc->sc_name,sc->sc_disks[no]->provider->name,no,error);
	}
}

static int
g_raid5_flush_sq(struct g_raid5_softc *sc, struct bintime *now)
{
	int twarr = 0; /* there was a read request */
	struct bio_queue_head pushback;
	bioq_init(&pushback);
	int no_write = 0;
	for (;;) {
		mtx_lock(&sc->sq_mtx);
		struct bio *sbp = bioq_first(&sc->sq);
		if (sbp == NULL) {
			TAILQ_CONCAT(&sc->sq.queue, &pushback.queue, bio_queue);
			mtx_unlock(&sc->sq_mtx);
			break;
		}
		bioq_remove(&sc->sq, sbp);
		mtx_unlock(&sc->sq_mtx);

		if (sbp->bio_cmd == BIO_WRITE) {
			if (no_write || sc->wqp > g_raid5_maxwql) {
				if (!no_write)
					no_write = 1;
				bioq_insert_tail(&pushback, sbp);
			} else
				g_raid5_write(sc, sbp, 0, now);
		} else if (sbp->bio_cmd == BIO_FLUSH) {
			if (no_write)
				bioq_insert_tail(&pushback, sbp);
			else
				g_raid5_flush(sc, sbp);
		} else {
			MYKASSERT(sbp->bio_cmd == BIO_READ, ("read expected."));
			MYKASSERT(sbp->bio_children==0, ("no children."));
			if (g_raid5_write(sc, sbp, 0, now))
				bioq_insert_tail(&pushback, sbp);
			else {
				g_raid5_dispatch(sc, sbp);
				if (!twarr)
					twarr = 1;
			}
		}
	}

	if (sc->wqp < g_raid5_maxwql) for (;;) {
		struct bio *wbp = bioq_first(&sc->wdq);
		if (wbp == NULL)
			break;
		bioq_remove(&sc->wdq,wbp);
		wbp->bio_completed = wbp->bio_length;
		g_io_deliver(wbp, sc->sc_provider->error);
	}

	g_raid5_write(sc, NULL, 0, now);

	return twarr;
}

static void
g_raid5_workerD(void *arg)
{
	struct g_raid5_softc *sc = arg;

	g_topology_assert_not();

	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);

	mtx_lock(&sc->dq_mtx);
	while (sc->workerD != NULL) {
		struct bio *dbp = bioq_first(&sc->dq);
		if (dbp == NULL) {
			msleep(&sc->workerD, &sc->dq_mtx, PRIBIO, "gr5do", hz);
			continue;
		}
		bioq_remove(&sc->dq, dbp);

		mtx_unlock(&sc->dq_mtx);
		MYKASSERT(dbp->bio_from->geom->softc == sc, ("done bp's sc no good."));
		g_raid5_ready(sc, dbp);
		mtx_lock(&sc->dq_mtx);
	}
	mtx_unlock(&sc->dq_mtx);

	sc->term = 2;
	wakeup(&sc->term);

	curthread->td_pflags &= ~TDP_GEOM;

	kthread_exit(0);
}

static __inline void
g_raid5_flush_msq(struct g_raid5_softc *sc,
                  struct bintime *lst, struct bintime *now)
{
	if (g_raid5_maxmem > 128*1024*1024) /* not more than 128MB */
		g_raid5_maxmem = 128*1024*1024;
	else if (g_raid5_maxmem < 1024*1024) /* not less than 1MB */
		g_raid5_maxmem = 1024*1024;

	if (lst != NULL && g_raid5_maxmem > sc->memuse) {
		if (now->sec - lst->sec <= 1)
			return; /* can wait */
		(*lst) = *now;
	}

	mtx_lock(&sc->msq_mtx);
	int m = MIN(sc->msl - 2*sc->wqp, sc->msl / 1024);
	off_t min = -1;
	int i = 0;
	struct bio *msp, *bp2;
	TAILQ_FOREACH_SAFE(msp, &sc->msq.queue, bio_queue, bp2) {
		if (lst != NULL && g_raid5_maxmem > sc->memuse)
			if (now->sec - msp->bio_t0.sec <= g_raid5_wdt + 1)
				continue;

		if (min < 0 || min > msp->bio_length)
			min = msp->bio_length;
		else if (min < msp->bio_length && sc->memuse < g_raid5_maxmem)
			continue;

		bioq_remove(&sc->msq, msp);
		sc->msl--;
		sc->memuse -= msp->bio_length;

		free(msp->bio_data, M_RAID5);
		g_destroy_bio(msp);

		i++;
		if (i >= m && sc->memuse < g_raid5_maxmem)
			break;
	}
	MYKASSERT(sc->memuse >= 0,
	          ("%s: memuse was negative (%d msl%d).",
	           sc->sc_provider->name, sc->memuse, sc->msl));
	mtx_unlock(&sc->msq_mtx);
}

static __inline void
g_raid5_set_stripesize(struct g_raid5_softc *sc, u_int s)
{
	MYKASSERT(powerof2(s), ("stripe size (%d) not power of 2.", s));
	sc->stripesize = s;
	sc->stripebits = bitcount32(s - 1);
}

static void
g_raid5_check_and_run(struct g_raid5_softc *sc, int forced)
{
	if (!forced && g_raid5_nvalid(sc) < sc->sc_ndisks)
		return;
	if (sc->sc_provider == NULL) {
		sc->sc_provider = g_new_providerf(sc->sc_geom, "raid5/%s", sc->sc_name);
		sc->sc_provider->mediasize = -1ULL;
	}

	off_t min = -1;
	u_int sectorsize = 0;
	u_int no;
	for (no = 0; no < sc->sc_ndisks; no++) {
		struct g_consumer *disk = sc->sc_disks[no];
		if (disk == NULL)
			continue;
		if (sectorsize==0 || min>disk->provider->mediasize) {
			min = disk->provider->mediasize;
			if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC)
				min -= disk->provider->sectorsize;
		}
		if (sectorsize == 0)
			sectorsize = disk->provider->sectorsize;
		else
			sectorsize = g_raid5_lcm(sectorsize,disk->provider->sectorsize);
	}
	min -= min & (sectorsize - 1);
	min -= min & (sc->stripesize - 1);
	sc->disksize = min;
	min *= sc->sc_ndisks - 1;
	if (sc->sc_provider->mediasize != -1ULL && sc->sc_provider->mediasize > min)
		G_RAID5_DEBUG(0,"%s: WARNING: reducing media size (from %jd to %jd).",
		              sc->sc_name, sc->sc_provider->mediasize, min);
	sc->sc_provider->mediasize = min;
	sc->sc_provider->sectorsize = sectorsize;
	if (sc->stripesize < sectorsize) {
		G_RAID5_DEBUG(0,"%s: WARNING: setting stripesize (%u) to sectorsize (%u)",
		              sc->sc_name, sc->stripesize, sectorsize);
		g_raid5_set_stripesize(sc, sectorsize);
	}
	if (sc->sc_provider->error != 0)
		g_error_provider(sc->sc_provider, 0);
}

static void
g_raid5_worker(void *arg)
{
	struct g_raid5_softc *sc = arg;

	int wt = 0;
	off_t curveri = -1;
	off_t last_upd = -1;
	int last_nv = -1;
	struct bintime now = {0,0};
	struct bintime lst_vbp = {0,0};
	struct bintime lst_msq = {0,0};
	struct bintime lst_rrq = {0,0};
	struct bintime lst_umd = {0,0};
	struct bintime veri_min = {0,0};
	struct bintime veri_max = {0,0};
	struct bintime veri_pa = {0,0};
	int veri_sc = 0;
	int veri_pau = 0;

	g_topology_assert_not();

	thread_lock(curthread);
	sched_prio(curthread, PRIBIO);
	thread_unlock(curthread);

	getbinuptime(&lst_rrq);
	while (sc->sc_provider == NULL && !sc->term) {
		g_raid5_sleep(sc, 0);
		getbinuptime(&now);
		if (now.sec - lst_rrq.sec > g_raid5_tooc)
			break;
	}
	MYKASSERT(sc->sc_rootmount != NULL, ("why-oh-why???"));
	root_mount_rel(sc->sc_rootmount);
	sc->sc_rootmount = NULL;
	if (!sc->term) {
		g_topology_lock();
		g_raid5_check_and_run(sc, 1);
		g_topology_unlock();
	}
	G_RAID5_DEBUG(0, "%s: activated%s (need about %dMiB kmem (max)).",
	              sc->sc_name,
	              sc->sc_ndisks > g_raid5_nvalid(sc) ? " (forced)" : "",
	              (g_raid5_maxwql *
	                  (2*MAXPHYS + (sc->sc_ndisks-1)*(3<<sc->stripebits))+
	                g_raid5_maxmem) >> 20);

	struct bio Vbp;
	struct bio vbp;

	int vbp_active = 0;
	while (!sc->term) {
		g_raid5_sleep(sc, &wt);
		if (curveri < 0)
			getbinuptime(&now);
		else
			binuptime(&now);

		g_raid5_flush_msq(sc, &lst_msq, &now);

		MYKASSERT(sc->sc_provider != NULL, ("provider needed here."));
		if (sc->sc_provider == NULL)
			break;

		g_topology_assert_not();
		if (!vbp_active && !g_raid5_try_open(sc)) {
			wt = 1;
			continue;
		}

		if (g_raid5_flush_sq(sc, &now))
			lst_rrq = now;

		int upd_meta = 0;
		char *cause = NULL;
		if (!vbp_active) {
			if (sc->conf_order) {
				upd_meta = 1;
				if (sc->conf_order == 1) {
					sc->verified = 0;
					cause = "verify forced";
				} else if (sc->conf_order == 2) {
					sc->verified = sc->disksize;
					cause = "verify aborted";
				} else
					cause = "new configuration";
				sc->conf_order = 0;
			}
			if (sc->verified  >= sc->disksize) {
				if (!upd_meta) {
					upd_meta = 1;
					cause = "verify completed";
				}
				sc->verified = -1;
				curveri = -1;
				sc->newest = -1;
				sc->state &= ~G_RAID5_STATE_VERIFY;
				sc->veri_pa = 0;
				if ((sc->state&G_RAID5_STATE_COWOP) && sc->sc_provider->error)
					g_error_provider(sc->sc_provider, 0);
			} else if (sc->verified >= 0) {
				if (sc->verified != curveri) {
					curveri = sc->verified;
					if (sc->newest < 0) {
						for (int i=0; i<sc->sc_ndisks; i++)
							if (!g_raid5_disk_good(sc,i)) {
								sc->newest = i;
								break;
							}
					}
					if (!upd_meta) {
						upd_meta = last_upd < 0 || curveri < last_upd ||
						           now.sec - lst_umd.sec > 200;
						if (upd_meta)
							cause = "store verify progress";
					}
				}
				if (!(sc->state & G_RAID5_STATE_VERIFY)) {
					sc->state |= G_RAID5_STATE_VERIFY;
					if (!upd_meta) {
						upd_meta = 1;
						cause = "store verify flag";
					}
				}
				MYKASSERT((curveri & (sc->stripesize - 1)) == 0,
						  ("invalid value for curveri."));
			} else if (curveri != -1)
				curveri = -1;
		}

		int nv = g_raid5_nvalid(sc);
		if (last_nv != nv) {
			if (!upd_meta) {
				upd_meta = 1;
				cause = "valid disk count";
			}
			last_nv = nv;
		}

		if (sc->wqp) {
			if (!(sc->state & G_RAID5_STATE_HOT)) {
				sc->state |= G_RAID5_STATE_HOT;
				if (!upd_meta && !sc->no_hot) {
					upd_meta = 1;
					cause = "state hot";
				}
			}
		} else {
			if (curveri < 0 && (sc->state & G_RAID5_STATE_HOT) &&
			    now.sec - lst_umd.sec > g_raid5_wdt) {
				sc->state &= ~G_RAID5_STATE_HOT;
				if (!upd_meta && !sc->no_hot) {
					upd_meta = 1;
					cause = "state calm";
				}
			}
		}

		if (upd_meta) {
			if (curveri >= 0) {
				int per = sc->disksize ? curveri * 10000 / sc->disksize : 0;
				if (nv < sc->sc_ndisks)
					G_RAID5_DEBUG(0, "%s: first write at %d.%02d%% (cause: %s).",
					              sc->sc_name, per/100,per%100, cause);
				else {
					int stepper = sc->disksize > 0 && curveri > last_upd ?
					                (curveri - last_upd)*10000 / sc->disksize :
					                0;
					struct g_consumer *np = sc->newest<0 ? NULL: sc->sc_disks[sc->newest];
					char *dna = np==NULL ? "all" : np->provider->name;
					if (stepper > 0) {
						veri_sc = 0;
						struct bintime delta = now;
						bintime_sub(&delta, &lst_vbp);
						int64_t dt = g_raid5_bintime2micro(&delta) / 1000;
						G_RAID5_DEBUG(0, "%s: %s(%d): re-sync in progress: %d.%02d%% "
						                 "p:%d ETA:%jdmin (cause: %s).", sc->sc_name,
						              dna,sc->newest, per/100,per%100,veri_pau,
						              dt*(10000-per)/(stepper*60*1000), cause);
						veri_pau = 0;
					} else
						G_RAID5_DEBUG(1,"%s: %s(%d): re-sync in progress: "
						                "%d.%02d%% (cause: %s).", sc->sc_name,
						                dna, sc->newest, per/100,per%100, cause);
					lst_vbp = now;
				}
			} else
				G_RAID5_DEBUG(last_upd>=0?0:1,
				              "%s: update meta data on %d disks (cause: %s).",
				              sc->sc_name, nv, cause);
			if (!sc->wqp && (sc->state & G_RAID5_STATE_HOT))
				sc->state &= ~G_RAID5_STATE_HOT;
			g_topology_assert_not();
			g_raid5_update_metadatas(sc);
			lst_umd = now;
			last_upd = curveri;
		}

		if (vbp_active || curveri < 0 || nv < sc->sc_ndisks) {
			wt = 1;
			if (vbp_active) {
				if (vbp.bio_driver2 == NULL) {
					vbp_active = 0;
					if (!vbp.bio_error) {
						if (curveri == sc->verified)
							sc->verified += vbp.bio_length;
						wt = 0;
					}
					bioq_remove(&sc->wq, &Vbp);
					struct bintime delta = now;
					bintime_sub(&delta,&Vbp.bio_t0);
					if (veri_sc == 0) {
						veri_min = delta;
						veri_max = delta;
					} else if (g_raid5_bintime_cmp(&delta, &veri_min) < 0)
						veri_min = delta;
					else if (g_raid5_bintime_cmp(&delta, &veri_max) > 0)
						veri_max = delta;
					delta = veri_max;
					bintime_sub(&delta, &veri_min);
					if (g_raid5_bintime2micro(&veri_max) >
					    g_raid5_bintime2micro(&veri_min)*g_raid5_veri_fac) {
						int dt = (veri_sc&3) + 3;
						veri_pau += dt;
						dt *= 100; /* milli seconds */
						veri_sc = 0;
						veri_pa = now;
						bintime_addx(&veri_pa, dt*18446744073709551ULL);
						sc->veri_pa++;
					} else
						veri_sc++;
				}
			} else
				g_raid5_unopen(sc);
			continue;
		}

		/* wait due to processing time fluctuation? */
		if (veri_sc == 0 && g_raid5_bintime_cmp(&now,&veri_pa) < 0) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		/* wait for concurrent read? */
		struct bintime tmptim = now;
		bintime_sub(&tmptim, &lst_rrq);
		if (tmptim.sec == 0 && g_raid5_veri_nice < 1000 &&
		    tmptim.frac < g_raid5_veri_nice*18446744073709551ULL) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		bzero(&Vbp,sizeof(Vbp));
		Vbp.bio_offset = curveri * (sc->sc_ndisks - 1);
		Vbp.bio_length = (sc->sc_ndisks - 1) << sc->stripebits;
		if (g_raid5_stripe_conflict(sc, &Vbp)) {
			g_raid5_unopen(sc);
			wt = 1;
			continue;
		}

		bzero(&vbp,sizeof(vbp));
		vbp.bio_cmd = BIO_READ;
		vbp.bio_offset = -1;
		vbp.bio_length = sc->stripesize;
		vbp.bio_driver2 = sc->worker;

		struct bio_queue_head queue;
		bioq_init(&queue);
		int pno = (curveri >> sc->stripebits) % sc->sc_ndisks;
		int dno = (pno + 1) % sc->sc_ndisks;
		if (g_raid5_conv_veri_read(&queue, sc, &vbp, curveri,
		                           vbp.bio_length,dno,pno,NULL)) {
 			g_raid5_des_sq(sc,&queue, &vbp);
			g_raid5_unopen(sc);
			wt = 1;
		} else {
			Vbp.bio_parent = (void*) sc;
			bioq_insert_tail(&sc->wq, &Vbp);
			Vbp.bio_t0 = now;

			g_raid5_req_sq(sc, &queue);

			vbp_active = 1;
		}
	}

	G_RAID5_DEBUG(0,"%s: worker thread exiting.",sc->sc_name);

	/* 1st term stage */
	g_topology_assert_not();
	mtx_lock(&sc->sq_mtx);
	sc->worker = NULL;
	mtx_unlock(&sc->sq_mtx);
G_RAID5_DEBUG(1,"%s: exit1.",sc->sc_name);
	while ((!TAILQ_EMPTY(&sc->sq.queue)) || (!TAILQ_EMPTY(&sc->wdq.queue)))
		g_raid5_flush_sq(sc, &now);
G_RAID5_DEBUG(1,"%s: exit2 %d %d.",sc->sc_name, sc->wqp,sc->wqpi);
	g_raid5_flush_wq(sc);
	sc->state &= ~G_RAID5_STATE_HOT;

	/* 2nd term stage */
G_RAID5_DEBUG(1,"%s: exit3.",sc->sc_name);
	g_raid5_no_more_open(sc); /* wait for read requests to be readied */
	g_raid5_more_open(sc);
G_RAID5_DEBUG(1,"%s: exit4.",sc->sc_name);
	mtx_lock(&sc->dq_mtx); /* make sure workerD thread sleeps or blocks */
	wakeup(&sc->workerD);
	sc->workerD = NULL;
	mtx_unlock(&sc->dq_mtx);
G_RAID5_DEBUG(1,"%s: exit5.",sc->sc_name);
	while (sc->msl > 0)
		g_raid5_flush_msq(sc, NULL,NULL);

	/* 3rd term stage */
G_RAID5_DEBUG(1,"%s: exit6.",sc->sc_name);
	while (sc->term < 2) /* workerD thread still active? */
		tsleep(&sc->term, PRIBIO, "gr5ds", hz);
G_RAID5_DEBUG(1,"%s: exit7.",sc->sc_name);
	sc->term = 3;
	wakeup(sc);

	curthread->td_pflags &= ~TDP_GEOM;

	kthread_exit(0);
}

/*
 * Add disk to given device.
 */
static int
g_raid5_add_disk(struct g_raid5_softc *sc, struct g_provider *pp, u_int no)
{
	struct g_consumer *cp, *fcp;
	struct g_geom *gp;
	int error;

	g_topology_assert();
	g_topology_unlock();
	g_raid5_no_more_open(sc);
	g_topology_lock();

	/* Metadata corrupted? */
	if (no >= sc->sc_ndisks) {
		error = EINVAL;
		goto unlock;
	}

	/* Check if disk is not already attached. */
	if (sc->sc_disks[no] != NULL) {
		error = EEXIST;
		goto unlock;
	}
	sc->sc_disk_states[no] = 0;

	gp = sc->sc_geom;
	fcp = LIST_FIRST(&gp->consumer);
	cp = g_new_consumer(gp);
	error = g_attach(cp, pp);
	if (error != 0) {
		g_destroy_consumer(cp);
		goto unlock;
	}

	if (fcp != NULL && (fcp->acr>0 || fcp->acw>0 || fcp->ace>0))
		error = g_access(cp, fcp->acr, fcp->acw, fcp->ace);
	else
		error = g_access(cp, 1,1,1);
	if (error)
		goto undo;

	if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC) {
		struct g_raid5_metadata md;

		/* Re-read metadata. */
		error = g_raid5_read_metadata(&cp, &md);
		if (error)
			goto undo;

		if (strcmp(md.md_magic, G_RAID5_MAGIC) != 0 ||
		    strcmp(md.md_name, sc->sc_name) != 0 ||
		    md.md_stripesize != sc->stripesize ||
		    md.md_id != sc->sc_id) {
			G_RAID5_DEBUG(0, "%s: %s: metadata changed.", sc->sc_name, pp->name);
			error = ENXIO;
			goto undo;
		}
		sc->hardcoded = md.md_provider[0]!=0;
		sc->no_hot = md.md_no_hot;

		if (md.md_state&G_RAID5_STATE_HOT) {
			if (sc->no_hot)
				G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: HOT although no_hot.",
				              sc->sc_name, pp->name, no);
			sc->state |= G_RAID5_STATE_VERIFY;
			sc->verified = 0;
			if (sc->newest >= 0)
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (HOT): %d/%d.",
				              sc->sc_name,pp->name,no,md.md_newest,sc->newest);
			else {
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (HOT): %d.",
				              sc->sc_name,pp->name,no,md.md_newest);
				if (md.md_newest >= 0)
					sc->newest = md.md_newest;
			}
		} else {
			if (md.md_newest >= 0 && md.md_verified != -1) {
				sc->newest = md.md_newest;
				G_RAID5_DEBUG(0, "%s: %s(%d): newest disk data (CALM): %d.",
				              sc->sc_name,pp->name,no,md.md_newest);
			}
			if (md.md_state&G_RAID5_STATE_VERIFY) {
				sc->state |= G_RAID5_STATE_VERIFY;
				if (sc->verified==-1 || md.md_verified<sc->verified)
					sc->verified = md.md_verified != -1 ? md.md_verified : 0;
			}
		}
		if (!(sc->state&G_RAID5_STATE_VERIFY) && sc->newest >= 0) {
			G_RAID5_DEBUG(0, "%s: %s(%d): WARNING: newest disk but no verify.",
			              sc->sc_name,pp->name,no);
			sc->newest = -1;
		}
		if (md.md_state&G_RAID5_STATE_SAFEOP)
			sc->state |= G_RAID5_STATE_SAFEOP;
		if (md.md_state&G_RAID5_STATE_COWOP)
			sc->state |= G_RAID5_STATE_COWOP;
	} else
		sc->hardcoded = 0;

	cp->private = &sc->sc_disks[no];
	sc->sc_disks[no] = cp;

	G_RAID5_DEBUG(0, "%s: %s(%d): disk attached.", sc->sc_name,pp->name,no);

	g_raid5_check_and_run(sc, 0);

unlock:
	g_raid5_more_open(sc);

	if (!error)
		g_raid5_wakeup(sc);

	return error;

undo:
	g_detach(cp);
	g_destroy_consumer(cp);
	goto unlock;
}

static struct g_geom *
g_raid5_create(struct g_class *mp, const struct g_raid5_metadata *md,
    u_int type)
{
	struct g_raid5_softc *sc;
	struct g_geom *gp;

	g_topology_assert();
	G_RAID5_DEBUG(1, "%s: creating device.", md->md_name);

	/* Two disks is minimum. */
	if (md->md_all < 2)
		return NULL;

	/* Check for duplicate unit */
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc != NULL && strcmp(sc->sc_name, md->md_name) == 0) {
			G_RAID5_DEBUG(0, "%s: device already configured.", gp->name);
			return (NULL);
		}
	}
	gp = g_new_geomf(mp, "%s", md->md_name);
	gp->softc = NULL;	/* for a moment */

	sc = malloc(sizeof(*sc), M_RAID5, M_WAITOK | M_ZERO);
/*XXX: use 2nd sector as bitfield, that denotes potentially damaged parity areas for better performance */
	gp->start = g_raid5_start;
	gp->spoiled = g_raid5_orphan;
	gp->orphan = g_raid5_orphan;
	gp->access = g_raid5_access;
	gp->dumpconf = g_raid5_dumpconf;

	sc->sc_id = md->md_id;
	sc->destroy_on_za = 0;
	sc->state = G_RAID5_STATE_CALM;
	sc->veri_pa = 0;
	sc->verified = -1;
	sc->newest = -1;
	g_raid5_set_stripesize(sc, md->md_stripesize);
	sc->no_more_open = 0;
	sc->open_count = 0;
	mtx_init(&sc->open_mtx, "graid5:open", NULL, MTX_DEF);
	sc->no_sleep = 0;
	mtx_init(&sc->sleep_mtx,"graid5:sleep/main", NULL, MTX_DEF);
	mtx_init(&sc->sq_mtx, "graid5:sq", NULL, MTX_DEF);
	bioq_init(&sc->sq);
	mtx_init(&sc->dq_mtx, "graid5:dq", NULL, MTX_DEF);
	bioq_init(&sc->dq);
	bioq_init(&sc->wq);
	bioq_init(&sc->wdq);
	sc->msl = 0;
	sc->memuse = 0;
	mtx_init(&sc->msq_mtx,"graid5:msq", NULL, MTX_DEF);
	bioq_init(&sc->msq);
	sc->wqp = 0;
	sc->wqpi = 0;
	sc->sc_ndisks = md->md_all;
	sc->sc_disks = malloc(sizeof(*sc->sc_disks) * sc->sc_ndisks,
	                      M_RAID5, M_WAITOK | M_ZERO);
	sc->sc_disk_states = malloc(sc->sc_ndisks, M_RAID5, M_WAITOK | M_ZERO);
	sc->lstdno = 0;
	sc->sc_type = type;

	sc->term = 0;

	if (kthread_create(g_raid5_worker, sc, &sc->worker, 0, 0,
							 "g_raid5/main %s", md->md_name) != 0) {
		sc->workerD = NULL;
		sc->worker = NULL;
	} else if (kthread_create(g_raid5_workerD, sc, &sc->workerD, 0, 0,
							 "g_raid5/done %s", md->md_name) != 0) {
		sc->workerD = NULL;
		sc->term = 1;
	}

	gp->softc = sc;
	sc->sc_geom = gp;
	sc->sc_provider = NULL;

	G_RAID5_DEBUG(0, "%s: device created (stripesize=%d).",
	              sc->sc_name, sc->stripesize);

	sc->sc_rootmount = root_mount_hold("GRAID5");

	return (gp);
}

static int
g_raid5_destroy(struct g_raid5_softc *sc, boolean_t force, boolean_t noyoyo)
{
	struct g_provider *pp;
	struct g_geom *gp;
	u_int no;

	g_topology_assert();

	if (sc == NULL)
		return ENXIO;

	pp = sc->sc_provider;
	if (pp != NULL && (pp->acr != 0 || pp->acw != 0 || pp->ace != 0)) {
		if (force) {
			if (sc->destroy_on_za) {
				G_RAID5_DEBUG(0, "%s: device is still open. It "
				              "will be removed on last close.", pp->name);
				return EBUSY;
			}
			else
				G_RAID5_DEBUG(0, "%s: device is still open, so it "
				              "cannot be definitely removed.", pp->name);
		} else {
			G_RAID5_DEBUG(1, "%s: device is still open (r%dw%de%d).",
			              pp->name, pp->acr, pp->acw, pp->ace);
			return EBUSY;
		}
	}

	sc->term = 1;
	g_raid5_wakeup(sc);
	while (sc->term < 3)
		tsleep(sc, PRIBIO, "gr5de", hz);

	for (no = 0; no < sc->sc_ndisks; no++)
		if (sc->sc_disks[no] != NULL)
			g_raid5_remove_disk(sc, &sc->sc_disks[no], 0, noyoyo);

	gp = sc->sc_geom;
	gp->softc = NULL;

	if (sc->sc_provider != NULL) {
		sc->sc_provider->flags |= G_PF_WITHER;
		g_orphan_provider(sc->sc_provider, ENXIO);
		sc->sc_provider = NULL;
	}

	G_RAID5_DEBUG(0, "%s: device removed.", sc->sc_name);

	mtx_destroy(&sc->open_mtx);
	mtx_destroy(&sc->sleep_mtx);
	mtx_destroy(&sc->sq_mtx);
	mtx_destroy(&sc->dq_mtx);
	mtx_destroy(&sc->msq_mtx);
	free(sc->sc_disks, M_RAID5);
	free(sc->sc_disk_states, M_RAID5);
	free(sc, M_RAID5);

	pp = LIST_FIRST(&gp->provider);
	if (pp == NULL || (pp->acr == 0 && pp->acw == 0 && pp->ace == 0))
		G_RAID5_DEBUG(0, "%s: device destroyed.", gp->name);

	g_wither_geom(gp, ENXIO);

	return 0;
}

static int
g_raid5_destroy_geom(struct gctl_req *req __unused,
                     struct g_class *mp __unused, struct g_geom *gp)
{
	struct g_raid5_softc *sc = gp->softc;
	return g_raid5_destroy(sc, 0, 0);
}

static struct g_geom *
g_raid5_taste(struct g_class *mp, struct g_provider *pp, int flags __unused)
{
	struct g_raid5_metadata md;
	struct g_consumer *cp;
	struct g_geom *gp;
	int error;

	g_trace(G_T_TOPOLOGY, "%s(%s, %s)", __func__, mp->name, pp->name);
	g_topology_assert();

	G_RAID5_DEBUG(2, "%s: tasting.", pp->name);

	gp = g_new_geomf(mp, "raid5:taste");
	gp->start = g_raid5_start;
	gp->access = g_raid5_access;
	gp->orphan = g_raid5_orphan;
	cp = g_new_consumer(gp);
	g_attach(cp, pp);
	error = g_raid5_read_metadata(&cp, &md);
	g_detach(cp);
	g_destroy_consumer(cp);
	g_destroy_geom(gp);
	if (error) {
		G_RAID5_DEBUG(2, "%s: read meta data failed: error:%d.", pp->name, error);
		return NULL;
	}
	gp = NULL;

	if (strcmp(md.md_magic, G_RAID5_MAGIC) != 0) {
		G_RAID5_DEBUG(2, "%s: wrong magic.", pp->name);
		return NULL;
	}
	if (md.md_version > G_RAID5_VERSION) {
		G_RAID5_DEBUG(0, "%s: geom_raid5.ko module is too old.", pp->name);
		return NULL;
	}
	/*
	 * Backward compatibility:
	 */
	if (md.md_provider[0] != '\0' && strcmp(md.md_provider, pp->name) != 0) {
		G_RAID5_DEBUG(0, "%s: %s: wrong hardcode.", md.md_name,pp->name);
		return NULL;
	}

	/* check for correct size */
	if (md.md_provsize != pp->mediasize)
		G_RAID5_DEBUG(0, "%s: %s: size mismatch (expected %jd, got %jd).",
		              md.md_name,pp->name, md.md_provsize,pp->mediasize);

	/*
	 * Let's check if device already exists.
	 */
	struct g_raid5_softc *sc = NULL;
	int found = 1;
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (sc->sc_type == G_RAID5_TYPE_AUTOMATIC &&
		    strcmp(md.md_name, sc->sc_name) == 0 &&
		    md.md_id == sc->sc_id) {
			break;
	        }
	}
	if (gp == NULL) {
		found = 0;
		gp = g_raid5_create(mp, &md, G_RAID5_TYPE_AUTOMATIC);
		if (gp == NULL) {
			G_RAID5_DEBUG(0, "%s: cannot create device.", md.md_name);
			return NULL;
		}
		sc = gp->softc;
	}
	G_RAID5_DEBUG(1, "%s: %s: adding disk.", gp->name, pp->name);
	error = g_raid5_add_disk(sc, pp, md.md_no);
	if (error) {
		G_RAID5_DEBUG(0, "%s: %s: cannot add disk (error=%d).",
						  gp->name, pp->name, error);
		if (!found)
			g_raid5_destroy(sc, 1, 0);
		return NULL;
	}

	return gp;
}

static void
g_raid5_ctl_create(struct gctl_req *req, struct g_class *mp)
{
	u_int attached, no;
	struct g_raid5_metadata md;
	struct g_provider *pp;
	struct g_raid5_softc *sc;
	struct g_geom *gp;
	struct sbuf *sb;
	const char *name;
	char param[16];
	int *nargs;

	g_topology_assert();
	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	strlcpy(md.md_magic, G_RAID5_MAGIC, sizeof(md.md_magic));
	md.md_version = G_RAID5_VERSION;
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	strlcpy(md.md_name, name, sizeof(md.md_name));
	md.md_id = arc4random();
	md.md_no = 0;
	md.md_all = *nargs - 1;
	bzero(md.md_provider, sizeof(md.md_provider));
	/* This field is not important here. */
	md.md_provsize = 0;

	/* Check all providers are valid */
	for (no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", no);
			return;
		}
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			G_RAID5_DEBUG(1, "%s: disk invalid.", name);
			gctl_error(req, "%s: disk invalid.", name);
			return;
		}
	}

	gp = g_raid5_create(mp, &md, G_RAID5_TYPE_MANUAL);
	if (gp == NULL) {
		gctl_error(req, "Can't configure %s.", md.md_name);
		return;
	}

	sc = gp->softc;
	sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	sbuf_printf(sb, "Can't attach disk(s) to %s:", gp->name);
	for (attached = 0, no = 1; no < *nargs; no++) {
		snprintf(param, sizeof(param), "arg%u", no);
		name = gctl_get_asciiparam(req, param);
		if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
			name += strlen("/dev/");
		pp = g_provider_by_name(name);
		MYKASSERT(pp != NULL, ("Provider %s disappear?!", name));
		if (g_raid5_add_disk(sc, pp, no - 1) != 0) {
			G_RAID5_DEBUG(1, "%s: %s(%d): disk not attached.",
			              gp->name, pp->name, no);
			sbuf_printf(sb, " %s", pp->name);
			continue;
		}
		attached++;
	}
	sbuf_finish(sb);
	if (md.md_all != attached) {
		g_raid5_destroy(gp->softc, 1, 0);
		gctl_error(req, "%s", sbuf_data(sb));
	}
	sbuf_delete(sb);
}

static struct g_raid5_softc *
g_raid5_find_device(struct g_class *mp, const char *name)
{
	struct g_raid5_softc *sc;
	struct g_geom *gp;

	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if (strcmp(sc->sc_name, name) == 0)
			return (sc);
	}
	return (NULL);
}

static void
g_raid5_ctl_destroy(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, error;
	const char *name;
	char param[16];
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	int *force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}
	int *noyoyo = gctl_get_paraml(req, "noyoyo", sizeof(*noyoyo));
	if (noyoyo == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_raid5_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		error = g_raid5_destroy(sc, *force, *noyoyo);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_name, error);
			return;
		}
	}
}

static void
g_raid5_ctl_remove(struct gctl_req *req, struct g_class *mp)
{
	/*just call remove_disk and clear meta*/
	struct g_raid5_softc *sc;
	int *nargs;
	const char *name;
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 2) {
		gctl_error(req, "Exactly one device and one provider name needed.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "provider (device %s) does not exist.", name);
		return;
	}

	for (i = 1; i < (u_int)*nargs; i++) {
		int j;
		struct g_provider *pp;
		char param[16];
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			gctl_error(req, "There is no provider '%s'.", name);
			return;
		}
		for (j=0; j<sc->sc_ndisks; j++) {
			struct g_consumer **cp = &sc->sc_disks[j];
			if ((*cp) != NULL && (*cp)->provider == pp) {
				g_raid5_remove_disk(sc, cp, 1, 0);
				break;
			}
		}
		if (j == sc->sc_ndisks)
			gctl_error(req, "Device '%s' has no provider '%s'.",sc->sc_name,name);
		
	}
}

static void
g_raid5_ctl_insert(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, *hardcode;
	const char *name;
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 2) {
		gctl_error(req, "Exactly one device and one provider name needed.");
		return;
	}

	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No <hardcode> argument.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	if (*hardcode && !sc->hardcoded)
		sc->hardcoded = 1;

	for (i = 1; i < (u_int)*nargs; i++) {
		int f, j;
		struct g_provider *pp;
		struct g_consumer *cp;
		char param[16];
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'prov%u' argument.", i);
			return;
		}
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			gctl_error(req, "There is no provider '%s'.", name);
			return;
		}
		if (sc->stripesize < pp->sectorsize) {
			gctl_error(req, "Provider '%s' has incompatible sector size"
			           " (stripe size is %d).", name, sc->stripesize);
			return;
		}
		for (j=0,f=-1; j < sc->sc_ndisks; j++) {
			struct g_consumer *cp = sc->sc_disks[j];
			if (cp == NULL) {
				if (f < 0)
					f = j;
			} else if (cp->provider == pp) {
				gctl_error(req, "Provider '%s' already attached to '%s'.",
				           name, sc->sc_name);
				return;
			}
		}
		if (f < 0) {
			gctl_error(req, "Device '%s' has enough providers.",sc->sc_name);
			return;
		}
		cp = g_new_consumer(sc->sc_geom);
		if (g_attach(cp, pp) != 0) {
			g_destroy_consumer(cp);
			gctl_error(req, "Cannot attach to provider %s.", name);
			return;
		}
		if (g_access(cp, 1,1,1) != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			gctl_error(req, "%s: cannot attach to provider.", name);
			return;
		}
		g_topology_unlock();
		int error = g_raid5_update_metadata(sc, &cp,
							G_RAID5_STATE_HOT|G_RAID5_STATE_VERIFY, f, NULL);
		g_topology_lock();
		g_access(cp, -1,-1,-1);
		G_RAID5_DEBUG(0, "%s: %s: wrote meta data on insert: error:%d.",
		              sc->sc_name, name, error);
		if (pp->acr || pp->acw || pp->ace) {
			G_RAID5_DEBUG(0,"%s: have to correct acc counts: ac(%d,%d,%d).",
			              sc->sc_name,pp->acr,pp->acw,pp->ace);
			g_access(cp,-pp->acr,-pp->acw,-pp->ace);
			cp->acr = cp->acw = cp->ace = 0;
		}
		g_detach(cp);
		g_destroy_consumer(cp);
	}
}

static void
g_raid5_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_raid5_softc *sc;
	int *nargs, *hardcode, *rebuild;
	const char *name;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 1) {
		gctl_error(req, "Wrong device count.");
		return;
	}

	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No <hardcode> argument.");
		return;
	}

	int *activate = gctl_get_paraml(req, "activate", sizeof(*activate));
	if (activate == NULL) {
		gctl_error(req, "No <activate> argument.");
		return;
	}

	int *no_hot = gctl_get_paraml(req, "nohot", sizeof(*no_hot));
	if (no_hot == NULL) {
		gctl_error(req, "No <nohot> argument.");
		return;
	}

	int *safeop = gctl_get_paraml(req, "safeop", sizeof(*safeop));
	if (safeop == NULL) {
		gctl_error(req, "No <safeop> argument.");
		return;
	}

	int *cowop = gctl_get_paraml(req, "cowop", sizeof(*cowop));
	if (cowop == NULL) {
		gctl_error(req, "No <cowop> argument.");
		return;
	}

	rebuild = gctl_get_paraml(req, "rebuild", sizeof(*rebuild));
	if (rebuild == NULL) {
		gctl_error(req, "No <rebuild> argument.");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No <name> argument.");
		return;
	}
	sc = g_raid5_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "Device %s not found.", name);
		return;
	}

	if (*activate) {
		for (int i=0; i<sc->sc_ndisks; i++)
			sc->sc_disk_states[i] = 0;
		g_error_provider(sc->sc_provider, 0);
	}

	if (*hardcode)
		sc->hardcoded = !sc->hardcoded;

	if (*safeop)
		sc->state ^= G_RAID5_STATE_SAFEOP;

	if (*cowop)
		sc->state ^= G_RAID5_STATE_COWOP;

	if (*no_hot)
		sc->no_hot = sc->no_hot ? 0 : 1;

	if (*rebuild) {
		if (sc->verified < 0)
			sc->conf_order = 1;
		else
			sc->conf_order = 2;
	} else
		sc->conf_order = 3;
}
		
static void
g_raid5_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_RAID5_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "create") == 0)
		g_raid5_ctl_create(req, mp);
	else if (strcmp(verb, "destroy") == 0 ||
	         strcmp(verb, "stop") == 0)
		g_raid5_ctl_destroy(req, mp);
	else if (strcmp(verb, "remove") == 0)
		g_raid5_ctl_remove(req, mp);
	else if (strcmp(verb, "insert") == 0)
		g_raid5_ctl_insert(req, mp);
	else if (strcmp(verb, "configure") == 0)
		g_raid5_ctl_configure(req, mp);
	else
		gctl_error(req, "Unknown verb.");
}

static void
g_raid5_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
                 struct g_consumer *cp, struct g_provider *pp)
{
	struct g_raid5_softc *sc;

	g_topology_assert();
	sc = gp->softc;
	if (sc == NULL)
		return;

	if (pp != NULL) {
		/* Nothing here. */
	} else if (cp != NULL) {
		int dn = g_raid5_find_disk(sc, cp);
		sbuf_printf(sb, "%s<Error>%s</Error>\n",
		            indent, dn < 0 || sc->sc_disk_states[dn] ? "Yes" : "No");
		sbuf_printf(sb, "%s<DiskNo>%d</DiskNo>\n", indent, dn);
		if (sc->state & G_RAID5_STATE_VERIFY &&
		    (sc->newest < 0 || dn == sc->newest)) {
			int per = sc->disksize>0?sc->verified * 100 / sc->disksize:-1;
			sbuf_printf(sb, "%s<Synchronized>%jd / %d%% (p:%d)</Synchronized>\n",
			            indent, sc->verified, per, sc->veri_pa);
		}
	} else {
		sbuf_printf(sb, "%s<ID>%u</ID>\n", indent, (u_int)sc->sc_id);
		sbuf_printf(sb, "%s<Newest>%d</Newest>\n", indent, sc->newest);
		sbuf_printf(sb, "%s<MemUse>%d (msl %d)</MemUse>\n",
		            indent, sc->memuse, sc->msl);
		sbuf_printf(sb, "%s<Stripesize>%u</Stripesize>\n",
		            indent, (u_int)sc->stripesize);

		sbuf_printf(sb, "%s<Pending>(wqp %d // %d)",
		            indent, sc->wqp, sc->wqpi);
		struct bio *bp;
		TAILQ_FOREACH(bp, &sc->wq.queue, bio_queue) {
			sbuf_printf(sb, " %jd..%jd/%jd%c",
			            bp->bio_offset,bp->bio_offset+bp->bio_length,
			            bp->bio_length,bp->bio_parent?'a':'p');
		}
		sbuf_printf(sb, "</Pending>\n");

		sbuf_printf(sb, "%s<Type>", indent);
		switch (sc->sc_type) {
		case G_RAID5_TYPE_AUTOMATIC:
			sbuf_printf(sb, "AUTOMATIC");
			break;
		case G_RAID5_TYPE_MANUAL:
			sbuf_printf(sb, "MANUAL");
			break;
		default:
			sbuf_printf(sb, "UNKNOWN");
			break;
		}
		sbuf_printf(sb, "</Type>\n");

		int nv = g_raid5_nvalid(sc);
		sbuf_printf(sb, "%s<Status>Total=%u, Online=%u</Status>\n",
		    indent, sc->sc_ndisks, nv);

		sbuf_printf(sb, "%s<State>", indent);
		if (sc->sc_provider == NULL || sc->sc_provider->error != 0)
			sbuf_printf(sb, "FAILURE (%d)",
			            sc->sc_provider==NULL? -1 : sc->sc_provider->error);
		else {
			if (nv < sc->sc_ndisks - 1)
				sbuf_printf(sb, "CRITICAL");
			else if (nv < sc->sc_ndisks)
				sbuf_printf(sb, "DEGRADED");
			else if (sc->state & G_RAID5_STATE_VERIFY)
				sbuf_printf(sb, "REBUILDING");
			else
				sbuf_printf(sb, "COMPLETE");
			if (sc->state & G_RAID5_STATE_HOT)
				sbuf_printf(sb, " HOT");
			else
				sbuf_printf(sb, " CALM");
		}

		if (sc->state&G_RAID5_STATE_COWOP)
			sbuf_printf(sb, " (cowop)");
		if (sc->state&G_RAID5_STATE_SAFEOP)
			sbuf_printf(sb, " (safeop)");
		if (sc->hardcoded)
			sbuf_printf(sb, " (hardcoded)");
		if (sc->term)
			sbuf_printf(sb, " (destroyed?)");
		if (sc->no_hot)
			sbuf_printf(sb, " (no hot)");
		if (sc->worker == NULL)
			sbuf_printf(sb, " (NO OPERATION POSSIBLE)");
		sbuf_printf(sb, "</State>\n");
	}
}

static void
g_raid5_shutdown(void *arg, int howto __unused)
{
	struct g_class *mp = arg;
	struct g_geom *gp, *gp2;

	DROP_GIANT();
	g_topology_lock();
	mp->ctlreq = NULL;
	mp->taste = NULL;
	LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
		struct g_raid5_softc *sc;
		if ((sc = gp->softc) == NULL)
			continue;
		sc->destroy_on_za = 1;
		g_raid5_destroy(sc, 1, 0);
	}
	g_topology_unlock();
	PICKUP_GIANT();
}

static void
g_raid5_init(struct g_class *mp)
{
	G_RAID5_DEBUG(0, "Module loaded, version %s (rev %s)", graid5_version, graid5_revision);
	g_raid5_post_sync = EVENTHANDLER_REGISTER(shutdown_post_sync,
	    g_raid5_shutdown, mp, SHUTDOWN_PRI_FIRST);
	if (g_raid5_post_sync == NULL)
		G_RAID5_DEBUG(1, "WARNING: cannot register shutdown event.");
	else
		G_RAID5_DEBUG(1,"registered shutdown event handler.");
}

static void
g_raid5_fini(struct g_class *mp)
{
	if (g_raid5_post_sync != NULL) {
		G_RAID5_DEBUG(1,"deregister shutdown event handler.");
		EVENTHANDLER_DEREGISTER(shutdown_post_sync, g_raid5_post_sync);
		g_raid5_post_sync = NULL;
	}
}

DECLARE_GEOM_CLASS(g_raid5_class, g_raid5);
