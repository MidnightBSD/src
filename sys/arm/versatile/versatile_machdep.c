/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2012 Oleksandr Tymoshenko.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#include <machine/platform.h> 

#include <dev/fdt/fdt_common.h>

/* Start of address space used for bootstrap map */
#define DEVMAP_BOOTSTRAP_MAP_START	0xE0000000

vm_offset_t
platform_lastaddr(void)
{

	return (DEVMAP_BOOTSTRAP_MAP_START);
}

void
platform_probe_and_attach(void)
{

}

void
platform_gpio_init(void)
{
}

void
platform_late_init(void)
{
}

#define FDT_DEVMAP_MAX	(2)		/* FIXME */
static struct devmap_entry fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, },
	{ 0, 0, 0, }
};


/*
 * Construct devmap table with DT-derived config data.
 */
int
platform_devmap_init(void)
{
	int i = 0;
	fdt_devmap[i].pd_va = 0xf0100000;
	fdt_devmap[i].pd_pa = 0x10100000;
	fdt_devmap[i].pd_size = 0x01000000;       /* 1 MB */

	devmap_register_table(&fdt_devmap[0]);
	return (0);
}

void
cpu_reset(void)
{
	printf("cpu_reset\n");
	while (1);
}

