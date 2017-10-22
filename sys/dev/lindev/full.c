/*-
 * Copyright (c) 2009 Ed Schouten <ed@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <dev/lindev/lindev.h>

static struct cdev *full_dev;

static d_read_t full_read;
static d_write_t full_write;

static struct cdevsw full_cdevsw = {
	.d_version =	D_VERSION,
	.d_read =	full_read,
	.d_write =	full_write,
	.d_name =	"full",
};

static void *zbuf;

/* ARGSUSED */
static int
full_read(struct cdev *dev __unused, struct uio *uio, int flags __unused)
{
	int error = 0;

	while (uio->uio_resid > 0 && error == 0)
		error = uiomove(zbuf, MIN(uio->uio_resid, PAGE_SIZE), uio);

	return (error);
}

/* ARGSUSED */
static int
full_write(struct cdev *dev __unused, struct uio *uio __unused,
    int flags __unused)
{

	return (ENOSPC);
}

/* ARGSUSED */
int
lindev_modevent_full(module_t mod __unused, int type, void *data __unused)
{

	switch(type) {
	case MOD_LOAD:
		zbuf = (void *)malloc(PAGE_SIZE, M_TEMP, M_WAITOK | M_ZERO);
		full_dev = make_dev(&full_cdevsw, 0, UID_ROOT, GID_WHEEL,
		    0666, "full");
		if (bootverbose)
			printf("full: <full device>\n");
		break;

	case MOD_UNLOAD:
		destroy_dev(full_dev);
		free(zbuf, M_TEMP);
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

