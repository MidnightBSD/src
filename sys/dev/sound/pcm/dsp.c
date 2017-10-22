/*-
 * Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
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

#include <dev/sound/pcm/sound.h>
#include <sys/ctype.h>

SND_DECLARE_FILE("$FreeBSD: release/7.0.0/sys/dev/sound/pcm/dsp.c 171204 2007-07-04 12:33:11Z ariff $");

static int dsp_mmap_allow_prot_exec = 0;
SYSCTL_INT(_hw_snd, OID_AUTO, compat_linux_mmap, CTLFLAG_RW,
    &dsp_mmap_allow_prot_exec, 0, "linux mmap compatibility");

struct dsp_cdevinfo {
	struct pcm_channel *rdch, *wrch;
	int busy, simplex;
	TAILQ_ENTRY(dsp_cdevinfo) link;
};

#define PCM_RDCH(x)		(((struct dsp_cdevinfo *)(x)->si_drv1)->rdch)
#define PCM_WRCH(x)		(((struct dsp_cdevinfo *)(x)->si_drv1)->wrch)
#define PCM_SIMPLEX(x)		(((struct dsp_cdevinfo *)(x)->si_drv1)->simplex)

#define DSP_CDEVINFO_CACHESIZE	8

#define DSP_REGISTERED(x, y)	(PCM_REGISTERED(x) &&			\
				 (y) != NULL && (y)->si_drv1 != NULL)

#define OLDPCM_IOCTL

static d_open_t dsp_open;
static d_close_t dsp_close;
static d_read_t dsp_read;
static d_write_t dsp_write;
static d_ioctl_t dsp_ioctl;
static d_poll_t dsp_poll;
static d_mmap_t dsp_mmap;

struct cdevsw dsp_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	dsp_open,
	.d_close =	dsp_close,
	.d_read =	dsp_read,
	.d_write =	dsp_write,
	.d_ioctl =	dsp_ioctl,
	.d_poll =	dsp_poll,
	.d_mmap =	dsp_mmap,
	.d_name =	"dsp",
};

#ifdef USING_DEVFS
static eventhandler_tag dsp_ehtag = NULL;
static int dsp_umax = -1;
static int dsp_cmax = -1;
#endif

static int dsp_oss_syncgroup(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_syncgroup *group);
static int dsp_oss_syncstart(int sg_id);
static int dsp_oss_policy(struct pcm_channel *wrch, struct pcm_channel *rdch, int policy);
#ifdef OSSV4_EXPERIMENT
static int dsp_oss_cookedmode(struct pcm_channel *wrch, struct pcm_channel *rdch, int enabled);
static int dsp_oss_getchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_setchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map);
static int dsp_oss_getlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_setlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label);
static int dsp_oss_getsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song);
static int dsp_oss_setname(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *name);
#endif

static struct snddev_info *
dsp_get_info(struct cdev *dev)
{
	return (devclass_get_softc(pcm_devclass, PCMUNIT(dev)));
}

static uint32_t
dsp_get_flags(struct cdev *dev)
{
	device_t bdev;

	bdev = devclass_get_device(pcm_devclass, PCMUNIT(dev));

	return ((bdev != NULL) ? pcm_getflags(bdev) : 0xffffffff);
}

static void
dsp_set_flags(struct cdev *dev, uint32_t flags)
{
	device_t bdev;

	bdev = devclass_get_device(pcm_devclass, PCMUNIT(dev));

	if (bdev != NULL)
		pcm_setflags(bdev, flags);
}

/*
 * return the channels associated with an open device instance.
 * lock channels specified.
 */
static int
getchns(struct cdev *dev, struct pcm_channel **rdch, struct pcm_channel **wrch,
    uint32_t prio)
{
	struct snddev_info *d;
	struct pcm_channel *ch;
	uint32_t flags;

	if (PCM_SIMPLEX(dev) != 0) {
		d = dsp_get_info(dev);
		if (!PCM_REGISTERED(d))
			return (ENXIO);
		pcm_lock(d);
		PCM_WAIT(d);
		PCM_ACQUIRE(d);
		/*
		 * Note: order is important -
		 *       pcm flags -> prio query flags -> wild guess
		 */
		ch = NULL;
		flags = dsp_get_flags(dev);
		if (flags & SD_F_PRIO_WR) {
			ch = PCM_RDCH(dev);
			PCM_RDCH(dev) = NULL;
		} else if (flags & SD_F_PRIO_RD) {
			ch = PCM_WRCH(dev);
			PCM_WRCH(dev) = NULL;
		} else if (prio & SD_F_PRIO_WR) {
			ch = PCM_RDCH(dev);
			PCM_RDCH(dev) = NULL;
			flags |= SD_F_PRIO_WR;
		} else if (prio & SD_F_PRIO_RD) {
			ch = PCM_WRCH(dev);
			PCM_WRCH(dev) = NULL;
			flags |= SD_F_PRIO_RD;
		} else if (PCM_WRCH(dev) != NULL) {
			ch = PCM_RDCH(dev);
			PCM_RDCH(dev) = NULL;
			flags |= SD_F_PRIO_WR;
		} else if (PCM_RDCH(dev) != NULL) {
			ch = PCM_WRCH(dev);
			PCM_WRCH(dev) = NULL;
			flags |= SD_F_PRIO_RD;
		}
		PCM_SIMPLEX(dev) = 0;
		dsp_set_flags(dev, flags);
		if (ch != NULL) {
			CHN_LOCK(ch);
			pcm_chnref(ch, -1);
			pcm_chnrelease(ch);
		}
		PCM_RELEASE(d);
		pcm_unlock(d);
	}

	*rdch = PCM_RDCH(dev);
	*wrch = PCM_WRCH(dev);

	if (*rdch != NULL && (prio & SD_F_PRIO_RD))
		CHN_LOCK(*rdch);
	if (*wrch != NULL && (prio & SD_F_PRIO_WR))
		CHN_LOCK(*wrch);

	return (0);
}

/* unlock specified channels */
static void
relchns(struct cdev *dev, struct pcm_channel *rdch, struct pcm_channel *wrch,
    uint32_t prio)
{
	if (wrch != NULL && (prio & SD_F_PRIO_WR))
		CHN_UNLOCK(wrch);
	if (rdch != NULL && (prio & SD_F_PRIO_RD))
		CHN_UNLOCK(rdch);
}

static void
dsp_cdevinfo_alloc(struct cdev *dev,
    struct pcm_channel *rdch, struct pcm_channel *wrch)
{
	struct snddev_info *d;
	struct dsp_cdevinfo *cdi;
	int simplex;

	d = dsp_get_info(dev);

	KASSERT(PCM_REGISTERED(d) && dev != NULL && dev->si_drv1 == NULL &&
	    rdch != wrch,
	    ("bogus %s(), what are you trying to accomplish here?", __func__));
	PCM_BUSYASSERT(d);
	mtx_assert(d->lock, MA_OWNED);

	simplex = (dsp_get_flags(dev) & SD_F_SIMPLEX) ? 1 : 0;

	/*
	 * Scan for free instance entry and put it into the end of list.
	 * Create new one if necessary.
	 */
	TAILQ_FOREACH(cdi, &d->dsp_cdevinfo_pool, link) {
		if (cdi->busy != 0)
			break;
		cdi->rdch = rdch;
		cdi->wrch = wrch;
		cdi->simplex = simplex;
		cdi->busy = 1;
		TAILQ_REMOVE(&d->dsp_cdevinfo_pool, cdi, link);
		TAILQ_INSERT_TAIL(&d->dsp_cdevinfo_pool, cdi, link);
		dev->si_drv1 = cdi;
		return;
	}
	pcm_unlock(d);
	cdi = malloc(sizeof(*cdi), M_DEVBUF, M_WAITOK | M_ZERO);
	pcm_lock(d);
	cdi->rdch = rdch;
	cdi->wrch = wrch;
	cdi->simplex = simplex;
	cdi->busy = 1;
	TAILQ_INSERT_TAIL(&d->dsp_cdevinfo_pool, cdi, link);
	dev->si_drv1 = cdi;
}

static void
dsp_cdevinfo_free(struct cdev *dev)
{
	struct snddev_info *d;
	struct dsp_cdevinfo *cdi, *tmp;
	uint32_t flags;
	int i;

	d = dsp_get_info(dev);

	KASSERT(PCM_REGISTERED(d) && dev != NULL && dev->si_drv1 != NULL &&
	    PCM_RDCH(dev) == NULL && PCM_WRCH(dev) == NULL,
	    ("bogus %s(), what are you trying to accomplish here?", __func__));
	PCM_BUSYASSERT(d);
	mtx_assert(d->lock, MA_OWNED);

	cdi = dev->si_drv1;
	dev->si_drv1 = NULL;
	cdi->rdch = NULL;
	cdi->wrch = NULL;
	cdi->simplex = 0;
	cdi->busy = 0;

	/*
	 * Once it is free, move it back to the beginning of list for
	 * faster new entry allocation.
	 */
	TAILQ_REMOVE(&d->dsp_cdevinfo_pool, cdi, link);
	TAILQ_INSERT_HEAD(&d->dsp_cdevinfo_pool, cdi, link);

	/*
	 * Scan the list, cache free entries up to DSP_CDEVINFO_CACHESIZE.
	 * Reset simplex flags.
	 */
	flags = dsp_get_flags(dev) & ~SD_F_PRIO_SET;
	i = DSP_CDEVINFO_CACHESIZE;
	TAILQ_FOREACH_SAFE(cdi, &d->dsp_cdevinfo_pool, link, tmp) {
		if (cdi->busy != 0) {
			if (cdi->simplex == 0) {
				if (cdi->rdch != NULL)
					flags |= SD_F_PRIO_RD;
				if (cdi->wrch != NULL)
					flags |= SD_F_PRIO_WR;
			}
		} else {
			if (i == 0) {
				TAILQ_REMOVE(&d->dsp_cdevinfo_pool, cdi, link);
				free(cdi, M_DEVBUF);
			} else
				i--;
		}
	}
	dsp_set_flags(dev, flags);
}

void
dsp_cdevinfo_init(struct snddev_info *d)
{
	struct dsp_cdevinfo *cdi;
	int i;

	KASSERT(d != NULL, ("NULL snddev_info"));
	PCM_BUSYASSERT(d);
	mtx_assert(d->lock, MA_NOTOWNED);

	TAILQ_INIT(&d->dsp_cdevinfo_pool);
	for (i = 0; i < DSP_CDEVINFO_CACHESIZE; i++) {
		cdi = malloc(sizeof(*cdi), M_DEVBUF, M_WAITOK | M_ZERO);
		TAILQ_INSERT_HEAD(&d->dsp_cdevinfo_pool, cdi, link);
	}
}

void
dsp_cdevinfo_flush(struct snddev_info *d)
{
	struct dsp_cdevinfo *cdi, *tmp;

	KASSERT(d != NULL, ("NULL snddev_info"));
	PCM_BUSYASSERT(d);
	mtx_assert(d->lock, MA_NOTOWNED);

	cdi = TAILQ_FIRST(&d->dsp_cdevinfo_pool);
	while (cdi != NULL) {
		tmp = TAILQ_NEXT(cdi, link);
		free(cdi, M_DEVBUF);
		cdi = tmp;
	}
	TAILQ_INIT(&d->dsp_cdevinfo_pool);
}

/* duplex / simplex cdev type */
enum {
	DSP_CDEV_TYPE_RDONLY,		/* simplex read-only (record)   */
	DSP_CDEV_TYPE_WRONLY,		/* simplex write-only (play)    */
	DSP_CDEV_TYPE_RDWR,		/* duplex read, write, or both  */
};

#define DSP_F_VALID(x)		((x) & (FREAD | FWRITE))
#define DSP_F_DUPLEX(x)		(((x) & (FREAD | FWRITE)) == (FREAD | FWRITE))
#define DSP_F_SIMPLEX(x)	(!DSP_F_DUPLEX(x))
#define DSP_F_READ(x)		((x) & FREAD)
#define DSP_F_WRITE(x)		((x) & FWRITE)

static const struct {
	int type;
	char *name;
	char *sep;
	int use_sep;
	int hw;
	int max;
	uint32_t fmt, spd;
	int query;
} dsp_cdevs[] = {
	{ SND_DEV_DSP,         "dsp",    ".", 0, 0, 0,
	    AFMT_U8,       DSP_DEFAULT_SPEED, DSP_CDEV_TYPE_RDWR   },
	{ SND_DEV_AUDIO,       "audio",  ".", 0, 0, 0,
	    AFMT_MU_LAW,   DSP_DEFAULT_SPEED, DSP_CDEV_TYPE_RDWR   },
	{ SND_DEV_DSP16,       "dspW",   ".", 0, 0, 0,
	    AFMT_S16_LE,   DSP_DEFAULT_SPEED, DSP_CDEV_TYPE_RDWR   },
	{ SND_DEV_DSPHW_PLAY,  "dsp",   ".p", 1, 1, SND_MAXHWCHAN,
	    AFMT_S16_LE | AFMT_STEREO, 48000, DSP_CDEV_TYPE_WRONLY },
	{ SND_DEV_DSPHW_VPLAY, "dsp",  ".vp", 1, 1, SND_MAXVCHANS,
	    AFMT_S16_LE | AFMT_STEREO, 48000, DSP_CDEV_TYPE_WRONLY },
	{ SND_DEV_DSPHW_REC,   "dsp",   ".r", 1, 1, SND_MAXHWCHAN,
	    AFMT_S16_LE | AFMT_STEREO, 48000, DSP_CDEV_TYPE_RDONLY },
	{ SND_DEV_DSPHW_VREC,  "dsp",  ".vr", 1, 1, SND_MAXVCHANS,
	    AFMT_S16_LE | AFMT_STEREO, 48000, DSP_CDEV_TYPE_RDONLY },
	{ SND_DEV_DSPHW_CD,    "dspcd",  ".", 0, 0, 0,
	    AFMT_S16_LE | AFMT_STEREO, 44100, DSP_CDEV_TYPE_RDWR   },
	{ SND_DEV_DSP_MMAP, "dsp_mmap",  ".", 0, 0, 0,
	    AFMT_S16_LE | AFMT_STEREO, 48000, DSP_CDEV_TYPE_RDWR   },
};

#define DSP_FIXUP_ERROR()		do {				\
	prio = dsp_get_flags(i_dev);					\
	if (!DSP_F_VALID(flags))					\
		error = EINVAL;						\
	if (!DSP_F_DUPLEX(flags) &&					\
	    ((DSP_F_READ(flags) && d->reccount == 0) ||			\
	    (DSP_F_WRITE(flags) && d->playcount == 0)))			\
		error = ENOTSUP;					\
	else if (!DSP_F_DUPLEX(flags) && (prio & SD_F_SIMPLEX) &&	\
	    ((DSP_F_READ(flags) && (prio & SD_F_PRIO_WR)) ||		\
	    (DSP_F_WRITE(flags) && (prio & SD_F_PRIO_RD))))		\
		error = EBUSY;						\
	else if (DSP_REGISTERED(d, i_dev))				\
		error = EBUSY;						\
} while(0)

static int
dsp_open(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	uint32_t fmt, spd, prio;
	int i, error, rderror, wrerror, devtype, wdevunit, rdevunit;

	/* Kind of impossible.. */
	if (i_dev == NULL || td == NULL)
		return (ENODEV);

	d = dsp_get_info(i_dev);
	if (!PCM_REGISTERED(d))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	/* Lock snddev so nobody else can monkey with it. */
	pcm_lock(d);
	PCM_WAIT(d);

	/*
	 * Try to acquire cloned device before someone else pick it.
	 * ENODEV means this is not a cloned droids.
	 */
	error = snd_clone_acquire(i_dev);
	if (!(error == 0 || error == ENODEV)) {
		DSP_FIXUP_ERROR();
		pcm_unlock(d);
		PCM_GIANT_EXIT(d);
		return (error);
	}

	error = 0;
	DSP_FIXUP_ERROR();

	if (error != 0) {
		(void)snd_clone_release(i_dev);
		pcm_unlock(d);
		PCM_GIANT_EXIT(d);
		return (error);
	}

	/*
	 * That is just enough. Acquire and unlock pcm lock so
	 * the other will just have to wait until we finish doing
	 * everything.
	 */
	PCM_ACQUIRE(d);
	pcm_unlock(d);

	devtype = PCMDEV(i_dev);
	wdevunit = -1;
	rdevunit = -1;
	fmt = 0;
	spd = 0;

	for (i = 0; i < (sizeof(dsp_cdevs) / sizeof(dsp_cdevs[0])); i++) {
		if (devtype != dsp_cdevs[i].type)
			continue;
		if (DSP_F_SIMPLEX(flags) &&
		    ((dsp_cdevs[i].query == DSP_CDEV_TYPE_WRONLY &&
		    DSP_F_READ(flags)) ||
		    (dsp_cdevs[i].query == DSP_CDEV_TYPE_RDONLY &&
		    DSP_F_WRITE(flags)))) {
			/*
			 * simplex, opposite direction? Please be gone..
			 */
			(void)snd_clone_release(i_dev);
			PCM_RELEASE_QUICK(d);
			PCM_GIANT_EXIT(d);
			return (ENOTSUP);
		}
		if (dsp_cdevs[i].query == DSP_CDEV_TYPE_WRONLY)
			wdevunit = dev2unit(i_dev);
		else if (dsp_cdevs[i].query == DSP_CDEV_TYPE_RDONLY)
			rdevunit = dev2unit(i_dev);
		fmt = dsp_cdevs[i].fmt;
		spd = dsp_cdevs[i].spd;
		break;
	}

	/* No matching devtype? */
	if (fmt == 0 || spd == 0)
		panic("impossible devtype %d", devtype);

	rdch = NULL;
	wrch = NULL;
	rderror = 0;
	wrerror = 0;

	/*
	 * if we get here, the open request is valid- either:
	 *   * we were previously not open
	 *   * we were open for play xor record and the opener wants
	 *     the non-open direction
	 */
	if (DSP_F_READ(flags)) {
		/* open for read */
		rderror = pcm_chnalloc(d, &rdch, PCMDIR_REC,
		    td->td_proc->p_pid, rdevunit);

		if (rderror == 0 && (chn_reset(rdch, fmt) != 0 ||
		    (chn_setspeed(rdch, spd) != 0)))
			rderror = ENXIO;

		if (rderror != 0) {
			if (rdch != NULL)
				pcm_chnrelease(rdch);
			if (!DSP_F_DUPLEX(flags)) {
				(void)snd_clone_release(i_dev);
				PCM_RELEASE_QUICK(d);
				PCM_GIANT_EXIT(d);
				return (rderror);
			}
			rdch = NULL;
		} else {
			if (flags & O_NONBLOCK)
				rdch->flags |= CHN_F_NBIO;
			pcm_chnref(rdch, 1);
		 	CHN_UNLOCK(rdch);
		}
	}

	if (DSP_F_WRITE(flags)) {
		/* open for write */
		wrerror = pcm_chnalloc(d, &wrch, PCMDIR_PLAY,
		    td->td_proc->p_pid, wdevunit);

		if (wrerror == 0 && (chn_reset(wrch, fmt) != 0 ||
		    (chn_setspeed(wrch, spd) != 0)))
			wrerror = ENXIO;

		if (wrerror != 0) {
			if (wrch != NULL)
				pcm_chnrelease(wrch);
			if (!DSP_F_DUPLEX(flags)) {
				if (rdch != NULL) {
					/*
					 * Lock, deref and release previously
					 * created record channel
					 */
					CHN_LOCK(rdch);
					pcm_chnref(rdch, -1);
					pcm_chnrelease(rdch);
				}
				(void)snd_clone_release(i_dev);
				PCM_RELEASE_QUICK(d);
				PCM_GIANT_EXIT(d);
				return (wrerror);
			}
			wrch = NULL;
		} else {
			if (flags & O_NONBLOCK)
				wrch->flags |= CHN_F_NBIO;
			pcm_chnref(wrch, 1);
			CHN_UNLOCK(wrch);
		}
	}

	if (rdch == NULL && wrch == NULL) {
		(void)snd_clone_release(i_dev);
		PCM_RELEASE_QUICK(d);
		PCM_GIANT_EXIT(d);
		return ((wrerror != 0) ? wrerror : rderror);
	}

	pcm_lock(d);

	/*
	 * We're done. Allocate channels information for this cdev.
	 */
	dsp_cdevinfo_alloc(i_dev, rdch, wrch);

	/*
	 * Increase clone refcount for its automatic garbage collector.
	 */
	(void)snd_clone_ref(i_dev);

	PCM_RELEASE(d);
	pcm_unlock(d);

	PCM_GIANT_LEAVE(d);

	return (0);
}

static int
dsp_close(struct cdev *i_dev, int flags, int mode, struct thread *td)
{
	struct pcm_channel *rdch, *wrch;
	struct snddev_info *d;
	int sg_ids, refs;

	d = dsp_get_info(i_dev);
	if (!DSP_REGISTERED(d, i_dev))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	pcm_lock(d);
	PCM_WAIT(d);

	rdch = PCM_RDCH(i_dev);
	wrch = PCM_WRCH(i_dev);

	if (rdch || wrch) {
		PCM_ACQUIRE(d);
		pcm_unlock(d);

		refs = 0;
		if (rdch) {
			/*
			 * The channel itself need not be locked because:
			 *   a)  Adding a channel to a syncgroup happens only in dsp_ioctl(),
			 *       which cannot run concurrently to dsp_close().
			 *   b)  The syncmember pointer (sm) is protected by the global
			 *       syncgroup list lock.
			 *   c)  A channel can't just disappear, invalidating pointers,
			 *       unless it's closed/dereferenced first.
			 */
			PCM_SG_LOCK();
			sg_ids = chn_syncdestroy(rdch);
			PCM_SG_UNLOCK();
			if (sg_ids != 0)
				free_unr(pcmsg_unrhdr, sg_ids);

			CHN_LOCK(rdch);
			refs += pcm_chnref(rdch, -1);
			chn_abort(rdch); /* won't sleep */
			rdch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
			chn_reset(rdch, 0);
			pcm_chnrelease(rdch);
			PCM_RDCH(i_dev) = NULL;
		}
		if (wrch) {
			/*
			 * Please see block above.
			 */
			PCM_SG_LOCK();
			sg_ids = chn_syncdestroy(wrch);
			PCM_SG_UNLOCK();
			if (sg_ids != 0)
				free_unr(pcmsg_unrhdr, sg_ids);

			CHN_LOCK(wrch);
			refs += pcm_chnref(wrch, -1);
			chn_flush(wrch); /* may sleep */
			wrch->flags &= ~(CHN_F_RUNNING | CHN_F_MAPPED | CHN_F_DEAD);
			chn_reset(wrch, 0);
			pcm_chnrelease(wrch);
			PCM_WRCH(i_dev) = NULL;
		}

		pcm_lock(d);
		/*
		 * If there are no more references, release the channels.
		 */
		if (refs == 0 && PCM_RDCH(i_dev) == NULL &&
		    PCM_WRCH(i_dev) == NULL) {
			dsp_cdevinfo_free(i_dev);
			/*
			 * Release clone busy state and unref it
			 * so the automatic garbage collector will
			 * get the hint and do the remaining cleanup
			 * process.
			 */
			(void)snd_clone_release(i_dev);
			(void)snd_clone_unref(i_dev);
		}
		PCM_RELEASE(d);
	}

	pcm_unlock(d);

	PCM_GIANT_LEAVE(d);

	return (0);
}

static __inline int
dsp_io_ops(struct cdev *i_dev, struct uio *buf)
{
	struct snddev_info *d;
	struct pcm_channel **ch, *rdch, *wrch;
	int (*chn_io)(struct pcm_channel *, struct uio *);
	int prio, ret;
	pid_t runpid;

	KASSERT(i_dev != NULL && buf != NULL &&
	    (buf->uio_rw == UIO_READ || buf->uio_rw == UIO_WRITE),
	    ("%s(): io train wreck!", __func__));

	d = dsp_get_info(i_dev);
	if (!DSP_REGISTERED(d, i_dev))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	switch (buf->uio_rw) {
	case UIO_READ:
		prio = SD_F_PRIO_RD;
		ch = &rdch;
		chn_io = chn_read;
		break;
	case UIO_WRITE:
		prio = SD_F_PRIO_WR;
		ch = &wrch;
		chn_io = chn_write;
		break;
	default:
		panic("invalid/corrupted uio direction: %d", buf->uio_rw);
		break;
	}

	rdch = NULL;
	wrch = NULL;
	runpid = buf->uio_td->td_proc->p_pid;

	getchns(i_dev, &rdch, &wrch, prio);

	if (*ch == NULL || !((*ch)->flags & CHN_F_BUSY)) {
		PCM_GIANT_EXIT(d);
		return (EBADF);
	}

	if (((*ch)->flags & (CHN_F_MAPPED | CHN_F_DEAD)) ||
	    (((*ch)->flags & CHN_F_RUNNING) && (*ch)->pid != runpid)) {
		relchns(i_dev, rdch, wrch, prio);
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	} else if (!((*ch)->flags & CHN_F_RUNNING)) {
		(*ch)->flags |= CHN_F_RUNNING;
		(*ch)->pid = runpid;
	}

	/*
	 * chn_read/write must give up channel lock in order to copy bytes
	 * from/to userland, so up the "in progress" counter to make sure
	 * someone else doesn't come along and muss up the buffer.
	 */
	++(*ch)->inprog;
	ret = chn_io(*ch, buf);
	--(*ch)->inprog;

	CHN_BROADCAST(&(*ch)->cv);

	relchns(i_dev, rdch, wrch, prio);

	PCM_GIANT_LEAVE(d);

	return (ret);
}

static int
dsp_read(struct cdev *i_dev, struct uio *buf, int flag)
{
	return (dsp_io_ops(i_dev, buf));
}

static int
dsp_write(struct cdev *i_dev, struct uio *buf, int flag)
{
	return (dsp_io_ops(i_dev, buf));
}

static int
dsp_ioctl(struct cdev *i_dev, u_long cmd, caddr_t arg, int mode, struct thread *td)
{
    	struct pcm_channel *chn, *rdch, *wrch;
	struct snddev_info *d;
	int *arg_i, ret, kill, tmp, xcmd;

	d = dsp_get_info(i_dev);
	if (!DSP_REGISTERED(d, i_dev))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	arg_i = (int *)arg;
	ret = 0;
	xcmd = 0;

	/*
	 * this is an evil hack to allow broken apps to perform mixer ioctls
	 * on dsp devices.
	 */
	if (IOCGROUP(cmd) == 'M') {
		/*
		 * This is at least, a bug to bug compatible with OSS.
		 */
		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, cmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = EBADF;

		PCM_GIANT_EXIT(d);

		return (ret);
	}

	/*
	 * Certain ioctls may be made on any type of device (audio, mixer,
	 * and MIDI).  Handle those special cases here.
	 */
	if (IOCGROUP(cmd) == 'X') {
		PCM_ACQUIRE_QUICK(d);
		switch(cmd) {
		case SNDCTL_SYSINFO:
			sound_oss_sysinfo((oss_sysinfo *)arg);
			break;
		case SNDCTL_AUDIOINFO:
			ret = dsp_oss_audioinfo(i_dev, (oss_audioinfo *)arg);
			break;
		case SNDCTL_MIXERINFO:
			ret = mixer_oss_mixerinfo(i_dev, (oss_mixerinfo *)arg);
			break;
		default:
			ret = EINVAL;
		}
		PCM_RELEASE_QUICK(d);
		PCM_GIANT_EXIT(d);
		return (ret);
	}

	getchns(i_dev, &rdch, &wrch, 0);

	kill = 0;
	if (wrch && (wrch->flags & CHN_F_DEAD))
		kill |= 1;
	if (rdch && (rdch->flags & CHN_F_DEAD))
		kill |= 2;
	if (kill == 3) {
		relchns(i_dev, rdch, wrch, 0);
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	}
	if (kill & 1)
		wrch = NULL;
	if (kill & 2)
		rdch = NULL;

	if (wrch == NULL && rdch == NULL) {
		relchns(i_dev, rdch, wrch, 0);
		PCM_GIANT_EXIT(d);
		return (EINVAL);
	}

    	switch(cmd) {
#ifdef OLDPCM_IOCTL
    	/*
     	 * we start with the new ioctl interface.
     	 */
    	case AIONWRITE:	/* how many bytes can write ? */
		if (wrch) {
			CHN_LOCK(wrch);
/*
		if (wrch && wrch->bufhard.dl)
			while (chn_wrfeed(wrch) == 0);
*/
			*arg_i = sndbuf_getfree(wrch->bufsoft);
			CHN_UNLOCK(wrch);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case AIOSSIZE:     /* set the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;

			p->play_size = 0;
			p->rec_size = 0;
			PCM_ACQUIRE_QUICK(d);
	    		if (wrch) {
				CHN_LOCK(wrch);
				chn_setblocksize(wrch, 2, p->play_size);
				p->play_size = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			}
	    		if (rdch) {
				CHN_LOCK(rdch);
				chn_setblocksize(rdch, 2, p->rec_size);
				p->rec_size = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
		}
		break;
    	case AIOGSIZE:	/* get the current blocksize */
		{
	    		struct snd_size *p = (struct snd_size *)arg;

	    		if (wrch) {
				CHN_LOCK(wrch);
				p->play_size = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			}
	    		if (rdch) {
				CHN_LOCK(rdch);
				p->rec_size = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			}
		}
		break;

    	case AIOSFMT:
    	case AIOGFMT:
		{
	    		snd_chan_param *p = (snd_chan_param *)arg;

			if (cmd == AIOSFMT &&
			    ((p->play_format != 0 && p->play_rate == 0) ||
			    (p->rec_format != 0 && p->rec_rate == 0))) {
				ret = EINVAL;
				break;
			}
			PCM_ACQUIRE_QUICK(d);
	    		if (wrch) {
				CHN_LOCK(wrch);
				if (cmd == AIOSFMT && p->play_format != 0) {
					chn_setformat(wrch, p->play_format);
					chn_setspeed(wrch, p->play_rate);
				}
	    			p->play_rate = wrch->speed;
	    			p->play_format = wrch->format;
				CHN_UNLOCK(wrch);
			} else {
	    			p->play_rate = 0;
	    			p->play_format = 0;
	    		}
	    		if (rdch) {
				CHN_LOCK(rdch);
				if (cmd == AIOSFMT && p->rec_format != 0) {
					chn_setformat(rdch, p->rec_format);
					chn_setspeed(rdch, p->rec_rate);
				}
				p->rec_rate = rdch->speed;
				p->rec_format = rdch->format;
				CHN_UNLOCK(rdch);
			} else {
	    			p->rec_rate = 0;
	    			p->rec_format = 0;
	    		}
			PCM_RELEASE_QUICK(d);
		}
		break;

    	case AIOGCAP:     /* get capabilities */
		{
	    		snd_capabilities *p = (snd_capabilities *)arg;
			struct pcmchan_caps *pcaps = NULL, *rcaps = NULL;
			struct cdev *pdev;

			pcm_lock(d);
			if (rdch) {
				CHN_LOCK(rdch);
				rcaps = chn_getcaps(rdch);
			}
			if (wrch) {
				CHN_LOCK(wrch);
				pcaps = chn_getcaps(wrch);
			}
	    		p->rate_min = max(rcaps? rcaps->minspeed : 0,
	                      		  pcaps? pcaps->minspeed : 0);
	    		p->rate_max = min(rcaps? rcaps->maxspeed : 1000000,
	                      		  pcaps? pcaps->maxspeed : 1000000);
	    		p->bufsize = min(rdch? sndbuf_getsize(rdch->bufsoft) : 1000000,
	                     		 wrch? sndbuf_getsize(wrch->bufsoft) : 1000000);
			/* XXX bad on sb16 */
	    		p->formats = (rdch? chn_getformats(rdch) : 0xffffffff) &
			 	     (wrch? chn_getformats(wrch) : 0xffffffff);
			if (rdch && wrch)
				p->formats |= (dsp_get_flags(i_dev) & SD_F_SIMPLEX)? 0 : AFMT_FULLDUPLEX;
			pdev = d->mixer_dev;
	    		p->mixers = 1; /* default: one mixer */
	    		p->inputs = pdev->si_drv1? mix_getdevs(pdev->si_drv1) : 0;
	    		p->left = p->right = 100;
			if (wrch)
				CHN_UNLOCK(wrch);
			if (rdch)
				CHN_UNLOCK(rdch);
			pcm_unlock(d);
		}
		break;

    	case AIOSTOP:
		if (*arg_i == AIOSYNC_PLAY && wrch) {
			CHN_LOCK(wrch);
			*arg_i = chn_abort(wrch);
			CHN_UNLOCK(wrch);
		} else if (*arg_i == AIOSYNC_CAPTURE && rdch) {
			CHN_LOCK(rdch);
			*arg_i = chn_abort(rdch);
			CHN_UNLOCK(rdch);
		} else {
	   	 	printf("AIOSTOP: bad channel 0x%x\n", *arg_i);
	    		*arg_i = 0;
		}
		break;

    	case AIOSYNC:
		printf("AIOSYNC chan 0x%03lx pos %lu unimplemented\n",
	    		((snd_sync_parm *)arg)->chan, ((snd_sync_parm *)arg)->pos);
		break;
#endif
	/*
	 * here follow the standard ioctls (filio.h etc.)
	 */
    	case FIONREAD: /* get # bytes to read */
		if (rdch) {
			CHN_LOCK(rdch);
/*			if (rdch && rdch->bufhard.dl)
				while (chn_rdfeed(rdch) == 0);
*/
			*arg_i = sndbuf_getready(rdch->bufsoft);
			CHN_UNLOCK(rdch);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case FIOASYNC: /*set/clear async i/o */
		DEB( printf("FIOASYNC\n") ; )
		break;

    	case SNDCTL_DSP_NONBLOCK: /* set non-blocking i/o */
    	case FIONBIO: /* set/clear non-blocking i/o */
		if (rdch) {
			CHN_LOCK(rdch);
			if (cmd == SNDCTL_DSP_NONBLOCK || *arg_i)
				rdch->flags |= CHN_F_NBIO;
			else
				rdch->flags &= ~CHN_F_NBIO;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			if (cmd == SNDCTL_DSP_NONBLOCK || *arg_i)
				wrch->flags |= CHN_F_NBIO;
			else
				wrch->flags &= ~CHN_F_NBIO;
			CHN_UNLOCK(wrch);
		}
		break;

    	/*
	 * Finally, here is the linux-compatible ioctl interface
	 */
#define THE_REAL_SNDCTL_DSP_GETBLKSIZE _IOWR('P', 4, int)
    	case THE_REAL_SNDCTL_DSP_GETBLKSIZE:
    	case SNDCTL_DSP_GETBLKSIZE:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = sndbuf_getblksz(chn->bufsoft);
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETBLKSIZE:
		RANGE(*arg_i, 16, 65536);
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			chn_setblocksize(wrch, 2, *arg_i);
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			chn_setblocksize(rdch, 2, *arg_i);
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		break;

    	case SNDCTL_DSP_RESET:
		DEB(printf("dsp reset\n"));
		if (wrch) {
			CHN_LOCK(wrch);
			chn_abort(wrch);
			chn_resetbuf(wrch);
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			chn_abort(rdch);
			chn_resetbuf(rdch);
			CHN_UNLOCK(rdch);
		}
		break;

    	case SNDCTL_DSP_SYNC:
		DEB(printf("dsp sync\n"));
		/* chn_sync may sleep */
		if (wrch) {
			CHN_LOCK(wrch);
			chn_sync(wrch, 0);
			CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_SPEED:
		/* chn_setspeed may sleep */
		tmp = 0;
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			ret = chn_setspeed(wrch, *arg_i);
			tmp = wrch->speed;
			CHN_UNLOCK(wrch);
		}
		if (rdch && ret == 0) {
			CHN_LOCK(rdch);
			ret = chn_setspeed(rdch, *arg_i);
			if (tmp == 0)
				tmp = rdch->speed;
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		*arg_i = tmp;
		break;

    	case SOUND_PCM_READ_RATE:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = chn->speed;
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_STEREO:
		tmp = -1;
		*arg_i = (*arg_i)? AFMT_STEREO : 0;
		PCM_ACQUIRE_QUICK(d);
		if (wrch) {
			CHN_LOCK(wrch);
			ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) | *arg_i);
			tmp = (wrch->format & AFMT_STEREO)? 1 : 0;
			CHN_UNLOCK(wrch);
		}
		if (rdch && ret == 0) {
			CHN_LOCK(rdch);
			ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) | *arg_i);
			if (tmp == -1)
				tmp = (rdch->format & AFMT_STEREO)? 1 : 0;
			CHN_UNLOCK(rdch);
		}
		PCM_RELEASE_QUICK(d);
		*arg_i = tmp;
		break;

    	case SOUND_PCM_WRITE_CHANNELS:
/*	case SNDCTL_DSP_CHANNELS: ( == SOUND_PCM_WRITE_CHANNELS) */
		if (*arg_i != 0) {
			tmp = 0;
			*arg_i = (*arg_i != 1)? AFMT_STEREO : 0;
			PCM_ACQUIRE_QUICK(d);
	  		if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch, (wrch->format & ~AFMT_STEREO) | *arg_i);
				tmp = (wrch->format & AFMT_STEREO)? 2 : 1;
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch, (rdch->format & ~AFMT_STEREO) | *arg_i);
				if (tmp == 0)
					tmp = (rdch->format & AFMT_STEREO)? 2 : 1;
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
			*arg_i = tmp;
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = (chn->format & AFMT_STEREO) ? 2 : 1;
			CHN_UNLOCK(chn);
		}
		break;

    	case SOUND_PCM_READ_CHANNELS:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = (chn->format & AFMT_STEREO) ? 2 : 1;
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETFMTS:	/* returns a mask of supported fmts */
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			*arg_i = chn_getformats(chn);
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETFMT:	/* sets _one_ format */
		if ((*arg_i != AFMT_QUERY)) {
			tmp = 0;
			PCM_ACQUIRE_QUICK(d);
			if (wrch) {
				CHN_LOCK(wrch);
				ret = chn_setformat(wrch, (*arg_i) | (wrch->format & AFMT_STEREO));
				tmp = wrch->format & ~AFMT_STEREO;
				CHN_UNLOCK(wrch);
			}
			if (rdch && ret == 0) {
				CHN_LOCK(rdch);
				ret = chn_setformat(rdch, (*arg_i) | (rdch->format & AFMT_STEREO));
				if (tmp == 0)
					tmp = rdch->format & ~AFMT_STEREO;
				CHN_UNLOCK(rdch);
			}
			PCM_RELEASE_QUICK(d);
			*arg_i = tmp;
		} else {
			chn = wrch ? wrch : rdch;
			CHN_LOCK(chn);
			*arg_i = chn->format & ~AFMT_STEREO;
			CHN_UNLOCK(chn);
		}
		break;

    	case SNDCTL_DSP_SETFRAGMENT:
		DEB(printf("SNDCTL_DSP_SETFRAGMENT 0x%08x\n", *(int *)arg));
		{
			uint32_t fragln = (*arg_i) & 0x0000ffff;
			uint32_t maxfrags = ((*arg_i) & 0xffff0000) >> 16;
			uint32_t fragsz;
			uint32_t r_maxfrags, r_fragsz;

			RANGE(fragln, 4, 16);
			fragsz = 1 << fragln;

			if (maxfrags == 0)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;
			if (maxfrags < 2)
				maxfrags = 2;
			if (maxfrags * fragsz > CHN_2NDBUFMAXSIZE)
				maxfrags = CHN_2NDBUFMAXSIZE / fragsz;

			DEB(printf("SNDCTL_DSP_SETFRAGMENT %d frags, %d sz\n", maxfrags, fragsz));
			PCM_ACQUIRE_QUICK(d);
		    	if (rdch) {
				CHN_LOCK(rdch);
				ret = chn_setblocksize(rdch, maxfrags, fragsz);
				r_maxfrags = sndbuf_getblkcnt(rdch->bufsoft);
				r_fragsz = sndbuf_getblksz(rdch->bufsoft);
				CHN_UNLOCK(rdch);
			} else {
				r_maxfrags = maxfrags;
				r_fragsz = fragsz;
			}
		    	if (wrch && ret == 0) {
				CHN_LOCK(wrch);
				ret = chn_setblocksize(wrch, maxfrags, fragsz);
 				maxfrags = sndbuf_getblkcnt(wrch->bufsoft);
				fragsz = sndbuf_getblksz(wrch->bufsoft);
				CHN_UNLOCK(wrch);
			} else { /* use whatever came from the read channel */
				maxfrags = r_maxfrags;
				fragsz = r_fragsz;
			}
			PCM_RELEASE_QUICK(d);

			fragln = 0;
			while (fragsz > 1) {
				fragln++;
				fragsz >>= 1;
			}
	    		*arg_i = (maxfrags << 16) | fragln;
		}
		break;

    	case SNDCTL_DSP_GETISPACE:
		/* return the size of data available in the input queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (rdch) {
	        		struct snd_dbuf *bs = rdch->bufsoft;

				CHN_LOCK(rdch);
				a->bytes = sndbuf_getready(bs);
	        		a->fragments = a->bytes / sndbuf_getblksz(bs);
	        		a->fragstotal = sndbuf_getblkcnt(bs);
	        		a->fragsize = sndbuf_getblksz(bs);
				CHN_UNLOCK(rdch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETOSPACE:
		/* return space available in the output queue */
		{
	    		audio_buf_info *a = (audio_buf_info *)arg;
	    		if (wrch) {
	        		struct snd_dbuf *bs = wrch->bufsoft;

				CHN_LOCK(wrch);
				/* XXX abusive DMA update: chn_wrupdate(wrch); */
				a->bytes = sndbuf_getfree(bs);
	        		a->fragments = a->bytes / sndbuf_getblksz(bs);
	        		a->fragstotal = sndbuf_getblkcnt(bs);
	        		a->fragsize = sndbuf_getblksz(bs);
				CHN_UNLOCK(wrch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETIPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (rdch) {
	        		struct snd_dbuf *bs = rdch->bufsoft;

				CHN_LOCK(rdch);
				/* XXX abusive DMA update: chn_rdupdate(rdch); */
	        		a->bytes = sndbuf_gettotal(bs);
	        		a->blocks = sndbuf_getblocks(bs) - rdch->blocks;
	        		a->ptr = sndbuf_getreadyptr(bs);
				rdch->blocks = sndbuf_getblocks(bs);
				CHN_UNLOCK(rdch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETOPTR:
		{
	    		count_info *a = (count_info *)arg;
	    		if (wrch) {
	        		struct snd_dbuf *bs = wrch->bufsoft;

				CHN_LOCK(wrch);
				/* XXX abusive DMA update: chn_wrupdate(wrch); */
	        		a->bytes = sndbuf_gettotal(bs);
	        		a->blocks = sndbuf_getblocks(bs) - wrch->blocks;
	        		a->ptr = sndbuf_getreadyptr(bs);
				wrch->blocks = sndbuf_getblocks(bs);
				CHN_UNLOCK(wrch);
	    		} else
				ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_GETCAPS:
		pcm_lock(d);
		*arg_i = DSP_CAP_REALTIME | DSP_CAP_MMAP | DSP_CAP_TRIGGER;
		if (rdch && wrch && !(dsp_get_flags(i_dev) & SD_F_SIMPLEX))
			*arg_i |= DSP_CAP_DUPLEX;
		pcm_unlock(d);
		break;

    	case SOUND_PCM_READ_BITS:
		chn = wrch ? wrch : rdch;
		if (chn) {
			CHN_LOCK(chn);
			if (chn->format & AFMT_8BIT)
				*arg_i = 8;
			else if (chn->format & AFMT_16BIT)
				*arg_i = 16;
			else if (chn->format & AFMT_24BIT)
				*arg_i = 24;
			else if (chn->format & AFMT_32BIT)
				*arg_i = 32;
			else
				ret = EINVAL;
			CHN_UNLOCK(chn);
		} else {
			*arg_i = 0;
			ret = EINVAL;
		}
		break;

    	case SNDCTL_DSP_SETTRIGGER:
		if (rdch) {
			CHN_LOCK(rdch);
			rdch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_INPUT)
				chn_start(rdch, 1);
			else
				rdch->flags |= CHN_F_NOTRIGGER;
			CHN_UNLOCK(rdch);
		}
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~(CHN_F_TRIGGERED | CHN_F_NOTRIGGER);
		    	if (*arg_i & PCM_ENABLE_OUTPUT)
				chn_start(wrch, 1);
			else
				wrch->flags |= CHN_F_NOTRIGGER;
			CHN_UNLOCK(wrch);
		}
		break;

    	case SNDCTL_DSP_GETTRIGGER:
		*arg_i = 0;
		if (wrch) {
			CHN_LOCK(wrch);
			if (wrch->flags & CHN_F_TRIGGERED)
				*arg_i |= PCM_ENABLE_OUTPUT;
			CHN_UNLOCK(wrch);
		}
		if (rdch) {
			CHN_LOCK(rdch);
			if (rdch->flags & CHN_F_TRIGGERED)
				*arg_i |= PCM_ENABLE_INPUT;
			CHN_UNLOCK(rdch);
		}
		break;

	case SNDCTL_DSP_GETODELAY:
		if (wrch) {
	        	struct snd_dbuf *bs = wrch->bufsoft;

			CHN_LOCK(wrch);
			/* XXX abusive DMA update: chn_wrupdate(wrch); */
			*arg_i = sndbuf_getready(bs);
			CHN_UNLOCK(wrch);
		} else
			ret = EINVAL;
		break;

    	case SNDCTL_DSP_POST:
		if (wrch) {
			CHN_LOCK(wrch);
			wrch->flags &= ~CHN_F_NOTRIGGER;
			chn_start(wrch, 1);
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_SETDUPLEX:
		/*
		 * switch to full-duplex mode if card is in half-duplex
		 * mode and is able to work in full-duplex mode
		 */
		pcm_lock(d);
		if (rdch && wrch && (dsp_get_flags(i_dev) & SD_F_SIMPLEX))
			dsp_set_flags(i_dev, dsp_get_flags(i_dev)^SD_F_SIMPLEX);
		pcm_unlock(d);
		break;

	/*
	 * The following four ioctls are simple wrappers around mixer_ioctl
	 * with no further processing.  xcmd is short for "translated
	 * command".
	 */
	case SNDCTL_DSP_GETRECVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_READ_RECLEV;
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETRECVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_WRITE_RECLEV;
		/* FALLTHROUGH */
	case SNDCTL_DSP_GETPLAYVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_READ_PCM;
		/* FALLTHROUGH */
	case SNDCTL_DSP_SETPLAYVOL:
		if (xcmd == 0)
			xcmd = SOUND_MIXER_WRITE_PCM;

		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, xcmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = ENOTSUP;
		break;

	case SNDCTL_DSP_GET_RECSRC_NAMES:
	case SNDCTL_DSP_GET_RECSRC:
	case SNDCTL_DSP_SET_RECSRC:
		if (d->mixer_dev != NULL) {
			PCM_ACQUIRE_QUICK(d);
			ret = mixer_ioctl_cmd(d->mixer_dev, cmd, arg, -1, td,
			    MIXER_CMD_DIRECT);
			PCM_RELEASE_QUICK(d);
		} else
			ret = ENOTSUP;
		break;

	/*
	 * The following 3 ioctls aren't very useful at the moment.  For
	 * now, only a single channel is associated with a cdev (/dev/dspN
	 * instance), so there's only a single output routing to use (i.e.,
	 * the wrch bound to this cdev).
	 */
	case SNDCTL_DSP_GET_PLAYTGT_NAMES:
		{
			oss_mixer_enuminfo *ei;
			ei = (oss_mixer_enuminfo *)arg;
			ei->dev = 0;
			ei->ctrl = 0;
			ei->version = 0; /* static for now */
			ei->strindex[0] = 0;

			if (wrch != NULL) {
				ei->nvalues = 1;
				strlcpy(ei->strings, wrch->name,
					sizeof(ei->strings));
			} else {
				ei->nvalues = 0;
				ei->strings[0] = '\0';
			}
		}
		break;
	case SNDCTL_DSP_GET_PLAYTGT:
	case SNDCTL_DSP_SET_PLAYTGT:	/* yes, they are the same for now */
		/*
		 * Re: SET_PLAYTGT
		 *   OSSv4: "The value that was accepted by the device will
		 *   be returned back in the variable pointed by the
		 *   argument."
		 */
		if (wrch != NULL)
			*arg_i = 0;
		else
			ret = EINVAL;
		break;

	case SNDCTL_DSP_SILENCE:
	/*
	 * Flush the software (pre-feed) buffer, but try to minimize playback
	 * interruption.  (I.e., record unplayed samples with intent to
	 * restore by SNDCTL_DSP_SKIP.) Intended for application "pause"
	 * functionality.
	 */
		if (wrch == NULL)
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			CHN_LOCK(wrch);
			while (wrch->inprog != 0)
				cv_wait(&wrch->cv, wrch->lock);
			bs = wrch->bufsoft;
			if ((bs->shadbuf != NULL) && (sndbuf_getready(bs) > 0)) {
				bs->sl = sndbuf_getready(bs);
				sndbuf_dispose(bs, bs->shadbuf, sndbuf_getready(bs));
				sndbuf_fillsilence(bs);
				chn_start(wrch, 0);
			}
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_SKIP:
	/*
	 * OSSv4 docs: "This ioctl call discards all unplayed samples in the
	 * playback buffer by moving the current write position immediately
	 * before the point where the device is currently reading the samples."
	 */
		if (wrch == NULL)
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			CHN_LOCK(wrch);
			while (wrch->inprog != 0)
				cv_wait(&wrch->cv, wrch->lock);
			bs = wrch->bufsoft;
			if ((bs->shadbuf != NULL) && (bs->sl > 0)) {
				sndbuf_softreset(bs);
				sndbuf_acquire(bs, bs->shadbuf, bs->sl);
				bs->sl = 0;
				chn_start(wrch, 0);
			}
			CHN_UNLOCK(wrch);
		}
		break;

	case SNDCTL_DSP_CURRENT_OPTR:
	case SNDCTL_DSP_CURRENT_IPTR:
	/**
	 * @note Changing formats resets the buffer counters, which differs
	 * 	 from the 4Front drivers.  However, I don't expect this to be
	 * 	 much of a problem.
	 *
	 * @note In a test where @c CURRENT_OPTR is called immediately after write
	 * 	 returns, this driver is about 32K samples behind whereas
	 * 	 4Front's is about 8K samples behind.  Should determine source
	 * 	 of discrepancy, even if only out of curiosity.
	 *
	 * @todo Actually test SNDCTL_DSP_CURRENT_IPTR.
	 */
		chn = (cmd == SNDCTL_DSP_CURRENT_OPTR) ? wrch : rdch;
		if (chn == NULL) 
			ret = EINVAL;
		else {
			struct snd_dbuf *bs;
			/* int tmp; */

			oss_count_t *oc = (oss_count_t *)arg;

			CHN_LOCK(chn);
			bs = chn->bufsoft;
#if 0
			tmp = (sndbuf_getsize(b) + chn_getptr(chn) - sndbuf_gethwptr(b)) % sndbuf_getsize(b);
			oc->samples = (sndbuf_gettotal(b) + tmp) / sndbuf_getbps(b);
			oc->fifo_samples = (sndbuf_getready(b) - tmp) / sndbuf_getbps(b);
#else
			oc->samples = sndbuf_gettotal(bs) / sndbuf_getbps(bs);
			oc->fifo_samples = sndbuf_getready(bs) / sndbuf_getbps(bs);
#endif
			CHN_UNLOCK(chn);
		}
		break;

	case SNDCTL_DSP_HALT_OUTPUT:
	case SNDCTL_DSP_HALT_INPUT:
		chn = (cmd == SNDCTL_DSP_HALT_OUTPUT) ? wrch : rdch;
		if (chn == NULL)
			ret = EINVAL;
		else {
			CHN_LOCK(chn);
			chn_abort(chn);
			CHN_UNLOCK(chn);
		}
		break;

	case SNDCTL_DSP_LOW_WATER:
	/*
	 * Set the number of bytes required to attract attention by
	 * select/poll.
	 */
		if (wrch != NULL) {
			CHN_LOCK(wrch);
			wrch->lw = (*arg_i > 1) ? *arg_i : 1;
			CHN_UNLOCK(wrch);
		}
		if (rdch != NULL) {
			CHN_LOCK(rdch);
			rdch->lw = (*arg_i > 1) ? *arg_i : 1;
			CHN_UNLOCK(rdch);
		}
		break;

	case SNDCTL_DSP_GETERROR:
	/*
	 * OSSv4 docs:  "All errors and counters will automatically be
	 * cleared to zeroes after the call so each call will return only
	 * the errors that occurred after the previous invocation. ... The
	 * play_underruns and rec_overrun fields are the only usefull fields
	 * returned by OSS 4.0."
	 */
		{
			audio_errinfo *ei = (audio_errinfo *)arg;

			bzero((void *)ei, sizeof(*ei));

			if (wrch != NULL) {
				CHN_LOCK(wrch);
				ei->play_underruns = wrch->xruns;
				wrch->xruns = 0;
				CHN_UNLOCK(wrch);
			}
			if (rdch != NULL) {
				CHN_LOCK(rdch);
				ei->rec_overruns = rdch->xruns;
				rdch->xruns = 0;
				CHN_UNLOCK(rdch);
			}
		}
		break;

	case SNDCTL_DSP_SYNCGROUP:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_syncgroup(wrch, rdch, (oss_syncgroup *)arg);
		PCM_RELEASE_QUICK(d);
		break;

	case SNDCTL_DSP_SYNCSTART:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_syncstart(*arg_i);
		PCM_RELEASE_QUICK(d);
		break;

	case SNDCTL_DSP_POLICY:
		PCM_ACQUIRE_QUICK(d);
		ret = dsp_oss_policy(wrch, rdch, *arg_i);
		PCM_RELEASE_QUICK(d);
		break;

#ifdef	OSSV4_EXPERIMENT
	/*
	 * XXX The following ioctls are not yet supported and just return
	 * EINVAL.
	 */
	case SNDCTL_DSP_GETOPEAKS:
	case SNDCTL_DSP_GETIPEAKS:
		chn = (cmd == SNDCTL_DSP_GETOPEAKS) ? wrch : rdch;
		if (chn == NULL)
			ret = EINVAL;
		else {
			oss_peaks_t *op = (oss_peaks_t *)arg;
			int lpeak, rpeak;

			CHN_LOCK(chn);
			ret = chn_getpeaks(chn, &lpeak, &rpeak);
			if (ret == -1)
				ret = EINVAL;
			else {
				(*op)[0] = lpeak;
				(*op)[1] = rpeak;
			}
			CHN_UNLOCK(chn);
		}
		break;

	/*
	 * XXX Once implemented, revisit this for proper cv protection
	 *     (if necessary).
	 */
	case SNDCTL_DSP_COOKEDMODE:
		ret = dsp_oss_cookedmode(wrch, rdch, *arg_i);
		break;
	case SNDCTL_DSP_GET_CHNORDER:
		ret = dsp_oss_getchnorder(wrch, rdch, (unsigned long long *)arg);
		break;
	case SNDCTL_DSP_SET_CHNORDER:
		ret = dsp_oss_setchnorder(wrch, rdch, (unsigned long long *)arg);
		break;
	case SNDCTL_GETLABEL:
		ret = dsp_oss_getlabel(wrch, rdch, (oss_label_t *)arg);
		break;
	case SNDCTL_SETLABEL:
		ret = dsp_oss_setlabel(wrch, rdch, (oss_label_t *)arg);
		break;
	case SNDCTL_GETSONG:
		ret = dsp_oss_getsong(wrch, rdch, (oss_longname_t *)arg);
		break;
	case SNDCTL_SETSONG:
		ret = dsp_oss_setsong(wrch, rdch, (oss_longname_t *)arg);
		break;
	case SNDCTL_SETNAME:
		ret = dsp_oss_setname(wrch, rdch, (oss_longname_t *)arg);
		break;
#if 0
	/**
	 * @note The SNDCTL_CARDINFO ioctl was omitted per 4Front developer
	 * documentation.  "The usability of this call is very limited. It's
	 * provided only for completeness of the API. OSS API doesn't have
	 * any concept of card. Any information returned by this ioctl calld
	 * is reserved exclusively for the utility programs included in the
	 * OSS package. Applications should not try to use for this
	 * information in any ways."
	 */
	case SNDCTL_CARDINFO:
		ret = EINVAL;
		break;
	/**
	 * @note The S/PDIF interface ioctls, @c SNDCTL_DSP_READCTL and
	 * @c SNDCTL_DSP_WRITECTL have been omitted at the suggestion of
	 * 4Front Technologies.
	 */
	case SNDCTL_DSP_READCTL:
	case SNDCTL_DSP_WRITECTL:
		ret = EINVAL;
		break;
#endif	/* !0 (explicitly omitted ioctls) */

#endif	/* !OSSV4_EXPERIMENT */
    	case SNDCTL_DSP_MAPINBUF:
    	case SNDCTL_DSP_MAPOUTBUF:
    	case SNDCTL_DSP_SETSYNCRO:
		/* undocumented */

    	case SNDCTL_DSP_SUBDIVIDE:
    	case SOUND_PCM_WRITE_FILTER:
    	case SOUND_PCM_READ_FILTER:
		/* dunno what these do, don't sound important */

    	default:
		DEB(printf("default ioctl fn 0x%08lx fail\n", cmd));
		ret = EINVAL;
		break;
    	}

	relchns(i_dev, rdch, wrch, 0);

	PCM_GIANT_LEAVE(d);

    	return (ret);
}

static int
dsp_poll(struct cdev *i_dev, int events, struct thread *td)
{
	struct snddev_info *d;
	struct pcm_channel *wrch, *rdch;
	int ret, e;

	d = dsp_get_info(i_dev);
	if (!DSP_REGISTERED(d, i_dev))
		return (EBADF);

	PCM_GIANT_ENTER(d);

	wrch = NULL;
	rdch = NULL;
	ret = 0;

	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	if (wrch != NULL && !(wrch->flags & CHN_F_DEAD)) {
		e = (events & (POLLOUT | POLLWRNORM));
		if (e)
			ret |= chn_poll(wrch, e, td);
	}

	if (rdch != NULL && !(rdch->flags & CHN_F_DEAD)) {
		e = (events & (POLLIN | POLLRDNORM));
		if (e)
			ret |= chn_poll(rdch, e, td);
	}

	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	PCM_GIANT_LEAVE(d);

	return (ret);
}

static int
dsp_mmap(struct cdev *i_dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	struct snddev_info *d;
	struct pcm_channel *wrch, *rdch, *c;

	/*
	 * Reject PROT_EXEC by default. It just doesn't makes sense.
	 * Unfortunately, we have to give up this one due to linux_mmap
	 * changes.
	 *
	 * http://lists.freebsd.org/pipermail/freebsd-emulation/2007-June/003698.html
	 *
	 */
	if ((nprot & PROT_EXEC) && dsp_mmap_allow_prot_exec == 0)
		return (-1);

	d = dsp_get_info(i_dev);
	if (!DSP_REGISTERED(d, i_dev))
		return (-1);

	PCM_GIANT_ENTER(d);

	getchns(i_dev, &rdch, &wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	/*
	 * XXX The linux api uses the nprot to select read/write buffer
	 *     our vm system doesn't allow this, so force write buffer.
	 *
	 *     This is just a quack to fool full-duplex mmap, so that at
	 *     least playback _or_ recording works. If you really got the
	 *     urge to make _both_ work at the same time, avoid O_RDWR.
	 *     Just open each direction separately and mmap() it.
	 *
	 *     Failure is not an option due to INVARIANTS check within
	 *     device_pager.c, which means, we have to give up one over
	 *     another.
	 */
	c = (wrch != NULL) ? wrch : rdch;

	if (c == NULL || (c->flags & CHN_F_MMAP_INVALID) ||
	    offset >= sndbuf_getsize(c->bufsoft) ||
	    (wrch != NULL && (wrch->flags & CHN_F_MMAP_INVALID)) ||
	    (rdch != NULL && (rdch->flags & CHN_F_MMAP_INVALID))) {
		relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);
		PCM_GIANT_EXIT(d);
		return (-1);
	}

	/* XXX full-duplex quack. */
	if (wrch != NULL)
		wrch->flags |= CHN_F_MAPPED;
	if (rdch != NULL)
		rdch->flags |= CHN_F_MAPPED;

	*paddr = vtophys(sndbuf_getbufofs(c->bufsoft, offset));
	relchns(i_dev, rdch, wrch, SD_F_PRIO_RD | SD_F_PRIO_WR);

	PCM_GIANT_LEAVE(d);

	return (0);
}

#ifdef USING_DEVFS

/* So much for dev_stdclone() */
static int
dsp_stdclone(char *name, char *namep, char *sep, int use_sep, int *u, int *c)
{
	size_t len;

	len = strlen(namep);

	if (bcmp(name, namep, len) != 0)
		return (ENODEV);

	name += len;

	if (isdigit(*name) == 0)
		return (ENODEV);

	len = strlen(sep);

	if (*name == '0' && !(name[1] == '\0' || bcmp(name + 1, sep, len) == 0))
		return (ENODEV);

	for (*u = 0; isdigit(*name) != 0; name++) {
		*u *= 10;
		*u += *name - '0';
		if (*u > dsp_umax)
			return (ENODEV);
	}

	if (*name == '\0')
		return ((use_sep == 0) ? 0 : ENODEV);

	if (bcmp(name, sep, len) != 0 || isdigit(name[len]) == 0)
		return (ENODEV);

	name += len;

	if (*name == '0' && name[1] != '\0')
		return (ENODEV);

	for (*c = 0; isdigit(*name) != 0; name++) {
		*c *= 10;
		*c += *name - '0';
		if (*c > dsp_cmax)
			return (ENODEV);
	}

	if (*name != '\0')
		return (ENODEV);

	return (0);
}

static void
dsp_clone(void *arg,
#if __FreeBSD_version >= 600034
    struct ucred *cred,
#endif
    char *name, int namelen, struct cdev **dev)
{
	struct snddev_info *d;
	struct snd_clone_entry *ce;
	struct pcm_channel *c;
	int i, unit, udcmask, cunit, devtype, devhw, devcmax, tumax;
	char *devname, *devsep;

	KASSERT(dsp_umax >= 0 && dsp_cmax >= 0, ("Uninitialized unit!"));

	if (*dev != NULL)
		return;

	unit = -1;
	cunit = -1;
	devtype = -1;
	devhw = 0;
	devcmax = -1;
	tumax = -1;
	devname = NULL;
	devsep = NULL;

	for (i = 0; unit == -1 &&
	    i < (sizeof(dsp_cdevs) / sizeof(dsp_cdevs[0])); i++) {
		devtype = dsp_cdevs[i].type;
		devname = dsp_cdevs[i].name;
		devsep = dsp_cdevs[i].sep;
		devhw = dsp_cdevs[i].hw;
		devcmax = dsp_cdevs[i].max - 1;
		if (strcmp(name, devname) == 0)
			unit = snd_unit;
		else if (dsp_stdclone(name, devname, devsep,
		    dsp_cdevs[i].use_sep, &unit, &cunit) != 0) {
			unit = -1;
			cunit = -1;
		}
	}

	d = devclass_get_softc(pcm_devclass, unit);
	if (!PCM_REGISTERED(d) || d->clones == NULL)
		return;

	/* XXX Need Giant magic entry ??? */

	pcm_lock(d);
	if (snd_clone_disabled(d->clones)) {
		pcm_unlock(d);
		return;
	}

	PCM_WAIT(d);
	PCM_ACQUIRE(d);
	pcm_unlock(d);

	udcmask = snd_u2unit(unit) | snd_d2unit(devtype);

	if (devhw != 0) {
		KASSERT(devcmax <= dsp_cmax,
		    ("overflow: devcmax=%d, dsp_cmax=%d", devcmax, dsp_cmax));
		if (cunit > devcmax) {
			PCM_RELEASE_QUICK(d);
			return;
		}
		udcmask |= snd_c2unit(cunit);
		CHN_FOREACH(c, d, channels.pcm) {
			CHN_LOCK(c);
			if (c->unit != udcmask) {
				CHN_UNLOCK(c);
				continue;
			}
			CHN_UNLOCK(c);
			udcmask &= ~snd_c2unit(cunit);
			/*
			 * Temporarily increase clone maxunit to overcome
			 * vchan flexibility.
			 *
			 * # sysctl dev.pcm.0.play.vchans=256
			 * dev.pcm.0.play.vchans: 1 -> 256
			 * # cat /dev/zero > /dev/dsp0.vp255 &
			 * [1] 17296
			 * # sysctl dev.pcm.0.play.vchans=0
			 * dev.pcm.0.play.vchans: 256 -> 1
			 * # fg
			 * [1]  + running    cat /dev/zero > /dev/dsp0.vp255
			 * ^C
			 * # cat /dev/zero > /dev/dsp0.vp255
			 * zsh: operation not supported: /dev/dsp0.vp255
			 */
			tumax = snd_clone_getmaxunit(d->clones);
			if (cunit > tumax)
				snd_clone_setmaxunit(d->clones, cunit);
			else
				tumax = -1;
			goto dsp_clone_alloc;
		}
		/*
		 * Ok, so we're requesting unallocated vchan, but still
		 * within maximum vchan limit.
		 */
		if (((devtype == SND_DEV_DSPHW_VPLAY && d->pvchancount > 0) ||
		    (devtype == SND_DEV_DSPHW_VREC && d->rvchancount > 0)) &&
		    cunit < snd_maxautovchans) {
			udcmask &= ~snd_c2unit(cunit);
			tumax = snd_clone_getmaxunit(d->clones);
			if (cunit > tumax)
				snd_clone_setmaxunit(d->clones, cunit);
			else
				tumax = -1;
			goto dsp_clone_alloc;
		}
		PCM_RELEASE_QUICK(d);
		return;
	}

dsp_clone_alloc:
	ce = snd_clone_alloc(d->clones, dev, &cunit, udcmask);
	if (tumax != -1)
		snd_clone_setmaxunit(d->clones, tumax);
	if (ce != NULL) {
		udcmask |= snd_c2unit(cunit);
		*dev = make_dev(&dsp_cdevsw, unit2minor(udcmask),
		    UID_ROOT, GID_WHEEL, 0666, "%s%d%s%d",
		    devname, unit, devsep, cunit);
		snd_clone_register(ce, *dev);
	}

	PCM_RELEASE_QUICK(d);

	if (*dev != NULL)
		dev_ref(*dev);
}

static void
dsp_sysinit(void *p)
{
	if (dsp_ehtag != NULL)
		return;
	/* initialize unit numbering */
	snd_unit_init();
	dsp_umax = PCMMAXUNIT;
	dsp_cmax = PCMMAXCHAN;
	dsp_ehtag = EVENTHANDLER_REGISTER(dev_clone, dsp_clone, 0, 1000);
}

static void
dsp_sysuninit(void *p)
{
	if (dsp_ehtag == NULL)
		return;
	EVENTHANDLER_DEREGISTER(dev_clone, dsp_ehtag);
	dsp_ehtag = NULL;
}

SYSINIT(dsp_sysinit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysinit, NULL);
SYSUNINIT(dsp_sysuninit, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, dsp_sysuninit, NULL);
#endif

char *
dsp_unit2name(char *buf, size_t len, int unit)
{
	int i, dtype;

	KASSERT(buf != NULL && len != 0,
	    ("bogus buf=%p len=%ju", buf, (uintmax_t)len));

	dtype = snd_unit2d(unit);

	for (i = 0; i < (sizeof(dsp_cdevs) / sizeof(dsp_cdevs[0])); i++) {
		if (dtype != dsp_cdevs[i].type)
			continue;
		snprintf(buf, len, "%s%d%s%d", dsp_cdevs[i].name,
		    snd_unit2u(unit), dsp_cdevs[i].sep, snd_unit2c(unit));
		return (buf);
	}

	return (NULL);
}

/**
 * @brief Handler for SNDCTL_AUDIOINFO.
 *
 * Gathers information about the audio device specified in ai->dev.  If
 * ai->dev == -1, then this function gathers information about the current
 * device.  If the call comes in on a non-audio device and ai->dev == -1,
 * return EINVAL.
 *
 * This routine is supposed to go practically straight to the hardware,
 * getting capabilities directly from the sound card driver, side-stepping
 * the intermediate channel interface.
 *
 * Note, however, that the usefulness of this command is significantly
 * decreased when requesting info about any device other than the one serving
 * the request. While each snddev_channel refers to a specific device node,
 * the converse is *not* true.  Currently, when a sound device node is opened,
 * the sound subsystem scans for an available audio channel (or channels, if
 * opened in read+write) and then assigns them to the si_drv[12] private
 * data fields.  As a result, any information returned linking a channel to
 * a specific character device isn't necessarily accurate.
 *
 * @note
 * Calling threads must not hold any snddev_info or pcm_channel locks.
 * 
 * @param dev		device on which the ioctl was issued
 * @param ai		ioctl request data container
 *
 * @retval 0		success
 * @retval EINVAL	ai->dev specifies an invalid device
 *
 * @todo Verify correctness of Doxygen tags.  ;)
 */
int
dsp_oss_audioinfo(struct cdev *i_dev, oss_audioinfo *ai)
{
	struct pcmchan_caps *caps;
	struct pcm_channel *ch;
	struct snddev_info *d;
	uint32_t fmts;
	int i, nchan, *rates, minch, maxch;
	char *devname, buf[CHN_NAMELEN];

	/*
	 * If probing the device that received the ioctl, make sure it's a
	 * DSP device.  (Users may use this ioctl with /dev/mixer and
	 * /dev/midi.)
	 */
	if (ai->dev == -1 && i_dev->si_devsw != &dsp_cdevsw)
		return (EINVAL);

	ch = NULL;
	devname = NULL;
	nchan = 0;
	bzero(buf, sizeof(buf));

	/*
	 * Search for the requested audio device (channel).  Start by
	 * iterating over pcm devices.
	 */ 
	for (i = 0; pcm_devclass != NULL &&
	    i < devclass_get_maxunit(pcm_devclass); i++) {
		d = devclass_get_softc(pcm_devclass, i);
		if (!PCM_REGISTERED(d))
			continue;

		/* XXX Need Giant magic entry ??? */

		/* See the note in function docblock */
		mtx_assert(d->lock, MA_NOTOWNED);
		pcm_lock(d);

		CHN_FOREACH(ch, d, channels.pcm) {
			mtx_assert(ch->lock, MA_NOTOWNED);
			CHN_LOCK(ch);
			if (ai->dev == -1) {
				if (DSP_REGISTERED(d, i_dev) &&
				    (ch == PCM_RDCH(i_dev) ||	/* record ch */
				    ch == PCM_WRCH(i_dev))) {	/* playback ch */
					devname = dsp_unit2name(buf,
					    sizeof(buf), ch->unit);
				}
			} else if (ai->dev == nchan) {
				devname = dsp_unit2name(buf, sizeof(buf),
				    ch->unit);
			}
			if (devname != NULL)
				break;
			CHN_UNLOCK(ch);
			++nchan;
		}

		if (devname != NULL) {
			/*
			 * At this point, the following synchronization stuff
			 * has happened:
			 * - a specific PCM device is locked.
			 * - a specific audio channel has been locked, so be
			 *   sure to unlock when exiting;
			 */

			caps = chn_getcaps(ch);

			/*
			 * With all handles collected, zero out the user's
			 * container and begin filling in its fields.
			 */
			bzero((void *)ai, sizeof(oss_audioinfo));

			ai->dev = nchan;
			strlcpy(ai->name, ch->name,  sizeof(ai->name));

			if ((ch->flags & CHN_F_BUSY) == 0)
				ai->busy = 0;
			else
				ai->busy = (ch->direction == PCMDIR_PLAY) ? OPEN_WRITE : OPEN_READ;

			/**
			 * @note
			 * @c cmd - OSSv4 docs: "Only supported under Linux at
			 *    this moment." Cop-out, I know, but I'll save
			 *    running around in the process table for later.
			 *    Is there a risk of leaking information?
			 */
			ai->pid = ch->pid;

			/*
			 * These flags stolen from SNDCTL_DSP_GETCAPS handler.
			 * Note, however, that a single channel operates in
			 * only one direction, so DSP_CAP_DUPLEX is out.
			 */
			/**
			 * @todo @c SNDCTL_AUDIOINFO::caps - Make drivers keep
			 *       these in pcmchan::caps?
			 */
			ai->caps = DSP_CAP_REALTIME | DSP_CAP_MMAP | DSP_CAP_TRIGGER;

			/*
			 * Collect formats supported @b natively by the
			 * device.  Also determine min/max channels.  (I.e.,
			 * mono, stereo, or both?)
			 *
			 * If any channel is stereo, maxch = 2;
			 * if all channels are stereo, minch = 2, too;
			 * if any channel is mono, minch = 1;
			 * and if all channels are mono, maxch = 1.
			 */
			minch = 0;
			maxch = 0;
			fmts = 0;
			for (i = 0; caps->fmtlist[i]; i++) {
				fmts |= caps->fmtlist[i];
				if (caps->fmtlist[i] & AFMT_STEREO) {
					minch = (minch == 0) ? 2 : minch;
					maxch = 2;
				} else {
					minch = 1;
					maxch = (maxch == 0) ? 1 : maxch;
				}
			}

			if (ch->direction == PCMDIR_PLAY)
				ai->oformats = fmts;
			else
				ai->iformats = fmts;

			/**
			 * @note
			 * @c magic - OSSv4 docs: "Reserved for internal use
			 *    by OSS."
			 *
			 * @par
			 * @c card_number - OSSv4 docs: "Number of the sound
			 *    card where this device belongs or -1 if this
			 *    information is not available.  Applications
			 *    should normally not use this field for any
			 *    purpose."
			 */
			ai->card_number = -1;
			/**
			 * @todo @c song_name - depends first on
			 *          SNDCTL_[GS]ETSONG @todo @c label - depends
			 *          on SNDCTL_[GS]ETLABEL
			 * @todo @c port_number - routing information?
			 */
			ai->port_number = -1;
			ai->mixer_dev = (d->mixer_dev != NULL) ? PCMUNIT(d->mixer_dev) : -1;
			/**
			 * @note
			 * @c real_device - OSSv4 docs:  "Obsolete."
			 */
			ai->real_device = -1;
			strlcpy(ai->devnode, devname, sizeof(ai->devnode));
			ai->enabled = device_is_attached(d->dev) ? 1 : 0;
			/**
			 * @note
			 * @c flags - OSSv4 docs: "Reserved for future use."
			 *
			 * @note
			 * @c binding - OSSv4 docs: "Reserved for future use."
			 *
			 * @todo @c handle - haven't decided how to generate
			 *       this yet; bus, vendor, device IDs?
			 */
			ai->min_rate = caps->minspeed;
			ai->max_rate = caps->maxspeed;

			ai->min_channels = minch;
			ai->max_channels = maxch;

			ai->nrates = chn_getrates(ch, &rates);
			if (ai->nrates > OSS_MAX_SAMPLE_RATES)
				ai->nrates = OSS_MAX_SAMPLE_RATES;

			for (i = 0; i < ai->nrates; i++)
				ai->rates[i] = rates[i];

			CHN_UNLOCK(ch);
		}

		pcm_unlock(d);

		if (devname != NULL)
			return (0);
	}

	/* Exhausted the search -- nothing is locked, so return. */
	return (EINVAL);
}

/**
 * @brief Assigns a PCM channel to a sync group.
 *
 * Sync groups are used to enable audio operations on multiple devices
 * simultaneously.  They may be used with any number of devices and may
 * span across applications.  Devices are added to groups with
 * the SNDCTL_DSP_SYNCGROUP ioctl, and operations are triggered with the
 * SNDCTL_DSP_SYNCSTART ioctl.
 *
 * If the @c id field of the @c group parameter is set to zero, then a new
 * sync group is created.  Otherwise, wrch and rdch (if set) are added to
 * the group specified.
 *
 * @todo As far as memory allocation, should we assume that things are
 * 	 okay and allocate with M_WAITOK before acquiring channel locks,
 * 	 freeing later if not?
 *
 * @param wrch	output channel associated w/ device (if any)
 * @param rdch	input channel associated w/ device (if any)
 * @param group Sync group parameters
 *
 * @retval 0		success
 * @retval non-zero	error to be propagated upstream
 */
static int
dsp_oss_syncgroup(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_syncgroup *group)
{
	struct pcmchan_syncmember *smrd, *smwr;
	struct pcmchan_syncgroup *sg;
	int ret, sg_ids[3];

	smrd = NULL;
	smwr = NULL;
	sg = NULL;
	ret = 0;

	/*
	 * Free_unr() may sleep, so store released syncgroup IDs until after
	 * all locks are released.
	 */
	sg_ids[0] = sg_ids[1] = sg_ids[2] = 0;

	PCM_SG_LOCK();

	/*
	 * - Insert channel(s) into group's member list.
	 * - Set CHN_F_NOTRIGGER on channel(s).
	 * - Stop channel(s).  
	 */

	/*
	 * If device's channels are already mapped to a group, unmap them.
	 */
	if (wrch) {
		CHN_LOCK(wrch);
		sg_ids[0] = chn_syncdestroy(wrch);
	}

	if (rdch) {
		CHN_LOCK(rdch);
		sg_ids[1] = chn_syncdestroy(rdch);
	}

	/*
	 * Verify that mode matches character device properites.
	 *  - Bail if PCM_ENABLE_OUTPUT && wrch == NULL.
	 *  - Bail if PCM_ENABLE_INPUT && rdch == NULL.
	 */
	if (((wrch == NULL) && (group->mode & PCM_ENABLE_OUTPUT)) ||
	    ((rdch == NULL) && (group->mode & PCM_ENABLE_INPUT))) {
		ret = EINVAL;
		goto out;
	}

	/*
	 * An id of zero indicates the user wants to create a new
	 * syncgroup.
	 */
	if (group->id == 0) {
		sg = (struct pcmchan_syncgroup *)malloc(sizeof(*sg), M_DEVBUF, M_NOWAIT);
		if (sg != NULL) {
			SLIST_INIT(&sg->members);
			sg->id = alloc_unr(pcmsg_unrhdr);

			group->id = sg->id;
			SLIST_INSERT_HEAD(&snd_pcm_syncgroups, sg, link);
		} else
			ret = ENOMEM;
	} else {
		SLIST_FOREACH(sg, &snd_pcm_syncgroups, link) {
			if (sg->id == group->id)
				break;
		}
		if (sg == NULL)
			ret = EINVAL;
	}

	/* Couldn't create or find a syncgroup.  Fail. */
	if (sg == NULL)
		goto out;

	/*
	 * Allocate a syncmember, assign it and a channel together, and
	 * insert into syncgroup.
	 */
	if (group->mode & PCM_ENABLE_INPUT) {
		smrd = (struct pcmchan_syncmember *)malloc(sizeof(*smrd), M_DEVBUF, M_NOWAIT);
		if (smrd == NULL) {
			ret = ENOMEM;
			goto out;
		}

		SLIST_INSERT_HEAD(&sg->members, smrd, link);
		smrd->parent = sg;
		smrd->ch = rdch;

		chn_abort(rdch);
		rdch->flags |= CHN_F_NOTRIGGER;
		rdch->sm = smrd;
	}

	if (group->mode & PCM_ENABLE_OUTPUT) {
		smwr = (struct pcmchan_syncmember *)malloc(sizeof(*smwr), M_DEVBUF, M_NOWAIT);
		if (smwr == NULL) {
			ret = ENOMEM;
			goto out;
		}

		SLIST_INSERT_HEAD(&sg->members, smwr, link);
		smwr->parent = sg;
		smwr->ch = wrch;

		chn_abort(wrch);
		wrch->flags |= CHN_F_NOTRIGGER;
		wrch->sm = smwr;
	}


out:
	if (ret != 0) {
		if (smrd != NULL)
			free(smrd, M_DEVBUF);
		if ((sg != NULL) && SLIST_EMPTY(&sg->members)) {
			sg_ids[2] = sg->id;
			SLIST_REMOVE(&snd_pcm_syncgroups, sg, pcmchan_syncgroup, link);
			free(sg, M_DEVBUF);
		}

		if (wrch)
			wrch->sm = NULL;
		if (rdch)
			rdch->sm = NULL;
	}

	if (wrch)
		CHN_UNLOCK(wrch);
	if (rdch)
		CHN_UNLOCK(rdch);

	PCM_SG_UNLOCK();

	if (sg_ids[0])
		free_unr(pcmsg_unrhdr, sg_ids[0]);
	if (sg_ids[1])
		free_unr(pcmsg_unrhdr, sg_ids[1]);
	if (sg_ids[2])
		free_unr(pcmsg_unrhdr, sg_ids[2]);

	return (ret);
}

/**
 * @brief Launch a sync group into action
 *
 * Sync groups are established via SNDCTL_DSP_SYNCGROUP.  This function
 * iterates over all members, triggering them along the way.
 *
 * @note Caller must not hold any channel locks.
 *
 * @param sg_id	sync group identifier
 *
 * @retval 0	success
 * @retval non-zero	error worthy of propagating upstream to user
 */
static int
dsp_oss_syncstart(int sg_id)
{
	struct pcmchan_syncmember *sm, *sm_tmp;
	struct pcmchan_syncgroup *sg;
	struct pcm_channel *c;
	int ret, needlocks;

	/* Get the synclists lock */
	PCM_SG_LOCK();

	do {
		ret = 0;
		needlocks = 0;

		/* Search for syncgroup by ID */
		SLIST_FOREACH(sg, &snd_pcm_syncgroups, link) {
			if (sg->id == sg_id)
				break;
		}

		/* Return EINVAL if not found */
		if (sg == NULL) {
			ret = EINVAL;
			break;
		}

		/* Any removals resulting in an empty group should've handled this */
		KASSERT(!SLIST_EMPTY(&sg->members), ("found empty syncgroup"));

		/*
		 * Attempt to lock all member channels - if any are already
		 * locked, unlock those acquired, sleep for a bit, and try
		 * again.
		 */
		SLIST_FOREACH(sm, &sg->members, link) {
			if (CHN_TRYLOCK(sm->ch) == 0) {
				int timo = hz * 5/1000; 
				if (timo < 1)
					timo = 1;

				/* Release all locked channels so far, retry */
				SLIST_FOREACH(sm_tmp, &sg->members, link) {
					/* sm is the member already locked */
					if (sm == sm_tmp)
						break;
					CHN_UNLOCK(sm_tmp->ch);
				}

				/** @todo Is PRIBIO correct/ */
				ret = msleep(sm, &snd_pcm_syncgroups_mtx,
				    PRIBIO | PCATCH, "pcmsg", timo);
				if (ret == EINTR || ret == ERESTART)
					break;

				needlocks = 1;
				ret = 0; /* Assumes ret == EAGAIN... */
			}
		}
	} while (needlocks && ret == 0);

	/* Proceed only if no errors encountered. */
	if (ret == 0) {
		/* Launch channels */
		while((sm = SLIST_FIRST(&sg->members)) != NULL) {
			SLIST_REMOVE_HEAD(&sg->members, link);

			c = sm->ch;
			c->sm = NULL;
			chn_start(c, 1);
			c->flags &= ~CHN_F_NOTRIGGER;
			CHN_UNLOCK(c);

			free(sm, M_DEVBUF);
		}

		SLIST_REMOVE(&snd_pcm_syncgroups, sg, pcmchan_syncgroup, link);
		free(sg, M_DEVBUF);
	}

	PCM_SG_UNLOCK();

	/*
	 * Free_unr() may sleep, so be sure to give up the syncgroup lock
	 * first.
	 */
	if (ret == 0)
		free_unr(pcmsg_unrhdr, sg_id);

	return (ret);
}

/**
 * @brief Handler for SNDCTL_DSP_POLICY
 *
 * The SNDCTL_DSP_POLICY ioctl is a simpler interface to control fragment
 * size and count like with SNDCTL_DSP_SETFRAGMENT.  Instead of the user
 * specifying those two parameters, s/he simply selects a number from 0..10
 * which corresponds to a buffer size.  Smaller numbers request smaller
 * buffers with lower latencies (at greater overhead from more frequent
 * interrupts), while greater numbers behave in the opposite manner.
 *
 * The 4Front spec states that a value of 5 should be the default.  However,
 * this implementation deviates slightly by using a linear scale without
 * consulting drivers.  I.e., even though drivers may have different default
 * buffer sizes, a policy argument of 5 will have the same result across
 * all drivers.
 *
 * See http://manuals.opensound.com/developer/SNDCTL_DSP_POLICY.html for
 * more information.
 *
 * @todo When SNDCTL_DSP_COOKEDMODE is supported, it'll be necessary to
 * 	 work with hardware drivers directly.
 *
 * @note PCM channel arguments must not be locked by caller.
 *
 * @param wrch	Pointer to opened playback channel (optional; may be NULL)
 * @param rdch	" recording channel (optional; may be NULL)
 * @param policy Integer from [0:10]
 *
 * @retval 0	constant (for now)
 */
static int
dsp_oss_policy(struct pcm_channel *wrch, struct pcm_channel *rdch, int policy)
{
	int ret;

	if (policy < CHN_POLICY_MIN || policy > CHN_POLICY_MAX)
		return (EIO);

	/* Default: success */
	ret = 0;

	if (rdch) {
		CHN_LOCK(rdch);
		ret = chn_setlatency(rdch, policy);
		CHN_UNLOCK(rdch);
	}

	if (wrch && ret == 0) {
		CHN_LOCK(wrch);
		ret = chn_setlatency(wrch, policy);
		CHN_UNLOCK(wrch);
	}

	if (ret)
		ret = EIO;

	return (ret);
}

#ifdef OSSV4_EXPERIMENT
/**
 * @brief Enable or disable "cooked" mode
 *
 * This is a handler for @c SNDCTL_DSP_COOKEDMODE.  When in cooked mode, which
 * is the default, the sound system handles rate and format conversions
 * automatically (ex: user writing 11025Hz/8 bit/unsigned but card only
 * operates with 44100Hz/16bit/signed samples).
 *
 * Disabling cooked mode is intended for applications wanting to mmap()
 * a sound card's buffer space directly, bypassing the FreeBSD 2-stage
 * feeder architecture, presumably to gain as much control over audio
 * hardware as possible.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_DSP_COOKEDMODE.html
 * for more details.
 *
 * @note Currently, this function is just a stub that always returns EINVAL.
 *
 * @todo Figure out how to and actually implement this.
 *
 * @param wrch		playback channel (optional; may be NULL)
 * @param rdch		recording channel (optional; may be NULL)
 * @param enabled	0 = raw mode, 1 = cooked mode
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_cookedmode(struct pcm_channel *wrch, struct pcm_channel *rdch, int enabled)
{
	return (EINVAL);
}

/**
 * @brief Retrieve channel interleaving order
 *
 * This is the handler for @c SNDCTL_DSP_GET_CHNORDER.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_DSP_GET_CHNORDER.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_DSP_GET_CHNORDER.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param map	channel map (result will be stored there)
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map)
{
	return (EINVAL);
}

/**
 * @brief Specify channel interleaving order
 *
 * This is the handler for @c SNDCTL_DSP_SET_CHNORDER.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support @c SNDCTL_DSP_SET_CHNORDER.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param map	channel map
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setchnorder(struct pcm_channel *wrch, struct pcm_channel *rdch, unsigned long long *map)
{
	return (EINVAL);
}

/**
 * @brief Retrieve an audio device's label
 *
 * This is a handler for the @c SNDCTL_GETLABEL ioctl.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETLABEL.html
 * for more details.
 *
 * From Hannu@4Front:  "For example ossxmix (just like some HW mixer
 * consoles) can show variable "labels" for certain controls. By default
 * the application name (say quake) is shown as the label but
 * applications may change the labels themselves."
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support @c SNDCTL_GETLABEL.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param label	label gets copied here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label)
{
	return (EINVAL);
}

/**
 * @brief Specify an audio device's label
 *
 * This is a handler for the @c SNDCTL_SETLABEL ioctl.  Please see the
 * comments for @c dsp_oss_getlabel immediately above.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETLABEL.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETLABEL.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param label	label gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setlabel(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_label_t *label)
{
	return (EINVAL);
}

/**
 * @brief Retrieve name of currently played song
 *
 * This is a handler for the @c SNDCTL_GETSONG ioctl.  Audio players could
 * tell the system the name of the currently playing song, which would be
 * visible in @c /dev/sndstat.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_GETSONG.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_GETSONG.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param song	song name gets copied here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_getsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song)
{
	return (EINVAL);
}

/**
 * @brief Retrieve name of currently played song
 *
 * This is a handler for the @c SNDCTL_SETSONG ioctl.  Audio players could
 * tell the system the name of the currently playing song, which would be
 * visible in @c /dev/sndstat.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_SETSONG.html
 * for more details.
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETSONG.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param song	song name gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setsong(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *song)
{
	return (EINVAL);
}

/**
 * @brief Rename a device
 *
 * This is a handler for the @c SNDCTL_SETNAME ioctl.
 *
 * See @c http://manuals.opensound.com/developer/SNDCTL_SETNAME.html for
 * more details.
 *
 * From Hannu@4Front:  "This call is used to change the device name
 * reported in /dev/sndstat and ossinfo. So instead of  using some generic
 * 'OSS loopback audio (MIDI) driver' the device may be given a meaningfull
 * name depending on the current context (for example 'OSS virtual wave table
 * synth' or 'VoIP link to London')."
 *
 * @note As the ioctl definition is still under construction, FreeBSD
 * 	 does not currently support SNDCTL_SETNAME.
 *
 * @param wrch	playback channel (optional; may be NULL)
 * @param rdch	recording channel (optional; may be NULL)
 * @param name	new device name gets copied from here
 *
 * @retval EINVAL	Operation not yet supported.
 */
static int
dsp_oss_setname(struct pcm_channel *wrch, struct pcm_channel *rdch, oss_longname_t *name)
{
	return (EINVAL);
}
#endif	/* !OSSV4_EXPERIMENT */
