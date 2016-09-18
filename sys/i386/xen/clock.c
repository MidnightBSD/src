/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)clock.c	7.2 (Berkeley) 5/12/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* #define DELAYDEBUG */
/*
 * Routines to handle clock hardware.
 */

#include "opt_ddb.h"
#include "opt_clock.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/clock.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timeet.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/sysctl.h>
#include <sys/cons.h>
#include <sys/power.h>

#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#if defined(SMP)
#include <machine/smp.h>
#endif
#include <machine/specialreg.h>
#include <machine/timerreg.h>

#include <x86/isa/icu.h>
#include <x86/isa/isa.h>
#include <isa/rtc.h>

#include <xen/xen_intr.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <xen/hypervisor.h>
#include <machine/xen/xen-os.h>
#include <machine/xen/xenfunc.h>
#include <xen/interface/vcpu.h>
#include <machine/cpu.h>
#include <machine/xen/xen_clock_util.h>

/*
 * 32-bit time_t's can't reach leap years before 1904 or after 2036, so we
 * can use a simple formula for leap years.
 */
#define	LEAPYEAR(y)	(!((y) % 4))
#define	DAYSPERYEAR	(28+30*4+31*7)

#ifndef TIMER_FREQ
#define	TIMER_FREQ	1193182
#endif

#ifdef CYC2NS_SCALE_FACTOR
#undef	CYC2NS_SCALE_FACTOR
#endif
#define CYC2NS_SCALE_FACTOR	10

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

struct mtx clock_lock;
#define	RTC_LOCK_INIT							\
	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_NOPROFILE)
#define	RTC_LOCK	mtx_lock_spin(&clock_lock)
#define	RTC_UNLOCK	mtx_unlock_spin(&clock_lock)

int adjkerntz;		/* local offset from GMT in seconds */
int clkintr_pending;
int pscnt = 1;
int psdiv = 1;
int wall_cmos_clock;
u_int timer_freq = TIMER_FREQ;
static int independent_wallclock;
static int xen_disable_rtc_set;
static u_long cyc2ns_scale; 
static struct timespec shadow_tv;
static uint32_t shadow_tv_version;	/* XXX: lazy locking */
static uint64_t processed_system_time;	/* stime (ns) at last processing. */

static	const u_char daysinmonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};

SYSCTL_INT(_machdep, OID_AUTO, independent_wallclock,
    CTLFLAG_RW, &independent_wallclock, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, xen_disable_rtc_set,
    CTLFLAG_RW, &xen_disable_rtc_set, 1, "");


#define do_div(n,base) ({ \
        unsigned long __upper, __low, __high, __mod, __base; \
        __base = (base); \
        __asm("":"=a" (__low), "=d" (__high):"A" (n)); \
        __upper = __high; \
        if (__high) { \
                __upper = __high % (__base); \
                __high = __high / (__base); \
        } \
        __asm("divl %2":"=a" (__low), "=d" (__mod):"rm" (__base), "0" (__low), "1" (__upper)); \
        __asm("":"=A" (n):"a" (__low),"d" (__high)); \
        __mod; \
})


#define NS_PER_TICK (1000000000ULL/hz)

#define rdtscll(val) \
    __asm__ __volatile__("rdtsc" : "=A" (val))


/* convert from cycles(64bits) => nanoseconds (64bits)
 *  basic equation:
 *		ns = cycles / (freq / ns_per_sec)
 *		ns = cycles * (ns_per_sec / freq)
 *		ns = cycles * (10^9 / (cpu_mhz * 10^6))
 *		ns = cycles * (10^3 / cpu_mhz)
 *
 *	Then we use scaling math (suggested by george@mvista.com) to get:
 *		ns = cycles * (10^3 * SC / cpu_mhz) / SC
 *		ns = cycles * cyc2ns_scale / SC
 *
 *	And since SC is a constant power of two, we can convert the div
 *  into a shift.   
 *			-johnstul@us.ibm.com "math is hard, lets go shopping!"
 */
static inline void set_cyc2ns_scale(unsigned long cpu_mhz)
{
	cyc2ns_scale = (1000 << CYC2NS_SCALE_FACTOR)/cpu_mhz;
}

static inline unsigned long long cycles_2_ns(unsigned long long cyc)
{
	return (cyc * cyc2ns_scale) >> CYC2NS_SCALE_FACTOR;
}

/*
 * Scale a 64-bit delta by scaling and multiplying by a 32-bit fraction,
 * yielding a 64-bit result.
 */
static inline uint64_t 
scale_delta(uint64_t delta, uint32_t mul_frac, int shift)
{
	uint64_t product;
	uint32_t tmp1, tmp2;

	if ( shift < 0 )
		delta >>= -shift;
	else
		delta <<= shift;

	__asm__ (
		"mul  %5       ; "
		"mov  %4,%%eax ; "
		"mov  %%edx,%4 ; "
		"mul  %5       ; "
		"xor  %5,%5    ; "
		"add  %4,%%eax ; "
		"adc  %5,%%edx ; "
		: "=A" (product), "=r" (tmp1), "=r" (tmp2)
		: "a" ((uint32_t)delta), "1" ((uint32_t)(delta >> 32)), "2" (mul_frac) );

	return product;
}

static uint64_t
get_nsec_offset(struct shadow_time_info *shadow)
{
	uint64_t now, delta;
	rdtscll(now);
	delta = now - shadow->tsc_timestamp;
	return scale_delta(delta, shadow->tsc_to_nsec_mul, shadow->tsc_shift);
}

static void update_wallclock(void)
{
	shared_info_t *s = HYPERVISOR_shared_info;

	do {
		shadow_tv_version = s->wc_version;
		rmb();
		shadow_tv.tv_sec  = s->wc_sec;
		shadow_tv.tv_nsec = s->wc_nsec;
		rmb();
	}
	while ((s->wc_version & 1) | (shadow_tv_version ^ s->wc_version));

}

static void
add_uptime_to_wallclock(void)
{
	struct timespec ut;

	xen_fetch_uptime(&ut);
	timespecadd(&shadow_tv, &ut);
}

/*
 * Reads a consistent set of time-base values from Xen, into a shadow data
 * area. Must be called with the xtime_lock held for writing.
 */
static void __get_time_values_from_xen(void)
{
	shared_info_t           *s = HYPERVISOR_shared_info;
	struct vcpu_time_info   *src;
	struct shadow_time_info *dst;
	uint32_t pre_version, post_version;

	src = &s->vcpu_info[smp_processor_id()].time;
	dst = &per_cpu(shadow_time, smp_processor_id());

	spinlock_enter();
	do {
	        pre_version = dst->version = src->version;
		rmb();
		dst->tsc_timestamp     = src->tsc_timestamp;
		dst->system_timestamp  = src->system_time;
		dst->tsc_to_nsec_mul   = src->tsc_to_system_mul;
		dst->tsc_shift         = src->tsc_shift;
		rmb();
		post_version = src->version;
	}
	while ((pre_version & 1) | (pre_version ^ post_version));

	dst->tsc_to_usec_mul = dst->tsc_to_nsec_mul / 1000;
	spinlock_exit();
}


static inline int time_values_up_to_date(int cpu)
{
	struct vcpu_time_info   *src;
	struct shadow_time_info *dst;

	src = &HYPERVISOR_shared_info->vcpu_info[cpu].time; 
	dst = &per_cpu(shadow_time, cpu); 

	rmb();
	return (dst->version == src->version);
}

static	unsigned xen_get_timecount(struct timecounter *tc);

static struct timecounter xen_timecounter = {
	xen_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"ixen",			/* name */
	0			/* quality */
};

static struct eventtimer xen_et;

struct xen_et_state {
	int		mode;
#define	MODE_STOP	0
#define	MODE_PERIODIC	1
#define	MODE_ONESHOT	2
	int64_t		period;
	int64_t		next;
};

static DPCPU_DEFINE(struct xen_et_state, et_state);

static int
clkintr(void *arg)
{
	int64_t now;
	int cpu = smp_processor_id();
	struct shadow_time_info *shadow = &per_cpu(shadow_time, cpu);
	struct xen_et_state *state = DPCPU_PTR(et_state);

	do {
		__get_time_values_from_xen();
		now = shadow->system_timestamp + get_nsec_offset(shadow);
	} while (!time_values_up_to_date(cpu));

	/* Process elapsed ticks since last call. */
	processed_system_time = now;
	if (state->mode == MODE_PERIODIC) {
		while (now >= state->next) {
		        state->next += state->period;
			if (xen_et.et_active)
				xen_et.et_event_cb(&xen_et, xen_et.et_arg);
		}
		HYPERVISOR_set_timer_op(state->next + 50000);
	} else if (state->mode == MODE_ONESHOT) {
		if (xen_et.et_active)
			xen_et.et_event_cb(&xen_et, xen_et.et_arg);
	}
	/*
	 * Take synchronised time from Xen once a minute if we're not
	 * synchronised ourselves, and we haven't chosen to keep an independent
	 * time base.
	 */
	
	if (shadow_tv_version != HYPERVISOR_shared_info->wc_version &&
	    !independent_wallclock) {
		printf("[XEN] hypervisor wallclock nudged; nudging TOD.\n");
		update_wallclock();
		add_uptime_to_wallclock();
		tc_setclock(&shadow_tv);
	}
	
	/* XXX TODO */
	return (FILTER_HANDLED);
}
static uint32_t
getit(void)
{
	struct shadow_time_info *shadow;
	uint64_t time;
	uint32_t local_time_version;

	shadow = &per_cpu(shadow_time, smp_processor_id());

	do {
	  local_time_version = shadow->version;
	  barrier();
	  time = shadow->system_timestamp + get_nsec_offset(shadow);
	  if (!time_values_up_to_date(smp_processor_id()))
	    __get_time_values_from_xen(/*cpu */);
	  barrier();
	} while (local_time_version != shadow->version);

	  return (time);
}


/*
 * XXX: timer needs more SMP work.
 */
void
i8254_init(void)
{

	RTC_LOCK_INIT;
}

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (timer_freq / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int delta, ticks_left;
	uint32_t tick, prev_tick;
#ifdef DELAYDEBUG
	int getit_calls = 1;
	int n1;
	static int state = 0;

	if (state == 0) {
		state = 1;
		for (n1 = 1; n1 <= 10000000; n1 *= 10)
			DELAY(n1);
		state = 2;
	}
	if (state == 1)
		printf("DELAY(%d)...", n);
#endif
	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 *
	 * However, if ddb is active then use a fake counter since reading
	 * the i8254 counter involves acquiring a lock.  ddb must not go
	 * locking for many reasons, but it calls here for at least atkbd
	 * input.
	 */
	prev_tick = getit();

	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (timer_freq / 1e6)) without using floating point
	 * and without any avoidable overflows.
	 */
	if (n <= 0)
		ticks_left = 0;
	else if (n < 256)
		/*
		 * Use fixed point to avoid a slow division by 1000000.
		 * 39099 = 1193182 * 2^15 / 10^6 rounded to nearest.
		 * 2^15 is the first power of 2 that gives exact results
		 * for n between 0 and 256.
		 */
		ticks_left = ((u_int)n * 39099 + (1 << 15) - 1) >> 15;
	else
		/*
		 * Don't bother using fixed point, although gcc-2.7.2
		 * generates particularly poor code for the long long
		 * division, since even the slow way will complete long
		 * before the delay is up (unless we're interrupted).
		 */
		ticks_left = ((u_int)n * (long long)timer_freq + 999999)
			/ 1000000;

	while (ticks_left > 0) {
		tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = tick - prev_tick;
		prev_tick = tick;
		if (delta < 0) {
			/*
			 * Guard against timer0_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_timer_freq() is
			 * traced.
			 */
			/* delta += timer0_max_count; ??? */
			if (delta < 0)
				delta = 0;
		}
		ticks_left -= delta;
	}
#ifdef DELAYDEBUG
	if (state == 1)
		printf(" %d calls to getit() at %d usec each\n",
		       getit_calls, (n + 5) / getit_calls);
#endif
}


/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 */
void
timer_restore(void)
{
	struct xen_et_state *state = DPCPU_PTR(et_state);

	/* Get timebases for new environment. */ 
	__get_time_values_from_xen();

	/* Reset our own concept of passage of system time. */
	processed_system_time = per_cpu(shadow_time, 0).system_timestamp;
	state->next = processed_system_time;
}

void
startrtclock()
{
	unsigned long long alarm;
	uint64_t __cpu_khz;
	uint32_t cpu_khz;
	struct vcpu_time_info *info;

	/* initialize xen values */
	__get_time_values_from_xen();
	processed_system_time = per_cpu(shadow_time, 0).system_timestamp;

	__cpu_khz = 1000000ULL << 32;
	info = &HYPERVISOR_shared_info->vcpu_info[0].time;

	(void)do_div(__cpu_khz, info->tsc_to_system_mul);
	if ( info->tsc_shift < 0 )
		cpu_khz = __cpu_khz << -info->tsc_shift;
	else
		cpu_khz = __cpu_khz >> info->tsc_shift;

	printf("Xen reported: %u.%03u MHz processor.\n", 
	       cpu_khz / 1000, cpu_khz % 1000);

	/* (10^6 * 2^32) / cpu_hz = (10^3 * 2^32) / cpu_khz =
	   (2^32 * 1 / (clocks/us)) */

	set_cyc2ns_scale(cpu_khz/1000);
	tsc_freq = cpu_khz * 1000;

        timer_freq = 1000000000LL;
	xen_timecounter.tc_frequency = timer_freq >> 9;
        tc_init(&xen_timecounter);

	rdtscll(alarm);
}

/*
 * RTC support routines
 */


static __inline int
readrtc(int port)
{
	return(bcd2bin(rtcin(port)));
}


#ifdef XEN_PRIVILEGED_GUEST

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
static void
domu_inittodr(time_t base)
{
	unsigned long   sec;
	int		s, y;
	struct timespec ts;

	update_wallclock();
	add_uptime_to_wallclock();
	
	RTC_LOCK;
	
	if (base) {
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}

	sec += tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	y = time_second - shadow_tv.tv_sec;
	if (y <= -2 || y >= 2) {
		/* badly off, adjust it */
		tc_setclock(&shadow_tv);
	}
	RTC_UNLOCK;
}

/*
 * Write system time back to RTC.  
 */
static void
domu_resettodr(void)
{
	unsigned long tm;
	int s;
	dom0_op_t op;
	struct shadow_time_info *shadow;

	shadow = &per_cpu(shadow_time, smp_processor_id());
	if (xen_disable_rtc_set)
		return;
	
	s = splclock();
	tm = time_second;
	splx(s);
	
	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);
	
	if ((xen_start_info->flags & SIF_INITDOMAIN) &&
	    !independent_wallclock)
	{
		op.cmd = DOM0_SETTIME;
		op.u.settime.secs        = tm;
		op.u.settime.nsecs       = 0;
		op.u.settime.system_time = shadow->system_timestamp;
		HYPERVISOR_dom0_op(&op);
		update_wallclock();
		add_uptime_to_wallclock();
	} else if (independent_wallclock) {
		/* notyet */
		;
	}		
}

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(time_t base)
{
	unsigned long	sec, days;
	int		year, month;
	int		y, m, s;
	struct timespec ts;

	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
	        domu_inittodr(base);
		return;
	}

	if (base) {
		s = splclock();
		ts.tv_sec = base;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
		splx(s);
	}

	/* Look if we have a RTC present and the time is valid */
	if (!(rtcin(RTC_STATUSD) & RTCSD_PWR))
		goto wrong_time;

	/* wait for time update to complete */
	/* If RTCSA_TUP is zero, we have at least 244us before next update */
	s = splhigh();
	while (rtcin(RTC_STATUSA) & RTCSA_TUP) {
		splx(s);
		s = splhigh();
	}

	days = 0;
#ifdef USE_RTC_CENTURY
	year = readrtc(RTC_YEAR) + readrtc(RTC_CENTURY) * 100;
#else
	year = readrtc(RTC_YEAR) + 1900;
	if (year < 1970)
		year += 100;
#endif
	if (year < 1970) {
		splx(s);
		goto wrong_time;
	}
	month = readrtc(RTC_MONTH);
	for (m = 1; m < month; m++)
		days += daysinmonth[m-1];
	if ((month > 2) && LEAPYEAR(year))
		days ++;
	days += readrtc(RTC_DAY) - 1;
	for (y = 1970; y < year; y++)
		days += DAYSPERYEAR + LEAPYEAR(y);
	sec = ((( days * 24 +
		  readrtc(RTC_HRS)) * 60 +
		readrtc(RTC_MIN)) * 60 +
	       readrtc(RTC_SEC));
	/* sec now contains the number of seconds, since Jan 1 1970,
	   in the local time zone */

	sec += tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	y = time_second - sec;
	if (y <= -2 || y >= 2) {
		/* badly off, adjust it */
		ts.tv_sec = sec;
		ts.tv_nsec = 0;
		tc_setclock(&ts);
	}
	splx(s);
	return;

 wrong_time:
	printf("Invalid time in real time clock.\n");
	printf("Check and reset the date immediately!\n");
}


/*
 * Write system time back to RTC
 */
void
resettodr()
{
	unsigned long	tm;
	int		y, m, s;

	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
	        domu_resettodr();
		return;
	}
	       
	if (xen_disable_rtc_set)
		return;

	s = splclock();
	tm = time_second;
	splx(s);

	/* Disable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_HALT | RTCSB_24HR);

	/* Calculate local time to put in RTC */

	tm -= tz_minuteswest * 60 + (wall_cmos_clock ? adjkerntz : 0);

	writertc(RTC_SEC, bin2bcd(tm%60)); tm /= 60;	/* Write back Seconds */
	writertc(RTC_MIN, bin2bcd(tm%60)); tm /= 60;	/* Write back Minutes */
	writertc(RTC_HRS, bin2bcd(tm%24)); tm /= 24;	/* Write back Hours   */

	/* We have now the days since 01-01-1970 in tm */
	writertc(RTC_WDAY, (tm + 4) % 7 + 1);		/* Write back Weekday */
	for (y = 1970, m = DAYSPERYEAR + LEAPYEAR(y);
	     tm >= m;
	     y++,      m = DAYSPERYEAR + LEAPYEAR(y))
		tm -= m;

	/* Now we have the years in y and the day-of-the-year in tm */
	writertc(RTC_YEAR, bin2bcd(y%100));		/* Write back Year    */
#ifdef USE_RTC_CENTURY
	writertc(RTC_CENTURY, bin2bcd(y/100));		/* ... and Century    */
#endif
	for (m = 0; ; m++) {
		int ml;

		ml = daysinmonth[m];
		if (m == 1 && LEAPYEAR(y))
			ml++;
		if (tm < ml)
			break;
		tm -= ml;
	}

	writertc(RTC_MONTH, bin2bcd(m + 1));            /* Write back Month   */
	writertc(RTC_DAY, bin2bcd(tm + 1));             /* Write back Month Day */

	/* Reenable RTC updates and interrupts. */
	writertc(RTC_STATUSB, RTCSB_24HR);
	rtcin(RTC_INTR);
}
#endif

static int
xen_et_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period)
{
	struct xen_et_state *state = DPCPU_PTR(et_state);
	struct shadow_time_info *shadow;
	int64_t fperiod;

	__get_time_values_from_xen();

	if (period != NULL) {
		state->mode = MODE_PERIODIC;
		state->period = (1000000000LL *
		    (uint32_t)(period->frac >> 32)) >> 32;
		if (period->sec != 0)
			state->period += 1000000000LL * period->sec;
	} else {
		state->mode = MODE_ONESHOT;
		state->period = 0;
	}
	if (first != NULL) {
		fperiod = (1000000000LL * (uint32_t)(first->frac >> 32)) >> 32;
		if (first->sec != 0)
			fperiod += 1000000000LL * first->sec;
	} else
		fperiod = state->period;

	shadow = &per_cpu(shadow_time, smp_processor_id());
	state->next = shadow->system_timestamp + get_nsec_offset(shadow);
	state->next += fperiod;
	HYPERVISOR_set_timer_op(state->next + 50000);
	return (0);
}

static int
xen_et_stop(struct eventtimer *et)
{
	struct xen_et_state *state = DPCPU_PTR(et_state);

	state->mode = MODE_STOP;
	HYPERVISOR_set_timer_op(0);
	return (0);
}

/*
 * Start clocks running.
 */
void
cpu_initclocks(void)
{
	unsigned int time_irq;
	int error;

	HYPERVISOR_vcpu_op(VCPUOP_stop_periodic_timer, 0, NULL);
	error = bind_virq_to_irqhandler(VIRQ_TIMER, 0, "cpu0:timer",
	    clkintr, NULL, NULL, INTR_TYPE_CLK, &time_irq);
	if (error)
		panic("failed to register clock interrupt\n");
	/* should fast clock be enabled ? */

	bzero(&xen_et, sizeof(xen_et));
	xen_et.et_name = "ixen";
	xen_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT |
	    ET_FLAGS_PERCPU;
	xen_et.et_quality = 600;
	xen_et.et_frequency = 0;
	xen_et.et_min_period.sec = 0;
	xen_et.et_min_period.frac = 0x00400000LL << 32;
	xen_et.et_max_period.sec = 2;
	xen_et.et_max_period.frac = 0;
	xen_et.et_start = xen_et_start;
	xen_et.et_stop = xen_et_stop;
	xen_et.et_priv = NULL;
	et_register(&xen_et);

	cpu_initclocks_bsp();
}

int
ap_cpu_initclocks(int cpu)
{
	char buf[MAXCOMLEN + 1];
	unsigned int time_irq;
	int error;

	HYPERVISOR_vcpu_op(VCPUOP_stop_periodic_timer, cpu, NULL);
	snprintf(buf, sizeof(buf), "cpu%d:timer", cpu);
	error = bind_virq_to_irqhandler(VIRQ_TIMER, cpu, buf,
	    clkintr, NULL, NULL, INTR_TYPE_CLK, &time_irq);
	if (error)
		panic("failed to register clock interrupt\n");

	return (0);
}

static uint32_t
xen_get_timecount(struct timecounter *tc)
{	
	uint64_t clk;
	struct shadow_time_info *shadow;
	shadow = &per_cpu(shadow_time, smp_processor_id());

	__get_time_values_from_xen();
	
        clk = shadow->system_timestamp + get_nsec_offset(shadow);

	return (uint32_t)(clk >> 9);

}

/* Return system time offset by ticks */
uint64_t
get_system_time(int ticks)
{
    return processed_system_time + (ticks * NS_PER_TICK);
}

void
idle_block(void)
{

	HYPERVISOR_sched_op(SCHEDOP_block, 0);
}

int
timer_spkr_acquire(void)
{

	return (0);
}

int
timer_spkr_release(void)
{

	return (0);
}

void
timer_spkr_setfreq(int freq)
{

}

