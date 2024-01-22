/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2011, 2013, 2015, 2021 Lucas Holt
 * Copyright (c) 2007-2009 Chris Reinhardt
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <pwd.h>
#include <grp.h>
#include <sha256.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <unistd.h>
#include <ftw.h>
#include <spawn.h>
#include <poll.h>
#include <libgen.h>
#include <unistd.h>
#include "mport.h"
#include "mport_private.h"

static char *mport_get_osrelease_userland(void);
static char *mport_get_osrelease_kern(void);

/* these two aren't really utilities, but there's no better place to put them */
MPORT_PUBLIC_API mportCreateExtras *
mport_createextras_new(void)
{
	mportCreateExtras *extra;
	extra = (mportCreateExtras *)calloc(1, sizeof(mportCreateExtras));
	if (extra == NULL)
		return NULL;

	extra->pkg_filename[0] = '\0';
	extra->sourcedir[0] = '\0';
	extra->mtree = NULL;
	extra->pkginstall = NULL;
	extra->pkgdeinstall = NULL;
	extra->pkgmessage = NULL;
	extra->conflicts = NULL;
	extra->depends = NULL;

	return extra;
}

MPORT_PUBLIC_API void
mport_createextras_free(mportCreateExtras *extra)
{
	size_t i;

	if (extra == NULL)
		return;

	free(extra->mtree);
	extra->mtree = NULL;
	free(extra->pkginstall);
	extra->pkginstall = NULL;
	free(extra->pkgdeinstall);
	extra->pkgdeinstall = NULL;
	free(extra->pkgmessage);
	extra->pkgmessage = NULL;

	if (extra->conflicts_count > 0 && extra->conflicts != NULL) {
		for (i = 0; i < extra->conflicts_count; i++) {
			if (extra->conflicts[i] == NULL) {
				break;
			}
			free(extra->conflicts[i]);
			extra->conflicts[i] = NULL;
		}
		
		free(extra->conflicts);
		extra->conflicts = NULL;
	}

	if (extra->depends_count > 0 && extra->depends != NULL) {
		for (i = 0; i < extra->depends_count; i++) {
			if (extra->depends[i] == NULL) {
				break;
			}
			free(extra->depends[i]);
			extra->depends[i] = NULL;
		}

		free(extra->depends);
		extra->depends = NULL;
	}

	free(extra);
}

MPORT_PUBLIC_API int
mport_verify_hash(const char *filename, const char *hash)
{
	char *filehash;

	filehash = mport_hash_file(filename);
#ifdef DEBUG
	printf("gen: '%s'\nsql: '%s'\n", filehash, hash);
#endif
	if (strncmp(filehash, hash, 65) == 0) {
		free(filehash);
		return 1;
	}

	free(filehash);
	return 0;
}

bool
mport_starts_with(const char *pre, const char *str)
{
	return strncmp(pre, str, strlen(pre)) == 0;
}

/* mport_hash_file(const char * filename)
 *
 * Return a SHA256 hash of a file.  Must free result
 */
char *
mport_hash_file(const char *filename)
{
	return SHA256_File(filename, NULL);
}

uid_t
mport_get_uid(const char *username)
{
	struct passwd *pw;

	if (username == NULL || *username == '\0')
		return 0; /* root */

	pw = getpwnam(username);
	if (pw == NULL)
		return 0; /* if we can't figure it out be safe */

	return pw->pw_uid;
}

gid_t
mport_get_gid(const char *group)
{
	struct group *gr;

	if (group == NULL || *group == '\0')
		return 0; /* wheel */

	gr = getgrnam(group);
	if (gr == NULL)
		return 0; /* wheel, could not look up */

	return gr->gr_gid;
}

/* a wrapper around chdir, to work with our error system */
int
mport_chdir(mportInstance *mport, const char *dir)
{

	if (mport != NULL) {
		char *finaldir;

		asprintf(&finaldir, "%s%s", mport->root, dir);

		if (finaldir == NULL)
			RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't building root'ed dir");

		if (chdir(finaldir) != 0) {
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't chdir to %s: %s", finaldir, strerror(errno));
		}

		free(finaldir);
	} else {
		if (chdir(dir) != 0)
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't chdir to %s: %s", dir, strerror(errno));
	}

	return (MPORT_OK);
}

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

/* deletes the entire directory tree at filename.
 * think rm -r filename
 */
int
mport_rmtree(const char *filename)
{
	int ret = nftw(filename, unlink_cb, 64, FTW_DEPTH | FTW_MOUNT | FTW_PHYS | FTW_CHDIR);

	if (ret!= 0)
		RETURN_ERROR(MPORT_ERR_FATAL, "Error removing directory tree");
	return MPORT_OK;
}

/*
 * Copy file fromname to toname
 */
int
mport_copy_file(const char *fromName, const char *toName)
{
	char buf[BUFSIZ];
	size_t size;

	FILE *fsrc = fopen(fromName, "re");
	if (fsrc == NULL)
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open source file for copying %s: %s",
		    fromName, strerror(errno));

	FILE *fdest = fopen(toName, "we");
	if (fdest == NULL) {
		fclose(fsrc);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open destination file for copying %s: %s",
		    toName, strerror(errno));
	}

	while ((size = fread(buf, 1, BUFSIZ, fsrc)) > 0) {
		fwrite(buf, 1, size, fdest);
	}

	fclose(fsrc);
	fclose(fdest);

	return (MPORT_OK);
}

/*
 * create a directory with mode 755.  Do not fail if the
 * directory exists already.
 */
int
mport_mkdir(const char *dir)
{
	if (mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
		if (errno != EEXIST)
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't mkdir %s: %s", dir, strerror(errno));
	}

	return (MPORT_OK);
}

/*
 * mport_rmdir(dir, ignore_nonempty)
 *
 * delete the given directory.  If ignore_nonempty is non-zero, then
 * we return OK even if we couldn't delete the dir because it wasn't empty or
 * didn't exist.
 */
int
mport_rmdir(const char *dir, int ignore_nonempty)
{
	if (rmdir(dir) != 0) {
		if (ignore_nonempty && (errno == ENOTEMPTY || errno == ENOENT)) {
			return (MPORT_OK);
		} else {
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't rmdir %s: %s. With ZFS, this could indicate a snapshot is blocking removal.", dir, strerror(errno));
		}
	}

	return (MPORT_OK);
}

/**
 * @brief remove all flags from a directory rooted at root
 * 
 * @param root 
 * @param dir 
 * @return int 
 */
int mport_removeflags(const char *root, const char *dir) {
	struct stat st;
	int statresult;
	
	int rootfd = open(root, O_RDONLY | O_DIRECTORY);

	if (mport_starts_with(root, dir)) {
		statresult = lstat(dir, &st);
	} else {
		statresult = fstatat(rootfd, dir, &st, AT_SYMLINK_NOFOLLOW);
	} 

	if (statresult != -1) {
		if (st.st_flags & (UF_IMMUTABLE | UF_APPEND | UF_NOUNLINK | SF_IMMUTABLE | SF_APPEND | SF_NOUNLINK)) {
			/* Disable all flags*/
			chflagsat(rootfd, dir, 0, AT_SYMLINK_NOFOLLOW);
		}
	}

	close(rootfd);

	return (MPORT_OK);
}

/**
 * @brief register a new user shell 
 * 
 * @param shell_file path to shell file
 * @return int MPORT_OK on success, MPORT_ERR_FATAL on failure
 */
int
mport_shell_register(const char *shell_file)
{
	if (shell_file == NULL || shell_file[0] != '/')
		RETURN_ERROR(MPORT_ERR_FATAL, "Shell to register is invalid.");

	FILE *fp;
	fp = fopen("/etc/shells", "a");
	if (fp == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Unable to open shell file.");

	fprintf(fp, "%s\n", shell_file);
	fclose(fp);

	return (MPORT_OK);
}

int
mport_shell_unregister(const char *shell_file)
{
	if (shell_file == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Shell to unregister is invalid.");

	return mport_xsystem(NULL,
	    "grep -v %s /etc/shells > /etc/shells.bak && mv /etc/shells.bak /etc/shells",
	    shell_file);
}

/**
 * @brief Remove a character from a string
 * 
 * @param str 
 * @param ch 
 * @return char* (must be freed)
 */
char * 
mport_str_remove(const char *str, const char ch)
{
	size_t i;
	size_t x;
	size_t len;
	char *output;
	
	if (str == NULL)
		return NULL;
	
	len = strlen(str);
	
	output = calloc(len + 1, sizeof(char));
	
	for (i = 0, x = 0; i <= len; i++) {
		if (str[i] != ch) {
			output[x] = str[i];
			x++;
		}
	}
	output[len] = '\0';
	
	return (output);
} 

/*
 * Quick test to see if a file exists.
 */
MPORT_PUBLIC_API int
mport_file_exists(const char *file)
{
	struct stat st;

	return (lstat(file, &st) == 0);
}

char *
mport_directory(const char *path)
{

	if (path[0] == '/') {
		// 'path' is a full path, so we can extract the directory directly
		char *dir = strdup(path);
		char *lastSlash = strrchr(dir, '/');
		if (lastSlash != NULL) {
			*lastSlash = '\0'; // Null-terminate at the last slash to get the directory
			return dir;
		} else {
			free(dir);
		}
	} else {
		// 'path' is just a filename, so get the current working directory
		char currentDir[PATH_MAX];
		if (getcwd(currentDir, sizeof(currentDir)) != NULL) {
			// Construct the full path by appending the filename
			strcat(currentDir, "/");
			strcat(currentDir, path);
			return strdup(currentDir);
		}
	}

	return NULL;
}

/* mport_xsystem(mportInstance *mport, char *fmt, ...)
 *
 * Our own version on system that takes a format string and a list
 * of values.  The fmt works exactly like the stdio output formats.
 *
 * If mport is non-NULL and has a root set, your command will run
 * chroot'ed into mport->root.
 */
int
mport_xsystem(mportInstance *mport, const char *fmt, ...)
{
	va_list args;
	char *cmnd;
	int ret;

	va_start(args, fmt);

	if (vasprintf(&cmnd, fmt, args) == -1) {
		/* XXX How will the caller know this is no mem, and not a failed exec? */
		va_end(args);
		if (cmnd != NULL)
			free(cmnd);
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate xsystem cmnd string.");
	}
	va_end(args);

	if (mport != NULL && *(mport->root) != '\0') {
		char *chroot_cmd;
		if (asprintf(&chroot_cmd, "%s %s %s", MPORT_CHROOT_BIN, mport->root, cmnd) == -1)
			RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't allocate xsystem chroot string.");

		free(cmnd);
		cmnd = chroot_cmd;
	}

	ret = system(cmnd);
	free(cmnd);

	/* system(3) ignores SIGINT and SIGQUIT */
	if (WIFSIGNALED(ret) && (WTERMSIG(ret) == SIGINT || WTERMSIG(ret) == SIGQUIT)) {
		RETURN_ERROR(MPORT_ERR_FATAL, "SIGINT or SIGQUIT while running command.");
	}

	if (ret == 127) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't execute sh(1)");
	}

	if (WIFEXITED(ret))
		return (MPORT_OK);

	RETURN_ERROR(MPORT_ERR_FATAL, "Error executing command");
}

/*
 * mport_parselist(input, string_array_pointer)
 *
 * hacks input into sub strings by whitespace.  Allocates a chunk or memory
 * for an array of those strings, and sets the pointer you pass to reference
 * that memory
 *
 * char input[] = "foo bar baz"
 * char **list;
 * size_t list_size;
 *
 * mport_parselist(input, &list, &list_size);
 * list = {"foo", "bar", "baz"};
 */
void
mport_parselist(char *opt, char ***list, size_t *list_size)
{
	char *input;
	char *field;

	if (opt == NULL || list_size == NULL || list == NULL)
		return;

	if ((input = strdup(opt)) == NULL) {
		*list = NULL;
		return;
	}

	/* first we need to get the length of the dependency list */
	for (*list_size = 0; (field = strsep(&opt, " \t\n")) != NULL;) {
		if (field != NULL && *field != '\0')
			(*list_size)++;
	}

	if (*list_size == 0) {
		*list = NULL;
		free(input);
		return;
	}

	if ((*list = (char **)calloc((*list_size + 1), sizeof(char *))) == NULL) {
		free(input);
		return;
	}

	/* dereference once so we don't lose our minds. */
	char **vec = *list;

	size_t loc = 0;
	while ((field = strsep(&input, " \t\n")) != NULL) {
		if (loc == *list_size)
			break;

		if (field == NULL || *field == '\0')
			continue;

		*vec = strdup(field);
		loc++;
		vec++;
	}

	*vec = NULL;
	free(input);
}

/*
 * mport_run_asset_exec(fmt, cwd, last_file)
 *
 * handles an @exec or an @unexec directive in a plist.  This function
 * does the substitions and then runs the command.  last_file is
 * absolute path.
 *
 * Substitutions:
 * %F	The last filename extracted (last_file argument)
 * %D	The current working directory (cwd)
 * %B	Return the directory part ("dirname") of %D/%F
 * %f	Return the filename part of ("basename") %D/%F
 */
int
mport_run_asset_exec(mportInstance *mport, const char *fmt, const char *cwd, const char *last_file)
{
	size_t l;
	char *cmnd;
	char *pos;
	char *name;
	char *lfcpy;
	int ret;
	static int max = 0;
	size_t maxlen = sizeof(max);

	if (max == 0) {
		if (sysctlbyname("kern.argmax", &max, &maxlen, NULL, 0) < 0)
			RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't determine maximum argument length");
	}

	if ((cmnd = malloc(max * sizeof(char))) == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
	pos = cmnd;

	while (*fmt && max > 0) {
		if (*fmt == '%') {
			fmt++;
			switch (*fmt) {
			case 'F':
				/* last_file is absolute, so we skip the cwd at the begining */
				(void)strlcpy(pos, last_file + strlen(cwd) + 1, max);
				l = strlen(last_file + strlen(cwd) + 1);
				pos += l;
				max -= l;
				break;
			case 'D':
				(void)strlcpy(pos, cwd, max);
				l = strlen(cwd);
				pos += l;
				max -= l;
				break;
			case 'B':
				lfcpy = malloc(strlen(last_file) * sizeof(char));
				name = dirname(lfcpy); /* dirname(3) in MidnightBSD 3.0 and higher
							  modifies the source. */
				(void)strlcpy(pos, name, max);
				l = strlen(name);
				pos += l;
				max -= l;
				free(lfcpy);
				break;
			case 'f':
				name = basename((char *)last_file);
				(void)strlcpy(pos, name, max);
				l = strlen(name);
				pos += l;
				max -= l;
				break;
			default:
				*pos = *fmt;
				max--;
				pos++;
			}
			fmt++;
		} else {
			*pos = *fmt;
			pos++;
			fmt++;
			max--;
		}
	}

	*pos = '\0';

	/* cmnd now hold the expanded command, now execute it*/
	ret = mport_xsystem(mport, cmnd);
	free(cmnd);
	return ret;
}

/* mport_free_vec(void **)
 *
 * free a null padded list of pointers, freeing each pointer as well.
 */
void
mport_free_vec(void *vec)
{
	char *p;

	if (vec == NULL)
		return;

	p = (char *)*(char **)vec;

	while (p != NULL) {
		free(p);
		p = NULL;
		p++;
	}

	free(vec);
	vec = NULL;
}

/* mport_decompress_bzip2(char * input, char * output)
 *
 * Extract a bzip2 file such as an index
 */
int
mport_decompress_bzip2(const char *input, const char *output)
{
	FILE *f;
	FILE *fout;
	BZFILE *b;
	int nBuf;
	char buf[4096];
	int bzerror;

	f = fopen(input, "r");
	if (!f) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't open bzip2 file for reading");
	}

	fout = fopen(output, "w");
	if (!fout) {
		fclose(f);
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't open file for writing");
	}

	b = BZ2_bzReadOpen(&bzerror, f, 0, 0, NULL, 0);
	if (bzerror != BZ_OK) {
		BZ2_bzReadClose(&bzerror, b);
		RETURN_ERROR(MPORT_ERR_FATAL, "Input error reading bzip2 file");
	}

	bzerror = BZ_OK;
	while (bzerror == BZ_OK) {
		nBuf = BZ2_bzRead(&bzerror, b, buf, 4096);
		if (bzerror == BZ_OK || bzerror == BZ_STREAM_END) {
			if (fwrite(buf, nBuf, 1, fout) < 1) {
				fclose(fout);
				RETURN_ERROR(MPORT_ERR_FATAL, "Error writing decompressed file");
			}
		}
	}

	if (bzerror != BZ_STREAM_END) {
		BZ2_bzReadClose(&bzerror, b);
		RETURN_ERROR(MPORT_ERR_FATAL, "Unknown error decompressing bzip2 file");
	} else {
		BZ2_bzReadClose(&bzerror, b);
	}

	fclose(f);
	fclose(fout);

	return (MPORT_OK);
}

MPORT_PUBLIC_API char *
mport_get_osrelease(mportInstance *mport)
{
	char *version = NULL;

	// honor settings first
	if (mport != NULL) {
		version = mport_setting_get(mport, MPORT_SETTING_TARGET_OS);
	}

	// try midnightbsd-version
	if (version == NULL) {
		version = mport_get_osrelease_userland();
	}

	// fall back to kernel version
	if (version == NULL) {
		version = mport_get_osrelease_kern();
	}

	return version;
}

static char *
mport_get_osrelease_kern(void)
{
	char osrelease[128];
	size_t len;
	char *version;

	len = sizeof(osrelease);
	if (sysctlbyname("kern.osrelease", &osrelease, &len, NULL, 0) < 0)
		return NULL;

	version = calloc(10, sizeof(char));
	if (version == NULL)
		return NULL;

	for (int i = 0; i < 10; i++) {
		// old versions contained an - in the name  e.g. 0.4-RELEASE
		if (osrelease[i] == '\0' || osrelease[i] == '-')
			break;

		version[i] = osrelease[i];
	}

	version[3] = '\0'; /* force major version only for now */

	return version;
}

static char *
mport_get_osrelease_userland(void)
{
	int exit_code;
	int cout_pipe[2];
	int cerr_pipe[2];
	int bytes_read;
	char *version = NULL;
	posix_spawn_file_actions_t action;

	if (pipe(cout_pipe) || pipe(cerr_pipe))
		return NULL;

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
	posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
	posix_spawn_file_actions_adddup2(&action, cerr_pipe[1], 2);

	posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

	char *command[] = { "/bin/midnightbsd-version", "-u" };
	char *argsmem[] = { "/bin/sh", "-c" };
	char *args[] = { &argsmem[0][0], &argsmem[1][0], &command[0][0], &command[0][1], NULL };

	pid_t pid;
	if (posix_spawnp(&pid, args[0], &action, NULL, &args[0], NULL) != 0) {
#ifdef DEBUG
		fprintf(stderr, "posix_spawnp failed with error: %d\n", errno);
#endif
		return NULL;
	}

	close(cout_pipe[1]), close(cerr_pipe[1]); // close child-side of pipes

	char buffer[1024];
	struct pollfd plist[2];
	plist[0] = (struct pollfd) { cout_pipe[0], POLLIN, 0 };
	plist[1] = (struct pollfd) { cerr_pipe[0], POLLIN, 0 };

	version = calloc(10, sizeof(char));
	if (version == NULL) {
		return NULL;
	}

	for (int rval; (rval = poll(&plist[0], 2, -1)) > 0;) {
		if (plist[0].revents & POLLIN) {
			bytes_read = read(cout_pipe[0], &buffer[0], 1024);
			snprintf(version, 10, "%.*s\n", bytes_read, (char *)&buffer);
			break;
		} else if (plist[1].revents & POLLIN) {
			bytes_read = read(cerr_pipe[0], &buffer[0], 1024);
			snprintf(version, 10, "%.*s\n", bytes_read, (char *)&buffer);
			break;
		} else
			break; // nothing left to read
	}

	waitpid(pid, &exit_code, 0);
#ifdef DEBUG
	fprintf(stderr, "exit code: %d\n", exit_code);
#endif

	posix_spawn_file_actions_destroy(&action);
	close(cout_pipe[0]), close(cerr_pipe[0]);

	version[3] = '\0'; /* force major version only for now */

	return version;
}

MPORT_PUBLIC_API char *
mport_version(mportInstance *mport)
{
	char *version;
	char *osrel = mport_get_osrelease(mport);
	asprintf(&version, "mport %s for MidnightBSD %s, Bundle Version %s\n", MPORT_VERSION, osrel,
	    MPORT_BUNDLE_VERSION_STR);
	free(osrel);

	return version;
}

MPORT_PUBLIC_API char *
mport_version_short(mportInstance *mport)
{
	char *version;
	char *osrel = mport_get_osrelease(mport);
	asprintf(&version, "%s\n", MPORT_VERSION);
	free(osrel);

	return version;
}

time_t
mport_get_time(void)
{
	struct timespec now;

	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		RETURN_ERROR(MPORT_ERR_FATAL, strerror(errno));
	}

	return (now.tv_sec);
}

MPORT_PUBLIC_API int
mport_drop_privileges(void)
{
	struct passwd *nobody;

	if (geteuid() == 0) {
		nobody = getpwnam("nobody");
		if (nobody == NULL) {
			RETURN_ERROR(MPORT_ERR_WARN, "nobody missing");
		}
		setgroups(1, &nobody->pw_gid);

		if (setgid(nobody->pw_gid) == -1) {
			RETURN_ERROR(MPORT_ERR_WARN, "setgid failed");
		}
		if (setuid(nobody->pw_uid) == -1) {
			RETURN_ERROR(MPORT_ERR_WARN, "setuid failed");
		}
	}

	return (MPORT_OK);
}

bool
mport_check_answer_bool(char *ans) {
	if (ans == NULL)
	    return (false);

    if (*ans == 'Y' || *ans == 'y' || *ans == 't' || *ans == 'T' || *ans == '1') 
      return (true);
    if (*ans == 'N' || *ans == 'n' || *ans == 'f' || *ans == 'F' || *ans == '0') 
      return (false);

	return (false);
}