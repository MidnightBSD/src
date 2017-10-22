/*-
 * Copyright (c) 2006 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
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
 * $FreeBSD: release/7.0.0/sys/security/audit/audit_pipe.c 173330 2007-11-04 16:44:48Z csjp $
 */

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/conf.h>
#include <sys/eventhandler.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/selinfo.h>
#include <sys/sigio.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <security/audit/audit.h>
#include <security/audit/audit_ioctl.h>
#include <security/audit/audit_private.h>

/*
 * Implementation of a clonable special device providing a live stream of BSM
 * audit data.  This is a "tee" of the data going to the file.  It provides
 * unreliable but timely access to audit events.  Consumers of this interface
 * should be very careful to avoid introducing event cycles.  Consumers may
 * express interest via a set of preselection ioctls.
 */

/*
 * Memory types.
 */
static MALLOC_DEFINE(M_AUDIT_PIPE, "audit_pipe", "Audit pipes");
static MALLOC_DEFINE(M_AUDIT_PIPE_ENTRY, "audit_pipeent",
    "Audit pipe entries and buffers");
static MALLOC_DEFINE(M_AUDIT_PIPE_PRESELECT, "audit_pipe_preselect",
    "Audit pipe preselection structure");

/*
 * Audit pipe buffer parameters.
 */
#define	AUDIT_PIPE_QLIMIT_DEFAULT	(128)
#define	AUDIT_PIPE_QLIMIT_MIN		(0)
#define	AUDIT_PIPE_QLIMIT_MAX		(1024)

/*
 * Description of an entry in an audit_pipe.
 */
struct audit_pipe_entry {
	void				*ape_record;
	u_int				 ape_record_len;
	TAILQ_ENTRY(audit_pipe_entry)	 ape_queue;
};

/*
 * Audit pipes allow processes to express "interest" in the set of records
 * that are delivered via the pipe.  They do this in a similar manner to the
 * mechanism for audit trail configuration, by expressing two global masks,
 * and optionally expressing per-auid masks.  The following data structure is
 * the per-auid mask description.  The global state is stored in the audit
 * pipe data structure.
 *
 * We may want to consider a more space/time-efficient data structure once
 * usage patterns for per-auid specifications are clear.
 */
struct audit_pipe_preselect {
	au_id_t					 app_auid;
	au_mask_t				 app_mask;
	TAILQ_ENTRY(audit_pipe_preselect)	 app_list;
};

/*
 * Description of an individual audit_pipe.  Consists largely of a bounded
 * length queue.
 */
#define	AUDIT_PIPE_ASYNC	0x00000001
#define	AUDIT_PIPE_NBIO		0x00000002
struct audit_pipe {
	int				 ap_open;	/* Device open? */
	u_int				 ap_flags;

	struct selinfo			 ap_selinfo;
	struct sigio			*ap_sigio;

	u_int				 ap_qlen;
	u_int				 ap_qlimit;

	u_int64_t			 ap_inserts;	/* Records added. */
	u_int64_t			 ap_reads;	/* Records read. */
	u_int64_t			 ap_drops;	/* Records dropped. */
	u_int64_t			 ap_truncates;	/* Records too long. */

	/*
	 * Fields relating to pipe interest: global masks for unmatched
	 * processes (attributable, non-attributable), and a list of specific
	 * interest specifications by auid.
	 */
	int				 ap_preselect_mode;
	au_mask_t			 ap_preselect_flags;
	au_mask_t			 ap_preselect_naflags;
	TAILQ_HEAD(, audit_pipe_preselect)	ap_preselect_list;

	/*
	 * Current pending record list.
	 */
	TAILQ_HEAD(, audit_pipe_entry)	 ap_queue;

	/*
	 * Global pipe list.
	 */
	TAILQ_ENTRY(audit_pipe)		 ap_list;
};

/*
 * Global list of audit pipes, mutex to protect it and the pipes.  Finer
 * grained locking may be desirable at some point.
 */
static TAILQ_HEAD(, audit_pipe)	 audit_pipe_list;
static struct mtx		 audit_pipe_mtx;

/*
 * This CV is used to wakeup on an audit record write.  Eventually, it might
 * be per-pipe to avoid unnecessary wakeups when several pipes with different
 * preselection masks are present.
 */
static struct cv		 audit_pipe_cv;

/*
 * Cloning related variables and constants.
 */
#define	AUDIT_PIPE_NAME		"auditpipe"
static eventhandler_tag		 audit_pipe_eh_tag;
static struct clonedevs		*audit_pipe_clones;

/*
 * Special device methods and definition.
 */
static d_open_t		audit_pipe_open;
static d_close_t	audit_pipe_close;
static d_read_t		audit_pipe_read;
static d_ioctl_t	audit_pipe_ioctl;
static d_poll_t		audit_pipe_poll;
static d_kqfilter_t	audit_pipe_kqfilter;

static struct cdevsw	audit_pipe_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_PSEUDO | D_NEEDGIANT,
	.d_open =	audit_pipe_open,
	.d_close =	audit_pipe_close,
	.d_read =	audit_pipe_read,
	.d_ioctl =	audit_pipe_ioctl,
	.d_poll =	audit_pipe_poll,
	.d_kqfilter =	audit_pipe_kqfilter,
	.d_name =	AUDIT_PIPE_NAME,
};

static int	audit_pipe_kqread(struct knote *note, long hint);
static void	audit_pipe_kqdetach(struct knote *note);

static struct filterops audit_pipe_read_filterops = {
	.f_isfd =	1,
	.f_attach =	NULL,
	.f_detach =	audit_pipe_kqdetach,
	.f_event =	audit_pipe_kqread,
};

/*
 * Some global statistics on audit pipes.
 */
static int		audit_pipe_count;	/* Current number of pipes. */
static u_int64_t	audit_pipe_ever;	/* Pipes ever allocated. */
static u_int64_t	audit_pipe_records;	/* Records seen. */
static u_int64_t	audit_pipe_drops;	/* Global record drop count. */

/*
 * Free an audit pipe entry.
 */
static void
audit_pipe_entry_free(struct audit_pipe_entry *ape)
{

	free(ape->ape_record, M_AUDIT_PIPE_ENTRY);
	free(ape, M_AUDIT_PIPE_ENTRY);
}

/*
 * Find an audit pipe preselection specification for an auid, if any.
 */
static struct audit_pipe_preselect *
audit_pipe_preselect_find(struct audit_pipe *ap, au_id_t auid)
{
	struct audit_pipe_preselect *app;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	TAILQ_FOREACH(app, &ap->ap_preselect_list, app_list) {
		if (app->app_auid == auid)
			return (app);
	}
	return (NULL);
}

/*
 * Query the per-pipe mask for a specific auid.
 */
static int
audit_pipe_preselect_get(struct audit_pipe *ap, au_id_t auid,
    au_mask_t *maskp)
{
	struct audit_pipe_preselect *app;
	int error;

	mtx_lock(&audit_pipe_mtx);
	app = audit_pipe_preselect_find(ap, auid);
	if (app != NULL) {
		*maskp = app->app_mask;
		error = 0;
	} else
		error = ENOENT;
	mtx_unlock(&audit_pipe_mtx);
	return (error);
}

/*
 * Set the per-pipe mask for a specific auid.  Add a new entry if needed;
 * otherwise, update the current entry.
 */
static void
audit_pipe_preselect_set(struct audit_pipe *ap, au_id_t auid, au_mask_t mask)
{
	struct audit_pipe_preselect *app, *app_new;

	/*
	 * Pessimistically assume that the auid doesn't already have a mask
	 * set, and allocate.  We will free it if it is unneeded.
	 */
	app_new = malloc(sizeof(*app_new), M_AUDIT_PIPE_PRESELECT, M_WAITOK);
	mtx_lock(&audit_pipe_mtx);
	app = audit_pipe_preselect_find(ap, auid);
	if (app == NULL) {
		app = app_new;
		app_new = NULL;
		app->app_auid = auid;
		TAILQ_INSERT_TAIL(&ap->ap_preselect_list, app, app_list);
	}
	app->app_mask = mask;
	mtx_unlock(&audit_pipe_mtx);
	if (app_new != NULL)
		free(app_new, M_AUDIT_PIPE_PRESELECT);
}

/*
 * Delete a per-auid mask on an audit pipe.
 */
static int
audit_pipe_preselect_delete(struct audit_pipe *ap, au_id_t auid)
{
	struct audit_pipe_preselect *app;
	int error;

	mtx_lock(&audit_pipe_mtx);
	app = audit_pipe_preselect_find(ap, auid);
	if (app != NULL) {
		TAILQ_REMOVE(&ap->ap_preselect_list, app, app_list);
		error = 0;
	} else
		error = ENOENT;
	mtx_unlock(&audit_pipe_mtx);
	if (app != NULL)
		free(app, M_AUDIT_PIPE_PRESELECT);
	return (error);
}

/*
 * Delete all per-auid masks on an audit pipe.
 */
static void
audit_pipe_preselect_flush_locked(struct audit_pipe *ap)
{
	struct audit_pipe_preselect *app;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	while ((app = TAILQ_FIRST(&ap->ap_preselect_list)) != NULL) {
		TAILQ_REMOVE(&ap->ap_preselect_list, app, app_list);
		free(app, M_AUDIT_PIPE_PRESELECT);
	}
}

static void
audit_pipe_preselect_flush(struct audit_pipe *ap)
{

	mtx_lock(&audit_pipe_mtx);
	audit_pipe_preselect_flush_locked(ap);
	mtx_unlock(&audit_pipe_mtx);
}

/*-
 * Determine whether a specific audit pipe matches a record with these
 * properties.  Algorithm is as follows:
 *
 * - If the pipe is configured to track the default trail configuration, then
 *   use the results of global preselection matching.
 * - If not, search for a specifically configured auid entry matching the
 *   event.  If an entry is found, use that.
 * - Otherwise, use the default flags or naflags configured for the pipe.
 */
static int
audit_pipe_preselect_check(struct audit_pipe *ap, au_id_t auid,
    au_event_t event, au_class_t class, int sorf, int trail_preselect)
{
	struct audit_pipe_preselect *app;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	switch (ap->ap_preselect_mode) {
	case AUDITPIPE_PRESELECT_MODE_TRAIL:
		return (trail_preselect);

	case AUDITPIPE_PRESELECT_MODE_LOCAL:
		app = audit_pipe_preselect_find(ap, auid);
		if (app == NULL) {
			if (auid == AU_DEFAUDITID)
				return (au_preselect(event, class,
				    &ap->ap_preselect_naflags, sorf));
			else
				return (au_preselect(event, class,
				    &ap->ap_preselect_flags, sorf));
		} else
			return (au_preselect(event, class, &app->app_mask,
			    sorf));

	default:
		panic("audit_pipe_preselect_check: mode %d",
		    ap->ap_preselect_mode);
	}

	return (0);
}

/*
 * Determine whether there exists a pipe interested in a record with specific
 * properties.
 */
int
audit_pipe_preselect(au_id_t auid, au_event_t event, au_class_t class,
    int sorf, int trail_preselect)
{
	struct audit_pipe *ap;

	mtx_lock(&audit_pipe_mtx);
	TAILQ_FOREACH(ap, &audit_pipe_list, ap_list) {
		if (audit_pipe_preselect_check(ap, auid, event, class, sorf,
		    trail_preselect)) {
			mtx_unlock(&audit_pipe_mtx);
			return (1);
		}
	}
	mtx_unlock(&audit_pipe_mtx);
	return (0);
}

/*
 * Append individual record to a queue -- allocate queue-local buffer, and
 * add to the queue.  We try to drop from the head of the queue so that more
 * recent events take precedence over older ones, but if allocation fails we
 * do drop the new event.
 */
static void
audit_pipe_append(struct audit_pipe *ap, void *record, u_int record_len)
{
	struct audit_pipe_entry *ape, *ape_remove;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	ape = malloc(sizeof(*ape), M_AUDIT_PIPE_ENTRY, M_NOWAIT | M_ZERO);
	if (ape == NULL) {
		ap->ap_drops++;
		audit_pipe_drops++;
		return;
	}

	ape->ape_record = malloc(record_len, M_AUDIT_PIPE_ENTRY, M_NOWAIT);
	if (ape->ape_record == NULL) {
		free(ape, M_AUDIT_PIPE_ENTRY);
		ap->ap_drops++;
		audit_pipe_drops++;
		return;
	}

	bcopy(record, ape->ape_record, record_len);
	ape->ape_record_len = record_len;

	if (ap->ap_qlen >= ap->ap_qlimit) {
		ape_remove = TAILQ_FIRST(&ap->ap_queue);
		TAILQ_REMOVE(&ap->ap_queue, ape_remove, ape_queue);
		audit_pipe_entry_free(ape_remove);
		ap->ap_qlen--;
		ap->ap_drops++;
		audit_pipe_drops++;
	}

	TAILQ_INSERT_TAIL(&ap->ap_queue, ape, ape_queue);
	ap->ap_inserts++;
	ap->ap_qlen++;
	selwakeuppri(&ap->ap_selinfo, PSOCK);
	KNOTE_LOCKED(&ap->ap_selinfo.si_note, 0);
	if (ap->ap_flags & AUDIT_PIPE_ASYNC)
		pgsigio(&ap->ap_sigio, SIGIO, 0);
}

/*
 * audit_pipe_submit(): audit_worker submits audit records via this
 * interface, which arranges for them to be delivered to pipe queues.
 */
void
audit_pipe_submit(au_id_t auid, au_event_t event, au_class_t class, int sorf,
    int trail_select, void *record, u_int record_len)
{
	struct audit_pipe *ap;

	/*
	 * Lockless read to avoid mutex overhead if pipes are not in use.
	 */
	if (TAILQ_FIRST(&audit_pipe_list) == NULL)
		return;

	mtx_lock(&audit_pipe_mtx);
	TAILQ_FOREACH(ap, &audit_pipe_list, ap_list) {
		if (audit_pipe_preselect_check(ap, auid, event, class, sorf,
		    trail_select))
			audit_pipe_append(ap, record, record_len);
	}
	audit_pipe_records++;
	mtx_unlock(&audit_pipe_mtx);
	cv_broadcastpri(&audit_pipe_cv, PSOCK);
}

/*
 * audit_pipe_submit_user(): the same as audit_pipe_submit(), except that
 * since we don't currently have selection information available, it is
 * delivered to the pipe unconditionally.
 *
 * XXXRW: This is a bug.  The BSM check routine for submitting a user record
 * should parse that information and return it.
 */
void
audit_pipe_submit_user(void *record, u_int record_len)
{
	struct audit_pipe *ap;

	/*
	 * Lockless read to avoid mutex overhead if pipes are not in use.
	 */
	if (TAILQ_FIRST(&audit_pipe_list) == NULL)
		return;

	mtx_lock(&audit_pipe_mtx);
	TAILQ_FOREACH(ap, &audit_pipe_list, ap_list)
		audit_pipe_append(ap, record, record_len);
	audit_pipe_records++;
	mtx_unlock(&audit_pipe_mtx);
	cv_broadcastpri(&audit_pipe_cv, PSOCK);
}


/*
 * Pop the next record off of an audit pipe.
 */
static struct audit_pipe_entry *
audit_pipe_pop(struct audit_pipe *ap)
{
	struct audit_pipe_entry *ape;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	ape = TAILQ_FIRST(&ap->ap_queue);
	KASSERT((ape == NULL && ap->ap_qlen == 0) ||
	    (ape != NULL && ap->ap_qlen != 0), ("audit_pipe_pop: qlen"));
	if (ape == NULL)
		return (NULL);
	TAILQ_REMOVE(&ap->ap_queue, ape, ape_queue);
	ap->ap_qlen--;
	return (ape);
}

/*
 * Allocate a new audit pipe.  Connects the pipe, on success, to the global
 * list and updates statistics.
 */
static struct audit_pipe *
audit_pipe_alloc(void)
{
	struct audit_pipe *ap;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	ap = malloc(sizeof(*ap), M_AUDIT_PIPE, M_NOWAIT | M_ZERO);
	if (ap == NULL)
		return (NULL);
	ap->ap_qlimit = AUDIT_PIPE_QLIMIT_DEFAULT;
	TAILQ_INIT(&ap->ap_queue);
	knlist_init(&ap->ap_selinfo.si_note, &audit_pipe_mtx, NULL, NULL,
	    NULL);

	/*
	 * Default flags, naflags, and auid-specific preselection settings to
	 * 0.  Initialize the mode to the global trail so that if praudit(1)
	 * is run on /dev/auditpipe, it sees events associated with the
	 * default trail.  Pipe-aware application can clear the flag, set
	 * custom masks, and flush the pipe as needed.
	 */
	bzero(&ap->ap_preselect_flags, sizeof(ap->ap_preselect_flags));
	bzero(&ap->ap_preselect_naflags, sizeof(ap->ap_preselect_naflags));
	TAILQ_INIT(&ap->ap_preselect_list);
	ap->ap_preselect_mode = AUDITPIPE_PRESELECT_MODE_TRAIL;

	/*
	 * Add to global list and update global statistics.
	 */
	TAILQ_INSERT_HEAD(&audit_pipe_list, ap, ap_list);
	audit_pipe_count++;
	audit_pipe_ever++;

	return (ap);
}

/*
 * Flush all records currently present in an audit pipe; assume mutex is held.
 */
static void
audit_pipe_flush(struct audit_pipe *ap)
{
	struct audit_pipe_entry *ape;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	while ((ape = TAILQ_FIRST(&ap->ap_queue)) != NULL) {
		TAILQ_REMOVE(&ap->ap_queue, ape, ape_queue);
		audit_pipe_entry_free(ape);
		ap->ap_qlen--;
	}
	KASSERT(ap->ap_qlen == 0, ("audit_pipe_free: ap_qlen"));
}

/*
 * Free an audit pipe; this means freeing all preselection state and all
 * records in the pipe.  Assumes mutex is held to prevent any new records
 * from being inserted during the free, and that the audit pipe is still on
 * the global list.
 */
static void
audit_pipe_free(struct audit_pipe *ap)
{

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	audit_pipe_preselect_flush_locked(ap);
	audit_pipe_flush(ap);
	knlist_destroy(&ap->ap_selinfo.si_note);
	TAILQ_REMOVE(&audit_pipe_list, ap, ap_list);
	free(ap, M_AUDIT_PIPE);
	audit_pipe_count--;
}

/*
 * Audit pipe clone routine -- provide specific requested audit pipe, or a
 * fresh one if a specific one is not requested.
 */
static void
audit_pipe_clone(void *arg, struct ucred *cred, char *name, int namelen,
    struct cdev **dev)
{
	int i, u;

	if (*dev != NULL)
		return;

	if (strcmp(name, AUDIT_PIPE_NAME) == 0)
		u = -1;
	else if (dev_stdclone(name, NULL, AUDIT_PIPE_NAME, &u) != 1)
		return;

	i = clone_create(&audit_pipe_clones, &audit_pipe_cdevsw, &u, dev, 0);
	if (i) {
		*dev = make_dev(&audit_pipe_cdevsw, unit2minor(u), UID_ROOT,
		    GID_WHEEL, 0600, "%s%d", AUDIT_PIPE_NAME, u);
		if (*dev != NULL) {
			dev_ref(*dev);
			(*dev)->si_flags |= SI_CHEAPCLONE;
		}
	}
}

/*
 * Audit pipe open method.  Explicit privilege check isn't used as this
 * allows file permissions on the special device to be used to grant audit
 * review access.  Those file permissions should be managed carefully.
 */
static int
audit_pipe_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct audit_pipe *ap;

	mtx_lock(&audit_pipe_mtx);
	ap = dev->si_drv1;
	if (ap == NULL) {
		ap = audit_pipe_alloc();
		if (ap == NULL) {
			mtx_unlock(&audit_pipe_mtx);
			return (ENOMEM);
		}
		dev->si_drv1 = ap;
	} else {
		KASSERT(ap->ap_open, ("audit_pipe_open: ap && !ap_open"));
		mtx_unlock(&audit_pipe_mtx);
		return (EBUSY);
	}
	ap->ap_open = 1;
	mtx_unlock(&audit_pipe_mtx);
	fsetown(td->td_proc->p_pid, &ap->ap_sigio);
	return (0);
}

/*
 * Close audit pipe, tear down all records, etc.
 */
static int
audit_pipe_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
	struct audit_pipe *ap;

	ap = dev->si_drv1;
	KASSERT(ap != NULL, ("audit_pipe_close: ap == NULL"));
	KASSERT(ap->ap_open, ("audit_pipe_close: !ap_open"));
	funsetown(&ap->ap_sigio);
	mtx_lock(&audit_pipe_mtx);
	ap->ap_open = 0;
	audit_pipe_free(ap);
	dev->si_drv1 = NULL;
	mtx_unlock(&audit_pipe_mtx);
	return (0);
}

/*
 * Audit pipe ioctl() routine.  Handle file descriptor and audit pipe layer
 * commands.
 *
 * Would be desirable to support filtering, although perhaps something simple
 * like an event mask, as opposed to something complicated like BPF.
 */
static int
audit_pipe_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
    struct thread *td)
{
	struct auditpipe_ioctl_preselect *aip;
	struct audit_pipe *ap;
	au_mask_t *maskp;
	int error, mode;
	au_id_t auid;

	ap = dev->si_drv1;
	KASSERT(ap != NULL, ("audit_pipe_ioctl: ap == NULL"));

	/*
	 * Audit pipe ioctls: first come standard device node ioctls, then
	 * manipulation of pipe settings, and finally, statistics query
	 * ioctls.
	 */
	switch (cmd) {
	case FIONBIO:
		mtx_lock(&audit_pipe_mtx);
		if (*(int *)data)
			ap->ap_flags |= AUDIT_PIPE_NBIO;
		else
			ap->ap_flags &= ~AUDIT_PIPE_NBIO;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case FIONREAD:
		mtx_lock(&audit_pipe_mtx);
		if (TAILQ_FIRST(&ap->ap_queue) != NULL)
			*(int *)data =
			    TAILQ_FIRST(&ap->ap_queue)->ape_record_len;
		else
			*(int *)data = 0;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case FIOASYNC:
		mtx_lock(&audit_pipe_mtx);
		if (*(int *)data)
			ap->ap_flags |= AUDIT_PIPE_ASYNC;
		else
			ap->ap_flags &= ~AUDIT_PIPE_ASYNC;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case FIOSETOWN:
		error = fsetown(*(int *)data, &ap->ap_sigio);
		break;

	case FIOGETOWN:
		*(int *)data = fgetown(&ap->ap_sigio);
		error = 0;
		break;

	case AUDITPIPE_GET_QLEN:
		*(u_int *)data = ap->ap_qlen;
		error = 0;
		break;

	case AUDITPIPE_GET_QLIMIT:
		*(u_int *)data = ap->ap_qlimit;
		error = 0;
		break;

	case AUDITPIPE_SET_QLIMIT:
		/* Lockless integer write. */
		if (*(u_int *)data >= AUDIT_PIPE_QLIMIT_MIN ||
		    *(u_int *)data <= AUDIT_PIPE_QLIMIT_MAX) {
			ap->ap_qlimit = *(u_int *)data;
			error = 0;
		} else
			error = EINVAL;
		break;

	case AUDITPIPE_GET_QLIMIT_MIN:
		*(u_int *)data = AUDIT_PIPE_QLIMIT_MIN;
		error = 0;
		break;

	case AUDITPIPE_GET_QLIMIT_MAX:
		*(u_int *)data = AUDIT_PIPE_QLIMIT_MAX;
		error = 0;
		break;

	case AUDITPIPE_GET_PRESELECT_FLAGS:
		mtx_lock(&audit_pipe_mtx);
		maskp = (au_mask_t *)data;
		*maskp = ap->ap_preselect_flags;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_SET_PRESELECT_FLAGS:
		mtx_lock(&audit_pipe_mtx);
		maskp = (au_mask_t *)data;
		ap->ap_preselect_flags = *maskp;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_GET_PRESELECT_NAFLAGS:
		mtx_lock(&audit_pipe_mtx);
		maskp = (au_mask_t *)data;
		*maskp = ap->ap_preselect_naflags;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_SET_PRESELECT_NAFLAGS:
		mtx_lock(&audit_pipe_mtx);
		maskp = (au_mask_t *)data;
		ap->ap_preselect_naflags = *maskp;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_GET_PRESELECT_AUID:
		aip = (struct auditpipe_ioctl_preselect *)data;
		error = audit_pipe_preselect_get(ap, aip->aip_auid,
		    &aip->aip_mask);
		break;

	case AUDITPIPE_SET_PRESELECT_AUID:
		aip = (struct auditpipe_ioctl_preselect *)data;
		audit_pipe_preselect_set(ap, aip->aip_auid, aip->aip_mask);
		error = 0;
		break;

	case AUDITPIPE_DELETE_PRESELECT_AUID:
		auid = *(au_id_t *)data;
		error = audit_pipe_preselect_delete(ap, auid);
		break;

	case AUDITPIPE_FLUSH_PRESELECT_AUID:
		audit_pipe_preselect_flush(ap);
		error = 0;
		break;

	case AUDITPIPE_GET_PRESELECT_MODE:
		mtx_lock(&audit_pipe_mtx);
		*(int *)data = ap->ap_preselect_mode;
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_SET_PRESELECT_MODE:
		mode = *(int *)data;
		switch (mode) {
		case AUDITPIPE_PRESELECT_MODE_TRAIL:
		case AUDITPIPE_PRESELECT_MODE_LOCAL:
			mtx_lock(&audit_pipe_mtx);
			ap->ap_preselect_mode = mode;
			mtx_unlock(&audit_pipe_mtx);
			error = 0;
			break;

		default:
			error = EINVAL;
		}
		break;

	case AUDITPIPE_FLUSH:
		mtx_lock(&audit_pipe_mtx);
		audit_pipe_flush(ap);
		mtx_unlock(&audit_pipe_mtx);
		error = 0;
		break;

	case AUDITPIPE_GET_MAXAUDITDATA:
		*(u_int *)data = MAXAUDITDATA;
		error = 0;
		break;

	case AUDITPIPE_GET_INSERTS:
		*(u_int *)data = ap->ap_inserts;
		error = 0;
		break;

	case AUDITPIPE_GET_READS:
		*(u_int *)data = ap->ap_reads;
		error = 0;
		break;

	case AUDITPIPE_GET_DROPS:
		*(u_int *)data = ap->ap_drops;
		error = 0;
		break;

	case AUDITPIPE_GET_TRUNCATES:
		*(u_int *)data = ap->ap_truncates;
		error = 0;
		break;

	default:
		error = ENOTTY;
	}
	return (error);
}

/*
 * Audit pipe read.  Pull one record off the queue and copy to user space.
 * On error, the record is dropped.
 *
 * Providing more sophisticated behavior, such as partial reads, is tricky
 * due to the potential for parallel I/O.  If partial read support is
 * required, it will require a per-pipe "current record being read" along
 * with an offset into that trecord which has already been read.  Threads
 * performing partial reads will need to allocate per-thread copies of the
 * data so that if another thread completes the read of the record, it can be
 * freed without adding reference count logic.  If this is added, a flag to
 * indicate that only atomic record reads are desired would be useful, as if
 * different threads are all waiting for records on the pipe, they will want
 * independent record reads, which is currently the behavior.
 */
static int
audit_pipe_read(struct cdev *dev, struct uio *uio, int flag)
{
	struct audit_pipe_entry *ape;
	struct audit_pipe *ap;
	int error;

	ap = dev->si_drv1;
	KASSERT(ap != NULL, ("audit_pipe_read: ap == NULL"));
	mtx_lock(&audit_pipe_mtx);
	do {
		/*
		 * Wait for a record that fits into the read buffer, dropping
		 * records that would be truncated if actually passed to the
		 * process.  This helps maintain the discreet record read
		 * interface.
		 */
		while ((ape = audit_pipe_pop(ap)) == NULL) {
			if (ap->ap_flags & AUDIT_PIPE_NBIO) {
				mtx_unlock(&audit_pipe_mtx);
				return (EAGAIN);
			}
			error = cv_wait_sig(&audit_pipe_cv, &audit_pipe_mtx);
			if (error) {
				mtx_unlock(&audit_pipe_mtx);
				return (error);
			}
		}
		if (ape->ape_record_len <= uio->uio_resid)
			break;
		audit_pipe_entry_free(ape);
		ap->ap_truncates++;
	} while (1);
	ap->ap_reads++;
	mtx_unlock(&audit_pipe_mtx);

	/*
	 * Now read record to user space memory.  Even if the read is short,
	 * we abandon the remainder of the record, supporting only discreet
	 * record reads.
	 */
	error = uiomove(ape->ape_record, ape->ape_record_len, uio);
	audit_pipe_entry_free(ape);
	return (error);
}

/*
 * Audit pipe poll.
 */
static int
audit_pipe_poll(struct cdev *dev, int events, struct thread *td)
{
	struct audit_pipe *ap;
	int revents;

	revents = 0;
	ap = dev->si_drv1;
	KASSERT(ap != NULL, ("audit_pipe_poll: ap == NULL"));
	if (events & (POLLIN | POLLRDNORM)) {
		mtx_lock(&audit_pipe_mtx);
		if (TAILQ_FIRST(&ap->ap_queue) != NULL)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(td, &ap->ap_selinfo);
		mtx_unlock(&audit_pipe_mtx);
	}
	return (revents);
}

/*
 * Audit pipe kqfilter.
 */
static int
audit_pipe_kqfilter(struct cdev *dev, struct knote *kn)
{
	struct audit_pipe *ap;

	ap = dev->si_drv1;
	KASSERT(ap != NULL, ("audit_pipe_kqfilter: ap == NULL"));

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);

	kn->kn_fop = &audit_pipe_read_filterops;
	kn->kn_hook = ap;

	mtx_lock(&audit_pipe_mtx);
	knlist_add(&ap->ap_selinfo.si_note, kn, 1);
	mtx_unlock(&audit_pipe_mtx);
	return (0);
}

/*
 * Return true if there are records available for reading on the pipe.
 */
static int
audit_pipe_kqread(struct knote *kn, long hint)
{
	struct audit_pipe_entry *ape;
	struct audit_pipe *ap;

	mtx_assert(&audit_pipe_mtx, MA_OWNED);

	ap = (struct audit_pipe *)kn->kn_hook;
	KASSERT(ap != NULL, ("audit_pipe_kqread: ap == NULL"));

	if (ap->ap_qlen != 0) {
		ape = TAILQ_FIRST(&ap->ap_queue);
		KASSERT(ape != NULL, ("audit_pipe_kqread: ape == NULL"));

		kn->kn_data = ape->ape_record_len;
		return (1);
	} else {
		kn->kn_data = 0;
		return (0);
	}
}

/*
 * Detach kqueue state from audit pipe.
 */
static void
audit_pipe_kqdetach(struct knote *kn)
{
	struct audit_pipe *ap;

	ap = (struct audit_pipe *)kn->kn_hook;
	KASSERT(ap != NULL, ("audit_pipe_kqdetach: ap == NULL"));

	mtx_lock(&audit_pipe_mtx);
	knlist_remove(&ap->ap_selinfo.si_note, kn, 1);
	mtx_unlock(&audit_pipe_mtx);
}

/*
 * Initialize the audit pipe system.
 */
static void
audit_pipe_init(void *unused)
{

	TAILQ_INIT(&audit_pipe_list);
	mtx_init(&audit_pipe_mtx, "audit_pipe_mtx", NULL, MTX_DEF);
	cv_init(&audit_pipe_cv, "audit_pipe_cv");

	clone_setup(&audit_pipe_clones);
	audit_pipe_eh_tag = EVENTHANDLER_REGISTER(dev_clone,
	    audit_pipe_clone, 0, 1000);
	if (audit_pipe_eh_tag == NULL)
		panic("audit_pipe_init: EVENTHANDLER_REGISTER");
}

SYSINIT(audit_pipe_init, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, audit_pipe_init,
    NULL);
