/*-
 * Kernel interface to machine-dependent clock driver.
 * Garrett Wollman, September 1994.
 * This file is in the public domain.
 *
 * $FreeBSD: src/sys/ia64/include/clock.h,v 1.10 2005/01/06 22:18:23 imp Exp $
 */

#ifndef _MACHINE_CLOCK_H_
#define	_MACHINE_CLOCK_H_

#ifdef _KERNEL

#define	CLOCK_VECTOR	254

extern int adjkerntz;
extern int disable_rtc_set;
extern int wall_cmos_clock;

extern uint64_t	ia64_clock_reload;
extern uint64_t	itc_frequency;

int sysbeep(int pitch, int period);

#endif

#endif /* !_MACHINE_CLOCK_H_ */
