/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/tools/tools/recoverdisk/recoverdisk.c,v 1.4.4.1 2005/12/15 03:50:03 sobomax Exp $
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/stat.h>

#define BIGSIZE		(1024 * 1024)
#define MEDIUMSIZE	(64 * 1024)
#define MINSIZE		(512)

struct lump {
	off_t			start;
	off_t			len;
	int			state;
	TAILQ_ENTRY(lump)	list;
};

static TAILQ_HEAD(, lump) lumps = TAILQ_HEAD_INITIALIZER(lumps);


static void
new_lump(off_t start, off_t len, int state)
{
	struct lump *lp;

	lp = malloc(sizeof *lp);
	if (lp == NULL)
		err(1, "Malloc failed");
	lp->start = start;
	lp->len = len;
	lp->state = state;
	TAILQ_INSERT_TAIL(&lumps, lp, list);
}

int
main(int argc, const char **argv)
{
	int fdr, fdw;
	struct lump *lp;
	off_t 	t, d;
	size_t i, j;
	int error, flags;
	u_char *buf;
	u_int sectorsize, minsize;
	time_t t1, t2;
	struct stat sb;


	if (argc < 2)
		errx(1, "Usage: %s source-drive [destination]", argv[0]);

	buf = malloc(BIGSIZE);
	if (buf == NULL)
		err(1, "Cannot allocate %d bytes buffer", BIGSIZE);
	fdr = open(argv[1], O_RDONLY);
	if (fdr < 0)
		err(1, "Cannot open read descriptor %s", argv[1]);

	error = fstat(fdr, &sb);
	if (error < 0)
		err(1, "fstat failed");
	flags = O_WRONLY;
	if (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)) {
		error = ioctl(fdr, DIOCGSECTORSIZE, &sectorsize);
		if (error < 0)
			err(1, "DIOCGSECTORSIZE failed");
		minsize = sectorsize;

		error = ioctl(fdr, DIOCGMEDIASIZE, &t);
		if (error < 0)
			err(1, "DIOCGMEDIASIZE failed");
	} else {
		sectorsize = 1;
		t = sb.st_size;
		minsize = MINSIZE;
		flags |= O_CREAT | O_TRUNC;
	}

	if (argc > 2) {
		fdw = open(argv[2], flags, DEFFILEMODE);
		if (fdw < 0)
			err(1, "Cannot open write descriptor %s", argv[2]);
	} else {
		fdw = -1;
	}

	new_lump(0, t, 0);
	d = 0;

	t1 = 0;
	for (;;) {
		lp = TAILQ_FIRST(&lumps);
		if (lp == NULL)
			break;
		TAILQ_REMOVE(&lumps, lp, list);
		while (lp->len > 0) {
			i = BIGSIZE;
			if (lp->len < BIGSIZE)
				i = lp->len;
			if (lp->state == 1)
				i = MEDIUMSIZE;
			if (lp->state > 1)
				i = minsize;
			time(&t2);
			if (t1 != t2 || lp->len < BIGSIZE) {
				printf("\r%13jd %7zu %13jd %3d %13jd %13jd %.8f",
				    (intmax_t)lp->start,
				    i, 
				    (intmax_t)lp->len,
				    lp->state,
				    (intmax_t)d,
				    (intmax_t)(t - d),
				    (double)d/(double)t);
				t1 = t2;
			}
			if (i == 0) {
				errx(1, "BOGUS i %10jd", (intmax_t)i);
			}
			fflush(stdout);
			j = pread(fdr, buf, i, lp->start);
			if (j == i) {
				d += i;
				if (fdw >= 0)
					j = pwrite(fdw, buf, i, lp->start);
				else
					j = i;
				if (j != i)
					printf("\nWrite error at %jd/%zu\n",
					    lp->start, i);
				lp->start += i;
				lp->len -= i;
				continue;
			}
			printf("\n%jd %zu failed %d\n", lp->start, i, errno);
			new_lump(lp->start, i, lp->state + 1);
			lp->start += i;
			lp->len -= i;
		}
		free(lp);
	}
	printf("\nCompleted\n");
	exit (0);
}

