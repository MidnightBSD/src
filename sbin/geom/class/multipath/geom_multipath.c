/*-
 * Copyright (c) 2006 Mathew Jacob <mjacob@FreeBSD.org>
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
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <libgeom.h>
#include <unistd.h>
#include <uuid.h>
#include <geom/multipath/g_multipath.h>

#include "core/geom.h"
#include "misc/subr.h"

uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_MULTIPATH_VERSION;

static void mp_main(struct gctl_req *, unsigned int);
static void mp_label(struct gctl_req *);
static void mp_clear(struct gctl_req *);

struct g_command class_commands[] = {
	{
		"create", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
		{
			{ 'A', "active_active", NULL, G_TYPE_BOOL },
			{ 'R', "active_read", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL
		},
		"[-vAR] name prov ..."
	},
	{
		"label", G_FLAG_VERBOSE | G_FLAG_LOADKLD, mp_main,
		{
			{ 'A', "active_active", NULL, G_TYPE_BOOL },
			{ 'R', "active_read", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL
		},
		"[-vAR] name prov ..."
	},
	{ "configure", G_FLAG_VERBOSE, NULL,
		{
			{ 'A', "active_active", NULL, G_TYPE_BOOL },
			{ 'P', "active_passive", NULL, G_TYPE_BOOL },
			{ 'R', "active_read", NULL, G_TYPE_BOOL },
			G_OPT_SENTINEL
		},
		"[-vAPR] name"
	},
	{
		"add", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name prov"
	},
	{
		"remove", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name prov"
	},
	{
		"fail", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name prov"
	},
	{
		"restore", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name prov"
	},
	{
		"rotate", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name"
	},
	{
		"getactive", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name"
	},
	{
		"destroy", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name"
	},
	{
		"stop", G_FLAG_VERBOSE, NULL, G_NULL_OPTS,
		"[-v] name"
	},
	{
		"clear", G_FLAG_VERBOSE, mp_main, G_NULL_OPTS,
		"[-v] prov ..."
	},
	G_CMD_SENTINEL
};

static void
mp_main(struct gctl_req *req, unsigned int flags __unused)
{
	const char *name;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "label") == 0) {
		mp_label(req);
	} else if (strcmp(name, "clear") == 0) {
		mp_clear(req);
	} else {
		gctl_error(req, "Unknown command: %s.", name);
	}
}

static void
mp_label(struct gctl_req *req)
{
	struct g_multipath_metadata md;
	off_t disksize = 0, msize;
	uint8_t *sector, *rsector;
	char *ptr;
	uuid_t uuid;
	uint32_t secsize = 0, ssize, status;
	const char *name, *name2, *mpname;
	int error, i, nargs, fd;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 2) {
		gctl_error(req, "wrong number of arguments.");
		return;
	}

	/*
	 * First, check each provider to make sure it's the same size.
	 * This also gets us our size and sectorsize for the metadata.
	 */
	for (i = 1; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		msize = g_get_mediasize(name);
		ssize = g_get_sectorsize(name);
		if (msize == 0 || ssize == 0) {
			gctl_error(req, "cannot get information about %s: %s.",
			    name, strerror(errno));
			return;
		}
		if (i == 1) {
			secsize = ssize;
			disksize = msize;
		} else {
			if (secsize != ssize) {
				gctl_error(req, "%s sector size %u different.",
				    name, ssize);
				return;
			}
			if (disksize != msize) {
				gctl_error(req, "%s media size %ju different.",
				    name, (intmax_t)msize);
				return;
			}
		}
		
	}

	/*
	 * Generate metadata.
	 */
	strlcpy(md.md_magic, G_MULTIPATH_MAGIC, sizeof(md.md_magic));
	md.md_version = G_MULTIPATH_VERSION;
	mpname = gctl_get_ascii(req, "arg0");
	strlcpy(md.md_name, mpname, sizeof(md.md_name));
	md.md_size = disksize;
	md.md_sectorsize = secsize;
	uuid_create(&uuid, &status);
	if (status != uuid_s_ok) {
		gctl_error(req, "cannot create a UUID.");
		return;
	}
	uuid_to_string(&uuid, &ptr, &status);
	if (status != uuid_s_ok) {
		gctl_error(req, "cannot stringify a UUID.");
		return;
	}
	strlcpy(md.md_uuid, ptr, sizeof (md.md_uuid));
	md.md_active_active = gctl_get_int(req, "active_active");
	if (gctl_get_int(req, "active_read"))
		md.md_active_active = 2;
	free(ptr);

	/*
	 * Allocate a sector to write as metadata.
	 */
	sector = malloc(secsize);
	if (sector == NULL) {
		gctl_error(req, "unable to allocate metadata buffer");
		return;
	}
	memset(sector, 0, secsize);
	rsector = malloc(secsize);
	if (rsector == NULL) {
		free(sector);
		gctl_error(req, "unable to allocate metadata buffer");
		return;
	}

	/*
	 * encode the metadata
	 */
	multipath_metadata_encode(&md, sector);

	/*
	 * Store metadata on the initial provider.
	 */
	name = gctl_get_ascii(req, "arg1");
	error = g_metadata_store(name, sector, secsize);
	if (error != 0) {
		gctl_error(req, "cannot store metadata on %s: %s.", name, strerror(error));
		return;
	}

	/*
	 * Now touch the rest of the providers to hint retaste.
	 */
	for (i = 2; i < nargs; i++) {
		name2 = gctl_get_ascii(req, "arg%d", i);
		fd = g_open(name2, 1);
		if (fd < 0) {
			fprintf(stderr, "Unable to open %s: %s.\n",
			    name2, strerror(errno));
			continue;
		}
		if (pread(fd, rsector, secsize, disksize - secsize) !=
		    (ssize_t)secsize) {
			fprintf(stderr, "Unable to read metadata from %s: %s.\n",
			    name2, strerror(errno));
			g_close(fd);
			continue;
		}
		g_close(fd);
		if (memcmp(sector, rsector, secsize)) {
			fprintf(stderr, "No metadata found on %s."
			    " It is not a path of %s.\n",
			    name2, name);
		}
	}
}


static void
mp_clear(struct gctl_req *req)
{
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(name, G_MULTIPATH_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Can't clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
	}
}

