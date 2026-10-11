/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
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

/*
 * Version lookups against a local ports tree.
 *
 * The version a port would build is whatever its Makefile computes for
 * PKGVERSION, so ask make(1) for it in ${PORTSDIR}/<origin>.  PKGVERSION
 * rather than PKGNAME keeps subpackages, whose names differ from the main
 * package, comparable.  PORTSDIR is passed on the command line so a tree
 * named with -P uses its own Mk/ framework rather than the default one.
 */

#include "mport.h"
#include "mport_private.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

#define PORTS_VERSION_MAX 256

static bool ports_valid_origin(/*@notnull@*/ const char *);
static bool ports_valid_flavor(/*@notnull@*/ const char *);
static int ports_make_version(/*@notnull@*/ const char *, /*@notnull@*/ const char *,
    /*@null@*/ const char *, /*@out@*/ char **);

/*
 * An origin is a relative path inside the tree: no leading slash, no empty,
 * "." or ".." component, and only the characters port directories use.
 */
static bool
ports_valid_origin(/*@notnull@*/ const char *origin)
{
	const char *p;
	const char *component;
	size_t len;

	if (origin[0] == '\0' || origin[0] == '/')
		return false;

	for (p = origin; *p != '\0'; p++) {
		if (!isalnum((unsigned char)*p) && strchr("._+-/", *p) == NULL)
			return false;
	}

	component = origin;
	for (;;) {
		p = strchr(component, '/');
		len = p != NULL ? (size_t)(p - component) : strlen(component);
		if (len == 0 || (len == 1 && component[0] == '.') ||
		    (len == 2 && component[0] == '.' && component[1] == '.'))
			return false;
		if (p == NULL)
			break;
		component = p + 1;
	}

	return true;
}

static bool
ports_valid_flavor(/*@notnull@*/ const char *flavor)
{
	const char *p;

	for (p = flavor; *p != '\0'; p++) {
		if (!isalnum((unsigned char)*p) && *p != '_')
			return false;
	}

	return true;
}

/*
 * Run "make -C portdir -V PKGVERSION PORTSDIR=... [FLAVOR=...]" and return
 * its trimmed output.  Any failure to run make, a non-zero exit or empty
 * output is an error.
 */
static int
ports_make_version(/*@notnull@*/ const char *portsdir, /*@notnull@*/ const char *portdir,
    /*@null@*/ const char *flavor, /*@out@*/ char **version)
{
	posix_spawn_file_actions_t actions;
	char *portsdir_arg = NULL;
	char *flavor_arg = NULL;
	char *argv[8];
	char buf[PORTS_VERSION_MAX];
	char discard[512];
	size_t used = 0;
	ssize_t n;
	int fds[2] = { -1, -1 };
	int status;
	int argc = 0;
	int ret = MPORT_OK;
	int err;
	pid_t pid;

	*version = NULL;

	if (asprintf(&portsdir_arg, "PORTSDIR=%s", portsdir) == -1)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	if (flavor != NULL && asprintf(&flavor_arg, "FLAVOR=%s", flavor) == -1) {
		free(portsdir_arg);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	argv[argc++] = MPORT_MAKE_BIN;
	argv[argc++] = "-C";
	argv[argc++] = (char *)portdir;
	argv[argc++] = "-V";
	argv[argc++] = "PKGVERSION";
	argv[argc++] = portsdir_arg;
	if (flavor_arg != NULL)
		argv[argc++] = flavor_arg;
	argv[argc] = NULL;

	if (pipe(fds) == -1) {
		ret = SET_ERRORX(MPORT_ERR_FATAL, "pipe failed: %s", strerror(errno));
		goto DONE;
	}

	if ((err = posix_spawn_file_actions_init(&actions)) != 0) {
		ret = SET_ERRORX(
		    MPORT_ERR_FATAL, "posix_spawn_file_actions_init failed: %s", strerror(err));
		goto DONE;
	}
	if ((err = posix_spawn_file_actions_addopen(
		 &actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0)) != 0 ||
	    (err = posix_spawn_file_actions_addopen(
		 &actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0)) != 0 ||
	    (err = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO)) != 0 ||
	    (err = posix_spawn_file_actions_addclose(&actions, fds[0])) != 0 ||
	    (err = posix_spawn_file_actions_addclose(&actions, fds[1])) != 0) {
		(void)posix_spawn_file_actions_destroy(&actions);
		ret = SET_ERRORX(MPORT_ERR_FATAL, "posix_spawn setup failed: %s", strerror(err));
		goto DONE;
	}

	err = posix_spawn(&pid, MPORT_MAKE_BIN, &actions, NULL, argv, environ);
	(void)posix_spawn_file_actions_destroy(&actions);
	if (err != 0) {
		ret = SET_ERRORX(
		    MPORT_ERR_WARN, "Unable to run %s: %s", MPORT_MAKE_BIN, strerror(err));
		goto DONE;
	}

	(void)close(fds[1]);
	fds[1] = -1;

	/* Read to EOF so make never blocks on a full pipe; keep only the start. */
	for (;;) {
		if (used < sizeof(buf) - 1)
			n = read(fds[0], buf + used, sizeof(buf) - 1 - used);
		else
			n = read(fds[0], discard, sizeof(discard));
		if (n == -1) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		if (used < sizeof(buf) - 1)
			used += (size_t)n;
	}
	buf[used] = '\0';

	while (waitpid(pid, &status, 0) == -1) {
		if (errno != EINTR) {
			ret = SET_ERRORX(MPORT_ERR_WARN, "waitpid failed: %s", strerror(errno));
			goto DONE;
		}
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		ret = SET_ERRORX(
		    MPORT_ERR_WARN, "%s -V PKGVERSION failed in %s", MPORT_MAKE_BIN, portdir);
		goto DONE;
	}

	while (used > 0 && isspace((unsigned char)buf[used - 1]))
		buf[--used] = '\0';

	if (used == 0 || strpbrk(buf, " \t\r\n") != NULL) {
		ret = SET_ERRORX(MPORT_ERR_WARN, "No usable PKGVERSION in %s", portdir);
		goto DONE;
	}

	if ((*version = strdup(buf)) == NULL)
		ret = SET_ERROR(MPORT_ERR_FATAL, "Out of memory.");

DONE:
	if (fds[0] != -1)
		(void)close(fds[0]);
	if (fds[1] != -1)
		(void)close(fds[1]);
	free(portsdir_arg);
	free(flavor_arg);
	return ret;
}

/*
 * Return the version the ports tree at portsdir would build for an installed
 * package, using its recorded origin and flavor.  *version remains NULL when
 * the tree has no Makefile for that origin.
 */
MPORT_PUBLIC_API int
mport_ports_version_get(/*@notnull@*/ mportInstance *mport, /*@notnull@*/ const char *portsdir,
    /*@notnull@*/ mportPackageMeta *pack, /*@out@*/ /*@null@*/ char **version)
{
	struct stat st;
	char *portdir = NULL;
	char *makefile = NULL;
	const char *flavor = NULL;
	int ret;

	if (mport == NULL || portsdir == NULL || pack == NULL || version == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid ports version lookup arguments");

	*version = NULL;

	if (portsdir[0] == '\0')
		RETURN_ERROR(MPORT_ERR_FATAL, "Ports directory not defined");

	if (pack->origin == NULL || !ports_valid_origin(pack->origin))
		RETURN_ERRORX(MPORT_ERR_WARN, "Invalid origin for %s",
		    pack->name != NULL ? pack->name : "package");

	if (pack->flavor != NULL && pack->flavor[0] != '\0') {
		if (!ports_valid_flavor(pack->flavor))
			RETURN_ERRORX(MPORT_ERR_WARN, "Invalid flavor for %s",
			    pack->name != NULL ? pack->name : "package");
		flavor = pack->flavor;
	}

	if (asprintf(&portdir, "%s/%s", portsdir, pack->origin) == -1)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	if (asprintf(&makefile, "%s/Makefile", portdir) == -1) {
		free(portdir);
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
	}

	if (stat(makefile, &st) == -1 || !S_ISREG(st.st_mode)) {
		free(makefile);
		free(portdir);
		return MPORT_OK;
	}

	ret = ports_make_version(portsdir, portdir, flavor, version);

	free(makefile);
	free(portdir);
	return ret;
}
