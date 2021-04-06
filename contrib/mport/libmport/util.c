/*-
 * Copyright (c) 2011, 2013, 2015 Lucas Holt
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
#include <libgen.h>
#include "mport.h"
#include "mport_private.h"


/* these two aren't really utilities, but there's no better place to put them */
MPORT_PUBLIC_API mportCreateExtras*
mport_createextras_new(void)
{
    return (mportCreateExtras *) calloc(1, sizeof(mportCreateExtras));
}

MPORT_PUBLIC_API void
mport_createextras_free(mportCreateExtras *extra)
{
    int i;

    free(extra->pkg_filename);
    extra->pkg_filename = NULL;
    free(extra->sourcedir);
    extra->sourcedir = NULL;
    free(extra->mtree);
    extra->mtree = NULL;
    free(extra->pkginstall);
    extra->pkginstall = NULL;
    free(extra->pkgdeinstall);
    extra->pkgdeinstall = NULL;
    free(extra->pkgmessage);
    extra->pkgmessage = NULL;

    i = 0;
    if (extra->conflicts != NULL) {
        while (extra->conflicts[i] != NULL) {
            free(extra->conflicts[i]);
            extra->conflicts[i] = NULL;
            i++;
        }
    }

    free(extra->conflicts);
    extra->conflicts = NULL;

    i = 0;
    if (extra->depends != NULL) {
        while (extra->depends[i] != NULL) {
            free(extra->depends[i]);
            extra->depends[i] = NULL;
            i++;
        }
    }

    free(extra->depends);
    extra->depends = NULL;

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
  if (strncmp(filehash, hash, 65) == 0)
  {
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
        return 0; /* if we cant figure it out be safe */

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
int mport_chdir(mportInstance *mport, const char *dir)
{

    if (mport != NULL) {
        char *finaldir;

        asprintf(&finaldir, "%s%s", mport->root, dir);

        if (finaldir == NULL)
            RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't building root'ed dir");

        if (chdir(finaldir) != 0) {
            RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't chdir to %s: %s", finaldir, strerror(errno));
        }

        free(finaldir);
    } else {
        if (chdir(dir) != 0)
            RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't chdir to %s: %s", dir, strerror(errno));
    }

    return MPORT_OK;
}    


/* deletes the entire directory tree at filename.
 * think rm -r filename
 */
int
mport_rmtree(const char *filename) 
{
    return mport_xsystem(NULL, "/bin/rm -r %s", filename);
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
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open source file for copying %s: %s", fromName, strerror(errno));

    FILE *fdest = fopen(toName, "we");
    if (fdest == NULL)
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open destination file for copying %s: %s", toName, strerror(errno));

    while ((size = fread(buf, 1, BUFSIZ, fsrc)) > 0) {
        fwrite(buf, 1, size, fdest);
    }

    fclose(fsrc);
    fclose(fdest);

    return MPORT_OK;
}



/* 
 * create a directory with mode 755.  Do not fail if the
 * directory exists already.
 */
int mport_mkdir(const char *dir) 
{
    if (mkdir(dir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
        if (errno != EEXIST)
            RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't mkdir %s: %s", dir, strerror(errno));
    }

    return MPORT_OK;
}


/*
 * mport_rmdir(dir, ignore_nonempty)
 *
 * delete the given directory.  If ignore_nonempty is non-zero, then
 * we return OK even if we couldn't delete the dir because it wasn't empty or
 * didn't exist.
 */
int mport_rmdir(const char *dir, int ignore_nonempty)
{
    if (rmdir(dir) != 0) {
        if (ignore_nonempty && (errno == ENOTEMPTY || errno == ENOENT)) {
            return MPORT_OK;
        } else {
            RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't rmdir %s: %s", dir, strerror(errno));
        }
    }

    return MPORT_OK;
}


int
mport_shell_register(const char *shell_file)
{
	if (shell_file == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Shell to register is invalid.");

	return mport_xsystem(NULL, "echo %s >> /etc/shells", shell_file);	
}


int
mport_shell_unregister(const char *shell_file)
{
    if (shell_file == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Shell to unregister is invalid.");

    return mport_xsystem(NULL, "grep -v %s /etc/shells > /etc/shells.bak && mv /etc/shells.bak /etc/shells",
                         shell_file);
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
        return MPORT_OK;

    RETURN_ERROR(MPORT_ERR_FATAL, "Error executing command");
}
  



/*
 * mport_parselist(input, string_array_pointer)
 *
 * hacks input into sub strings by whitespace.  Allocates a chunk or memory
 * for a array of those strings, and sets the pointer you pass to reference
 * that memory
 *
 * char input[] = "foo bar baz"
 * char **list;
 * 
 * mport_parselist(input, &list);
 * list = {"foo", "bar", "baz"};
 */
void
mport_parselist(char *opt, char ***list) 
{
    size_t len;
    char *input;
    char *field;

    /* intentionally not freed in here */
    if ((input = strdup(opt)) == NULL) {
        *list = NULL;
        return;
    }

    /* first we need to get the length of the depends list */
    for (len = 0; (field = strsep(&opt, " \t\n")) != NULL;) {
        if (*field != '\0')
            len++;
    }

    if ((*list = (char **) calloc((len + 1), sizeof(char *))) == NULL) {
        return;
    }

    if (len == 0) {
        **list = NULL;
        return;
    }

    /* dereference once so we don't loose our minds. */
    char **vec = *list;

    while ((field = strsep(&input, " \t\n")) != NULL) {
        if (*field == '\0')
            continue;

        *vec = field;
        vec++;
    }

    *vec = NULL;
}

/*
 * mport_run_asset_exec(fmt, cwd, last_file)
 * 
 * handles a @exec or a @unexec directive in a plist.  This function
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
                    (void) strlcpy(pos, last_file + strlen(cwd) + 1, max);
                    l = strlen(last_file + strlen(cwd) + 1);
                    pos += l;
                    max -= l;
                    break;
                case 'D':
                    (void) strlcpy(pos, cwd, max);
                    l = strlen(cwd);
                    pos += l;
                    max -= l;
                    break;
                case 'B':
                    name = dirname(last_file);
                    (void) strlcpy(pos, name, max);
                    l = strlen(name);
                    pos += l;
                    max -= l;
                    break;
                case 'f':
                    name = basename(last_file);
                    (void) strlcpy(pos, name, max);
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

    p = (char *) *(char **) vec;

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

    return MPORT_OK;
}

MPORT_PUBLIC_API char *
mport_get_osrelease(void)
{
    char osrelease[128];
    size_t len;
    char *version;

    len = sizeof(osrelease);
    if (sysctlbyname("kern.osrelease", &osrelease, &len, NULL, 0) < 0)
        return NULL;

    if (osrelease == NULL)
        return NULL;

    version = calloc(10, sizeof(char));
    if (version == NULL)
        return NULL;

    for (int i = 0; i < 10; i++) {
    	// old versions contained a - in the name  e.g. 0.4-RELEASE
        if (osrelease[i] == '\0' || osrelease[i] == '-')
            break;

        version[i] = osrelease[i];
    }

    version[3] = '\0'; /* force major version only for now */

    return version;
}


MPORT_PUBLIC_API char *
mport_version(void)
{
    char *version;
    char *osrel = mport_get_osrelease();
    asprintf(&version, "mport %s for MidnightBSD %s, Bundle Version %s\n",
             MPORT_VERSION, osrel, MPORT_BUNDLE_VERSION_STR);
    free(osrel);

    return version;
}
