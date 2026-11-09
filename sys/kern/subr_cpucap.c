/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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

/*
 * CPU capability layer: per-CPU class, capacity and scores fed by
 * machine-dependent providers and consumed by the scheduler.  See
 * cpucap(9) and the overview in <sys/cpucap.h>.
 *
 * Storage and concurrency
 *
 * Values live in static MAXCPU arrays of one naturally aligned byte or
 * 16-bit word per CPU rather than in struct pcpu: the hot-path consumer
 * (SCHED_MIC) already keeps its derived tables in such arrays, so a candidate
 * CPU costs one load, and no per-architecture pcpu padding has to change.
 * Writers store each value with one relaxed atomic store and readers load it
 * with one relaxed atomic load, so a reader racing a provider sees the old
 * or the new value of a CPU, never a torn one.  No lock protects the
 * values themselves; there is at most one active provider per kind, so each
 * array has a single writer.
 *
 * cpucap_lock (a default mutex) protects the provider and consumer lists,
 * provider selection, the platform hints and the pending-change mask.
 * Consumer callbacks run with it held, in the caller's thread at boot and
 * from taskqueue_thread afterwards, so they serialize against registration
 * and against each other, and must not sleep.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cpucap.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/atomic.h>

/* Per-CPU tables; see the storage note above. */
static uint8_t	cpucap_cls[MAXCPU] = {
	[0 ... MAXCPU - 1] = CPU_CLASS_PERF
};
static uint16_t	cpucap_cap[MAXCPU] = {
	[0 ... MAXCPU - 1] = CPUCAP_CAPACITY_DEFAULT
};
/*
 * Scores, one row per thread class (see CPUCAP_MAX_CLASSES in <sys/cpucap.h>);
 * row 0 holds the plain scores.
 */
#define	CPUCAP_SCORE_ROW	{ [0 ... MAXCPU - 1] = CPUCAP_SCORE_DEFAULT }
static uint8_t	cpucap_perf_score[CPUCAP_MAX_CLASSES][MAXCPU] = {
	[0 ... CPUCAP_MAX_CLASSES - 1] = CPUCAP_SCORE_ROW
};
static uint8_t	cpucap_eff_score[CPUCAP_MAX_CLASSES][MAXCPU] = {
	[0 ... CPUCAP_MAX_CLASSES - 1] = CPUCAP_SCORE_ROW
};
/*
 * Scores as the dynamic provider last supplied them.  The tables above hold
 * what readers see: the provider's value, or the forced value for CPUs
 * covered by kern.cpucap.force_scores (class 0) or
 * kern.cpucap.force_class_scores (other classes), so clearing a forced
 * score brings the provider's value back.
 */
static uint8_t	cpucap_perf_prov[CPUCAP_MAX_CLASSES][MAXCPU] = {
	[0 ... CPUCAP_MAX_CLASSES - 1] = CPUCAP_SCORE_ROW
};
static uint8_t	cpucap_eff_prov[CPUCAP_MAX_CLASSES][MAXCPU] = {
	[0 ... CPUCAP_MAX_CLASSES - 1] = CPUCAP_SCORE_ROW
};
/*
 * Classes published: what readers see (the larger of the provider's count
 * and the highest forced class plus one) and the provider's own count.
 * Both are at least 1 and at most CPUCAP_MAX_CLASSES.
 */
static u_int	cpucap_ncls = 1;
static u_int	cpucap_ncls_prov = 1;
/* Class forced by the tunable below for CPUs 0..N-1; 0 means not forced. */
static uint8_t	cpucap_forced[MAXCPU];
/*
 * Scores forced by kern.cpucap.force_scores (row 0) and
 * kern.cpucap.force_class_scores (rows 1..) for CPUs 0..N-1 of each class.
 */
static uint8_t	cpucap_forced_perf[CPUCAP_MAX_CLASSES][MAXCPU];
static uint8_t	cpucap_forced_eff[CPUCAP_MAX_CLASSES][MAXCPU];
/* N above per class; 0 means none forced. */
static int	cpucap_nforced_scores[CPUCAP_MAX_CLASSES];

static struct mtx cpucap_lock;
MTX_SYSINIT(cpucap_lock, &cpucap_lock, "cpucap", MTX_DEF);

static SLIST_HEAD(, cpucap_provider) cpucap_providers =
    SLIST_HEAD_INITIALIZER(cpucap_providers);
static SLIST_HEAD(, cpucap_consumer) cpucap_consumers =
    SLIST_HEAD_INITIALIZER(cpucap_consumers);
static struct cpucap_provider *cpucap_static_active;
static struct cpucap_provider *cpucap_dynamic_active;

static uint8_t	cpucap_compute_cls = CPU_CLASS_PERF;
static uint8_t	cpucap_prefer_compute_hint = 1;
static uint8_t	cpucap_scores_valid;
/* Bumped on every commit, read by sysctl and consumers. */
static u_int	cpucap_gen;
/* CPUCAP_CHANGED_* bits accumulated by cpucap_set_*() since the last notify. */
static u_int	cpucap_pending;
/* Set once boot-time selection has run; commits are then asynchronous. */
static bool	cpucap_async;
static struct task cpucap_notify_task;

/*
 * kern.cpucap.force_classes="c,c,..." (loader tunable, debugging aid):
 * override the provided class of CPUs 0..N-1 so the hybrid placement paths
 * can be exercised on homogeneous hardware or in a VM without a CPUID hybrid
 * leaf.  Each entry must be 1 (perf), 2 (eff) or 4 (LP) and sets the class
 * and its nominal capacity (1024, 640 or 320); CPUs past the end
 * of the list keep their provided class.  Parsed once at boot; the whole
 * string is rejected if any entry is malformed or it names more than MAXCPU
 * CPUs.  "c," per CPU, with the last entry unterminated, fits in MAXCPU * 2
 * bytes including the NUL.  kern.sched.mic.force_classes is accepted as a
 * legacy alias.
 */
static char cpucap_force_classes[MAXCPU * 2];

/*
 * kern.cpucap.force_scores="p/e,p/e,..." (loader tunable and sysctl,
 * debugging aid): override the dynamic perf/eff scores of CPUs 0..N-1 so
 * that a scheduler's score handling can be exercised where no HFI exists
 * (bhyve) and runtime score updates can be simulated.  Each entry is two
 * decimal numbers 0..255 separated by '/'; CPUs past the end of the list
 * keep their provider's score, and an empty string clears every override.
 * While any score is forced cpucap_has_scores() is true even without a
 * dynamic provider.  A sysctl write is parsed in the caller's thread and
 * installed under cpucap_lock with a commit, so consumers see it exactly
 * as they would a provider update.  "ppp/eee," per CPU, with the last
 * entry unterminated, fits in MAXCPU * 8 bytes including the NUL.
 * cpucap_force_sx serializes writers, which parse into the staging buffer
 * and arrays before taking cpucap_lock.
 */
#define	CPUCAP_FORCE_SCORES_LEN	(MAXCPU * 8)
static char cpucap_force_scores[CPUCAP_FORCE_SCORES_LEN];
static char cpucap_force_scores_new[CPUCAP_FORCE_SCORES_LEN];
static uint8_t	cpucap_force_scores_perf[MAXCPU];
static uint8_t	cpucap_force_scores_eff[MAXCPU];
static struct sx cpucap_force_sx;
SX_SYSINIT(cpucap_force_sx, &cpucap_force_sx, "cpucap force");

/*
 * kern.cpucap.force_class_scores="c:p/e,p/e,...;c:..." (loader tunable
 * and sysctl, debugging aid): the same for the per-class scores of classes
 * 1..CPUCAP_MAX_CLASSES-1, so a scheduler's per-class placement can be
 * exercised where no Thread Director exists.  Each ';'-separated segment
 * names a class and a perf/eff list in the kern.cpucap.force_scores
 * format; class 0 is refused (use force_scores).  While a class is forced
 * cpucap_nclasses() covers it.  A write is received in the _new buffer,
 * parsed from a scratch copy that the parser NUL-terminates in place, and
 * only copied over the live string once it parsed; all three buffers and
 * the staging arrays are protected by cpucap_force_sx.
 */
#define	CPUCAP_FORCE_CLASS_SCORES_LEN	4096
static char cpucap_force_class_scores[CPUCAP_FORCE_CLASS_SCORES_LEN];
static char cpucap_force_class_scores_new[CPUCAP_FORCE_CLASS_SCORES_LEN];
static char cpucap_force_class_scores_buf[CPUCAP_FORCE_CLASS_SCORES_LEN];
static uint8_t	cpucap_force_class_perf[CPUCAP_MAX_CLASSES][MAXCPU];
static uint8_t	cpucap_force_class_eff[CPUCAP_MAX_CLASSES][MAXCPU];
static int	cpucap_force_class_n[CPUCAP_MAX_CLASSES];

static void	cpucap_notify(void *arg, int pending);

static bool
cpucap_class_valid(uint8_t cls)
{

	return (cls == CPU_CLASS_PERF || cls == CPU_CLASS_EFF ||
	    cls == CPU_CLASS_LP);
}

static bool
cpucap_cpu_valid(int cpu)
{

	return (cpu >= 0 && cpu < MAXCPU);
}

/* The nominal capacity of a class, used for CPUs whose class is forced. */
static uint16_t
cpucap_class_capacity(uint8_t cls)
{

	switch (cls) {
	case CPU_CLASS_EFF:
		return (CPUCAP_CAPACITY_EFF_NOMINAL);
	case CPU_CLASS_LP:
		return (CPUCAP_CAPACITY_LP_NOMINAL);
	default:
		return (CPUCAP_CAPACITY_PERF_NOMINAL);
	}
}

/*
 * Read interface.
 */

uint8_t
cpucap_class(int cpu)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPU_CLASS_PERF);
	return (atomic_load_8(&cpucap_cls[cpu]));
}

uint16_t
cpucap_capacity(int cpu)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPUCAP_CAPACITY_DEFAULT);
	return (atomic_load_16(&cpucap_cap[cpu]));
}

uint8_t
cpucap_perf(int cpu)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPUCAP_SCORE_DEFAULT);
	return (atomic_load_8(&cpucap_perf_score[0][cpu]));
}

uint8_t
cpucap_eff(int cpu)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPUCAP_SCORE_DEFAULT);
	return (atomic_load_8(&cpucap_eff_score[0][cpu]));
}

u_int
cpucap_nclasses(void)
{

	return (atomic_load_int(&cpucap_ncls));
}

/* A class at or beyond the published count reads as class 0. */
static u_int
cpucap_class_index(u_int cls)
{

	if (cls >= CPUCAP_MAX_CLASSES || cls >= cpucap_nclasses())
		return (0);
	return (cls);
}

uint8_t
cpucap_class_perf(int cpu, u_int cls)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPUCAP_SCORE_DEFAULT);
	cls = cpucap_class_index(cls);
	return (atomic_load_8(&cpucap_perf_score[cls][cpu]));
}

uint8_t
cpucap_class_eff(int cpu, u_int cls)
{

	if (!cpucap_cpu_valid(cpu))
		return (CPUCAP_SCORE_DEFAULT);
	cls = cpucap_class_index(cls);
	return (atomic_load_8(&cpucap_eff_score[cls][cpu]));
}

uint8_t
cpucap_compute_class(void)
{

	return (atomic_load_8(&cpucap_compute_cls));
}

bool
cpucap_prefer_compute(void)
{

	return (atomic_load_8(&cpucap_prefer_compute_hint) != 0);
}

bool
cpucap_has_scores(void)
{

	return (atomic_load_8(&cpucap_scores_valid) != 0);
}

u_int
cpucap_generation(void)
{

	return (atomic_load_acq_int(&cpucap_gen));
}

const char *
cpucap_force_classes_string(void)
{

	return (cpucap_force_classes);
}

/*
 * Provider write interface.
 */

void
cpucap_set_class(int cpu, uint8_t cls)
{

	if (!cpucap_cpu_valid(cpu) || !cpucap_class_valid(cls))
		return;
	/*
	 * A forced class brings its nominal capacity with it, whatever the
	 * provider measured for the real core, so a forced map looks the
	 * same to consumers whichever CPUs it lands on.
	 */
	if (cpucap_forced[cpu] != 0) {
		cls = cpucap_forced[cpu];
		atomic_store_16(&cpucap_cap[cpu], cpucap_class_capacity(cls));
	}
	atomic_store_8(&cpucap_cls[cpu], cls);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_STATIC);
}

void
cpucap_set_capacity(int cpu, uint16_t capacity)
{

	if (!cpucap_cpu_valid(cpu) || capacity > CPUCAP_CAPACITY_MAX)
		return;
	if (cpucap_forced[cpu] != 0)
		return;		/* Fixed by the forced class, see above. */
	atomic_store_16(&cpucap_cap[cpu], capacity);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_STATIC);
}

void
cpucap_set_class_scores(int cpu, u_int cls, uint8_t perf, uint8_t eff)
{

	if (!cpucap_cpu_valid(cpu) || cls >= CPUCAP_MAX_CLASSES)
		return;
	/*
	 * Providers write without the lock, so an update that races a
	 * kern.cpucap.force_scores write clearing this CPU's override may
	 * be published only by the provider's next commit.
	 */
	cpucap_perf_prov[cls][cpu] = perf;
	cpucap_eff_prov[cls][cpu] = eff;
	if (cpu < cpucap_nforced_scores[cls])
		return;		/* Readers see the forced score. */
	atomic_store_8(&cpucap_perf_score[cls][cpu], perf);
	atomic_store_8(&cpucap_eff_score[cls][cpu], eff);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_SCORES);
}

void
cpucap_set_scores(int cpu, uint8_t perf, uint8_t eff)
{

	cpucap_set_class_scores(cpu, 0, perf, eff);
}

/* Highest class with a forced score, plus one; at least 1. */
static u_int
cpucap_forced_nclasses(void)
{
	u_int cls, n;

	n = 1;
	for (cls = 1; cls < CPUCAP_MAX_CLASSES; cls++)
		if (cpucap_nforced_scores[cls] > 0)
			n = cls + 1;
	return (n);
}

void
cpucap_set_nclasses(u_int nclasses)
{

	if (nclasses < 1 || nclasses > CPUCAP_MAX_CLASSES)
		return;
	cpucap_ncls_prov = nclasses;
	atomic_store_int(&cpucap_ncls, MAX(nclasses,
	    cpucap_forced_nclasses()));
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_SCORES);
}

void
cpucap_set_compute_class(uint8_t cls, bool prefer_compute)
{

	if (cls != CPU_CLASS_PERF && cls != CPU_CLASS_EFF)
		return;
	atomic_store_8(&cpucap_compute_cls, cls);
	atomic_store_8(&cpucap_prefer_compute_hint, prefer_compute ? 1 : 0);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_STATIC);
}

/*
 * Run the consumer callbacks for every change committed so far.  Loops
 * until no commit raced with a notification, so a consumer always ends up
 * having seen the latest values.
 */
static void
cpucap_notify_locked(void)
{
	struct cpucap_consumer *cc;
	int changed;

	mtx_assert(&cpucap_lock, MA_OWNED);
	for (;;) {
		changed = atomic_readandclear_int(&cpucap_pending);
		if (changed == 0)
			break;
		SLIST_FOREACH(cc, &cpucap_consumers, cc_link)
			cc->cc_update(cc->cc_arg, changed);
	}
}

static void
cpucap_notify(void *arg __unused, int pending __unused)
{

	mtx_lock(&cpucap_lock);
	cpucap_notify_locked();
	mtx_unlock(&cpucap_lock);
}

static void
cpucap_commit_locked(void)
{

	mtx_assert(&cpucap_lock, MA_OWNED);
	atomic_add_rel_int(&cpucap_gen, 1);
	if (cpucap_async) {
		/* Enqueueing an already pending task coalesces commits. */
		taskqueue_enqueue(taskqueue_thread, &cpucap_notify_task);
		return;
	}
	cpucap_notify_locked();
}

void
cpucap_commit(void)
{

	mtx_lock(&cpucap_lock);
	cpucap_commit_locked();
	mtx_unlock(&cpucap_lock);
}

/*
 * Provider selection.
 */

/* Reset the static tables to their defaults, honouring forced classes. */
static void
cpucap_reset_static(void)
{
	int cpu;

	for (cpu = 0; cpu < MAXCPU; cpu++) {
		if (cpucap_forced[cpu] != 0) {
			atomic_store_8(&cpucap_cls[cpu], cpucap_forced[cpu]);
			atomic_store_16(&cpucap_cap[cpu],
			    cpucap_class_capacity(cpucap_forced[cpu]));
		} else {
			atomic_store_8(&cpucap_cls[cpu], CPU_CLASS_PERF);
			atomic_store_16(&cpucap_cap[cpu],
			    CPUCAP_CAPACITY_DEFAULT);
		}
	}
	atomic_store_8(&cpucap_compute_cls, CPU_CLASS_PERF);
	atomic_store_8(&cpucap_prefer_compute_hint, 1);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_STATIC);
}

/*
 * Publish every CPU's score in every class: the forced one if any, else
 * the provider's.
 */
static void
cpucap_publish_scores(void)
{
	u_int cls;
	int cpu;

	mtx_assert(&cpucap_lock, MA_OWNED);
	for (cls = 0; cls < CPUCAP_MAX_CLASSES; cls++) {
		for (cpu = 0; cpu < MAXCPU; cpu++) {
			if (cpu < cpucap_nforced_scores[cls]) {
				atomic_store_8(&cpucap_perf_score[cls][cpu],
				    cpucap_forced_perf[cls][cpu]);
				atomic_store_8(&cpucap_eff_score[cls][cpu],
				    cpucap_forced_eff[cls][cpu]);
			} else {
				atomic_store_8(&cpucap_perf_score[cls][cpu],
				    cpucap_perf_prov[cls][cpu]);
				atomic_store_8(&cpucap_eff_score[cls][cpu],
				    cpucap_eff_prov[cls][cpu]);
			}
		}
	}
	atomic_store_int(&cpucap_ncls, MAX(cpucap_ncls_prov,
	    cpucap_forced_nclasses()));
	atomic_store_8(&cpucap_scores_valid, cpucap_dynamic_active != NULL ||
	    cpucap_nforced_scores[0] > 0 ? 1 : 0);
	atomic_set_int(&cpucap_pending, CPUCAP_CHANGED_SCORES);
}

static void
cpucap_reset_scores(void)
{
	u_int cls;
	int cpu;

	for (cls = 0; cls < CPUCAP_MAX_CLASSES; cls++)
		for (cpu = 0; cpu < MAXCPU; cpu++) {
			cpucap_perf_prov[cls][cpu] = CPUCAP_SCORE_DEFAULT;
			cpucap_eff_prov[cls][cpu] = CPUCAP_SCORE_DEFAULT;
		}
	cpucap_ncls_prov = 1;
	cpucap_publish_scores();
}

/*
 * Make cp the active provider of its kind: reset that kind's tables, run
 * its init routine and commit.  On failure the tables stay at their
 * defaults and the previous provider, if any, stays deactivated; its values
 * were overwritten by the reset and cannot be recovered.
 */
static void
cpucap_activate(struct cpucap_provider *cp)
{
	struct cpucap_provider **active;
	int error;

	mtx_assert(&cpucap_lock, MA_OWNED);
	active = cp->cp_kind == CPUCAP_PROVIDER_STATIC ?
	    &cpucap_static_active : &cpucap_dynamic_active;
	if (*active != NULL) {
		(*active)->cp_state = CPUCAP_PS_PROBED;
		*active = NULL;
	}
	if (cp->cp_kind == CPUCAP_PROVIDER_STATIC)
		cpucap_reset_static();
	else
		cpucap_reset_scores();
	error = cp->cp_init();
	if (error != 0) {
		printf("cpucap: %s provider %s failed to initialize: %d\n",
		    cp->cp_kind == CPUCAP_PROVIDER_STATIC ? "static" :
		    "dynamic", cp->cp_name, error);
		cp->cp_state = CPUCAP_PS_FAILED;
	} else {
		cp->cp_state = CPUCAP_PS_ACTIVE;
		*active = cp;
		if (cp->cp_kind == CPUCAP_PROVIDER_DYNAMIC)
			cpucap_publish_scores();
	}
	cpucap_commit_locked();
}

/*
 * Probe every registered provider that has not been probed and activate the
 * best usable one of each kind, if it beats the current one.
 */
static void
cpucap_select(int kind)
{
	struct cpucap_provider *cp, *best, *active;

	mtx_assert(&cpucap_lock, MA_OWNED);
	active = kind == CPUCAP_PROVIDER_STATIC ? cpucap_static_active :
	    cpucap_dynamic_active;
	best = NULL;
	SLIST_FOREACH(cp, &cpucap_providers, cp_link) {
		if (cp->cp_kind != kind)
			continue;
		if (cp->cp_state == CPUCAP_PS_REGISTERED)
			cp->cp_state = cp->cp_probe() == 0 ? CPUCAP_PS_PROBED :
			    CPUCAP_PS_ABSENT;
		if (cp->cp_state != CPUCAP_PS_PROBED)
			continue;
		if (best == NULL || cp->cp_priority > best->cp_priority)
			best = cp;
	}
	if (best == NULL)
		return;
	if (active != NULL && active->cp_priority >= best->cp_priority)
		return;
	cpucap_activate(best);
}

int
cpucap_provider_register(struct cpucap_provider *cp)
{
	struct cpucap_provider *p;

	if (cp == NULL || cp->cp_name == NULL || cp->cp_probe == NULL ||
	    cp->cp_init == NULL || (cp->cp_kind != CPUCAP_PROVIDER_STATIC &&
	    cp->cp_kind != CPUCAP_PROVIDER_DYNAMIC))
		return (EINVAL);
	mtx_lock(&cpucap_lock);
	SLIST_FOREACH(p, &cpucap_providers, cp_link) {
		if (p == cp) {
			mtx_unlock(&cpucap_lock);
			return (EEXIST);
		}
	}
	cp->cp_state = CPUCAP_PS_REGISTERED;
	SLIST_INSERT_HEAD(&cpucap_providers, cp, cp_link);
	/* Before boot-time selection the SYSINIT below does the probing. */
	if (cpucap_async)
		cpucap_select(cp->cp_kind);
	mtx_unlock(&cpucap_lock);
	return (0);
}

void
cpucap_provider_unregister(struct cpucap_provider *cp)
{

	mtx_lock(&cpucap_lock);
	if (cp == cpucap_static_active) {
		cpucap_static_active = NULL;
		cpucap_reset_static();
	} else if (cp == cpucap_dynamic_active) {
		cpucap_dynamic_active = NULL;
		cpucap_reset_scores();
	}
	SLIST_REMOVE(&cpucap_providers, cp, cpucap_provider, cp_link);
	/* Fall back to the next best provider of that kind, if any. */
	cpucap_select(cp->cp_kind);
	if (cpucap_pending != 0)
		cpucap_commit_locked();
	mtx_unlock(&cpucap_lock);
}

/*
 * Consumers.
 */

void
cpucap_consumer_register(struct cpucap_consumer *cc)
{

	KASSERT(cc != NULL && cc->cc_update != NULL,
	    ("cpucap: consumer without update callback"));
	mtx_lock(&cpucap_lock);
	SLIST_INSERT_HEAD(&cpucap_consumers, cc, cc_link);
	/* Catch up on anything already committed. */
	if (cpucap_gen != 0)
		cc->cc_update(cc->cc_arg,
		    CPUCAP_CHANGED_STATIC | CPUCAP_CHANGED_SCORES);
	mtx_unlock(&cpucap_lock);
}

void
cpucap_consumer_unregister(struct cpucap_consumer *cc)
{

	mtx_lock(&cpucap_lock);
	SLIST_REMOVE(&cpucap_consumers, cc, cpucap_consumer, cc_link);
	mtx_unlock(&cpucap_lock);
}

/*
 * Boot.
 */

static void
cpucap_force_classes_parse(void)
{
	uint8_t cls[MAXCPU];
	const char *p;
	char *end;
	long v;
	int c, i;

	if (cpucap_force_classes[0] == '\0')
		TUNABLE_STR_FETCH("kern.sched.mic.force_classes",
		    cpucap_force_classes, sizeof(cpucap_force_classes));
	if (cpucap_force_classes[0] == '\0')
		return;
	p = cpucap_force_classes;
	for (c = 0;; c++) {
		if (c >= MAXCPU)
			goto bad;
		v = strtol(p, &end, 10);
		if (end == p || (v != CPU_CLASS_PERF && v != CPU_CLASS_EFF &&
		    v != CPU_CLASS_LP))
			goto bad;
		cls[c] = (uint8_t)v;
		if (*end == '\0')
			break;
		if (*end != ',')
			goto bad;
		p = end + 1;
	}
	c++;
	for (i = 0; i < c; i++)
		cpucap_forced[i] = cls[i];
	printf("cpucap: core class of %d CPU%s forced by "
	    "kern.cpucap.force_classes\n", c, c == 1 ? "" : "s");
	return;
bad:
	printf("cpucap: ignoring malformed kern.cpucap.force_classes "
	    "\"%s\" (expected a comma-separated list of 1, 2 or 4, at most "
	    "%d entries)\n", cpucap_force_classes, MAXCPU);
}

/*
 * Parse a force_scores string into perf[]/eff[] and return the number of
 * entries, or -1 after a console message if it is malformed.  An empty
 * string is zero entries.
 */
static int
cpucap_force_scores_parse(const char *str, uint8_t *perf, uint8_t *eff)
{
	const char *p;
	char *end;
	long v;
	int c;

	if (str[0] == '\0')
		return (0);
	p = str;
	for (c = 0;; c++) {
		if (c >= MAXCPU)
			goto bad;
		v = strtol(p, &end, 10);
		if (end == p || *end != '/' || v < 0 || v > CPUCAP_SCORE_MAX)
			goto bad;
		perf[c] = (uint8_t)v;
		p = end + 1;
		v = strtol(p, &end, 10);
		if (end == p || v < 0 || v > CPUCAP_SCORE_MAX)
			goto bad;
		eff[c] = (uint8_t)v;
		if (*end == '\0')
			break;
		if (*end != ',')
			goto bad;
		p = end + 1;
	}
	return (c + 1);
bad:
	printf("cpucap: ignoring malformed kern.cpucap.force_scores \"%s\" "
	    "(expected a comma-separated list of perf/eff pairs, each 0-%d, "
	    "at most %d entries)\n", str, CPUCAP_SCORE_MAX, MAXCPU);
	return (-1);
}

/*
 * Install parsed forced scores for one class; the caller republishes and
 * commits.
 */
static void
cpucap_force_scores_install(u_int cls, const uint8_t *perf,
    const uint8_t *eff, int n)
{
	int cpu;

	mtx_assert(&cpucap_lock, MA_OWNED);
	KASSERT(cls < CPUCAP_MAX_CLASSES, ("cpucap: forced class %u", cls));
	KASSERT(n >= 0 && n <= MAXCPU, ("cpucap: %d forced scores", n));
	for (cpu = 0; cpu < n; cpu++) {
		cpucap_forced_perf[cls][cpu] = perf[cpu];
		cpucap_forced_eff[cls][cpu] = eff[cpu];
	}
	cpucap_nforced_scores[cls] = n;
}

/* Install parsed class-0 forced scores and republish; the caller commits. */
static void
cpucap_force_scores_apply(const uint8_t *perf, const uint8_t *eff, int n)
{

	cpucap_force_scores_install(0, perf, eff, n);
	cpucap_publish_scores();
}

/*
 * Parse kern.cpucap.force_class_scores into the staging arrays (str is
 * the writable staging buffer; segments are NUL-terminated in place).
 * Returns 0, or -1 after a console message if malformed.
 */
static int
cpucap_force_class_scores_parse(char *str)
{
	char *end, *seg, *next;
	long cls;
	int n;

	memset(cpucap_force_class_n, 0, sizeof(cpucap_force_class_n));
	for (seg = str; seg != NULL && *seg != '\0'; seg = next) {
		next = strchr(seg, ';');
		if (next != NULL)
			*next++ = '\0';
		cls = strtol(seg, &end, 10);
		if (end == seg || *end != ':' || cls < 1 ||
		    cls >= CPUCAP_MAX_CLASSES)
			goto bad;
		n = cpucap_force_scores_parse(end + 1,
		    cpucap_force_class_perf[cls], cpucap_force_class_eff[cls]);
		if (n < 0)
			return (-1);	/* Already reported. */
		cpucap_force_class_n[cls] = n;
	}
	return (0);
bad:
	printf("cpucap: ignoring malformed kern.cpucap.force_class_scores "
	    "(expected \"class:perf/eff,...;class:...\" with class 1-%d)\n",
	    CPUCAP_MAX_CLASSES - 1);
	return (-1);
}

/* Install every class from the staging arrays and republish. */
static void
cpucap_force_class_scores_apply(void)
{
	u_int cls;

	mtx_assert(&cpucap_lock, MA_OWNED);
	for (cls = 1; cls < CPUCAP_MAX_CLASSES; cls++)
		cpucap_force_scores_install(cls, cpucap_force_class_perf[cls],
		    cpucap_force_class_eff[cls], cpucap_force_class_n[cls]);
	cpucap_publish_scores();
}

/*
 * Runs after release_aps() (SI_ORDER_FIRST) and after providers have
 * registered (SI_ORDER_SECOND).  The commit made here runs consumers
 * synchronously; everything after it goes through the taskqueue.
 */
static void
cpucap_init(void *arg __unused)
{
	int n;

	TASK_INIT(&cpucap_notify_task, 0, cpucap_notify, NULL);
	TUNABLE_STR_FETCH("kern.cpucap.force_scores", cpucap_force_scores,
	    sizeof(cpucap_force_scores));
	n = cpucap_force_scores_parse(cpucap_force_scores,
	    cpucap_force_scores_perf, cpucap_force_scores_eff);
	if (n < 0) {
		cpucap_force_scores[0] = '\0';
		n = 0;
	}
	TUNABLE_STR_FETCH("kern.cpucap.force_class_scores",
	    cpucap_force_class_scores, sizeof(cpucap_force_class_scores));
	strlcpy(cpucap_force_class_scores_buf, cpucap_force_class_scores,
	    sizeof(cpucap_force_class_scores_buf));
	if (cpucap_force_class_scores_parse(cpucap_force_class_scores_buf)
	    != 0) {
		cpucap_force_class_scores[0] = '\0';
		memset(cpucap_force_class_n, 0, sizeof(cpucap_force_class_n));
	}
	mtx_lock(&cpucap_lock);
	cpucap_force_classes_parse();
	cpucap_reset_static();
	if (n > 0) {
		cpucap_force_scores_apply(cpucap_force_scores_perf,
		    cpucap_force_scores_eff, n);
		printf("cpucap: scores of %d CPU%s forced by "
		    "kern.cpucap.force_scores\n", n, n == 1 ? "" : "s");
	}
	if (cpucap_force_class_scores[0] != '\0') {
		cpucap_force_class_scores_apply();
		printf("cpucap: per-class scores forced by "
		    "kern.cpucap.force_class_scores\n");
	}
	cpucap_select(CPUCAP_PROVIDER_STATIC);
	cpucap_select(CPUCAP_PROVIDER_DYNAMIC);
	if (cpucap_pending != 0)
		cpucap_commit_locked();
	cpucap_async = true;
	mtx_unlock(&cpucap_lock);
}
SYSINIT(cpucap, SI_SUB_SMP, SI_ORDER_MIDDLE, cpucap_init, NULL);

/*
 * Sysctl export.
 */

SYSCTL_NODE(_kern, OID_AUTO, cpucap, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Per-CPU capability layer (class, capacity, scores)");

SYSCTL_UINT(_kern_cpucap, OID_AUTO, generation, CTLFLAG_RD, &cpucap_gen, 0,
    "Number of provider commits so far");
SYSCTL_U8(_kern_cpucap, OID_AUTO, has_scores, CTLFLAG_RD,
    &cpucap_scores_valid, 0,
    "A dynamic provider or kern.cpucap.force_scores is supplying "
    "perf/eff scores");
SYSCTL_U8(_kern_cpucap, OID_AUTO, compute_class, CTLFLAG_RD,
    &cpucap_compute_cls, 0,
    "Core class holding this platform's compute cores (1=perf 2=eff)");
SYSCTL_U8(_kern_cpucap, OID_AUTO, prefer_compute, CTLFLAG_RD,
    &cpucap_prefer_compute_hint, 0,
    "Platform default for kern.sched.prefer_compute");
SYSCTL_STRING(_kern_cpucap, OID_AUTO, force_classes, CTLFLAG_RDTUN,
    cpucap_force_classes, sizeof(cpucap_force_classes),
    "Boot-time override of per-CPU core classes, e.g. \"1,1,2,2,4\" "
    "(1=perf 2=eff 4=LP), applied to CPUs 0..N-1 in order");

/*
 * Writable at run time; the tunable is fetched by cpucap_init() instead of
 * the sysctl layer (CTLFLAG_NOFETCH) so it is applied once the lock exists
 * and before provider selection.
 */
static int
sysctl_cpucap_force_scores(SYSCTL_HANDLER_ARGS)
{
	int error, n;

	sx_xlock(&cpucap_force_sx);
	strlcpy(cpucap_force_scores_new, cpucap_force_scores,
	    sizeof(cpucap_force_scores_new));
	error = sysctl_handle_string(oidp, cpucap_force_scores_new,
	    sizeof(cpucap_force_scores_new), req);
	if (error != 0 || req->newptr == NULL)
		goto out;
	n = cpucap_force_scores_parse(cpucap_force_scores_new,
	    cpucap_force_scores_perf, cpucap_force_scores_eff);
	if (n < 0) {
		error = EINVAL;
		goto out;
	}
	mtx_lock(&cpucap_lock);
	cpucap_force_scores_apply(cpucap_force_scores_perf,
	    cpucap_force_scores_eff, n);
	strlcpy(cpucap_force_scores, cpucap_force_scores_new,
	    sizeof(cpucap_force_scores));
	cpucap_commit_locked();
	mtx_unlock(&cpucap_lock);
out:
	sx_xunlock(&cpucap_force_sx);
	return (error);
}
SYSCTL_PROC(_kern_cpucap, OID_AUTO, force_scores, CTLTYPE_STRING |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_cpucap_force_scores, "A",
    "Debug override of per-CPU perf/eff scores, e.g. \"60/92,34/100\" "
    "(each 0-255), applied to CPUs 0..N-1 in order; \"\" clears it");

static int
sysctl_cpucap_force_class_scores(SYSCTL_HANDLER_ARGS)
{
	int error;

	sx_xlock(&cpucap_force_sx);
	strlcpy(cpucap_force_class_scores_new, cpucap_force_class_scores,
	    sizeof(cpucap_force_class_scores_new));
	error = sysctl_handle_string(oidp, cpucap_force_class_scores_new,
	    sizeof(cpucap_force_class_scores_new), req);
	if (error != 0 || req->newptr == NULL)
		goto out;
	/* The parser NUL-terminates segments in place, so parse a copy. */
	strlcpy(cpucap_force_class_scores_buf, cpucap_force_class_scores_new,
	    sizeof(cpucap_force_class_scores_buf));
	if (cpucap_force_class_scores_parse(cpucap_force_class_scores_buf)
	    != 0) {
		error = EINVAL;
		goto out;
	}
	mtx_lock(&cpucap_lock);
	cpucap_force_class_scores_apply();
	strlcpy(cpucap_force_class_scores, cpucap_force_class_scores_new,
	    sizeof(cpucap_force_class_scores));
	cpucap_commit_locked();
	mtx_unlock(&cpucap_lock);
out:
	sx_xunlock(&cpucap_force_sx);
	return (error);
}
SYSCTL_PROC(_kern_cpucap, OID_AUTO, force_class_scores, CTLTYPE_STRING |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    sysctl_cpucap_force_class_scores, "A",
    "Debug override of per-class perf/eff scores, e.g. "
    "\"1:60/92,34/100;3:10/10\", applied to CPUs 0..N-1 of each class; "
    "\"\" clears it");

SYSCTL_UINT(_kern_cpucap, OID_AUTO, classes, CTLFLAG_RD, &cpucap_ncls, 0,
    "Thread classes with scores (1 = class 0 only)");

/* Every class's perf/eff for every present CPU: "cpu:p/e,p/e,...". */
static int
sysctl_cpucap_class_scores(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *sb;
	u_int cls, n;
	int cpu, error;

	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL)
		return (ENOMEM);
	n = cpucap_nclasses();
	CPU_FOREACH(cpu) {
		sbuf_printf(sb, "%s%d:", cpu == 0 ? "" : " ", cpu);
		for (cls = 0; cls < n && cls < CPUCAP_MAX_CLASSES; cls++)
			sbuf_printf(sb, "%s%u/%u", cls == 0 ? "" : ",",
			    cpucap_class_perf(cpu, cls),
			    cpucap_class_eff(cpu, cls));
	}
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}
SYSCTL_PROC(_kern_cpucap, OID_AUTO, class_scores, CTLTYPE_STRING |
    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0, sysctl_cpucap_class_scores, "A",
    "Per-CPU perf/eff score of every thread class, class 0 first");

#define	CPUCAP_FIELD_CLASS	0
#define	CPUCAP_FIELD_CAPACITY	1
#define	CPUCAP_FIELD_PERF	2
#define	CPUCAP_FIELD_EFF	3

static u_int
cpucap_field(int cpu, int field)
{

	switch (field) {
	case CPUCAP_FIELD_CLASS:
		return (cpucap_class(cpu));
	case CPUCAP_FIELD_CAPACITY:
		return (cpucap_capacity(cpu));
	case CPUCAP_FIELD_PERF:
		return (cpucap_perf(cpu));
	default:
		return (cpucap_eff(cpu));
	}
}

/* One value for every present CPU as "cpuid:value" pairs; arg2 is the field. */
static int
sysctl_cpucap_table(SYSCTL_HANDLER_ARGS)
{
	struct sbuf *sb;
	int cpu, error;

	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL)
		return (ENOMEM);
	CPU_FOREACH(cpu)
		sbuf_printf(sb, "%s%d:%u", cpu == 0 ? "" : " ", cpu,
		    cpucap_field(cpu, arg2));
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_kern_cpucap, OID_AUTO, class, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, CPUCAP_FIELD_CLASS, sysctl_cpucap_table, "A",
    "Per-CPU core class (1=perf 2=eff 4=LP)");
SYSCTL_PROC(_kern_cpucap, OID_AUTO, capacity, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, CPUCAP_FIELD_CAPACITY, sysctl_cpucap_table, "A",
    "Per-CPU static capacity (0-1024)");
SYSCTL_PROC(_kern_cpucap, OID_AUTO, perf, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, CPUCAP_FIELD_PERF, sysctl_cpucap_table, "A",
    "Per-CPU dynamic performance score (0-255)");
SYSCTL_PROC(_kern_cpucap, OID_AUTO, eff, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, CPUCAP_FIELD_EFF, sysctl_cpucap_table, "A",
    "Per-CPU dynamic efficiency score (0-255)");

static const char *
cpucap_state_name(int state)
{

	switch (state) {
	case CPUCAP_PS_REGISTERED:
		return ("registered");
	case CPUCAP_PS_ABSENT:
		return ("absent");
	case CPUCAP_PS_FAILED:
		return ("failed");
	case CPUCAP_PS_PROBED:
		return ("probed");
	case CPUCAP_PS_ACTIVE:
		return ("active");
	default:
		return ("?");
	}
}

static int
sysctl_cpucap_providers(SYSCTL_HANDLER_ARGS)
{
	struct cpucap_provider *cp;
	struct sbuf *sb;
	int error;
	bool first;

	sb = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (sb == NULL)
		return (ENOMEM);
	first = true;
	mtx_lock(&cpucap_lock);
	SLIST_FOREACH(cp, &cpucap_providers, cp_link) {
		sbuf_printf(sb, "%s%s(%s,%d,%s)", first ? "" : " ",
		    cp->cp_name, cp->cp_kind == CPUCAP_PROVIDER_STATIC ?
		    "static" : "dynamic", cp->cp_priority,
		    cpucap_state_name(cp->cp_state));
		first = false;
	}
	mtx_unlock(&cpucap_lock);
	error = sbuf_finish(sb);
	sbuf_delete(sb);
	return (error);
}

SYSCTL_PROC(_kern_cpucap, OID_AUTO, providers, CTLTYPE_STRING | CTLFLAG_RD |
    CTLFLAG_MPSAFE, NULL, 0, sysctl_cpucap_providers, "A",
    "Registered providers as name(kind,priority,state)");

/* Per-CPU value; arg2 is cpu | field << 16. */
static int
sysctl_cpucap_cpu_field(SYSCTL_HANDLER_ARGS)
{
	u_int v;

	v = cpucap_field(arg2 & 0xffff, arg2 >> 16);
	return (sysctl_handle_int(oidp, &v, 0, req));
}

static void
cpucap_sysctl_init(void *arg __unused)
{
	static const struct {
		const char	*name;
		const char	*descr;
	} fields[] = {
		{ "class", "Core class (1=perf 2=eff 4=LP)" },
		{ "capacity", "Static capacity (0-1024)" },
		{ "perf", "Dynamic performance score (0-255)" },
		{ "eff", "Dynamic efficiency score (0-255)" },
	};
	struct sysctl_oid *root, *node;
	char name[8];
	int cpu, f;

	root = SYSCTL_ADD_NODE(NULL, SYSCTL_STATIC_CHILDREN(_kern_cpucap),
	    OID_AUTO, "cpu", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "Per-CPU capabilities");
	if (root == NULL)
		return;
	CPU_FOREACH(cpu) {
		snprintf(name, sizeof(name), "%d", cpu);
		node = SYSCTL_ADD_NODE(NULL, SYSCTL_CHILDREN(root), OID_AUTO,
		    name, CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "CPU");
		if (node == NULL)
			continue;
		for (f = 0; f < (int)nitems(fields); f++)
			SYSCTL_ADD_PROC(NULL, SYSCTL_CHILDREN(node), OID_AUTO,
			    fields[f].name, CTLTYPE_UINT | CTLFLAG_RD |
			    CTLFLAG_MPSAFE, NULL, cpu | (f << 16),
			    sysctl_cpucap_cpu_field, "IU", fields[f].descr);
	}
}
SYSINIT(cpucap_sysctl, SI_SUB_SMP, SI_ORDER_ANY, cpucap_sysctl_init, NULL);

MODULE_VERSION(cpucap, 1);
