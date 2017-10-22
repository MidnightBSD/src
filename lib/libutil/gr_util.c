/*-
 * Copyright (c) 2008 Sean C. Farley <scf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct group_storage {
	struct group	 gr;
	char		*members[];
};

static int lockfd = -1;
static char group_dir[PATH_MAX];
static char group_file[PATH_MAX];
static char tempname[PATH_MAX];
static int initialized;

static const char group_line_format[] = "%s:%s:%ju:";

/*
 * Initialize statics
 */
int
gr_init(const char *dir, const char *group)
{
	if (dir == NULL) {
		strcpy(group_dir, _PATH_ETC);
	} else {
		if (strlen(dir) >= sizeof(group_dir)) {
			errno = ENAMETOOLONG;
			return (-1);
		}
		strcpy(group_dir, dir);
	}

	if (group == NULL) {
		if (dir == NULL) {
			strcpy(group_file, _PATH_GROUP);
		} else if (snprintf(group_file, sizeof(group_file), "%s/group",
			group_dir) > (int)sizeof(group_file)) {
			errno = ENAMETOOLONG;
			return (-1);
		}
	} else {
		if (strlen(group) >= sizeof(group_file)) {
			errno = ENAMETOOLONG;
			return (-1);
		}
		strcpy(group_file, group);
	}
	initialized = 1;
	return (0);
}

/*
 * Lock the group file
 */
int
gr_lock(void)
{
	if (*group_file == '\0')
		return (-1);

	for (;;) {
		struct stat st;

		lockfd = open(group_file, O_RDONLY, 0);
		if (lockfd < 0 || fcntl(lockfd, F_SETFD, 1) == -1)
			err(1, "%s", group_file);
		if (flock(lockfd, LOCK_EX|LOCK_NB) == -1) {
			if (errno == EWOULDBLOCK) {
				errx(1, "the group file is busy");
			} else {
				err(1, "could not lock the group file: ");
			}
		}
		if (fstat(lockfd, &st) == -1)
			err(1, "fstat() failed: ");
		if (st.st_nlink != 0)
			break;
		close(lockfd);
		lockfd = -1;
	}
	return (lockfd);
}

/*
 * Create and open a presmuably safe temp file for editing group data
 */
int
gr_tmp(int mfd)
{
	char buf[8192];
	ssize_t nr;
	const char *p;
	int tfd;

	if (*group_file == '\0')
		return (-1);
	if ((p = strrchr(group_file, '/')))
		++p;
	else
		p = group_file;
	if (snprintf(tempname, sizeof(tempname), "%.*sgroup.XXXXXX",
		(int)(p - group_file), group_file) >= (int)sizeof(tempname)) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	if ((tfd = mkstemp(tempname)) == -1)
		return (-1);
	if (mfd != -1) {
		while ((nr = read(mfd, buf, sizeof(buf))) > 0)
			if (write(tfd, buf, (size_t)nr) != nr)
				break;
		if (nr != 0) {
			unlink(tempname);
			*tempname = '\0';
			close(tfd);
			return (-1);
		}
	}
	return (tfd);
}

/*
 * Copy the group file from one descriptor to another, replacing, deleting
 * or adding a single record on the way.
 */
int
gr_copy(int ffd, int tfd, const struct group *gr, struct group *old_gr)
{
	char buf[8192], *end, *line, *p, *q, *r, t;
	struct group *fgr;
	const struct group *sgr;
	size_t len;
	int eof, readlen;

	sgr = gr;
	if (gr == NULL) {
		line = NULL;
		if (old_gr == NULL)
			return (-1);
		sgr = old_gr;
	} else if ((line = gr_make(gr)) == NULL)
		return (-1);

	eof = 0;
	len = 0;
	p = q = end = buf;
	for (;;) {
		/* find the end of the current line */
		for (p = q; q < end && *q != '\0'; ++q)
			if (*q == '\n')
				break;

		/* if we don't have a complete line, fill up the buffer */
		if (q >= end) {
			if (eof)
				break;
			if ((size_t)(q - p) >= sizeof(buf)) {
				warnx("group line too long");
				errno = EINVAL; /* hack */
				goto err;
			}
			if (p < end) {
				q = memmove(buf, p, end -p);
				end -= p - buf;
			} else {
				p = q = end = buf;
			}
			readlen = read(ffd, end, sizeof(buf) - (end -buf));
			if (readlen == -1)
				goto err;
			else
				len = (size_t)readlen;
			if (len == 0 && p == buf)
				break;
			end += len;
			len = end - buf;
			if (len < (ssize_t)sizeof(buf)) {
				eof = 1;
				if (len > 0 && buf[len -1] != '\n')
					++len, *end++ = '\n';
			}
			continue;
		}

		/* is it a blank line or a comment? */
		for (r = p; r < q && isspace(*r); ++r)
			/* nothing */;
		if (r == q || *r == '#') {
			/* yep */
			if (write(tfd, p, q -p + 1) != q - p + 1)
				goto err;
			++q;
			continue;
		}

		/* is it the one we're looking for? */

		t = *q;
		*q = '\0';

		fgr = gr_scan(r);

		/* fgr is either a struct group for the current line,
		 * or NULL if the line is malformed.
		 */

		*q = t;
		if (fgr == NULL || fgr->gr_gid != sgr->gr_gid) {
			/* nope */
			if (fgr != NULL)
				free(fgr);
			if (write(tfd, p, q - p + 1) != q - p + 1)
				goto err;
			++q;
			continue;
		}
		if (old_gr && !gr_equal(fgr, old_gr)) {
			warnx("entry inconsistent");
			free(fgr);
			errno = EINVAL; /* hack */
			goto err;
		}
		free(fgr);

		/* it is, replace or remove it */
		if (line != NULL) {
			len = strlen(line);
			if (write(tfd, line, len) != (int) len)
				goto err;
		} else {
			/* when removed, avoid the \n */
			q++;
		}
		/* we're done, just copy the rest over */
		for (;;) {
			if (write(tfd, q, end - q) != end - q)
				goto err;
			q = buf;
			readlen = read(ffd, buf, sizeof(buf));
			if (readlen == 0)
				break;
			else
				len = (size_t)readlen;
			if (readlen == -1)
				goto err;
			end = buf + len;
		}
		goto done;
	}

	/* if we got here, we didn't find the old entry */
	if (line == NULL) {
		errno = ENOENT;
		goto err;
	}
	len = strlen(line);
	if ((size_t)write(tfd, line, len) != len ||
	   write(tfd, "\n", 1) != 1)
		goto err;
 done:
	if (line != NULL)
		free(line);
	return (0);
 err:
	if (line != NULL)
		free(line);
	return (-1);
}

/*
 * Regenerate the group file
 */
int
gr_mkdb(void)
{
	return (rename(tempname, group_file));
}

/*
 * Clean up. Preserver errno for the caller's convenience.
 */
void
gr_fini(void)
{
	int serrno;

	if (!initialized)
		return;
	initialized = 0;
	serrno = errno;
	if (*tempname != '\0') {
		unlink(tempname);
		*tempname = '\0';
	}
	if (lockfd != -1)
		close(lockfd);
	errno = serrno;
}

/*
 * Compares two struct group's.
 */
int
gr_equal(const struct group *gr1, const struct group *gr2)
{
	int gr1_ndx;
	int gr2_ndx;
	bool found;

	/* Check that the non-member information is the same. */
	if (gr1->gr_name == NULL || gr2->gr_name == NULL) {
		if (gr1->gr_name != gr2->gr_name)
			return (false);
	} else if (strcmp(gr1->gr_name, gr2->gr_name) != 0)
		return (false);
	if (gr1->gr_passwd == NULL || gr2->gr_passwd == NULL) {
		if (gr1->gr_passwd != gr2->gr_passwd)
			return (false);
	} else if (strcmp(gr1->gr_passwd, gr2->gr_passwd) != 0)
		return (false);
	if (gr1->gr_gid != gr2->gr_gid)
		return (false);

	/* Check all members in both groups. */
	if (gr1->gr_mem == NULL || gr2->gr_mem == NULL) {
		if (gr1->gr_mem != gr2->gr_mem)
			return (false);
	} else {
		for (found = false, gr1_ndx = 0; gr1->gr_mem[gr1_ndx] != NULL;
		    gr1_ndx++) {
			for (gr2_ndx = 0; gr2->gr_mem[gr2_ndx] != NULL;
			    gr2_ndx++)
				if (strcmp(gr1->gr_mem[gr1_ndx],
				    gr2->gr_mem[gr2_ndx]) == 0) {
					found = true;
					break;
				}
			if (!found)
				return (false);
		}

		/* Check that group2 does not have more members than group1. */
		if (gr2->gr_mem[gr1_ndx] != NULL)
			return (false);
	}

	return (true);
}

/*
 * Make a group line out of a struct group.
 */
char *
gr_make(const struct group *gr)
{
	char *line;
	size_t line_size;
	int ndx;

	/* Calculate the length of the group line. */
	line_size = snprintf(NULL, 0, group_line_format, gr->gr_name,
	    gr->gr_passwd, (uintmax_t)gr->gr_gid) + 1;
	if (gr->gr_mem != NULL) {
		for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++)
			line_size += strlen(gr->gr_mem[ndx]) + 1;
		if (ndx > 0)
			line_size--;
	}

	/* Create the group line and fill it. */
	if ((line = malloc(line_size)) == NULL)
		return (NULL);
	snprintf(line, line_size, group_line_format, gr->gr_name, gr->gr_passwd,
	    (uintmax_t)gr->gr_gid);
	if (gr->gr_mem != NULL)
		for (ndx = 0; gr->gr_mem[ndx] != NULL; ndx++) {
			strcat(line, gr->gr_mem[ndx]);
			if (gr->gr_mem[ndx + 1] != NULL)
				strcat(line, ",");
		}

	return (line);
}

/*
 * Duplicate a struct group.
 */
struct group *
gr_dup(const struct group *gr)
{
	char *dst;
	size_t len;
	struct group_storage *gs;
	int ndx;
	int num_mem;

	/* Calculate size of the group. */
	len = sizeof(*gs);
	if (gr->gr_name != NULL)
		len += strlen(gr->gr_name) + 1;
	if (gr->gr_passwd != NULL)
		len += strlen(gr->gr_passwd) + 1;
	if (gr->gr_mem != NULL) {
		for (num_mem = 0; gr->gr_mem[num_mem] != NULL; num_mem++)
			len += strlen(gr->gr_mem[num_mem]) + 1;
		len += (num_mem + 1) * sizeof(*gr->gr_mem);
	} else
		num_mem = -1;

	/* Create new group and copy old group into it. */
	if ((gs = calloc(1, len)) == NULL)
		return (NULL);
	dst = (char *)&gs->members[num_mem + 1];
	if (gr->gr_name != NULL) {
		gs->gr.gr_name = dst;
		dst = stpcpy(gs->gr.gr_name, gr->gr_name) + 1;
	}
	if (gr->gr_passwd != NULL) {
		gs->gr.gr_passwd = dst;
		dst = stpcpy(gs->gr.gr_passwd, gr->gr_passwd) + 1;
	}
	gs->gr.gr_gid = gr->gr_gid;
	if (gr->gr_mem != NULL) {
		gs->gr.gr_mem = gs->members;
		for (ndx = 0; ndx < num_mem; ndx++) {
			gs->gr.gr_mem[ndx] = dst;
			dst = stpcpy(gs->gr.gr_mem[ndx], gr->gr_mem[ndx]) + 1;
		}
		gs->gr.gr_mem[ndx] = NULL;
	}

	return (&gs->gr);
}

/*
 * Scan a line and place it into a group structure.
 */
static bool
__gr_scan(char *line, struct group *gr)
{
	char *loc;
	int ndx;

	/* Assign non-member information to structure. */
	gr->gr_name = line;
	if ((loc = strchr(line, ':')) == NULL)
		return (false);
	*loc = '\0';
	gr->gr_passwd = loc + 1;
	if (*gr->gr_passwd == ':')
		*gr->gr_passwd = '\0';
	else {
		if ((loc = strchr(loc + 1, ':')) == NULL)
			return (false);
		*loc = '\0';
	}
	if (sscanf(loc + 1, "%u", &gr->gr_gid) != 1)
		return (false);

	/* Assign member information to structure. */
	if ((loc = strchr(loc + 1, ':')) == NULL)
		return (false);
	line = loc + 1;
	gr->gr_mem = NULL;
	ndx = 0;
	do {
		gr->gr_mem = reallocf(gr->gr_mem, sizeof(*gr->gr_mem) *
		    (ndx + 1));
		if (gr->gr_mem == NULL)
			return (false);

		/* Skip locations without members (i.e., empty string). */
		do {
			gr->gr_mem[ndx] = strsep(&line, ",");
		} while (gr->gr_mem[ndx] != NULL && *gr->gr_mem[ndx] == '\0');
	} while (gr->gr_mem[ndx++] != NULL);

	return (true);
}

/*
 * Create a struct group from a line.
 */
struct group *
gr_scan(const char *line)
{
	struct group gr;
	char *line_copy;
	struct group *new_gr;

	if ((line_copy = strdup(line)) == NULL)
		return (NULL);
	if (!__gr_scan(line_copy, &gr)) {
		free(line_copy);
		return (NULL);
	}
	new_gr = gr_dup(&gr);
	free(line_copy);
	if (gr.gr_mem != NULL)
		free(gr.gr_mem);

	return (new_gr);
}
