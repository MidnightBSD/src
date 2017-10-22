/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/lib/libgeom/geom_util.c 169299 2007-05-06 01:17:46Z pjd $");

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <paths.h>

#include <libgeom.h>

/*
 * Open the given provider and at least check if this is a block device.
 */
int
g_open(const char *name, int write)
{
	char path[MAXPATHLEN];
	int fd;

	if (name[0] == '/')
		strlcpy(path, name, sizeof(path));
	else
		snprintf(path, sizeof(path), "%s%s", _PATH_DEV, name);

	fd = open(path, write ? O_RDWR : O_RDONLY);
	if (fd == -1)
		return (-1);
	/* Let try to get sectorsize, which will prove it is a GEOM provider. */
	if (g_sectorsize(fd) == -1) {
		close(fd);
		errno = EFTYPE;
		return (-1);
	}
	return (fd);
}

int
g_close(int fd)
{

	return (close(fd));
}

static int
g_ioctl_arg(int fd, unsigned long cmd, void *arg)
{
	int ret;

	if (arg != NULL)
		ret = ioctl(fd, cmd, arg);
	else
		ret = ioctl(fd, cmd);
	return (ret >= 0 ? 0 : -1);
}

static int
g_ioctl(int fd, unsigned long cmd)
{

	return (g_ioctl_arg(fd, cmd, NULL));
}

/*
 * Return media size of the given provider.
 */
off_t
g_mediasize(int fd)
{
	off_t mediasize;

	if (g_ioctl_arg(fd, DIOCGMEDIASIZE, &mediasize) == -1)
		mediasize = -1;
	return (mediasize);
}

/*
 * Return sector size of the given provider.
 */
ssize_t
g_sectorsize(int fd)
{
	u_int sectorsize;

	if (g_ioctl_arg(fd, DIOCGSECTORSIZE, &sectorsize) == -1)
		return (-1);
	return ((ssize_t)sectorsize);
}

/*
 * Call BIO_FLUSH for the given provider.
 */
int
g_flush(int fd)
{

	return (g_ioctl(fd, DIOCGFLUSH));
}

/*
 * Call BIO_DELETE for the given range.
 */
int
g_delete(int fd, off_t offset, off_t length)
{
	off_t arg[2];

	arg[0] = offset;
	arg[1] = length;
	return (g_ioctl_arg(fd, DIOCGDELETE, arg));
}

/*
 * Return ID of the given provider.
 */
int
g_get_ident(int fd, char *ident, size_t size)
{
	char lident[DISK_IDENT_SIZE];

	if (g_ioctl_arg(fd, DIOCGIDENT, lident) == -1)
		return (-1);
	if (lident[0] == '\0') {
		errno = ENOENT;
		return (-1);
	}
	if (strlcpy(ident, lident, size) >= size) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return (0);
}

/*
 * Return name of the provider, which has the given ID.
 */
int
g_get_name(const char *ident, char *name, size_t size)
{
	int fd;

	fd = g_open_by_ident(ident, 0, name, size);
	if (fd == -1)
		return (-1);
	g_close(fd);
	return (0);
}

/*
 * Find provider name by the given ID.
 */
int
g_open_by_ident(const char *ident, int write, char *name, size_t size)
{
	char lident[DISK_IDENT_SIZE];
	struct gmesh mesh;
	struct gclass *mp;
	struct ggeom *gp;
	struct gprovider *pp;
	int error, fd;

	error = geom_gettree(&mesh);
	if (error != 0) {
		errno = error;
		return (-1);
	}

	error = ENOENT;
	fd = -1;

	LIST_FOREACH(mp, &mesh.lg_class, lg_class) {
		LIST_FOREACH(gp, &mp->lg_geom, lg_geom) {
			LIST_FOREACH(pp, &gp->lg_provider, lg_provider) {
				fd = g_open(pp->lg_name, write);
				if (fd == -1)
					continue;
				if (g_get_ident(fd, lident,
				    sizeof(lident)) == -1) {
					g_close(fd);
					continue;
				}
				if (strcmp(ident, lident) != 0) {
					g_close(fd);
					continue;
				}
				error = 0;
				if (name != NULL && strlcpy(name, pp->lg_name,
				    size) >= size) {
					error = ENAMETOOLONG;
					g_close(fd);
				}
				goto end;
			}
		}
	}
end:
	geom_deletetree(&mesh);
	if (error != 0) {
		errno = error;
		return (-1);
	}
	return (fd);
}
