/*-
 * Copyright (c) 2003 John Baldwin <jhb@FreeBSD.org>
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
 *
 * $FreeBSD: src/sys/i386/include/intr_machdep.h,v 1.7.2.3 2006/03/10 19:37:33 jhb Exp $
 */

#ifndef __MACHINE_INTR_MACHDEP_H__
#define	__MACHINE_INTR_MACHDEP_H__

#ifdef _KERNEL

/*
 * The maximum number of I/O interrupts we allow.  This number is rather
 * arbitrary as it is just the maximum IRQ resource value.  The interrupt
 * source for a given IRQ maps that I/O interrupt to device interrupt
 * source whether it be a pin on an interrupt controller or an MSI interrupt.
 * The 16 ISA IRQs are assigned fixed IDT vectors, but all other device
 * interrupts allocate IDT vectors on demand.  Currently we have 191 IDT
 * vectors available for device interrupts.  On many systems with I/O APICs,
 * a lot of the IRQs are not used, so this number can be much larger than
 * 191 and still be safe since only interrupt sources in actual use will
 * allocate IDT vectors.
 *
 * For now we stick with 255 as ISA IRQs and PCI intline IRQs only allow
 * for IRQs in the range 0 - 254.  When MSI support is added this number
 * will likely increase.
 */
#define	NUM_IO_INTS	255

/*
 * - 1 ??? dummy counter.
 * - 2 counters for each I/O interrupt.
 * - 1 counter for each CPU for lapic timer.
 * - 7 counters for each CPU for IPI counters for SMP.
 */
#ifdef SMP
#define	INTRCNT_COUNT	(1 + NUM_IO_INTS * 2 + 1)
#else
#define	INTRCNT_COUNT	(1 + NUM_IO_INTS * 2 + (1 + 7) * MAXCPU)
#endif

#ifndef LOCORE

typedef void inthand_t(u_int cs, u_int ef, u_int esp, u_int ss);

#define	IDTVEC(name)	__CONCAT(X,name)

struct intsrc;

/*
 * Methods that a PIC provides to mask/unmask a given interrupt source,
 * "turn on" the interrupt on the CPU side by setting up an IDT entry, and
 * return the vector associated with this source.
 */
struct pic {
	void (*pic_enable_source)(struct intsrc *);
	void (*pic_disable_source)(struct intsrc *, int);
	void (*pic_eoi_source)(struct intsrc *);
	void (*pic_enable_intr)(struct intsrc *);
	int (*pic_vector)(struct intsrc *);
	int (*pic_source_pending)(struct intsrc *);
	void (*pic_suspend)(struct intsrc *);
	void (*pic_resume)(struct intsrc *);
	int (*pic_config_intr)(struct intsrc *, enum intr_trigger,
	    enum intr_polarity);
	void (*pic_assign_cpu)(struct intsrc *, u_int apic_id);
};

/* Flags for pic_disable_source() */
enum {
	PIC_EOI,
	PIC_NO_EOI,
};

/*
 * An interrupt source.  The upper-layer code uses the PIC methods to
 * control a given source.  The lower-layer PIC drivers can store additional
 * private data in a given interrupt source such as an interrupt pin number
 * or an I/O APIC pointer.
 */
struct intsrc {
	struct pic *is_pic;
	struct intr_event *is_event;
	u_long *is_count;
	u_long *is_straycount;
	u_int is_index;
	u_int is_enabled:1;
};

struct intrframe;

extern struct mtx icu_lock;
extern int elcr_found;

/* XXX: The elcr_* prototypes probably belong somewhere else. */
int	elcr_probe(void);
enum intr_trigger elcr_read_trigger(u_int irq);
void	elcr_resume(void);
void	elcr_write_trigger(u_int irq, enum intr_trigger trigger);
#ifdef SMP
void	intr_add_cpu(u_int apic_id);
#else
#define	intr_add_cpu(apic_id)
#endif
int	intr_add_handler(const char *name, int vector, driver_intr_t handler,
    void *arg, enum intr_type flags, void **cookiep);
int	intr_config_intr(int vector, enum intr_trigger trig,
    enum intr_polarity pol);
void	intr_execute_handlers(struct intsrc *isrc, struct intrframe *iframe);
struct intsrc *intr_lookup_source(int vector);
int	intr_register_source(struct intsrc *isrc);
int	intr_remove_handler(void *cookie);
void	intr_resume(void);
void	intr_suspend(void);
void	intrcnt_add(const char *name, u_long **countp);

#endif	/* !LOCORE */
#endif	/* _KERNEL */
#endif	/* !__MACHINE_INTR_MACHDEP_H__ */
