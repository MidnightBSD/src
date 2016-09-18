/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998,2000 Doug Rabson <dfr@freebsd.org>
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
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include "bootstrap.h"
#include "disk.h"
#include "libuserboot.h"

#define	USERBOOT_VERSION	USERBOOT_VERSION_2

struct loader_callbacks *callbacks;
void *callbacks_arg;

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];
static jmp_buf jb;

struct arch_switch archsw;	/* MI/MD interface boundary */

static void	extract_currdev(void);

void
delay(int usec)
{

        CALLBACK(delay, usec);
}

void
exit(int v)
{

	CALLBACK(exit, v);
	longjmp(jb, 1);
}

void
loader_main(struct loader_callbacks *cb, void *arg, int version, int ndisks)
{
	static char malloc[512*1024];
	int i;

        if (version != USERBOOT_VERSION)
                abort();

	callbacks = cb;
        callbacks_arg = arg;
	userboot_disk_maxunit = ndisks;

	/*
	 * initialise the heap as early as possible.  Once this is done,
	 * alloc() is usable. The stack is buried inside us, so this is
	 * safe.
	 */
	setheap((void *)malloc, (void *)(malloc + 512*1024));

        /*
         * Hook up the console
         */
	cons_probe();

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
#if 0
	printf("Memory: %ld k\n", memsize() / 1024);
#endif

	setenv("LINES", "24", 1);	/* optional */

	archsw.arch_autoload = userboot_autoload;
	archsw.arch_getdev = userboot_getdev;
	archsw.arch_copyin = userboot_copyin;
	archsw.arch_copyout = userboot_copyout;
	archsw.arch_readin = userboot_readin;

	extract_currdev();

	if (setjmp(jb))
		return;

	interact();			/* doesn't return */

	exit(0);
}

/*
 * Set the 'current device' by (if possible) recovering the boot device as 
 * supplied by the initial bootstrap.
 */
static void
extract_currdev(void)
{
	struct disk_devdesc dev;

	//bzero(&dev, sizeof(dev));

	if (userboot_disk_maxunit > 0) {
		dev.d_dev = &userboot_disk;
		dev.d_type = dev.d_dev->dv_type;
		dev.d_unit = 0;
		dev.d_slice = 0;
		dev.d_partition = 0;
		/*
		 * Figure out if we are using MBR or GPT - for GPT we
		 * set the partition to 0 since everything is a GPT slice.
		 */
		if (dev.d_dev->dv_open(NULL, &dev))
			dev.d_partition = 255;
	} else {
		dev.d_dev = &host_dev;
		dev.d_type = dev.d_dev->dv_type;
		dev.d_unit = 0;
	}

	env_setenv("currdev", EV_VOLATILE, userboot_fmtdev(&dev),
            userboot_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, userboot_fmtdev(&dev),
            env_noset, env_nounset);
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{

	exit(USERBOOT_EXIT_QUIT);
	return (CMD_OK);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{

	exit(USERBOOT_EXIT_REBOOT);
	return (CMD_OK);
}
