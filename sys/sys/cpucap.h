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
 * CPU capability layer (cpucap(9)).
 *
 * A machine-independent store of per-CPU scheduling capabilities that sits
 * between machine-dependent providers (the x86 CPUID hybrid probe today, a
 * driver for Intel's Hardware Feedback Interface later) and the scheduler.
 * It is modelled on FreeBSD's proposed hmp(4) (reviews D56546/D56547) so the
 * two can converge: each CPU has a static capacity (0..1024, 1024 by default)
 * set once at boot and optional dynamic performance/efficiency scores that a
 * driver may update at run time.  MidnightBSD adds the core class that the
 * SCHED_MIC scheduler already keys its placement on.
 *
 * Nothing here is read on a scheduler hot path.  Consumers register a
 * callback that runs in thread context after a provider commits a batch of
 * updates, derive whatever precomputed per-CPU tables they need, and
 * publish those; see sched_mic(4).
 */

#ifndef	_SYS_CPUCAP_H_
#define	_SYS_CPUCAP_H_

#ifdef _KERNEL

#include <sys/queue.h>

/*
 * Per-CPU values.  Class values are the CPU_CLASS_* constants from
 * <sys/smp.h> (1 perf, 2 eff, 4 LP); every CPU is CPU_CLASS_PERF until a
 * provider says otherwise.  Capacity uses hmp(4)'s 0..1024 scale.  Scores
 * use the 8-bit scale the Intel HFI table reports natively; 255 means a
 * fully capable CPU and is the default when no dynamic provider is active.
 */
#define	CPUCAP_CAPACITY_MAX	1024
#define	CPUCAP_CAPACITY_DEFAULT	CPUCAP_CAPACITY_MAX
#define	CPUCAP_SCORE_MAX	255
#define	CPUCAP_SCORE_DEFAULT	CPUCAP_SCORE_MAX

/*
 * Nominal capacities a static provider may assign from a core class alone
 * when the platform offers no better figure (no ACPI CPPC data, for example).
 */
#define	CPUCAP_CAPACITY_PERF_NOMINAL	CPUCAP_CAPACITY_MAX
#define	CPUCAP_CAPACITY_EFF_NOMINAL	640
#define	CPUCAP_CAPACITY_LP_NOMINAL	320

/* Convert a score to the 0..1024 capacity scale, as hmp(4) would store it. */
#define	CPUCAP_SCORE_TO_CAPACITY(s)					\
	(((unsigned int)(s) * CPUCAP_CAPACITY_MAX) / CPUCAP_SCORE_MAX)

/*
 * Providers.
 *
 * A static provider supplies class and capacity (and the platform hints
 * below) once from its init routine.  A dynamic provider supplies scores
 * from its init routine and thereafter whenever it likes, from thread or
 * taskqueue context.  At most one provider of each kind is active: the
 * highest-priority one whose probe succeeds.  Providers that register after
 * boot-time selection (device drivers, loadable modules) are considered on
 * registration and replace a lower-priority active provider of their kind.
 *
 * A provider writes its values with the cpucap_set_*() routines, each of
 * which stores one naturally aligned value, and then calls cpucap_commit()
 * once for the batch.  cpucap_commit() never calls consumers inline after
 * boot; it bumps the generation and queues a single notification on
 * taskqueue_thread.  It may be called from any context that may take a
 * default (sleep) mutex, so not from an interrupt filter.
 *
 * The init routine of a static provider must set every CPU it classifies;
 * the layer resets class and capacity to their defaults before calling it.
 */
#define	CPUCAP_PROVIDER_STATIC	1	/* Class and capacity, once. */
#define	CPUCAP_PROVIDER_DYNAMIC	2	/* Scores, updated at run time. */

/* cp_state values, owned by the layer and exported via sysctl. */
#define	CPUCAP_PS_REGISTERED	0
#define	CPUCAP_PS_ABSENT	1	/* probe failed on this machine. */
#define	CPUCAP_PS_FAILED	2	/* init failed. */
#define	CPUCAP_PS_PROBED	3	/* usable but outranked. */
#define	CPUCAP_PS_ACTIVE	4

struct cpucap_provider {
	const char	*cp_name;
	int		cp_kind;		/* CPUCAP_PROVIDER_* */
	int		cp_priority;		/* Higher wins. */
	int		(*cp_probe)(void);	/* 0 if usable here. */
	int		(*cp_init)(void);	/* 0 on success. */
	/* Owned by the layer. */
	int		cp_state;
	SLIST_ENTRY(cpucap_provider) cp_link;
};

int	cpucap_provider_register(struct cpucap_provider *cp);
void	cpucap_provider_unregister(struct cpucap_provider *cp);

/*
 * Register a provider from a SYSINIT early enough for boot-time selection,
 * which runs at SI_SUB_SMP, SI_ORDER_MIDDLE, once the APs are up.
 */
#define	CPUCAP_PROVIDER_DECLARE(sym, provider)				\
	static void							\
	cpucap_provider_##sym##_register(void *arg __unused)		\
	{								\
		(void)cpucap_provider_register(&(provider));		\
	}								\
	SYSINIT(cpucap_provider_##sym, SI_SUB_SMP, SI_ORDER_SECOND,	\
	    cpucap_provider_##sym##_register, NULL)

/* Provider write interface; out-of-range CPUs and values are ignored. */
void	cpucap_set_class(int cpu, uint8_t cls);
void	cpucap_set_capacity(int cpu, uint16_t capacity);
void	cpucap_set_scores(int cpu, uint8_t perf, uint8_t eff);
/*
 * Platform hints from a static provider: which class holds the platform's
 * compute cores (CPU_CLASS_PERF on Intel, CPU_CLASS_EFF on AMD parts with
 * asymmetric L3) and whether that class should be preferred by default.
 */
void	cpucap_set_compute_class(uint8_t cls, bool prefer_compute);
void	cpucap_commit(void);

/*
 * Consumers.
 *
 * cc_update runs in thread context with the layer's lock held, so it must
 * not sleep and must not register or unregister providers or consumers.
 * "changed" is the union of the CPUCAP_CHANGED_* bits of every commit since
 * the previous notification.  Registration calls cc_update at once if any
 * commit has already happened, so consumers need not order themselves
 * against providers.
 */
#define	CPUCAP_CHANGED_STATIC	0x01	/* Class, capacity or hints. */
#define	CPUCAP_CHANGED_SCORES	0x02

struct cpucap_consumer {
	const char	*cc_name;
	void		(*cc_update)(void *arg, int changed);
	void		*cc_arg;
	SLIST_ENTRY(cpucap_consumer) cc_link;
};

void	cpucap_consumer_register(struct cpucap_consumer *cc);
void	cpucap_consumer_unregister(struct cpucap_consumer *cc);

/*
 * Read interface.  Each call is one relaxed atomic load of a naturally
 * aligned value, so a reader racing a provider sees either the old or the
 * new value of that CPU, never a torn one, and never blocks.  Out-of-range
 * CPUs read as the defaults.
 */
uint8_t		cpucap_class(int cpu);
uint16_t	cpucap_capacity(int cpu);
uint8_t		cpucap_perf(int cpu);
uint8_t		cpucap_eff(int cpu);
uint8_t		cpucap_compute_class(void);
bool		cpucap_prefer_compute(void);
bool		cpucap_has_scores(void);
u_int		cpucap_generation(void);
const char	*cpucap_force_classes_string(void);

#endif /* _KERNEL */
#endif /* !_SYS_CPUCAP_H_ */
