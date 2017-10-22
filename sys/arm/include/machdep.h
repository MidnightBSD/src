/* $NetBSD: machdep.h,v 1.7 2002/02/21 02:52:21 thorpej Exp $ */
/* $FreeBSD: release/7.0.0/sys/arm/include/machdep.h 142570 2005-02-26 18:59:01Z cognet $ */

#ifndef _MACHDEP_BOOT_MACHDEP_H_
#define _MACHDEP_BOOT_MACHDEP_H_

/* misc prototypes used by the many arm machdeps */
void halt (void);
void data_abort_handler (trapframe_t *);
void prefetch_abort_handler (trapframe_t *);
void undefinedinstruction_bounce (trapframe_t *);

void arm_lock_cache_line(vm_offset_t);

#endif /* !_MACHINE_MACHDEP_H_ */
