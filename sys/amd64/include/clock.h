/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: release/10.0.0/sys/amd64/include/clock.h 249324 2013-04-10 05:59:07Z neel $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL
/*
 * i386 to clock driver interface.
 * XXX large parts of the driver and its interface are misplaced.
 */
extern int	clkintr_pending;
extern u_int	i8254_freq;
extern int	i8254_max_count;
extern uint64_t	tsc_freq;
extern int	tsc_is_invariant;
extern int	tsc_perf_stat;
#ifdef SMP
extern int	smp_tsc;
#endif

void	i8254_init(void);

/*
 * Driver to clock driver interface.
 */

void	startrtclock(void);
void	init_TSC(void);

#define	HAS_TIMER_SPKR 1
int	timer_spkr_acquire(void);
int	timer_spkr_release(void);
void	timer_spkr_setfreq(int freq);

#endif /* _KERNEL */

#endif /* !_MACHINE_CLOCK_H_ */
