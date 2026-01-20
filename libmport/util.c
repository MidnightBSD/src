/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <sha256.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
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
#include <libelf.h>
#include <gelf.h>

#include "mport.h"
#include "mport_private.h"

static char *mport_get_osrelease_userland(void);
static char *mport_get_osrelease_kern(void);

/* these two aren't really utilities, but there's no better place to put them */
MPORT_PUBLIC_API mportCreateExtras *
mport_createextras_new(void)
{
	mportCreateExtras *extra = NULL;
	extra = (mportCreateExtras *)calloc(1, sizeof(mportCreateExtras));
	if (extra == NULL)
		return NULL;

	extra->pkg_filename[0] = '\0';
	extra->sourcedir[0] = '\0';
	extra->mtree = NULL;
	
	extra->pkginstall = NULL;
	extra->pkgdeinstall = NULL;
	extra->luapkgpostdeinstall = NULL;
	extra->luapkgpostinstall = NULL;
	extra->luapkgpreinstall = NULL;
	extra->luapkgpredeinstall = NULL;

	extra->pkgmessage = NULL;
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

	free(extra->luapkgpostdeinstall);
	extra->luapkgpostdeinstall = NULL;
	free(extra->luapkgpostinstall);
	extra->luapkgpostinstall = NULL;
	free(extra->luapkgpreinstall);
	extra->luapkgpreinstall = NULL;
	free(extra->luapkgpredeinstall);
	extra->luapkgpredeinstall = NULL;

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

	tll_free_and_free(extra->conflicts, free);

	free(extra);
	extra = NULL;
}

MPORT_PUBLIC_API int
mport_verify_hash(const char *filename, const char *hash)
{
	char *filehash;

	if (filename == NULL || hash == NULL)
		return 0;

	filehash = mport_hash_file(filename);
	if (filehash == NULL)
		return 0;

#ifdef DEBUG
	printf("gen: '%s'\nsql: '%s'\n", filehash, hash);
#endif
	if (strncmp(filehash, hash, 64) == 0) {
		free(filehash);
		return 1;
	}

	free(filehash);
	return 0;
}

char* 
mport_extract_hash_from_file(const char *filename)
{

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    char *hash = calloc(65, sizeof(char)); // SHA256 hash is 64 characters + null terminator
    if (!hash) {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    char buffer[256];
    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        perror("Failed to read hash from file");
        free(hash);
        fclose(file);
        return NULL;
    }

    fclose(file);

    // Extract the hash from the format "SHA256 (index.db.zst) = <hash>"
    char *hash_start = strchr(buffer, '=');
    if (hash_start == NULL) {
        perror("Invalid hash format");
        free(hash);
        return NULL;
    }

    hash_start += 2; // Skip the "= " part
    strlcpy(hash, hash_start, 65);

    return hash;
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
		char *finaldir = NULL;

		if (asprintf(&finaldir, "%s%s", mport->root, dir) == -1)
			RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't building root'ed dir");

		if (chdir(finaldir) != 0) {
			RETURN_ERRORX(
			    MPORT_ERR_FATAL, "Couldn't chdir to %s: %s", finaldir, strerror(errno));
		}

		free(finaldir);
		finaldir = NULL;
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


int
mport_copy_fd(int from_fd, int to_fd)
{
    char buf[BUFSIZ];
    ssize_t size;

    while ((size = read(from_fd, buf, BUFSIZ)) > 0) {
        if (write(to_fd, buf, size) != size) {
            RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't write to destination file descriptor: %s", strerror(errno));
        }
    }

    if (size < 0) {
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't read from source file descriptor: %s", strerror(errno));
    }

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
	fp = fopen(_PATH_SHELLS, "a");
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
	    "/usr/bin/grep -v %s %s > %s.bak && mv %s.bak %s",
	    shell_file, _PATH_SHELLS, _PATH_SHELLS, _PATH_SHELLS, _PATH_SHELLS);
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
	char *output = NULL;
	
	if (str == NULL)
		return NULL;
	
	len = strlen(str);
	
	output = calloc(len + 1, sizeof(char));
	if (output == NULL)
		return NULL;
	
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
		if (dir == NULL)
			return NULL;
		char *lastSlash = strrchr(dir, '/');
		if (lastSlash != NULL) {
			*lastSlash = '\0'; // Null-terminate at the last slash to get the directory
			return dir;
		} else {
			free(dir);
			dir = NULL;
		}
	} else {
		// 'path' is just a filename, so get the current working directory
		char currentDir[PATH_MAX];
		if (getcwd(currentDir, sizeof(currentDir)) != NULL) {
			// Construct the full path by appending the filename
			strcat(currentDir, "/");
			strcat(currentDir, path);

			char *lastSlash = strrchr(currentDir, '/');
			if (lastSlash != NULL) {
				*lastSlash = '\0'; // Null-terminate at the last slash to get the directory
			}

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
		if (cmnd != NULL) {
			free(cmnd);
			cmnd = NULL;
		}
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
	cmnd = NULL;

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
		input = NULL;
		return;
	}

	if ((*list = (char **)calloc((*list_size + 1), sizeof(char *))) == NULL) {
		free(input);
		input = NULL;
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
	input = NULL;
}

/*
 * mport_parselist_tll(input, string_array_pointer)
 *
 * hacks input into sub strings by whitespace.  Allocates a chunk or memory
 * for an array of those strings, and sets the pointer you pass to reference
 * that memory
 *
 * char input[] = "foo bar baz"
 * stringlist_t list;
 * size_t list_size;
 *
 * mport_parselist(input, &list);
 * list = {"foo", "bar", "baz"};
 */
void
mport_parselist_tll(char *opt, stringlist_t *list)
{
	char *input;
	char *field;

	if (opt == NULL || list == NULL)
		return;

	if ((input = strdup(opt)) == NULL) {
		return;
	}

	while ((field = strsep(&input, " \t\n")) != NULL) {
		if (field != NULL && *field != '\0')
			tll_push_back(*list, strdup(field));
	}

	free(input);
	input = NULL;
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
	char *cmnd = NULL;
	char *pos = NULL;
	char *name = NULL;
	char *lfcpy = NULL;
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
				lfcpy = strdup(last_file);
				if (lfcpy == NULL)
					RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
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
	cmnd = NULL;

	return ret;
}

/* mport_free_vec(void **)
 *
 * free a null padded list of pointers, freeing each pointer as well.
 */
void
mport_free_vec(void *vec)
{
	char *p = NULL;

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

int 
mport_decompress_zstd(const char *input, const char *output)
{
    FILE *f;
    FILE *fout;
    size_t const buffInSize = ZSTD_DStreamInSize();
    size_t const buffOutSize = ZSTD_DStreamOutSize();
    void* const buffIn = malloc(buffInSize);
    void* const buffOut = malloc(buffOutSize);
    ZSTD_DStream* const dstream = ZSTD_createDStream();
    size_t const initResult = ZSTD_initDStream(dstream);

    if (ZSTD_isError(initResult)) {
        free(buffIn);
        free(buffOut);
        ZSTD_freeDStream(dstream);
        RETURN_ERROR(MPORT_ERR_FATAL, "Failed to initialize ZSTD decompression stream");
    }

    f = fopen(input, "rb");
    if (!f) {
        free(buffIn);
        free(buffOut);
        ZSTD_freeDStream(dstream);
        RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't open zstd file for reading");
    }

    fout = fopen(output, "wb");
    if (!fout) {
        fclose(f);
        free(buffIn);
        free(buffOut);
        ZSTD_freeDStream(dstream);
        RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't open file for writing");
    }

    size_t toRead = buffInSize;
    while (1) {
        size_t const read = fread(buffIn, 1, toRead, f);
        if (read == 0) break;

        ZSTD_inBuffer input = { buffIn, read, 0 };
        while (input.pos < input.size) {
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            size_t const result = ZSTD_decompressStream(dstream, &output, &input);
            if (ZSTD_isError(result)) {
                fclose(f);
                fclose(fout);
                free(buffIn);
                free(buffOut);
                ZSTD_freeDStream(dstream);
                RETURN_ERROR(MPORT_ERR_FATAL, "Error decompressing zstd file");
            }
            fwrite(buffOut, 1, output.pos, fout);
        }
    }

    fclose(f);
    fclose(fout);
    free(buffIn);
    free(buffOut);
    ZSTD_freeDStream(dstream);

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

char *
mport_get_osreleasedate(void)
{
	int osreleasedate;
	size_t len = sizeof(osreleasedate);
	char *date = NULL;

	if (sysctlbyname("kern.osreldate", &osreleasedate, &len, NULL, 0) < 0)
		return NULL;

	if (asprintf(&date, "%d", osreleasedate) == -1)
		return NULL;

	return date;
}

static char *
mport_get_osrelease_kern(void)
{
	char osrelease[128];
	size_t len;
	char *version = NULL;

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
	char *version = NULL;
	char *osrel = mport_get_osrelease(mport);
	if (asprintf(&version, "mport %s for MidnightBSD %s, Bundle Version %s\n", MPORT_VERSION, osrel,
	    MPORT_BUNDLE_VERSION_STR) == -1) {
		free(osrel);
		return NULL;
	}
	free(osrel);
	osrel = NULL;

	return version;
}

MPORT_PUBLIC_API char *
mport_version_short(mportInstance *mport)
{
	char *version = NULL;
	char *osrel = mport_get_osrelease(mport);
	if (asprintf(&version, "%s\n", MPORT_VERSION) == -1) {
		free(osrel);
		return NULL;
	}
	free(osrel);
	osrel = NULL;

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
	struct passwd *nobody = NULL;

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

/**
 * @brief generate PURL URI for a package
 * Caller must free the returned string.
 * @param packs mport package metadata
 * @return PURL URI or NULL
 */
MPORT_PUBLIC_API char * 
mport_purl_uri(mportPackageMeta *packs) 
{
	char *purl = NULL;

	// https://github.com/package-url/purl-spec/blob/main/PURL-TYPES.rst
	//asprintf(&purl, "pkg:mport/midnightbsd/%s@%s?arch=%s&osrel=%s", (*indexEntry)->pkgname, (*packs)->version, MPORT_ARCH, os_release);
	// the purl format requires registration.  i'm switching to generic and requested above from them.
		
	int ret = asprintf(&purl, "pkg:generic/%s@%s?arch=%s&distro=midnightbsd-%s", packs->name, packs->version, MPORT_ARCH, packs->os_release);
	if (ret == -1) {
		return NULL;
	}

	return (purl);
}

bool
mport_check_answer_bool(char *ans) 
{
	if (ans == NULL)
	    return (false);

	if (*ans == 'Y' || *ans == 'y' || *ans == 't' || *ans == 'T' || *ans == '1') 
		return (true);
	if (*ans == 'N' || *ans == 'n' || *ans == 'f' || *ans == 'F' || *ans == '0') 
		return (false);

	return (false);
}

MPORT_PUBLIC_API mportVerbosity 
mport_verbosity(bool quiet, bool verbose, bool brief) 
{

	/* if both are specified, we need quiet for backward compatibility */

	if (quiet)
		return (MPORT_VQUIET);

	if (brief)
		return (MPORT_VBRIEF);

	if (verbose)
		return (MPORT_VVERBOSE);
		
	return (MPORT_VNORMAL);
}

MPORT_PUBLIC_API char *
mport_string_replace(const char *str, const char *old, const char *new)
{
	char *ret = NULL;
	char *r = NULL;
	const char *p, *q;
	size_t oldlen = strlen(old);
	size_t count, retlen, newlen = strlen(new);

	if (oldlen != newlen) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	} else {
		retlen = strlen(str);
	}

	ret = malloc(retlen + 1);
	if (ret == NULL)
	    return NULL;

	for (r = ret, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen) {
		ptrdiff_t l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new, newlen);
		r += newlen;
	}
	strcpy(r, p);

	return (ret);
}

int
mport_count_spaces(const char *str)
{
	int spaces;
	const char *p;

	for (spaces = 0, p = str; *p != '\0'; p++) {
		if (isspace(*p)) {
			spaces++;
		}
	}

	return (spaces);
}

/* 
 * A bit like strsep(), except it accounts for "double" and 'single' quotes.
 * Unlike strsep(), this function returns the next argument string, trimmed of 
 * whitespace or enclosing quotes, and updates **args to point at the character 
 * after that. 
 * 
 * Sets *args to NULL when it has been processed.
 * 
 * Quoted strings run from the first quote mark to the next one of 
 * the same type or the terminating NULL. Quoted strings can contain the other 
 * type of quote mark, which loses any special meaning. 
 * 
 * There is no escape character.
 */
char *
mport_tokenize(char **args)
{
	char *p, *p_start;
	enum parse_states parse_state = START;

	if (args == NULL || *args == NULL)
		return NULL;

	for (p = p_start = *args; *p != '\0'; p++) {
		switch (parse_state) {
		case START:
			if (!isspace(*p)) {
				if (*p == '"')
					parse_state = OPEN_DOUBLE_QUOTES;
				else if (*p == '\'')
					parse_state = OPEN_SINGLE_QUOTES;
				else {
					parse_state = ORDINARY_TEXT;
					p_start = p;
				}
			} else
				p_start = p;
			break;
		case ORDINARY_TEXT:
			if (isspace(*p))
				goto finish;
			break;
		case OPEN_SINGLE_QUOTES:
			p_start = p;
			if (*p == '\'')
				goto finish;

			parse_state = IN_SINGLE_QUOTES;
			break;
		case IN_SINGLE_QUOTES:
			if (*p == '\'')
				goto finish;
			break;
		case OPEN_DOUBLE_QUOTES:
			p_start = p;
			if (*p == '"')
				goto finish;
			parse_state = IN_DOUBLE_QUOTES;
			break;
		case IN_DOUBLE_QUOTES:
			if (*p == '"')
				goto finish;
			break;
		}
	}

finish:
	if (*p == '\0')
		*args = NULL;
	else {
		*p = '\0';
		p++;
		if (*p == '\0' || parse_state == START)
			*args = NULL;
		else
			*args = p;
	}

	if (parse_state == START)
		return NULL;

	return (p_start);
}


#define ELF_MAGIC "\x7F""ELF"
#define ELF_MAGIC_SIZE 4

MPORT_PUBLIC_API bool 
mport_is_elf_file(const char *file) 
{
    FILE *f = fopen(file, "rb");
    if (f == NULL) {
        perror("fopen");
        return false;
    }

    char magic[ELF_MAGIC_SIZE];
    if (fread(magic, 1, ELF_MAGIC_SIZE, f) != ELF_MAGIC_SIZE) {
        fclose(f);
        return false;
    }

    fclose(f);

    // Compare the magic number
    return (memcmp(magic, ELF_MAGIC, ELF_MAGIC_SIZE) == 0);
}

MPORT_PUBLIC_API bool 
mport_is_statically_linked(const char *file) {
    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        return false;
    }

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return false;
    }

    Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
    if (elf == NULL) {
        fprintf(stderr, "elf_begin failed: %s\n", elf_errmsg(-1));
        close(fd);
        return false;
    }

    // Check if the file is an ELF file
    if (elf_kind(elf) != ELF_K_ELF) {
        fprintf(stderr, "%s is not an ELF file\n", file);
        elf_end(elf);
        close(fd);
        return false;
    }

    // Iterate through the sections to find the .dynamic section
    size_t shstrndx;
    if (elf_getshdrstrndx(elf, &shstrndx) != 0) {
        fprintf(stderr, "elf_getshdrstrndx failed: %s\n", elf_errmsg(-1));
        elf_end(elf);
        close(fd);
        return false;
    }

    Elf_Scn *scn = NULL;
    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        GElf_Shdr shdr;
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            fprintf(stderr, "gelf_getshdr failed: %s\n", elf_errmsg(-1));
            elf_end(elf);
            close(fd);
            return false;
        }

        const char *section_name = elf_strptr(elf, shstrndx, shdr.sh_name);
        if (section_name != NULL && strcmp(section_name, ".dynamic") == 0) {
            // Found the .dynamic section, meaning the file is dynamically linked
            elf_end(elf);
            close(fd);
            return false;
        }
    }

    // If no .dynamic section is found, the file is statically linked
    elf_end(elf);
    close(fd);
    return true;
}