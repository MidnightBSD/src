/*-
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */
#include "opt_ah.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/stdarg.h>

#include <net/ethernet.h>		/* XXX for ether_sprintf */

#include <dev/ath/ath_hal/ah.h>

/*
 * WiSoC boards overload the bus tag with information about the
 * board layout.  We must extract the bus space tag from that
 * indirect structure.  For everyone else the tag is passed in
 * directly.
 * XXX cache indirect ref privately
 */
#ifdef AH_SUPPORT_AR5312
#define	BUSTAG(ah) \
	((bus_space_tag_t) ((struct ar531x_config *)((ah)->ah_st))->tag)
#else
#define	BUSTAG(ah)	((ah)->ah_st)
#endif

extern	void ath_hal_printf(struct ath_hal *, const char*, ...)
		__printflike(2,3);
extern	void ath_hal_vprintf(struct ath_hal *, const char*, __va_list)
		__printflike(2, 0);
extern	const char* ath_hal_ether_sprintf(const u_int8_t *mac);
extern	void *ath_hal_malloc(size_t);
extern	void ath_hal_free(void *);
#ifdef AH_ASSERT
extern	void ath_hal_assert_failed(const char* filename,
		int lineno, const char* msg);
#endif
#ifdef AH_DEBUG
extern	void DO_HALDEBUG(struct ath_hal *ah, u_int mask, const char* fmt, ...);
#endif /* AH_DEBUG */

/* NB: put this here instead of the driver to avoid circular references */
SYSCTL_NODE(_hw, OID_AUTO, ath, CTLFLAG_RD, 0, "Atheros driver parameters");
SYSCTL_NODE(_hw_ath, OID_AUTO, hal, CTLFLAG_RD, 0, "Atheros HAL parameters");

#ifdef AH_DEBUG
int ath_hal_debug = 0;
SYSCTL_INT(_hw_ath_hal, OID_AUTO, debug, CTLFLAG_RW, &ath_hal_debug,
    0, "Atheros HAL debugging printfs");
TUNABLE_INT("hw.ath.hal.debug", &ath_hal_debug);
#endif /* AH_DEBUG */

MALLOC_DEFINE(M_ATH_HAL, "ath_hal", "ath hal data");

void*
ath_hal_malloc(size_t size)
{
	return malloc(size, M_ATH_HAL, M_NOWAIT | M_ZERO);
}

void
ath_hal_free(void* p)
{
	free(p, M_ATH_HAL);
}

void
ath_hal_vprintf(struct ath_hal *ah, const char* fmt, va_list ap)
{
	vprintf(fmt, ap);
}

void
ath_hal_printf(struct ath_hal *ah, const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	ath_hal_vprintf(ah, fmt, ap);
	va_end(ap);
}

const char*
ath_hal_ether_sprintf(const u_int8_t *mac)
{
	return ether_sprintf(mac);
}

#ifdef AH_DEBUG

/* This must match the definition in ath_hal/ah_debug.h */
#define	HAL_DEBUG_UNMASKABLE	0xf0000000
void
DO_HALDEBUG(struct ath_hal *ah, u_int mask, const char* fmt, ...)
{
	if ((mask == HAL_DEBUG_UNMASKABLE) ||
	    (ah->ah_config.ah_debug & mask) ||
	    (ath_hal_debug & mask)) {
		__va_list ap;
		va_start(ap, fmt);
		ath_hal_vprintf(ah, fmt, ap);
		va_end(ap);
	}
}
#undef	HAL_DEBUG_UNMASKABLE
#endif /* AH_DEBUG */

#ifdef AH_DEBUG_ALQ
/*
 * ALQ register tracing support.
 *
 * Setting hw.ath.hal.alq=1 enables tracing of all register reads and
 * writes to the file /tmp/ath_hal.log.  The file format is a simple
 * fixed-size array of records.  When done logging set hw.ath.hal.alq=0
 * and then decode the file with the arcode program (that is part of the
 * HAL).  If you start+stop tracing the data will be appended to an
 * existing file.
 *
 * NB: doesn't handle multiple devices properly; only one DEVICE record
 *     is emitted and the different devices are not identified.
 */
#include <sys/alq.h>
#include <sys/pcpu.h>
#include <dev/ath/ath_hal/ah_decode.h>

static	struct alq *ath_hal_alq;
static	int ath_hal_alq_emitdev;	/* need to emit DEVICE record */
static	u_int ath_hal_alq_lost;		/* count of lost records */
static	char ath_hal_logfile[MAXPATHLEN] = "/tmp/ath_hal.log";

SYSCTL_STRING(_hw_ath_hal, OID_AUTO, alq_logfile, CTLFLAG_RW,
    &ath_hal_logfile, sizeof(kernelname), "Name of ALQ logfile");

static	u_int ath_hal_alq_qsize = 64*1024;

static int
ath_hal_setlogging(int enable)
{
	int error;

	if (enable) {
		error = alq_open(&ath_hal_alq, ath_hal_logfile,
			curthread->td_ucred, ALQ_DEFAULT_CMODE,
			sizeof (struct athregrec), ath_hal_alq_qsize);
		ath_hal_alq_lost = 0;
		ath_hal_alq_emitdev = 1;
		printf("ath_hal: logging to %s enabled\n",
			ath_hal_logfile);
	} else {
		if (ath_hal_alq)
			alq_close(ath_hal_alq);
		ath_hal_alq = NULL;
		printf("ath_hal: logging disabled\n");
		error = 0;
	}
	return (error);
}

static int
sysctl_hw_ath_hal_log(SYSCTL_HANDLER_ARGS)
{
	int error, enable;

	enable = (ath_hal_alq != NULL);
        error = sysctl_handle_int(oidp, &enable, 0, req);
        if (error || !req->newptr)
                return (error);
	else
		return (ath_hal_setlogging(enable));
}
SYSCTL_PROC(_hw_ath_hal, OID_AUTO, alq, CTLTYPE_INT|CTLFLAG_RW,
	0, 0, sysctl_hw_ath_hal_log, "I", "Enable HAL register logging");
SYSCTL_INT(_hw_ath_hal, OID_AUTO, alq_size, CTLFLAG_RW,
	&ath_hal_alq_qsize, 0, "In-memory log size (#records)");
SYSCTL_INT(_hw_ath_hal, OID_AUTO, alq_lost, CTLFLAG_RW,
	&ath_hal_alq_lost, 0, "Register operations not logged");

static struct ale *
ath_hal_alq_get(struct ath_hal *ah)
{
	struct ale *ale;

	if (ath_hal_alq_emitdev) {
		ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
		if (ale) {
			struct athregrec *r =
				(struct athregrec *) ale->ae_data;
			r->op = OP_DEVICE;
			r->reg = 0;
			r->val = ah->ah_devid;
			alq_post(ath_hal_alq, ale);
			ath_hal_alq_emitdev = 0;
		} else
			ath_hal_alq_lost++;
	}
	ale = alq_get(ath_hal_alq, ALQ_NOWAIT);
	if (!ale)
		ath_hal_alq_lost++;
	return ale;
}

void
ath_hal_reg_write(struct ath_hal *ah, u_int32_t reg, u_int32_t val)
{
	bus_space_tag_t tag = BUSTAG(ah);
	bus_space_handle_t h = ah->ah_sh;

	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_WRITE;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
	}
#if _BYTE_ORDER == _BIG_ENDIAN
	if (OS_REG_UNSWAPPED(reg))
		bus_space_write_4(tag, h, reg, val);
	else
#endif
		bus_space_write_stream_4(tag, h, reg, val);
}

u_int32_t
ath_hal_reg_read(struct ath_hal *ah, u_int32_t reg)
{
	bus_space_tag_t tag = BUSTAG(ah);
	bus_space_handle_t h = ah->ah_sh;
	u_int32_t val;

#if _BYTE_ORDER == _BIG_ENDIAN
	if (OS_REG_UNSWAPPED(reg))
		val = bus_space_read_4(tag, h, reg);
	else
#endif
		val = bus_space_read_stream_4(tag, h, reg);
	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_READ;
			r->reg = reg;
			r->val = val;
			alq_post(ath_hal_alq, ale);
		}
	}
	return val;
}

void
OS_MARK(struct ath_hal *ah, u_int id, u_int32_t v)
{
	if (ath_hal_alq) {
		struct ale *ale = ath_hal_alq_get(ah);
		if (ale) {
			struct athregrec *r = (struct athregrec *) ale->ae_data;
			r->op = OP_MARK;
			r->reg = id;
			r->val = v;
			alq_post(ath_hal_alq, ale);
		}
	}
}
#elif defined(AH_DEBUG) || defined(AH_REGOPS_FUNC)
/*
 * Memory-mapped device register read/write.  These are here
 * as routines when debugging support is enabled and/or when
 * explicitly configured to use function calls.  The latter is
 * for architectures that might need to do something before
 * referencing memory (e.g. remap an i/o window).
 *
 * NB: see the comments in ah_osdep.h about byte-swapping register
 *     reads and writes to understand what's going on below.
 */

void
ath_hal_reg_write(struct ath_hal *ah, u_int32_t reg, u_int32_t val)
{
	bus_space_tag_t tag = BUSTAG(ah);
	bus_space_handle_t h = ah->ah_sh;

#if _BYTE_ORDER == _BIG_ENDIAN
	if (OS_REG_UNSWAPPED(reg))
		bus_space_write_4(tag, h, reg, val);
	else
#endif
		bus_space_write_stream_4(tag, h, reg, val);
}

u_int32_t
ath_hal_reg_read(struct ath_hal *ah, u_int32_t reg)
{
	bus_space_tag_t tag = BUSTAG(ah);
	bus_space_handle_t h = ah->ah_sh;
	u_int32_t val;

#if _BYTE_ORDER == _BIG_ENDIAN
	if (OS_REG_UNSWAPPED(reg))
		val = bus_space_read_4(tag, h, reg);
	else
#endif
		val = bus_space_read_stream_4(tag, h, reg);
	return val;
}
#endif /* AH_DEBUG || AH_REGOPS_FUNC */

#ifdef AH_ASSERT
void
ath_hal_assert_failed(const char* filename, int lineno, const char *msg)
{
	printf("Atheros HAL assertion failure: %s: line %u: %s\n",
		filename, lineno, msg);
	panic("ath_hal_assert");
}
#endif /* AH_ASSERT */
