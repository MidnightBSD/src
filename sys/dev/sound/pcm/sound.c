/*-
* Copyright (c) 1999 Cameron Grant <cg@freebsd.org>
* (C) 1997 Luigi Rizzo (luigi@iet.unipi.it)
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
#include <dev/sound/pcm/vchan.h>
#include <dev/sound/pcm/dsp.h>
#include <sys/sysctl.h>

#include "feeder_if.h"

SND_DECLARE_FILE("$MidnightBSD$");

devclass_t pcm_devclass;

int pcm_veto_load = 1;

#ifdef USING_DEVFS
int snd_unit = 0;
TUNABLE_INT("hw.snd.unit", &snd_unit);
#endif

int snd_maxautovchans = 0;
TUNABLE_INT("hw.snd.maxautovchans", &snd_maxautovchans);

SYSCTL_NODE(_hw, OID_AUTO, snd, CTLFLAG_RD, 0, "Sound driver");

static int sndstat_prepare_pcm(struct sbuf *s, device_t dev, int verbose);

struct sysctl_ctx_list *
snd_sysctl_tree(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);

return &d->sysctl_tree;
}

struct sysctl_oid *
snd_sysctl_tree_top(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);

return d->sysctl_tree_top;
}

void *
snd_mtxcreate(const char *desc, const char *type)
{
#ifdef USING_MUTEX
struct mtx *m;

m = malloc(sizeof(*m), M_DEVBUF, M_WAITOK | M_ZERO);
if (m == NULL)
return NULL;
mtx_init(m, desc, type, MTX_DEF);
return m;
#else
return (void *)0xcafebabe;
#endif
}

void
snd_mtxfree(void *m)
{
#ifdef USING_MUTEX
struct mtx *mtx = m;

/* mtx_assert(mtx, MA_OWNED); */
mtx_destroy(mtx);
free(mtx, M_DEVBUF);
#endif
}

void
snd_mtxassert(void *m)
{
#ifdef USING_MUTEX
#ifdef INVARIANTS
struct mtx *mtx = m;

mtx_assert(mtx, MA_OWNED);
#endif
#endif
}
/*
void
snd_mtxlock(void *m)
{
#ifdef USING_MUTEX
struct mtx *mtx = m;

mtx_lock(mtx);
#endif
}

void
snd_mtxunlock(void *m)
{
#ifdef USING_MUTEX
struct mtx *mtx = m;

mtx_unlock(mtx);
#endif
}
*/
int
snd_setup_intr(device_t dev, struct resource *res, int flags, driver_intr_t hand, void *param, void **cookiep)
{
#ifdef USING_MUTEX
flags &= INTR_MPSAFE;
flags |= INTR_TYPE_AV;
#else
flags = INTR_TYPE_AV;
#endif
return bus_setup_intr(dev, res, flags, hand, param, cookiep);
}

#ifndef	PCM_DEBUG_MTX
void
pcm_lock(struct snddev_info *d)
{
snd_mtxlock(d->lock);
}

void
pcm_unlock(struct snddev_info *d)
{
snd_mtxunlock(d->lock);
}
#endif

struct pcm_channel *
pcm_getfakechan(struct snddev_info *d)
{
return d->fakechan;
}

static int
pcm_setvchans(struct snddev_info *d, int newcnt)
{
struct snddev_channel *sce = NULL;
struct pcm_channel *c = NULL;
int err = 0, vcnt, dcnt, i;

pcm_inprog(d, 1);

if (!(d->flags & SD_F_AUTOVCHAN)) {
err = EINVAL;
goto setvchans_out;
}

vcnt = d->vchancount;
dcnt = d->playcount + d->reccount;

if (newcnt < 0 || (dcnt + newcnt) > (PCMMAXCHAN + 1)) {
err = E2BIG;
goto setvchans_out;
}

dcnt += vcnt;

if (newcnt > vcnt) {
/* add new vchans - find a parent channel first */
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
CHN_LOCK(c);
if (c->direction == PCMDIR_PLAY &&
((c->flags & CHN_F_HAS_VCHAN) ||
(vcnt == 0 &&
!(c->flags & (CHN_F_BUSY | CHN_F_VIRTUAL)))))
goto addok;
CHN_UNLOCK(c);
}
err = EBUSY;
goto setvchans_out;
addok:
c->flags |= CHN_F_BUSY;
while (err == 0 && newcnt > vcnt) {
if (dcnt > PCMMAXCHAN) {
device_printf(d->dev, "%s: Maximum channel reached.n", __func__);
break;
}
err = vchan_create(c);
if (err == 0) {
vcnt++;
dcnt++;
} else if (err == E2BIG && newcnt > vcnt)
device_printf(d->dev, "%s: err=%d Maximum channel reached.n", __func__, err);
}
if (vcnt == 0)
c->flags &= ~CHN_F_BUSY;
CHN_UNLOCK(c);
} else if (newcnt < vcnt) {
#define ORPHAN_CDEVT(cdevt) 	((cdevt) == NULL || ((cdevt)->si_drv1 == NULL && 	(cdevt)->si_drv2 == NULL))
while (err == 0 && newcnt < vcnt) {
i = 0;
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
CHN_LOCK(c);
if (c->direction == PCMDIR_PLAY &&
(c->flags & CHN_F_VIRTUAL) &&
(i++ == newcnt)) {
if (!(c->flags & CHN_F_BUSY) &&
ORPHAN_CDEVT(sce->dsp_devt) &&
ORPHAN_CDEVT(sce->dspW_devt) &&
ORPHAN_CDEVT(sce->audio_devt) &&
ORPHAN_CDEVT(sce->dspr_devt))
goto remok;
/*
* Either we're busy, or our cdev
* has been stolen by dsp_clone().
* Skip, and increase newcnt.
*/
if (!(c->flags & CHN_F_BUSY))
device_printf(d->dev,
"%s: <%s> somebody steal my cdev!n",
__func__, c->name);
newcnt++;
}
CHN_UNLOCK(c);
}
if (vcnt != newcnt)
err = EBUSY;
break;
remok:
CHN_UNLOCK(c);
err = vchan_destroy(c);
if (err == 0)
vcnt--;
else
device_printf(d->dev,
"%s: WARNING: vchan_destroy() failed!",
__func__);
}
}

setvchans_out:
pcm_inprog(d, -1);
return err;
}

/* return error status and a locked channel */
int
pcm_chnalloc(struct snddev_info *d, struct pcm_channel **ch, int direction,
pid_t pid, int chnum)
{
struct pcm_channel *c;
struct snddev_channel *sce;
int err;

retry_chnalloc:
err = ENODEV;
/* scan for a free channel */
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
CHN_LOCK(c);
if (c->direction == direction && !(c->flags & CHN_F_BUSY)) {
if (chnum < 0 || sce->chan_num == chnum) {
c->flags |= CHN_F_BUSY;
c->pid = pid;
*ch = c;
return 0;
}
}
if (sce->chan_num == chnum) {
if (c->direction != direction)
err = EOPNOTSUPP;
else if (c->flags & CHN_F_BUSY)
err = EBUSY;
else
err = EINVAL;
CHN_UNLOCK(c);
return err;
} else if (c->direction == direction && (c->flags & CHN_F_BUSY))
err = EBUSY;
CHN_UNLOCK(c);
}

/* no channel available */
if (chnum == -1 && direction == PCMDIR_PLAY && d->vchancount > 0 &&
d->vchancount < snd_maxautovchans &&
d->devcount <= PCMMAXCHAN) {
err = pcm_setvchans(d, d->vchancount + 1);
if (err == 0) {
chnum = -2;
goto retry_chnalloc;
}
}

return err;
}

/* release a locked channel and unlock it */
int
pcm_chnrelease(struct pcm_channel *c)
{
CHN_LOCKASSERT(c);
c->flags &= ~CHN_F_BUSY;
c->pid = -1;
CHN_UNLOCK(c);
return 0;
}

int
pcm_chnref(struct pcm_channel *c, int ref)
{
int r;

CHN_LOCKASSERT(c);
c->refcount += ref;
r = c->refcount;
return r;
}

int
pcm_inprog(struct snddev_info *d, int delta)
{
int r;

if (delta == 0)
return d->inprog;

/* backtrace(); */
pcm_lock(d);
d->inprog += delta;
r = d->inprog;
pcm_unlock(d);
return r;
}

static void
pcm_setmaxautovchans(struct snddev_info *d, int num)
{
if (num > 0 && d->vchancount == 0)
pcm_setvchans(d, 1);
else if (num == 0 && d->vchancount > 0)
pcm_setvchans(d, 0);
}

#ifdef USING_DEVFS
static int
sysctl_hw_snd_unit(SYSCTL_HANDLER_ARGS)
{
struct snddev_info *d;
int error, unit;

unit = snd_unit;
error = sysctl_handle_int(oidp, &unit, sizeof(unit), req);
if (error == 0 && req->newptr != NULL) {
if (unit < 0 || unit >= devclass_get_maxunit(pcm_devclass))
return EINVAL;
d = devclass_get_softc(pcm_devclass, unit);
if (d == NULL || SLIST_EMPTY(&d->channels))
return EINVAL;
snd_unit = unit;
}
return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, unit, CTLTYPE_INT | CTLFLAG_RW,
0, sizeof(int), sysctl_hw_snd_unit, "I", "");
#endif

static int
sysctl_hw_snd_maxautovchans(SYSCTL_HANDLER_ARGS)
{
struct snddev_info *d;
int i, v, error;

v = snd_maxautovchans;
error = sysctl_handle_int(oidp, &v, sizeof(v), req);
if (error == 0 && req->newptr != NULL) {
if (v < 0 || v > PCMMAXCHAN)
return E2BIG;
if (pcm_devclass != NULL && v != snd_maxautovchans) {
for (i = 0; i < devclass_get_maxunit(pcm_devclass); i++) {
d = devclass_get_softc(pcm_devclass, i);
if (!d)
continue;
pcm_setmaxautovchans(d, v);
}
}
snd_maxautovchans = v;
}
return (error);
}
SYSCTL_PROC(_hw_snd, OID_AUTO, maxautovchans, CTLTYPE_INT | CTLFLAG_RW,
0, sizeof(int), sysctl_hw_snd_maxautovchans, "I", "");

struct pcm_channel *
pcm_chn_create(struct snddev_info *d, struct pcm_channel *parent, kobj_class_t cls, int dir, void *devinfo)
{
struct snddev_channel *sce;
struct pcm_channel *ch, *c;
char *dirs;
uint32_t flsearch = 0;
int direction, err, rpnum, *pnum;

switch(dir) {
case PCMDIR_PLAY:
dirs = "play";
direction = PCMDIR_PLAY;
pnum = &d->playcount;
break;

case PCMDIR_REC:
dirs = "record";
direction = PCMDIR_REC;
pnum = &d->reccount;
break;

case PCMDIR_VIRTUAL:
dirs = "virtual";
direction = PCMDIR_PLAY;
pnum = &d->vchancount;
flsearch = CHN_F_VIRTUAL;
break;

default:
return NULL;
}

ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
if (!ch)
return NULL;

ch->methods = kobj_create(cls, M_DEVBUF, M_WAITOK);
if (!ch->methods) {
free(ch, M_DEVBUF);

return NULL;
}

snd_mtxlock(d->lock);
ch->num = 0;
rpnum = 0;
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
if (direction != c->direction ||
(c->flags & CHN_F_VIRTUAL) != flsearch)
continue;
if (ch->num == c->num)
ch->num++;
else {
#if 0
device_printf(d->dev,
"%s: %s channel numbering screwed (Expect: %d, Got: %d)n",
__func__, dirs, ch->num, c->num);
#endif
goto retry_num_search;
}
rpnum++;
}
goto retry_num_search_out;
retry_num_search:
rpnum = 0;
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
if (direction != c->direction ||
(c->flags & CHN_F_VIRTUAL) != flsearch)
continue;
if (ch->num == c->num) {
ch->num++;
goto retry_num_search;
}
rpnum++;
}
retry_num_search_out:
if (*pnum != rpnum) {
device_printf(d->dev,
"%s: WARNING: pnum screwed : dirs=%s, pnum=%d, rpnum=%dn",
__func__, dirs, *pnum, rpnum);
*pnum = rpnum;
}
(*pnum)++;
snd_mtxunlock(d->lock);

ch->pid = -1;
ch->parentsnddev = d;
ch->parentchannel = parent;
ch->dev = d->dev;
snprintf(ch->name, CHN_NAMELEN, "%s:%s:%d", device_get_nameunit(ch->dev), dirs, ch->num);

err = chn_init(ch, devinfo, dir, direction);
if (err) {
device_printf(d->dev, "chn_init(%s) failed: err = %dn", ch->name, err);
kobj_delete(ch->methods, M_DEVBUF);
free(ch, M_DEVBUF);
snd_mtxlock(d->lock);
(*pnum)--;
snd_mtxunlock(d->lock);

return NULL;
}

return ch;
}

int
pcm_chn_destroy(struct pcm_channel *ch)
{
struct snddev_info *d;
int err;

d = ch->parentsnddev;
err = chn_kill(ch);
if (err) {
device_printf(d->dev, "chn_kill(%s) failed, err = %dn", ch->name, err);
return err;
}

kobj_delete(ch->methods, M_DEVBUF);
free(ch, M_DEVBUF);

return 0;
}

int
pcm_chn_add(struct snddev_info *d, struct pcm_channel *ch)
{
struct snddev_channel *sce, *tmp, *after;
unsigned rdevcount;
int device = device_get_unit(d->dev);
size_t namelen;

/*
* Note it's confusing nomenclature.
* dev_t
* device -> pcm_device
* unit -> pcm_channel
* channel -> snddev_channel
* device_t
* unit -> pcm_device
*/

sce = malloc(sizeof(*sce), M_DEVBUF, M_WAITOK | M_ZERO);
if (!sce) {
return ENOMEM;
}

snd_mtxlock(d->lock);
sce->channel = ch;
sce->chan_num = 0;
rdevcount = 0;
after = NULL;
SLIST_FOREACH(tmp, &d->channels, link) {
if (sce->chan_num == tmp->chan_num)
sce->chan_num++;
else {
#if 0
device_printf(d->dev,
"%s: cdev numbering screwed (Expect: %d, Got: %d)n",
__func__, sce->chan_num, tmp->chan_num);
#endif
goto retry_chan_num_search;
}
after = tmp;
rdevcount++;
}
goto retry_chan_num_search_out;
retry_chan_num_search:
/*
* Look for possible channel numbering collision. This may not
* be optimized, but it will ensure that no collision occured.
* Can be considered cheap since none of the locking/unlocking
* operations involved.
*/
rdevcount = 0;
after = NULL;
SLIST_FOREACH(tmp, &d->channels, link) {
if (sce->chan_num == tmp->chan_num) {
sce->chan_num++;
goto retry_chan_num_search;
}
if (sce->chan_num > tmp->chan_num)
after = tmp;
rdevcount++;
}
retry_chan_num_search_out:
/*
* Don't overflow PCMMKMINOR / PCMMAXCHAN.
*/
if (sce->chan_num > PCMMAXCHAN) {
snd_mtxunlock(d->lock);
device_printf(d->dev,
"%s: WARNING: sce->chan_num overflow! (%d)n",
__func__, sce->chan_num);
free(sce, M_DEVBUF);
return E2BIG;
}
if (d->devcount != rdevcount) {
device_printf(d->dev,
"%s: WARNING: devcount screwed! d->devcount=%u, rdevcount=%un",
__func__, d->devcount, rdevcount);
d->devcount = rdevcount;
}
d->devcount++;
if (after == NULL) {
SLIST_INSERT_HEAD(&d->channels, sce, link);
} else {
SLIST_INSERT_AFTER(after, sce, link);
}
#if 0
if (1) {
int cnum = 0;
SLIST_FOREACH(tmp, &d->channels, link) {
if (cnum != tmp->chan_num)
device_printf(d->dev,
"%s: WARNING: inconsistent cdev numbering! (Expect: %d, Got: %d)n",
__func__, cnum, tmp->chan_num);
cnum++;
}
}
#endif

namelen = strlen(ch->name);
if ((CHN_NAMELEN - namelen) > 10) {	/* ":dspXX.YYY" */
snprintf(ch->name + namelen,
CHN_NAMELEN - namelen, ":dsp%d.%d",
device, sce->chan_num);
}
snd_mtxunlock(d->lock);

/*
* I will revisit these someday, and nuke it mercilessly..
*/
sce->dsp_devt = make_dev(&dsp_cdevsw,
PCMMKMINOR(device, SND_DEV_DSP, sce->chan_num),
UID_ROOT, GID_WHEEL, 0666, "dsp%d.%d",
device, sce->chan_num);

sce->dspW_devt = make_dev(&dsp_cdevsw,
PCMMKMINOR(device, SND_DEV_DSP16, sce->chan_num),
UID_ROOT, GID_WHEEL, 0666, "dspW%d.%d",
device, sce->chan_num);

sce->audio_devt = make_dev(&dsp_cdevsw,
PCMMKMINOR(device, SND_DEV_AUDIO, sce->chan_num),
UID_ROOT, GID_WHEEL, 0666, "audio%d.%d",
device, sce->chan_num);

if (ch->direction == PCMDIR_REC)
sce->dspr_devt = make_dev(&dsp_cdevsw,
PCMMKMINOR(device, SND_DEV_DSPREC,
sce->chan_num), UID_ROOT, GID_WHEEL,
0666, "dspr%d.%d", device, sce->chan_num);

return 0;
}

int
pcm_chn_remove(struct snddev_info *d, struct pcm_channel *ch)
{
struct snddev_channel *sce;
#if 0
int ourlock;

ourlock = 0;
if (!mtx_owned(d->lock)) {
snd_mtxlock(d->lock);
ourlock = 1;
}
#endif

SLIST_FOREACH(sce, &d->channels, link) {
if (sce->channel == ch)
goto gotit;
}
#if 0
if (ourlock)
snd_mtxunlock(d->lock);
#endif
return EINVAL;
gotit:
SLIST_REMOVE(&d->channels, sce, snddev_channel, link);

if (ch->flags & CHN_F_VIRTUAL)
d->vchancount--;
else if (ch->direction == PCMDIR_REC)
d->reccount--;
else
d->playcount--;

#if 0
if (ourlock)
snd_mtxunlock(d->lock);
#endif
free(sce, M_DEVBUF);

return 0;
}

int
pcm_addchan(device_t dev, int dir, kobj_class_t cls, void *devinfo)
{
struct snddev_info *d = device_get_softc(dev);
struct pcm_channel *ch;
int err;

ch = pcm_chn_create(d, NULL, cls, dir, devinfo);
if (!ch) {
device_printf(d->dev, "pcm_chn_create(%s, %d, %p) failedn", cls->name, dir, devinfo);
return ENODEV;
}

err = pcm_chn_add(d, ch);
if (err) {
device_printf(d->dev, "pcm_chn_add(%s) failed, err=%dn", ch->name, err);
pcm_chn_destroy(ch);
return err;
}

return err;
}

static int
pcm_killchan(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);
struct snddev_channel *sce;
struct pcm_channel *ch;
int error = 0;

sce = SLIST_FIRST(&d->channels);
ch = sce->channel;

error = pcm_chn_remove(d, sce->channel);
if (error)
return (error);
return (pcm_chn_destroy(ch));
}

int
pcm_setstatus(device_t dev, char *str)
{
struct snddev_info *d = device_get_softc(dev);

snd_mtxlock(d->lock);
strncpy(d->status, str, SND_STATUSLEN);
snd_mtxunlock(d->lock);
if (snd_maxautovchans > 0)
pcm_setvchans(d, 1);
return 0;
}

uint32_t
pcm_getflags(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);

return d->flags;
}

void
pcm_setflags(device_t dev, uint32_t val)
{
struct snddev_info *d = device_get_softc(dev);

d->flags = val;
}

void *
pcm_getdevinfo(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);

return d->devinfo;
}

unsigned int
pcm_getbuffersize(device_t dev, unsigned int min, unsigned int deflt, unsigned int max)
{
struct snddev_info *d = device_get_softc(dev);
int sz, x;

sz = 0;
if (resource_int_value(device_get_name(dev), device_get_unit(dev), "buffersize", &sz) == 0) {
x = sz;
RANGE(sz, min, max);
if (x != sz)
device_printf(dev, "'buffersize=%d' hint is out of range (%d-%d), using %dn", x, min, max, sz);
x = min;
while (x < sz)
x <<= 1;
if (x > sz)
x >>= 1;
if (x != sz) {
device_printf(dev, "'buffersize=%d' hint is not a power of 2, using %dn", sz, x);
sz = x;
}
} else {
sz = deflt;
}

d->bufsz = sz;

return sz;
}

int
pcm_register(device_t dev, void *devinfo, int numplay, int numrec)
{
struct snddev_info *d = device_get_softc(dev);

if (pcm_veto_load) {
device_printf(dev, "disabled due to an error while initialising: %dn", pcm_veto_load);

return EINVAL;
}

d->lock = snd_mtxcreate(device_get_nameunit(dev), "sound cdev");

#if 0
/*
* d->flags should be cleared by the allocator of the softc.
* We cannot clear this field here because several devices set
* this flag before calling pcm_register().
*/
d->flags = 0;
#endif
d->dev = dev;
d->devinfo = devinfo;
d->devcount = 0;
d->reccount = 0;
d->playcount = 0;
d->vchancount = 0;
d->inprog = 0;

SLIST_INIT(&d->channels);

if ((numplay == 0 || numrec == 0) && numplay != numrec)
d->flags |= SD_F_SIMPLEX;

d->fakechan = fkchan_setup(dev);
chn_init(d->fakechan, NULL, 0, 0);

#ifdef SND_DYNSYSCTL
sysctl_ctx_init(&d->sysctl_tree);
d->sysctl_tree_top = SYSCTL_ADD_NODE(&d->sysctl_tree,
SYSCTL_STATIC_CHILDREN(_hw_snd), OID_AUTO,
device_get_nameunit(dev), CTLFLAG_RD, 0, "");
if (d->sysctl_tree_top == NULL) {
sysctl_ctx_free(&d->sysctl_tree);
goto no;
}
SYSCTL_ADD_INT(snd_sysctl_tree(dev), SYSCTL_CHILDREN(snd_sysctl_tree_top(dev)),
OID_AUTO, "buffersize", CTLFLAG_RD, &d->bufsz, 0, "");
#endif
if (numplay > 0) {
d->flags |= SD_F_AUTOVCHAN;
vchan_initsys(dev);
}

sndstat_register(dev, d->status, sndstat_prepare_pcm);
return 0;
no:
snd_mtxfree(d->lock);
return ENXIO;
}

int
pcm_unregister(device_t dev)
{
struct snddev_info *d = device_get_softc(dev);
struct snddev_channel *sce;
struct pcmchan_children *pce;
struct pcm_channel *ch;

if (sndstat_acquire() != 0) {
device_printf(dev, "unregister: sndstat busyn");
return EBUSY;
}

snd_mtxlock(d->lock);
if (d->inprog) {
device_printf(dev, "unregister: operation in progressn");
snd_mtxunlock(d->lock);
sndstat_release();
return EBUSY;
}

SLIST_FOREACH(sce, &d->channels, link) {
ch = sce->channel;
if (ch->refcount > 0) {
device_printf(dev, "unregister: channel %s busy (pid %d)n", ch->name, ch->pid);
snd_mtxunlock(d->lock);
sndstat_release();
return EBUSY;
}
}

if (mixer_uninit(dev) == EBUSY) {
device_printf(dev, "unregister: mixer busyn");
snd_mtxunlock(d->lock);
sndstat_release();
return EBUSY;
}

SLIST_FOREACH(sce, &d->channels, link) {
if (sce->dsp_devt) {
destroy_dev(sce->dsp_devt);
sce->dsp_devt = NULL;
}
if (sce->dspW_devt) {
destroy_dev(sce->dspW_devt);
sce->dspW_devt = NULL;
}
if (sce->audio_devt) {
destroy_dev(sce->audio_devt);
sce->audio_devt = NULL;
}
if (sce->dspr_devt) {
destroy_dev(sce->dspr_devt);
sce->dspr_devt = NULL;
}
d->devcount--;
ch = sce->channel;
if (ch == NULL)
continue;
pce = SLIST_FIRST(&ch->children);
while (pce != NULL) {
#if 0
device_printf(d->dev, "<%s> removing <%s>n",
ch->name, (pce->channel != NULL) ?
pce->channel->name : "unknown");
#endif
SLIST_REMOVE(&ch->children, pce, pcmchan_children, link);
free(pce, M_DEVBUF);
pce = SLIST_FIRST(&ch->children);
}
}

#ifdef SND_DYNSYSCTL
d->sysctl_tree_top = NULL;
sysctl_ctx_free(&d->sysctl_tree);
#endif

#if 0
SLIST_FOREACH(sce, &d->channels, link) {
ch = sce->channel;
if (ch == NULL)
continue;
if (!SLIST_EMPTY(&ch->children))
device_printf(d->dev, "%s: WARNING: <%s> dangling child!n",
__func__, ch->name);
}
#endif
while (!SLIST_EMPTY(&d->channels))
pcm_killchan(dev);

chn_kill(d->fakechan);
fkchan_kill(d->fakechan);

#if 0
device_printf(d->dev, "%s: devcount=%u, playcount=%u, "
"reccount=%u, vchancount=%un",
__func__, d->devcount, d->playcount, d->reccount,
d->vchancount);
#endif
snd_mtxunlock(d->lock);
snd_mtxfree(d->lock);
sndstat_unregister(dev);
sndstat_release();
return 0;
}

/************************************************************************/

static int
sndstat_prepare_pcm(struct sbuf *s, device_t dev, int verbose)
{
struct snddev_info *d;
struct snddev_channel *sce;
struct pcm_channel *c;
struct pcm_feeder *f;
int pc, rc, vc;

if (verbose < 1)
return 0;

d = device_get_softc(dev);
if (!d)
return ENXIO;

snd_mtxlock(d->lock);
if (!SLIST_EMPTY(&d->channels)) {
pc = rc = vc = 0;
SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;
if (c->direction == PCMDIR_PLAY) {
if (c->flags & CHN_F_VIRTUAL)
vc++;
else
pc++;
} else
rc++;
}
sbuf_printf(s, " (%dp/%dr/%dv channels%s%s)", d->playcount, d->reccount, d->vchancount,
(d->flags & SD_F_SIMPLEX)? "" : " duplex",
#ifdef USING_DEVFS
(device_get_unit(dev) == snd_unit)? " default" : ""
#else
""
#endif
);

if (verbose <= 1) {
snd_mtxunlock(d->lock);
return 0;
}

SLIST_FOREACH(sce, &d->channels, link) {
c = sce->channel;

KASSERT(c->bufhard != NULL && c->bufsoft != NULL,
("hosed pcm channel setup"));

sbuf_printf(s, "nt");

/* it would be better to indent child channels */
sbuf_printf(s, "%s[%s]: ", c->parentchannel? c->parentchannel->name : "", c->name);
sbuf_printf(s, "spd %d", c->speed);
if (c->speed != sndbuf_getspd(c->bufhard))
sbuf_printf(s, "/%d", sndbuf_getspd(c->bufhard));
sbuf_printf(s, ", fmt 0x%08x", c->format);
if (c->format != sndbuf_getfmt(c->bufhard))
sbuf_printf(s, "/0x%08x", sndbuf_getfmt(c->bufhard));
sbuf_printf(s, ", flags 0x%08x, 0x%08x", c->flags, c->feederflags);
if (c->pid != -1)
sbuf_printf(s, ", pid %d", c->pid);
sbuf_printf(s, "nt");

sbuf_printf(s, "interrupts %d, ", c->interrupts);
if (c->direction == PCMDIR_REC)
sbuf_printf(s, "overruns %d, hfree %d, sfree %d [b:%d/%d/%d|bs:%d/%d/%d]",
c->xruns, sndbuf_getfree(c->bufhard), sndbuf_getfree(c->bufsoft),
sndbuf_getsize(c->bufhard), sndbuf_getblksz(c->bufhard),
sndbuf_getblkcnt(c->bufhard),
sndbuf_getsize(c->bufsoft), sndbuf_getblksz(c->bufsoft),
sndbuf_getblkcnt(c->bufsoft));
else
sbuf_printf(s, "underruns %d, ready %d [b:%d/%d/%d|bs:%d/%d/%d]",
c->xruns, sndbuf_getready(c->bufsoft),
sndbuf_getsize(c->bufhard), sndbuf_getblksz(c->bufhard),
sndbuf_getblkcnt(c->bufhard),
sndbuf_getsize(c->bufsoft), sndbuf_getblksz(c->bufsoft),
sndbuf_getblkcnt(c->bufsoft));
sbuf_printf(s, "nt");

sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC)? "hardware" : "userland");
sbuf_printf(s, " -> ");
f = c->feeder;
while (f->source != NULL)
f = f->source;
while (f != NULL) {
sbuf_printf(s, "%s", f->class->name);
if (f->desc->type == FEEDER_FMT)
sbuf_printf(s, "(0x%08x -> 0x%08x)", f->desc->in, f->desc->out);
if (f->desc->type == FEEDER_RATE)
sbuf_printf(s, "(%d -> %d)", FEEDER_GET(f, FEEDRATE_SRC), FEEDER_GET(f, FEEDRATE_DST));
if (f->desc->type == FEEDER_ROOT || f->desc->type == FEEDER_MIXER ||
f->desc->type == FEEDER_VOLUME)
sbuf_printf(s, "(0x%08x)", f->desc->out);
sbuf_printf(s, " -> ");
f = f->parent;
}
sbuf_printf(s, "{%s}", (c->direction == PCMDIR_REC)? "userland" : "hardware");
}
} else
sbuf_printf(s, " (mixer only)");
snd_mtxunlock(d->lock);

return 0;
}

/************************************************************************/

#ifdef SND_DYNSYSCTL
int
sysctl_hw_snd_vchans(SYSCTL_HANDLER_ARGS)
{
struct snddev_info *d;
int err, newcnt;

d = oidp->oid_arg1;

newcnt = d->vchancount;
err = sysctl_handle_int(oidp, &newcnt, sizeof(newcnt), req);

if (err == 0 && req->newptr != NULL && d->vchancount != newcnt)
err = pcm_setvchans(d, newcnt);

return err;
}
#endif

/************************************************************************/

static int
sound_modevent(module_t mod, int type, void *data)
{
#if 0
return (midi_modevent(mod, type, data));
#else
return 0;
#endif
}

DEV_MODULE(sound, sound_modevent, NULL);
MODULE_VERSION(sound, SOUND_MODVER);
