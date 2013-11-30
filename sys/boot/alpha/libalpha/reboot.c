/*-
 * Copyright (c) 1998 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/boot/alpha/libalpha/reboot.c,v 1.3 2004/01/04 23:21:01 obrien Exp $");

#include <stand.h>
#include <machine/rpb.h>

void
reboot(void)
{
    struct rpb *hwrpb = (struct rpb *)HWRPB_ADDR;
    struct pcs *pcs = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
    pcs->pcs_flags |= PCS_HALT_WARM_BOOT;
    alpha_pal_halt();
}

void
exit(int code)
{
    struct rpb *hwrpb = (struct rpb *)HWRPB_ADDR;
    struct pcs *pcs = LOCATE_PCS(hwrpb, hwrpb->rpb_primary_cpu_id);
    pcs->pcs_flags |= PCS_HALT_STAY_HALTED;
    alpha_pal_halt();
}
