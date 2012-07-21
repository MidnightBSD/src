/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2010 Alexander Motin <mav@FreeBSD.org>
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

/*
 * Routines to handle clock hardware.
 */

#include "opt_clock.h"
#include "opt_isa.h"
#include "opt_mca.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kdb.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/timeet.h>
#include <sys/timetc.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/ppireg.h>
#include <machine/timerreg.h>

#ifdef PC98
#include <pc98/pc98/pc98_machdep.h>
#else
#include <isa/rtc.h>
#endif
#ifdef DEV_ISA
#ifdef PC98
#include <pc98/cbus/cbus.h>
#else
#include <isa/isareg.h>
#endif
#include <isa/isavar.h>
#endif

#ifdef DEV_MCA
#include <i386/bios/mca_machdep.h>
#endif

int	clkintr_pending;
#ifndef TIMER_FREQ
#ifdef PC98
#define TIMER_FREQ   2457600
#else
#define TIMER_FREQ   1193182
#endif
#endif
u_int	i8254_freq = TIMER_FREQ;
TUNABLE_INT("hw.i8254.freq", &i8254_freq);
int	i8254_max_count;
static int i8254_timecounter = 1;

struct mtx clock_lock;
static	struct intsrc *i8254_intsrc;
static	uint16_t i8254_lastcount;
static	uint16_t i8254_offset;
static	int	(*i8254_pending)(struct intsrc *);
static	int	i8254_ticked;

struct attimer_softc {
	int intr_en;
	int port_rid, intr_rid;
	struct resource *port_res;
	struct resource *intr_res;
#ifdef PC98
	int port_rid2;
	struct resource *port_res2;
#endif
	void *intr_handler;
	struct timecounter tc;
	struct eventtimer et;
	int		mode;
#define	MODE_STOP	0
#define	MODE_PERIODIC	1
#define	MODE_ONESHOT	2
	uint32_t	period;
};
static struct attimer_softc *attimer_sc = NULL;

static int timer0_period = -2;

/* Values for timerX_state: */
#define	RELEASED	0
#define	RELEASE_PENDING	1
#define	ACQUIRED	2
#define	ACQUIRE_PENDING	3

static	u_char	timer2_state;

static	unsigned i8254_get_timecount(struct timecounter *tc);
static	void	set_i8254_freq(int mode, uint32_t period);

static int
clkintr(void *arg)
{
	struct attimer_softc *sc = (struct attimer_softc *)arg;

	if (i8254_timecounter && sc->period != 0) {
		mtx_lock_spin(&clock_lock);
		if (i8254_ticked)
			i8254_ticked = 0;
		else {
			i8254_offset += i8254_max_count;
			i8254_lastcount = 0;
		}
		clkintr_pending = 0;
		mtx_unlock_spin(&clock_lock);
	}

	if (sc && sc->et.et_active && sc->mode != MODE_STOP)
		sc->et.et_event_cb(&sc->et, sc->et.et_arg);

#ifdef DEV_MCA
	/* Reset clock interrupt by asserting bit 7 of port 0x61 */
	if (MCA_system)
		outb(0x61, inb(0x61) | 0x80);
#endif
	return (FILTER_HANDLED);
}

int
timer_spkr_acquire(void)
{
	int mode;

#ifdef PC98
	mode = TIMER_SEL1 | TIMER_SQWAVE | TIMER_16BIT;
#else
	mode = TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT;
#endif

	if (timer2_state != RELEASED)
		return (-1);
	timer2_state = ACQUIRED;

	/*
	 * This access to the timer registers is as atomic as possible
	 * because it is a single instruction.  We could do better if we
	 * knew the rate.  Use of splclock() limits glitches to 10-100us,
	 * and this is probably good enough for timer2, so we aren't as
	 * careful with it as with timer0.
	 */
#ifdef PC98
	outb(TIMER_MODE, TIMER_SEL1 | (mode & 0x3f));
#else
	outb(TIMER_MODE, TIMER_SEL2 | (mode & 0x3f));
#endif
	ppi_spkr_on();		/* enable counter2 output to speaker */
	return (0);
}

int
timer_spkr_release(void)
{

	if (timer2_state != ACQUIRED)
		return (-1);
	timer2_state = RELEASED;
#ifdef PC98
	outb(TIMER_MODE, TIMER_SEL1 | TIMER_SQWAVE | TIMER_16BIT);
#else
	outb(TIMER_MODE, TIMER_SEL2 | TIMER_SQWAVE | TIMER_16BIT);
#endif
	ppi_spkr_off();		/* disable counter2 output to speaker */
	return (0);
}

void
timer_spkr_setfreq(int freq)
{

	freq = i8254_freq / freq;
	mtx_lock_spin(&clock_lock);
#ifdef PC98
	outb(TIMER_CNTR1, freq & 0xff);
	outb(TIMER_CNTR1, freq >> 8);
#else
	outb(TIMER_CNTR2, freq & 0xff);
	outb(TIMER_CNTR2, freq >> 8);
#endif
	mtx_unlock_spin(&clock_lock);
}

static int
getit(void)
{
	int high, low;

	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);

	mtx_unlock_spin(&clock_lock);
	return ((high << 8) | low);
}

#ifndef DELAYDEBUG
static u_int
get_tsc(__unused struct timecounter *tc)
{

	return (rdtsc32());
}

static __inline int
delay_tc(int n)
{
	struct timecounter *tc;
	timecounter_get_t *func;
	uint64_t end, freq, now;
	u_int last, mask, u;

	tc = timecounter;
	freq = atomic_load_acq_64(&tsc_freq);
	if (tsc_is_invariant && freq != 0) {
		func = get_tsc;
		mask = ~0u;
	} else {
		if (tc->tc_quality <= 0)
			return (0);
		func = tc->tc_get_timecount;
		mask = tc->tc_counter_mask;
		freq = tc->tc_frequency;
	}
	now = 0;
	end = freq * n / 1000000;
	if (func == get_tsc)
		sched_pin();
	last = func(tc) & mask;
	do {
		cpu_spinwait();
		u = func(tc) & mask;
		if (u < last)
			now += mask - last + u + 1;
		else
			now += u - last;
		last = u;
	} while (now < end);
	if (func == get_tsc)
		sched_unpin();
	return (1);
}
#endif

/*
 * Wait "n" microseconds.
 * Relies on timer 1 counting down from (i8254_freq / hz)
 * Note: timer had better have been programmed before this is first used!
 */
void
DELAY(int n)
{
	int delta, prev_tick, tick, ticks_left;
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
#else
	if (delay_tc(n))
		return;
#endif
	/*
	 * Read the counter first, so that the rest of the setup overhead is
	 * counted.  Guess the initial overhead is 20 usec (on most systems it
	 * takes about 1.5 usec for each of the i/o's in getit().  The loop
	 * takes about 6 usec on a 486/33 and 13 usec on a 386/20.  The
	 * multiplications and divisions to scale the count take a while).
	 *
	 * However, if ddb is active then use a fake counter since reading
	 * the i8254 counter involves acquiring a lock.  ddb must not do
	 * locking for many reasons, but it calls here for at least atkbd
	 * input.
	 */
#ifdef KDB
	if (kdb_active)
		prev_tick = 1;
	else
#endif
		prev_tick = getit();
	n -= 0;			/* XXX actually guess no initial overhead */
	/*
	 * Calculate (n * (i8254_freq / 1e6)) without using floating point
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
		ticks_left = ((u_int)n * (long long)i8254_freq + 999999)
			     / 1000000;

	while (ticks_left > 0) {
#ifdef KDB
		if (kdb_active) {
#ifdef PC98
			outb(0x5f, 0);
#else
			inb(0x84);
#endif
			tick = prev_tick - 1;
			if (tick <= 0)
				tick = i8254_max_count;
		} else
#endif
			tick = getit();
#ifdef DELAYDEBUG
		++getit_calls;
#endif
		delta = prev_tick - tick;
		prev_tick = tick;
		if (delta < 0) {
			delta += i8254_max_count;
			/*
			 * Guard against i8254_max_count being wrong.
			 * This shouldn't happen in normal operation,
			 * but it may happen if set_i8254_freq() is
			 * traced.
			 */
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

static void
set_i8254_freq(int mode, uint32_t period)
{
	int new_count;

	mtx_lock_spin(&clock_lock);
	if (mode == MODE_STOP) {
		if (i8254_timecounter) {
			mode = MODE_PERIODIC;
			new_count = 0x10000;
		} else
			new_count = -1;
	} else {
		new_count = min(((uint64_t)i8254_freq * period +
		    0x80000000LLU) >> 32, 0x10000);
	}
	if (new_count == timer0_period)
		goto out;
	i8254_max_count = ((new_count & ~0xffff) != 0) ? 0xffff : new_count;
	timer0_period = (mode == MODE_PERIODIC) ? new_count : -1;
	switch (mode) {
	case MODE_STOP:
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_INTTC | TIMER_16BIT);
		outb(TIMER_CNTR0, 0);
		outb(TIMER_CNTR0, 0);
		break;
	case MODE_PERIODIC:
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
		outb(TIMER_CNTR0, new_count & 0xff);
		outb(TIMER_CNTR0, new_count >> 8);
		break;
	case MODE_ONESHOT:
		outb(TIMER_MODE, TIMER_SEL0 | TIMER_INTTC | TIMER_16BIT);
		outb(TIMER_CNTR0, new_count & 0xff);
		outb(TIMER_CNTR0, new_count >> 8);
		break;
	}
out:
	mtx_unlock_spin(&clock_lock);
}

static void
i8254_restore(void)
{

	timer0_period = -2;
	if (attimer_sc != NULL)
		set_i8254_freq(attimer_sc->mode, attimer_sc->period);
	else
		set_i8254_freq(0, 0);
}

#ifndef __amd64__
/*
 * Restore all the timers non-atomically (XXX: should be atomically).
 *
 * This function is called from pmtimer_resume() to restore all the timers.
 * This should not be necessary, but there are broken laptops that do not
 * restore all the timers on resume.
 * As long as pmtimer is not part of amd64 suport, skip this for the amd64
 * case.
 */
void
timer_restore(void)
{

	i8254_restore();		/* restore i8254_freq and hz */
#ifndef PC98
	atrtc_restore();		/* reenable RTC interrupts */
#endif
}
#endif

/* This is separate from startrtclock() so that it can be called early. */
void
i8254_init(void)
{

	mtx_init(&clock_lock, "clk", NULL, MTX_SPIN | MTX_NOPROFILE);
#ifdef PC98
	if (pc98_machine_type & M_8M)
		i8254_freq = 1996800L; /* 1.9968 MHz */
#endif
	set_i8254_freq(0, 0);
}

void
startrtclock()
{

	init_TSC();
}

void
cpu_initclocks(void)
{

	cpu_initclocks_bsp();
}

static int
sysctl_machdep_i8254_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	u_int freq;

	/*
	 * Use `i8254' instead of `timer' in external names because `timer'
	 * is too generic.  Should use it everywhere.
	 */
	freq = i8254_freq;
	error = sysctl_handle_int(oidp, &freq, 0, req);
	if (error == 0 && req->newptr != NULL) {
		i8254_freq = freq;
		if (attimer_sc != NULL) {
			set_i8254_freq(attimer_sc->mode, attimer_sc->period);
			attimer_sc->tc.tc_frequency = freq;
		} else {
			set_i8254_freq(0, 0);
		}
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, i8254_freq, CTLTYPE_INT | CTLFLAG_RW,
    0, sizeof(u_int), sysctl_machdep_i8254_freq, "IU",
    "i8254 timer frequency");

static unsigned
i8254_get_timecount(struct timecounter *tc)
{
	device_t dev = (device_t)tc->tc_priv;
	struct attimer_softc *sc = device_get_softc(dev);
	register_t flags;
	uint16_t count;
	u_int high, low;

	if (sc->period == 0)
		return (i8254_max_count - getit());

#ifdef __amd64__
	flags = read_rflags();
#else
	flags = read_eflags();
#endif
	mtx_lock_spin(&clock_lock);

	/* Select timer0 and latch counter value. */
	outb(TIMER_MODE, TIMER_SEL0 | TIMER_LATCH);

	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);
	count = i8254_max_count - ((high << 8) | low);
	if (count < i8254_lastcount ||
	    (!i8254_ticked && (clkintr_pending ||
	    ((count < 20 || (!(flags & PSL_I) &&
	    count < i8254_max_count / 2u)) &&
	    i8254_pending != NULL && i8254_pending(i8254_intsrc))))) {
		i8254_ticked = 1;
		i8254_offset += i8254_max_count;
	}
	i8254_lastcount = count;
	count += i8254_offset;
	mtx_unlock_spin(&clock_lock);
	return (count);
}

static int
attimer_start(struct eventtimer *et,
    struct bintime *first, struct bintime *period)
{
	device_t dev = (device_t)et->et_priv;
	struct attimer_softc *sc = device_get_softc(dev);

	if (period != NULL) {
		sc->mode = MODE_PERIODIC;
		sc->period = period->frac >> 32;
	} else {
		sc->mode = MODE_ONESHOT;
		sc->period = first->frac >> 32;
	}
	if (!sc->intr_en) {
		i8254_intsrc->is_pic->pic_enable_source(i8254_intsrc);
		sc->intr_en = 1;
	}
	set_i8254_freq(sc->mode, sc->period);
	return (0);
}

static int
attimer_stop(struct eventtimer *et)
{
	device_t dev = (device_t)et->et_priv;
	struct attimer_softc *sc = device_get_softc(dev);
	
	sc->mode = MODE_STOP;
	sc->period = 0;
	set_i8254_freq(sc->mode, sc->period);
	return (0);
}

#ifdef DEV_ISA
/*
 * Attach to the ISA PnP descriptors for the timer
 */
static struct isa_pnp_id attimer_ids[] = {
	{ 0x0001d041 /* PNP0100 */, "AT timer" },
	{ 0 }
};

#ifdef PC98
static void
pc98_alloc_resource(device_t dev)
{
	static bus_addr_t iat1[] = {0, 2, 4, 6};
	static bus_addr_t iat2[] = {0, 4};
	struct attimer_softc *sc;

	sc = device_get_softc(dev);

	sc->port_rid = 0;
	bus_set_resource(dev, SYS_RES_IOPORT, sc->port_rid, IO_TIMER1, 1);
	sc->port_res = isa_alloc_resourcev(dev, SYS_RES_IOPORT,
	    &sc->port_rid, iat1, 4, RF_ACTIVE);
	if (sc->port_res == NULL)
		device_printf(dev, "Warning: Couldn't map I/O.\n");
	else
		isa_load_resourcev(sc->port_res, iat1, 4);

	sc->port_rid2 = 4;
	bus_set_resource(dev, SYS_RES_IOPORT, sc->port_rid2, TIMER_CNTR1, 1);
	sc->port_res2 = isa_alloc_resourcev(dev, SYS_RES_IOPORT,
	    &sc->port_rid2, iat2, 2, RF_ACTIVE);
	if (sc->port_res2 == NULL)
		device_printf(dev, "Warning: Couldn't map I/O.\n");
	else
		isa_load_resourcev(sc->port_res2, iat2, 2);
}

static void
pc98_release_resource(device_t dev)
{
	struct attimer_softc *sc;

	sc = device_get_softc(dev);

	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid,
		    sc->port_res);
	if (sc->port_res2)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid2,
		    sc->port_res2);
}
#endif

static int
attimer_probe(device_t dev)
{
	int result;
	
	result = ISA_PNP_PROBE(device_get_parent(dev), dev, attimer_ids);
	/* ENOENT means no PnP-ID, device is hinted. */
	if (result == ENOENT) {
		device_set_desc(dev, "AT timer");
#ifdef PC98
		/* To print resources correctly. */
		pc98_alloc_resource(dev);
		pc98_release_resource(dev);
#endif
		return (BUS_PROBE_LOW_PRIORITY);
	}
	return (result);
}

static int
attimer_attach(device_t dev)
{
	struct attimer_softc *sc;
	u_long s;
	int i;

	attimer_sc = sc = device_get_softc(dev);
	bzero(sc, sizeof(struct attimer_softc));
#ifdef PC98
	pc98_alloc_resource(dev);
#else
	if (!(sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &sc->port_rid, IO_TIMER1, IO_TIMER1 + 3, 4, RF_ACTIVE)))
		device_printf(dev,"Warning: Couldn't map I/O.\n");
#endif
	i8254_intsrc = intr_lookup_source(0);
	if (i8254_intsrc != NULL)
		i8254_pending = i8254_intsrc->is_pic->pic_source_pending;
	resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "timecounter", &i8254_timecounter);
	set_i8254_freq(0, 0);
	if (i8254_timecounter) {
		sc->tc.tc_get_timecount = i8254_get_timecount;
		sc->tc.tc_counter_mask = 0xffff;
		sc->tc.tc_frequency = i8254_freq;
		sc->tc.tc_name = "i8254";
		sc->tc.tc_quality = 0;
		sc->tc.tc_priv = dev;
		tc_init(&sc->tc);
	}
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "clock", &i) != 0 || i != 0) {
	    	sc->intr_rid = 0;
		while (bus_get_resource(dev, SYS_RES_IRQ, sc->intr_rid,
		    &s, NULL) == 0 && s != 0)
			sc->intr_rid++;
		if (!(sc->intr_res = bus_alloc_resource(dev, SYS_RES_IRQ,
		    &sc->intr_rid, 0, 0, 1, RF_ACTIVE))) {
			device_printf(dev,"Can't map interrupt.\n");
			return (0);
		}
		/* Dirty hack, to make bus_setup_intr to not enable source. */
		i8254_intsrc->is_handlers++;
		if ((bus_setup_intr(dev, sc->intr_res,
		    INTR_MPSAFE | INTR_TYPE_CLK,
		    (driver_filter_t *)clkintr, NULL,
		    sc, &sc->intr_handler))) {
			device_printf(dev, "Can't setup interrupt.\n");
			i8254_intsrc->is_handlers--;
			return (0);
		}
		i8254_intsrc->is_handlers--;
		i8254_intsrc->is_pic->pic_enable_intr(i8254_intsrc);
		sc->et.et_name = "i8254";
		sc->et.et_flags = ET_FLAGS_PERIODIC;
		if (!i8254_timecounter)
			sc->et.et_flags |= ET_FLAGS_ONESHOT;
		sc->et.et_quality = 100;
		sc->et.et_frequency = i8254_freq;
		sc->et.et_min_period.sec = 0;
		sc->et.et_min_period.frac =
		    ((0x0002LLU << 48) / i8254_freq) << 16;
		sc->et.et_max_period.sec = 0xffff / i8254_freq;
		sc->et.et_max_period.frac =
		    ((0xfffeLLU << 48) / i8254_freq) << 16;
		sc->et.et_start = attimer_start;
		sc->et.et_stop = attimer_stop;
		sc->et.et_priv = dev;
		et_register(&sc->et);
	}
	return(0);
}

static int
attimer_resume(device_t dev)
{

	i8254_restore();
	return (0);
}

static device_method_t attimer_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		attimer_probe),
	DEVMETHOD(device_attach,	attimer_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	attimer_resume),
	{ 0, 0 }
};

static driver_t attimer_driver = {
	"attimer",
	attimer_methods,
	sizeof(struct attimer_softc),
};

static devclass_t attimer_devclass;

DRIVER_MODULE(attimer, isa, attimer_driver, attimer_devclass, 0, 0);
DRIVER_MODULE(attimer, acpi, attimer_driver, attimer_devclass, 0, 0);

#endif /* DEV_ISA */
