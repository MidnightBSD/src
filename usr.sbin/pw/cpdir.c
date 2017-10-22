/*-
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>

#include "pw.h"
#include "pwupd.h"

void
copymkdir(char const * dir, char const * skel, mode_t mode, uid_t uid, gid_t gid)
{
	char            src[MAXPATHLEN];
	char            dst[MAXPATHLEN];
	char            lnk[MAXPATHLEN];
	int             len;

	if (mkdir(dir, mode) != 0 && errno != EEXIST) {
		warn("mkdir(%s)", dir);
	} else {
		int             infd, outfd;
		struct stat     st;

		static char     counter = 0;
		static char    *copybuf = NULL;

		++counter;
		chown(dir, uid, gid);
		if (skel != NULL && *skel != '\0') {
			DIR            *d = opendir(skel);

			if (d != NULL) {
				struct dirent  *e;

				while ((e = readdir(d)) != NULL) {
					char           *p = e->d_name;

					if (snprintf(src, sizeof(src), "%s/%s", skel, p) >= (int)sizeof(src))
						warn("warning: pathname too long '%s/%s' (skel not copied)", skel, p);
					else if (lstat(src, &st) == 0) {
						if (strncmp(p, "dot.", 4) == 0)	/* Conversion */
							p += 3;
						if (snprintf(dst, sizeof(dst), "%s/%s", dir, p) >= (int)sizeof(dst))
							warn("warning: path too long '%s/%s' (skel file skipped)", dir, p);
						else {
						    if (S_ISDIR(st.st_mode)) {	/* Recurse for this */
							if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0)
								copymkdir(dst, src, st.st_mode & _DEF_DIRMODE, uid, gid);
								chflags(dst, st.st_flags);	/* propogate flags */
						    } else if (S_ISLNK(st.st_mode) && (len = readlink(src, lnk, sizeof(lnk))) != -1) {
							lnk[len] = '\0';
							symlink(lnk, dst);
							lchown(dst, uid, gid);
							/*
							 * Note: don't propogate special attributes
							 * but do propogate file flags
							 */
						    } else if (S_ISREG(st.st_mode) && (outfd = open(dst, O_RDWR | O_CREAT | O_EXCL, st.st_mode)) != -1) {
							if ((infd = open(src, O_RDONLY)) == -1) {
								close(outfd);
								remove(dst);
							} else {
								int             b;

								/*
								 * Allocate our copy buffer if we need to
								 */
								if (copybuf == NULL)
									copybuf = malloc(4096);
								while ((b = read(infd, copybuf, 4096)) > 0)
									write(outfd, copybuf, b);
								close(infd);
								/*
								 * Propogate special filesystem flags
								 */
								fchown(outfd, uid, gid);
								fchflags(outfd, st.st_flags);
								close(outfd);
								chown(dst, uid, gid);
							}
						    }
						}
					}
				}
				closedir(d);
			}
		}
		if (--counter == 0 && copybuf != NULL) {
			free(copybuf);
			copybuf = NULL;
		}
	}
}

