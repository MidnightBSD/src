/*-
 * Copyright (c) 1999, 2000 John D. Polstra.
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
 * $FreeBSD: stable/10/libexec/rtld-elf/ia64/rtld_machdep.h 309061 2016-11-23 17:48:43Z kib $
 */

#ifndef RTLD_MACHDEP_H
#define RTLD_MACHDEP_H	1

#include <sys/types.h>
#include <machine/atomic.h>

/*
 * Macros for cracking ia64 function pointers.
 */
struct fptr {
	Elf_Addr	target;
	Elf_Addr	gp;
};

#define FPTR_TARGET(f)	(((struct fptr *) (f))->target)
#define FPTR_GP(f)	(((struct fptr *) (f))->gp)

/* Return the address of the .dynamic section in the dynamic linker. */
#define rtld_dynamic(obj)	(&_DYNAMIC)

struct Struct_Obj_Entry;

Elf_Addr reloc_jmpslot(Elf_Addr *, Elf_Addr, const struct Struct_Obj_Entry *,
		       const struct Struct_Obj_Entry *, const Elf_Rel *);
void *make_function_pointer(const Elf_Sym *, const struct Struct_Obj_Entry *);
void call_initfini_pointer(const struct Struct_Obj_Entry *, Elf_Addr);
void call_init_pointer(const struct Struct_Obj_Entry *, Elf_Addr);

#define        call_ifunc_resolver(ptr) \
       (((Elf_Addr (*)(void))ptr)())

#define	TLS_TCB_SIZE	16

#define round(size, align) \
	(((size) + (align) - 1) & ~((align) - 1))
#define calculate_first_tls_offset(size, align) \
	round(TLS_TCB_SIZE, align)
#define calculate_tls_offset(prev_offset, prev_size, size, align) \
	round(prev_offset + prev_size, align)
#define calculate_tls_end(off, size) 	((off) + (size))

extern void *__tls_get_addr(unsigned long module, unsigned long offset);

#define	RTLD_DEFAULT_STACK_PF_EXEC	0
#define	RTLD_DEFAULT_STACK_EXEC		0

#define	RTLD_INIT_PAGESIZES_EARLY	1

#endif
