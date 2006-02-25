/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD: src/sys/sparc64/include/intr_machdep.h,v 1.13 2003/07/16 00:08:43 jmg Exp $
 */

#ifndef	_MACHINE_INTR_MACHDEP_H_
#define	_MACHINE_INTR_MACHDEP_H_

#define	IRSR_BUSY	(1 << 5)

#define	PIL_MAX		(1 << 4)
#define	IV_MAX		(1 << 11)
#define IV_NAMLEN	1024

#define	IR_FREE		(PIL_MAX * 2)

#define	IH_SHIFT	PTR_SHIFT
#define	IQE_SHIFT	5
#define	IV_SHIFT	5

#define	PIL_LOW		1	/* stray interrupts */
#define	PIL_ITHREAD	2	/* interrupts that use ithreads */
#define	PIL_RENDEZVOUS	3	/* smp rendezvous ipi */
#define	PIL_AST		4	/* ast ipi */
#define	PIL_STOP	5	/* stop cpu ipi */
#define	PIL_FAST	13	/* fast interrupts */
#define	PIL_TICK	14

struct trapframe;

typedef	void ih_func_t(struct trapframe *);
typedef	void iv_func_t(void *);

struct ithd;

struct intr_request {
	struct	intr_request *ir_next;
	iv_func_t *ir_func;
	void	*ir_arg;
	u_int	ir_vec;
	u_int	ir_pri;
};

struct intr_vector {
	iv_func_t *iv_func;
	void	*iv_arg;
	struct	ithd *iv_ithd;
	u_int	iv_pri;
	u_int	iv_vec;
};

extern ih_func_t *intr_handlers[];
extern struct intr_vector intr_vectors[];

void	intr_setup(int level, ih_func_t *ihf, int pri, iv_func_t *ivf,
		   void *iva);
void	intr_init1(void);
void	intr_init2(void);
int	inthand_add(const char *name, int vec, void (*handler)(void *),
    void *arg, int flags, void **cookiep);
int	inthand_remove(int vec, void *cookie);

ih_func_t intr_fast;

#endif
