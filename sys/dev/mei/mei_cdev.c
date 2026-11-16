/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt <luke@foolishgames.com>
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

/*
 * mei(4) character device: one host client per open file.
 *
 * All user data is staged in kernel buffers here; the core (mei.c) never
 * touches user memory and this file never holds sc_mtx.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <dev/mei/mei_reg.h>
#include <dev/mei/mei_var.h>
#include <dev/mei/meiio.h>

static d_open_t		mei_cdev_open;
static d_read_t		mei_cdev_read;
static d_write_t	mei_cdev_write;
static d_ioctl_t	mei_cdev_ioctl;
static d_poll_t		mei_cdev_poll;

static struct cdevsw mei_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"mei",
	.d_open =	mei_cdev_open,
	.d_read =	mei_cdev_read,
	.d_write =	mei_cdev_write,
	.d_ioctl =	mei_cdev_ioctl,
	.d_poll =	mei_cdev_poll,
};

static void
mei_cdev_dtor(void *arg)
{
	struct mei_cl *cl = arg;

	mei_cl_free(cl);
}

static int
mei_cdev_open(struct cdev *dev, int oflags __unused, int devtype __unused,
    struct thread *td __unused)
{
	struct mei_softc *sc = dev->si_drv1;
	struct mei_cl *cl;
	int error;

	error = mei_wait_ready(sc);
	if (error != 0)
		return (error);
	error = mei_cl_alloc(sc, &cl);
	if (error != 0)
		return (error);
	error = devfs_set_cdevpriv(cl, mei_cdev_dtor);
	if (error != 0) {
		mei_cl_free(cl);
		return (error);
	}
	return (0);
}

/*
 * One read returns at most one message.  A message larger than the
 * caller's buffer is returned in pieces by successive reads.
 */
static int
mei_cdev_read(struct cdev *dev __unused, struct uio *uio, int ioflag)
{
	struct mei_cl *cl;
	uint8_t *buf;
	size_t n, done;
	int error;

	error = devfs_get_cdevpriv((void **)&cl);
	if (error != 0)
		return (error);
	if (uio->uio_resid <= 0)
		return (0);
	n = MIN((size_t)uio->uio_resid, MEI_MAX_MSG_LEN);
	buf = malloc(n, M_MEI, M_WAITOK);
	done = 0;
	error = mei_cl_read(cl, buf, n, (ioflag & IO_NDELAY) != 0, &done);
	if (error == 0 && done > n)
		error = EIO;
	if (error == 0 && done > 0)
		error = uiomove(buf, done, uio);
	free(buf, M_MEI);
	return (error);
}

/*
 * One write sends exactly one message of uio_resid bytes.
 */
static int
mei_cdev_write(struct cdev *dev __unused, struct uio *uio, int ioflag)
{
	struct mei_cl *cl;
	uint8_t *buf;
	ssize_t resid;
	off_t offset;
	size_t len;
	int error;

	error = devfs_get_cdevpriv((void **)&cl);
	if (error != 0)
		return (error);
	if (uio->uio_resid <= 0)
		return (0);
	if (uio->uio_resid > MEI_MAX_MSG_LEN)
		return (EMSGSIZE);
	resid = uio->uio_resid;
	offset = uio->uio_offset;
	len = (size_t)resid;
	buf = malloc(len, M_MEI, M_WAITOK);
	error = uiomove(buf, len, uio);
	if (error == 0)
		error = mei_cl_write(cl, buf, len, (ioflag & IO_NDELAY) != 0);
	free(buf, M_MEI);
	if (error != 0) {
		/*
		 * The message was not sent, so none of it counts as
		 * written.  dofilewrite() computes the byte count from
		 * uio_resid alone and drops EINTR, ERESTART and
		 * EWOULDBLOCK when uio_resid has changed, which would make
		 * a failed write look like a successful one.  Restore
		 * uio_resid (and uio_offset, which devfs copies back into
		 * the file offset) so the error is reported.  The iovecs
		 * stay advanced, but nothing reads them after an error.
		 */
		uio->uio_resid = resid;
		uio->uio_offset = offset;
	}
	return (error);
}

static int
mei_cdev_ioctl(struct cdev *dev __unused, u_long cmd, caddr_t data,
    int fflag __unused, struct thread *td __unused)
{
	struct mei_connect_client_data *ccd;
	struct mei_client_props props;
	struct mei_cl *cl;
	uint8_t uuid[16];
	int error;

	error = devfs_get_cdevpriv((void **)&cl);
	if (error != 0)
		return (error);

	switch (cmd) {
	case IOCTL_MEI_CONNECT_CLIENT:
		/*
		 * data is a kernel copy of the user structure, sized by the
		 * ioctl number.  The input GUID and the output properties
		 * share storage, so take the GUID out before writing.
		 */
		ccd = (struct mei_connect_client_data *)data;
		CTASSERT(sizeof(uuid) == sizeof(ccd->in_client_uuid));
		memcpy(uuid, ccd->in_client_uuid, sizeof(uuid));
		memset(&props, 0, sizeof(props));
		error = mei_cl_connect(cl, uuid, &props);
		if (error != 0)
			break;
		memset(ccd, 0, sizeof(*ccd));
		ccd->out_client_properties.max_msg_length =
		    props.max_msg_length;
		ccd->out_client_properties.protocol_version =
		    props.protocol_version;
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

static int
mei_cdev_poll(struct cdev *dev __unused, int events, struct thread *td)
{
	struct mei_cl *cl;

	if (devfs_get_cdevpriv((void **)&cl) != 0)
		return (events &
		    (POLLHUP | POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM));
	return (mei_cl_poll(cl, events, td));
}

int
mei_cdev_create(struct mei_softc *sc)
{
	struct make_dev_args args;
	int error;

	make_dev_args_init(&args);
	args.mda_flags = MAKEDEV_WAITOK;
	args.mda_devsw = &mei_cdevsw;
	args.mda_uid = UID_ROOT;
	args.mda_gid = GID_WHEEL;
	args.mda_mode = 0600;
	args.mda_si_drv1 = sc;
	error = make_dev_s(&args, &sc->sc_cdev, "mei%d",
	    device_get_unit(sc->sc_dev));
	if (error != 0)
		sc->sc_cdev = NULL;
	return (error);
}

void
mei_cdev_destroy(struct mei_softc *sc)
{

	/*
	 * destroy_dev() waits for threads inside the cdevsw methods and
	 * runs the cdevpriv destructors of files still open.
	 */
	if (sc->sc_cdev != NULL) {
		destroy_dev(sc->sc_cdev);
		sc->sc_cdev = NULL;
	}
}
